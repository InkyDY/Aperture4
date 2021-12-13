/*
 * Copyright (c) 2021 Alex Chen.
 * This file is part of Aperture (https://github.com/fizban007/Aperture4.git).
 *
 * Aperture is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * Aperture is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IC_RADIATION_SCHEME_H_
#define IC_RADIATION_SCHEME_H_

#include "core/cuda_control.h"
#include "core/particle_structs.h"
#include "core/random.h"
#include "data/fields.h"
#include "data/multi_array_data.hpp"
#include "framework/environment.h"
#include "systems/grid.h"
#include "systems/inverse_compton.h"
#include "systems/physics/ic_scattering.hpp"
#include "systems/physics/spectra.hpp"

namespace Aperture {

template <typename Conf>
struct IC_radiation_scheme {
  using value_t = typename Conf::value_t;

  const grid_t<Conf> &m_grid;
  ic_scatter_t m_ic_module;
  int m_num_bins = 256;
  float m_lim_lower = 1.0e-4;
  float m_lim_upper = 1.0e4;
  int m_downsample = 16;
  value_t m_IC_compactness = 0.0;
  mutable ndptr<value_t, Conf::dim> m_IC_loss;
  ndptr<float, Conf::dim + 1> m_spec_ptr;

  IC_radiation_scheme(const grid_t<Conf> &grid) : m_grid(grid) {}

  void init() {
    value_t emin = 1.0e-5;
    sim_env().params().get_value("IC_emin", emin);
    value_t ic_alpha = 1.25;
    sim_env().params().get_value("IC_alpha", ic_alpha);
    value_t bb_kT = 1e-5;
    sim_env().params().get_value("IC_bb_kT", bb_kT);
    std::string spec_type = "soft_power_law";
    sim_env().params().get_value("IC_spectrum", spec_type);
    sim_env().params().get_value("IC_compactness", m_IC_compactness);

    // Configure the spectrum here and initialize the ic module
    // Spectra::black_body spec(bb_kT);

    auto ic = sim_env().register_system<inverse_compton_t>();
    // ic->compute_coefficients(spec, spec.emin(), spec.emax(), 1.5e24 /
    // ic_path);
    Logger::print_info("Using background spectrum {}", spec_type);
    if (spec_type == "soft_power_law") {
      Spectra::power_law_soft spec(ic_alpha, emin, 1.0);
      ic->compute_coefficients(spec, spec.emin(), spec.emax());
    } else if (spec_type == "black_body") {
      Spectra::black_body spec(bb_kT);
      ic->compute_coefficients(spec, spec.emin(), spec.emax());
    } else {
      Logger::err("Spectrum type {} is not recognized!", spec_type);
      exit(1);
    }

    m_ic_module = ic->get_ic_module();

    sim_env().params().get_value("ph_num_bins", m_num_bins);
    sim_env().params().get_value("ph_spec_lower", m_lim_lower);
    sim_env().params().get_value("ph_spec_upper", m_lim_upper);
    sim_env().params().get_value("momentum_downsample", m_downsample);

    m_lim_lower = math::log(m_lim_lower);
    m_lim_upper = math::log(m_lim_upper);

    // Scalar field to tally up the low energy IC loss not accounted for using
    // photon spectrum
    auto IC_loss = sim_env().register_data<scalar_field<Conf>>(
        "IC_loss", this->m_grid, field_type::cell_centered,
        MemType::host_device);
    m_IC_loss = IC_loss->dev_ndptr();
    IC_loss->reset_after_output(true);

    extent_t<Conf::dim + 1> ext;
    for (int i = 0; i < Conf::dim; i++) {
      ext[i + 1] = m_grid.N[i] / m_downsample;
    }
    ext[0] = m_num_bins;
    ext.get_strides();

    auto photon_dist =
        sim_env().register_data<multi_array_data<float, Conf::dim + 1>>(
            "photon_spectrum", ext, MemType::host_device);
    m_spec_ptr = photon_dist->dev_ndptr();
    photon_dist->reset_after_output(true);
  }

  HOST_DEVICE size_t emit_photon(const Grid<Conf::dim, value_t> &grid,
                                 const extent_t<Conf::dim> &ext, ptc_ptrs &ptc,
                                 size_t tid, ph_ptrs &ph, size_t ph_num,
                                 unsigned long long int *ph_pos, rng_t &rng,
                                 value_t dt) {
    using idx_t = default_idx_t<Conf::dim + 1>;
    value_t gamma = ptc.E[tid];
    value_t p1 = ptc.p1[tid];
    value_t p2 = ptc.p2[tid];
    value_t p3 = ptc.p3[tid];
    auto flag = ptc.flag[tid];

    if (check_flag(flag, PtcFlag::ignore_radiation)) {
      return 0;
    }

    // We don't care too much about the radiation from lowest energy particles.
    // Just cool them using usual formula
    if (gamma < 2.0f) {
      if (gamma < 1.0001) return 0;

      ptc.p1[tid] -=
          (4.0f / 3.0f) * m_IC_compactness * gamma * ptc.p1[tid] * dt;
      ptc.p2[tid] -=
          (4.0f / 3.0f) * m_IC_compactness * gamma * ptc.p2[tid] * dt;
      ptc.p3[tid] -=
          (4.0f / 3.0f) * m_IC_compactness * gamma * ptc.p3[tid] * dt;

      ptc.E[tid] = math::sqrt(1.0f + square(ptc.p1[tid]) + square(ptc.p2[tid]) +
                              square(ptc.p3[tid]));

      // Since this part of cooling is not accounted for in the photon spectrum,
      // we need to accumulate it separately
      auto idx = Conf::idx(ptc.cell[tid], ext);
      atomic_add(&m_IC_loss[idx],
                 ptc.weight[tid] * max(gamma - ptc.E[tid], 0.0));

      return 0;
    }

    value_t p_i = math::sqrt(square(p1) + square(p2) + square(p3));
    auto cell = ptc.cell[tid];
    auto idx = Conf::idx(cell, ext);
    auto pos = get_pos(idx, ext);

    auto ext_out = grid.extent_less() / m_downsample;
    auto ext_spec = extent_t<Conf::dim + 1>(m_num_bins, ext_out);
    index_t<Conf::dim + 1> pos_out(0, (pos - grid.guards()) / m_downsample);

    value_t lambda = m_ic_module.ic_scatter_rate(gamma) * dt;
    // printf("ic_prob is %f\n", ic_prob);
    int num_scattering = rng.poisson(lambda);

    // printf("num_scattering is %f, e_ph / gamma is %f\n", weight, e_ph /
    // gamma);
    for (int i = 0; i < num_scattering; i++) {
      value_t e_ph = m_ic_module.gen_photon_e(gamma, rng) * gamma;
      // value_t e_ph = m_ic_module.e_mean * gamma * gamma;
      if (e_ph <= 0.0) {
        e_ph = 0.0;
        continue;
      }
      if (e_ph > gamma - 1.0001) {
        e_ph = abs(gamma - 1.0001);
        num_scattering = 0;
      }
      gamma -= abs(e_ph);

      // add energy loss no matter what, for debug purposes
      // atomic_add(&m_IC_loss[idx], ptc.weight[tid] * max(e_ph, 0.0));

      value_t log_e_ph = clamp(math::log(max(e_ph, math::exp(m_lim_lower))),
                               m_lim_lower, m_lim_upper);
      int bin = round((log_e_ph - m_lim_lower) / (m_lim_upper - m_lim_lower) *
                      (m_num_bins - 1));
      pos_out[0] = bin;
      atomic_add(&m_spec_ptr[idx_t(pos_out, ext_spec)], ptc.weight[tid]);
    }

    // value_t e_ph = m_ic_module.gen_photon_e(gamma, rng) * gamma;
    // gamma -= e_ph * num_scattering;

    value_t new_p = math::sqrt(max(square(gamma) - 1.0f, 0.0f));

    ptc.p1[tid] = p1 * new_p / p_i;
    ptc.p2[tid] = p2 * new_p / p_i;
    ptc.p3[tid] = p3 * new_p / p_i;
    ptc.E[tid] = gamma;

    return 0;
  }

  HOST_DEVICE size_t produce_pair(const Grid<Conf::dim, value_t> &grid,
                                  const extent_t<Conf::dim> &ext, ph_ptrs &ph,
                                  size_t tid, ptc_ptrs &ptc, size_t ptc_num,
                                  unsigned long long int *ptc_pos, rng_t &rng,
                                  value_t dt) {
    return 0;
  }
};

}  // namespace Aperture

#endif  // IC_RADIATION_SCHEME_H_
