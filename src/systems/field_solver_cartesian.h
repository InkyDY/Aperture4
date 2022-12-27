/*
 * Copyright (c) 2022 Alex Chen.
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

#ifndef _FIELD_SOLVER_CARTESIAN_H_
#define _FIELD_SOLVER_CARTESIAN_H_


#include "field_solver.h"
#include "systems/policies/coord_policy_cartesian.hpp"

namespace Aperture {

// System that updates Maxwell equations
template <typename Conf, template <class> class ExecPolicy>
class field_solver<Conf, ExecPolicy, coord_policy_cartesian>
    : public field_solver_base<Conf> {
 public:
  field_solver(const grid_t<Conf>& grid,
               const domain_comm<Conf, ExecPolicy>* comm = nullptr)
      : field_solver_base<Conf>(grid), m_comm(comm) {}

  virtual ~field_solver() {}

  virtual void init() override;
  virtual void register_data_components() override;
  virtual void update_explicit(double dt, double time) override;
  // Note Cartesian semi-implicit update is not very compatible with pml, so we
  // don't really implement it
  virtual void update_semi_implicit(double dt, double alpha, double beta,
                                    double time) override;

  void compute_e_update_pml(double dt);
  void compute_b_update_pml(double dt);
  virtual void compute_divs_e_b() override;
  virtual void compute_flux() override;
  virtual void compute_EB_sqr() override;

 protected:
  const domain_comm<Conf, ExecPolicy>* m_comm;

  // Option to use pml for Cartesian field solver
  bool m_damping[Conf::dim * 2] = {};
  int m_pml_length = 16;

  // These are temporary fields required for pml
  nonown_ptr<vector_field<Conf>> m_tmp_b1, m_tmp_b2, m_tmp_e1, m_tmp_e2;
};

}  // namespace Aperture


#endif  // _FIELD_SOLVER_CARTESIAN_H_