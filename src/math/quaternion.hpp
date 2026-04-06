#pragma once

#include "./common.hpp"
#include <utility>

namespace kappa {

template<math::numeric_type T>
struct Quat {
public:
  using value_type = T;
  static constexpr u32 component_count = 4;

  static constexpr Quat identity() noexcept { return Quat{T(1)}; }

public:
  KA_MATH_DECLARE_VECTOR_SPECIAL_MEMBERS(Quat);

  KA_MATH_DEF Quat(T w_, T x_, T y_, T z_) noexcept : w(w_), x(x_), y(y_), z(z_) {}

  template<math::numeric_convertible<T> W, math::numeric_convertible<T> X,
           math::numeric_convertible<T> Y, math::numeric_convertible<T> Z>
  KA_MATH_DEF Quat(W w_, X x_, Y y_, Z z_) noexcept :
      w(static_cast<T>(w_)), x(static_cast<T>(x_)), y(static_cast<T>(y_)), z(static_cast<T>(z_)) {}

  template<math::numeric_convertible<T> U>
  KA_MATH_DEF explicit Quat(const Quat<U>& other) noexcept :
      w(static_cast<T>(other.w)), x(static_cast<T>(other.x)), y(static_cast<T>(other.y)),
      z(static_cast<T>(other.z)) {}

  template<math::numeric_convertible<T> U, math::numeric_convertible<T> V>
  KA_MATH_DEF Quat(U w_, const VecNum<3, V>& v) noexcept :
      w(static_cast<T>(w_)), x(static_cast<T>(v.x)), y(static_cast<T>(v.y)),
      z(static_cast<T>(v.z)) {}

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL explicit Quat(const VecNum<3, U>& euler) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL explicit Quat(const Mat<3, 3, U>& m) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL explicit Quat(const Mat<4, 4, U>& m) noexcept;

public:
  KA_MATH_DEF const T& operator[](size_t idx) const noexcept {
    switch (idx) {
      default:
        [[fallthrough]];
      case 0:
        return w;
      case 1:
        return x;
      case 2:
        return y;
      case 3:
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
  KA_MATH_DECL Quat& operator=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL Quat& operator+=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL Quat& operator-=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL Quat& operator*=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL Quat& operator/=(U scalar) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL Quat& operator=(const Quat<U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL Quat& operator+=(const Quat<U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL Quat& operator-=(const Quat<U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL Quat& operator*=(const Quat<U>& other) noexcept;

public:
  T w, x, y, z;
};

template<typename T>
KA_MATH_DECL bool operator==(const Quat<T>& a, const Quat<T>& b) noexcept;

template<typename T>
KA_MATH_DECL bool operator!=(const Quat<T>& a, const Quat<T>& b) noexcept;

template<typename T>
KA_MATH_DECL Quat<T> operator+(const Quat<T>& q) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator+(const Quat<T>& a, const Quat<U>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator+(const Quat<T>& q, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator+(U scalar, const Quat<T>& q) noexcept;

template<typename T>
KA_MATH_DECL Quat<T> operator-(const Quat<T>& q) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator-(const Quat<T>& a, const Quat<U>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator-(const Quat<T>& q, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator-(U scalar, const Quat<T>& q) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator*(const Quat<T>& a, const Quat<U>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator*(const Quat<T>& q, const VecNum<3, U>& v) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<3, T> operator*(const VecNum<3, U>& v, const Quat<T>& q) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator*(const Quat<T>& q, const VecNum<4, U>& v) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator*(const VecNum<4, U>& v, const Quat<T>& q) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator*(const Quat<T>& q, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator*(U scalar, const Quat<T>& q) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator/(const Quat<T>& q, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> operator/(U scalar, const Quat<T>& q) noexcept;

template<typename U, typename T>
requires(math::numeric_convertible<U, T>)
KA_MATH_DECL Quat<U> vec_cast(const Quat<T>& q) noexcept;

} // namespace kappa

namespace kappa::math {

template<typename T>
KA_MATH_DECL Quat<T> conjugate(const Quat<T>& q) noexcept;

template<typename T>
KA_MATH_DECL Quat<T> inverse(const Quat<T>& q) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL T dot(const Quat<T>& a, const Quat<U>& b) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> to_euler(const Quat<T>& q) noexcept;

template<typename T>
KA_MATH_DECL T roll(const Quat<T>& q) noexcept;

template<typename T>
KA_MATH_DECL T pitch(const Quat<T>& q) noexcept;

template<typename T>
KA_MATH_DECL T yaw(const Quat<T>& q) noexcept;

template<typename T>
KA_MATH_DECL Mat<3, 3, T> to_mat3(const Quat<T>& q) noexcept;

template<typename T>
KA_MATH_DECL Mat<4, 4, T> to_mat4(const Quat<T>& q) noexcept;

template<typename T>
KA_MATH_DECL Quat<T> to_quat(const Mat<3, 3, T>& m) noexcept;

template<typename T>
KA_MATH_DECL Quat<T> to_quat(const Mat<4, 4, T>& m) noexcept;

template<typename T>
KA_MATH_DECL Quat<T> to_quat(const VecNum<3, T>& euler) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL Quat<T> to_quat(T angle, const VecNum<3, U>& axis) noexcept;

template<typename T>
KA_MATH_DECL VecNum<3, T> to_vec3(const Quat<T>& q) noexcept;

} // namespace kappa::math

#ifndef KA_MATH_QUATERNION_INL
#include "./quaternion.inl"
#endif
