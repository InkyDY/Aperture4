#include "fields.hpp"
#include "framework/config.h"
#include "framework/environment.hpp"
#include "systems/grid.h"
#include <exception>

namespace Aperture {

template <int N, typename Conf>
field_t<N, Conf>::field_t(const Grid<Conf::dim>& grid) {
  resize(grid);
}

template <int N, typename Conf>
field_t<N, Conf>::field_t(const Grid<Conf::dim>& grid,
                          const std::array<stagger_t, N> st)
    : m_stagger(st) {
  resize(grid);
}

template <int N, typename Conf>
field_t<N, Conf>::field_t(const Grid<Conf::dim>& grid,
                          field_type type) {
  if (type == field_type::face_centered) {
    m_stagger[0] = stagger_t(0b001);
    m_stagger[1] = stagger_t(0b010);
    m_stagger[2] = stagger_t(0b100);
  } else if (type == field_type::edge_centered) {
    m_stagger[0] = stagger_t(0b110);
    m_stagger[1] = stagger_t(0b101);
    m_stagger[2] = stagger_t(0b011);
  } else if (type == field_type::cell_centered) {
    m_stagger[0] = m_stagger[1] = m_stagger[2] = stagger_t(0b000);
  } else if (type == field_type::vert_centered) {
    m_stagger[0] = m_stagger[1] = m_stagger[2] = stagger_t(0b111);
  }
  resize(grid);
}

template <int N, typename Conf>
void
field_t<N, Conf>::resize(const Grid<Conf::dim>& grid) {
  m_grid = &grid;
  for (int i = 0; i < N; i++) {
    m_data[i].resize(m_grid->extent());
  }
}

///////////////////////////////////////////////////////
// Explicitly instantiate some fields
template class field_t<3, Config<1, float>>;
template class field_t<3, Config<2, float>>;
template class field_t<3, Config<3, float>>;
template class field_t<3, Config<1, double>>;
template class field_t<3, Config<2, double>>;
template class field_t<3, Config<3, double>>;

template class field_t<1, Config<1, float>>;
template class field_t<1, Config<2, float>>;
template class field_t<1, Config<3, float>>;
template class field_t<1, Config<1, double>>;
template class field_t<1, Config<2, double>>;
template class field_t<1, Config<3, double>>;

}  // namespace Aperture
