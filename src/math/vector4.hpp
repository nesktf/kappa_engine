#pragma once

#include "./common.hpp"
#include <utility>

namespace kappa {

template<math::numeric_type T>
struct VecNum<4, T> {
public:
  using value_type = T;
  static constexpr u32 component_count = 4;

public:
  KA_MATH_DECLARE_VECTOR_SPECIAL_MEMBERS(VecNum);

  KA_MATH_DEF VecNum(T x_, T y_, T z_, T w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}

  template<math::numeric_convertible<T> U>
  KA_MATH_DEF explicit VecNum(U scalar) noexcept :
      x(static_cast<T>(scalar)), y(static_cast<T>(scalar)), z(static_cast<T>(scalar)),
      w(static_cast<T>(scalar)) {}

  template<math::numeric_convertible<T> X, math::numeric_convertible<T> Y,
           math::numeric_convertible<T> Z, math::numeric_convertible<T> W>
  KA_MATH_DEF VecNum(X x_, Y y_, Z z_, W w_) noexcept :
      x(static_cast<T>(x_)), y(static_cast<T>(y_)), z(static_cast<T>(z_)), w(static_cast<T>(w_)) {}

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
      case 3:
        return w;
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
  KA_MATH_DECL VecNum& operator=(const VecNum<4, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator+=(const VecNum<4, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator-=(const VecNum<4, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator*=(const VecNum<4, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator/=(const VecNum<4, U>& other) noexcept;

public:
  T x, y, z, w;
};

template<typename T>
KA_MATH_DECL bool operator==(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept;

template<typename T>
KA_MATH_DECL bool operator!=(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept;

template<typename T>
KA_MATH_DECL VecNum<4, T> operator+(const VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<4, T> operator+(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator+(const VecNum<4, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator+(U scalar, const VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<4, T> operator-(const VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<4, T> operator-(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator-(const VecNum<4, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator-(U scalar, const VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<4, T> operator*(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator*(const VecNum<4, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator*(U scalar, const VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<4, T> operator/(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator/(const VecNum<4, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<4, T> operator/(U scalar, const VecNum<4, T>& vec) noexcept;

template<typename U, typename T>
requires(math::numeric_convertible<U, T>)
KA_MATH_DECL VecNum<4, U> vec_cast(const VecNum<4, T>& vec) noexcept;

} // namespace kappa

namespace kappa::math {

template<typename T>
KA_MATH_DECL T length2(const VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL T length(const VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL void normalize_at(VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<4, T> normalize(const VecNum<4, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL T dot(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept;

} // namespace kappa::math

#ifndef KA_MATH_VECTOR4_INL
#include "./vector4.inl"
#endif
