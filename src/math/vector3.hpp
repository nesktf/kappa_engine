#pragma once

#include "./common.hpp"
#include <utility>

namespace kappa {

template<math::numeric_type T>
struct VecNum<3, T> {
public:
  using value_type = T;
  static constexpr u32 component_count = 3;

public:
  KA_MATH_DECLARE_VECTOR_SPECIAL_MEMBERS(VecNum);

  KA_MATH_DEF VecNum(T x_, T y_, T z_) noexcept : x(x_), y(y_), z(z_) {}

  KA_MATH_DEF explicit VecNum(T scalar) noexcept : x(scalar), y(scalar), z(scalar) {}

  template<math::numeric_convertible<T> U>
  KA_MATH_DEF explicit VecNum(U scalar) noexcept :
      x(static_cast<T>(scalar)), y(static_cast<T>(scalar)), z(static_cast<T>(scalar)) {}

  template<math::numeric_convertible<T> X, math::numeric_convertible<T> Y,
           math::numeric_convertible<T> Z>
  KA_MATH_DEF VecNum(X x_, Y y_, Z z_) noexcept :
      x(static_cast<T>(x_)), y(static_cast<T>(y_)), z(static_cast<T>(z_)) {}

public:
  KA_MATH_DEF const T& operator[](size_t idx) const noexcept {
    switch (idx) {
      default:
        [[fallthrough]];
      case 0:
        return x;
      case 1:
        return y;
      case 2:
        return z;
    }
  }

  KA_MATH_DEF T& operator[](size_t idx) noexcept {
    return const_cast<T&>(std::as_const(*this)[idx]);
  }

public:
  KA_MATH_DEF const T* data() const noexcept { return &this->x; }

  KA_MATH_DEF T* data() noexcept { return const_cast<T*>(std::as_const(*this).data()); }

public:
  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator+=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator-=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator*=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator/=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator=(const VecNum<3, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator+=(const VecNum<3, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator-=(const VecNum<3, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator*=(const VecNum<3, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator/=(const VecNum<3, U>& other) noexcept;

public:
  T x, y, z;
};

template<typename T>
KA_MATH_DECL bool operator==(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept;

template<typename T>
KA_MATH_DECL bool operator!=(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> operator+(const VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> operator+(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator+(const VecNum<3, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator+(U scalar, const VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> operator-(const VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> operator-(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator-(const VecNum<3, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator-(U scalar, const VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> operator*(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator*(const VecNum<3, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator*(U scalar, const VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> operator/(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator/(const VecNum<3, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator/(U scalar, const VecNum<3, T>& vec) noexcept;

template<typename U, typename T>
requires(math::numeric_convertible<U, T>)
KA_MATH_DECL VecNum<3, U> vec_cast(const VecNum<3, T>& vec) noexcept;

} // namespace kappa

namespace kappa::math {

template<typename T>
KA_MATH_DECL T length2(const VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL T length(const VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL void normalize_at(VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> normalize(const VecNum<3, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL T dot(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> cross(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept;

template<numeric_type T>
KA_MATH_DECL void gl_to_cartesian_at(VecNum<3, T>& vec) noexcept;

template<numeric_type T>
KA_MATH_DECL VecNum<3, T> gl_to_cartesian(const VecNum<3, T>& vec) noexcept;

template<numeric_type T>
KA_MATH_DECL VecNum<3, T> sph_to_cartesian(T rho, T theta, T phi) noexcept;

template<numeric_type T>
KA_MATH_DECL void cartesian_to_gl_at(VecNum<3, T>& vec) noexcept;

template<numeric_type T>
KA_MATH_DECL VecNum<3, T> cartesian_to_gl(const VecNum<3, T>& vec) noexcept;

} // namespace kappa::math

#ifndef KA_MATH_VECTOR3_INL
#include "./vector3.inl"
#endif
