#ifndef __VEC_H_
#define __VEC_H_

#include "core/cuda_control.h"
#include "utils/util_functions.h"
#include <cmath>
#include <iostream>

namespace Aperture {

template <typename T, int Rank>
class vec_t {
 private:
  T memory[Rank];

 public:
  typedef vec_t<T, Rank> self_type;

  HOST_DEVICE vec_t() {}
  template <typename... Args>
  HOST_DEVICE vec_t(Args... args) : memory{T(args)...} {}
  HOST_DEVICE ~vec_t() {}

  HD_INLINE T& operator[](std::size_t n) { return memory[n]; }
  HD_INLINE const T& operator[](std::size_t n) const {
    return memory[n];
  }

  HD_INLINE bool operator<(const self_type& other) const {
#pragma unroll
    for (int i = 0; i < Rank; i++) {
      if (memory[i] >= other.memory[i]) return false;
    }
    return true;
  }

  HD_INLINE bool operator==(const self_type& other) const {
#pragma unroll
    for (int i = 0; i < Rank; i++) {
      if (memory[i] != other.memory[i]) return false;
    }
    return true;
  }

  HD_INLINE bool operator!=(const self_type& other) const {
    return !operator==(other);
  }

  HD_INLINE self_type& operator+=(const self_type& other) {
#pragma unroll
    for (int i = 0; i < Rank; i++)
      memory[i] += other.memory[i];
    return *this;
  }

  HD_INLINE self_type operator+(const self_type& other) const {
    self_type result = *this;
    result += other;
    return result;
  }

  HD_INLINE self_type& operator-=(const self_type& other) {
#pragma unroll
    for (int i = 0; i < Rank; i++)
      memory[i] -= other.memory[i];
    return *this;
  }

  HD_INLINE self_type operator-(const self_type& other) const {
    self_type result = *this;
    result -= other;
    return result;
  }

  HD_INLINE T dot(const self_type& other) const {
    T result = 0;
#pragma unroll
    for (int i = 0; i < Rank; i++)
      result += memory[i] * other.memory[i];
    return result;
  }

  constexpr int rank() const { return Rank; }

  T size() const {
    T result = memory[0];
#pragma unroll
    for (int i = 1; i < Rank; i++)
      result *= memory[i];
    return result;
  }
};

template <typename T, typename... Args>
HD_INLINE auto
vec(Args... args) {
  return vec_t<T, sizeof...(Args)>(T(args)...);
}

template <int Rank>
using extent_t = vec_t<uint32_t, Rank>;

template <int Rank>
using index_t = vec_t<uint32_t, Rank>;

template <int Rank>
bool not_power_of_two(const extent_t<Rank>& ext) {
  for (int i = 0; i < Rank; i++) {
    if (not_power_of_two(ext[i])) return true;
  }
  return false;
}

}  // namespace Aperture

#endif  // __VEC_H_