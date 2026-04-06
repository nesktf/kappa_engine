#pragma once

#include "./common.hpp"

#include <utility>

namespace kappa {

template<math::numeric_type T>
struct VecNum<2, T> {
public:
  using value_type = T;
  static constexpr u32 component_count = 2;

public:
  KA_MATH_DECLARE_VECTOR_SPECIAL_MEMBERS(VecNum);

  KA_MATH_DEF VecNum(T x_, T y_) noexcept : x(x_), y(y_) {}

  KA_MATH_DEF explicit VecNum(T scalar) noexcept : x(scalar), y(scalar) {}

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL explicit VecNum(U scalar) noexcept :
      x(static_cast<T>(scalar)), y(static_cast<T>(scalar)) {}

  template<math::numeric_type X, math::numeric_type Y>
  KA_MATH_DECL VecNum(X x_, Y y_) noexcept : x(static_cast<T>(x_)), y(static_cast<T>(y_)) {}

public:
  KA_MATH_DEF const T& operator[](size_t idx) const noexcept {
    switch (idx) {
      default:
        [[fallthrough]];
      case 0:
        return x;
      case 1:
        return y;
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
  KA_MATH_DECL VecNum& operator=(const VecNum<2, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator+=(const VecNum<2, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator-=(const VecNum<2, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator*=(const VecNum<2, U>& other) noexcept;

  template<math::numeric_convertible<T> U>
  KA_MATH_DECL VecNum& operator/=(const VecNum<2, U>& other) noexcept;

public:
  T x, y;
};

template<typename T>
KA_MATH_DECL VecNum<2, T> operator==(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept;

template<typename T>
KA_MATH_DECL VecNum<2, T> operator!=(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept;

template<typename T>
KA_MATH_DECL VecNum<2, T> operator+(const VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<2, T> operator+(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<2, T> operator+(const VecNum<2, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<2, T> operator+(U scalar, const VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<2, T> operator-(const VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<2, T> operator-(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<2, T> operator-(const VecNum<2, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<2, T> operator-(U scalar, const VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<2, T> operator*(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<2, T> operator*(const VecNum<2, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<2, T> operator*(U scalar, const VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<2, T> operator/(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<2, T> operator/(const VecNum<2, T>& vec, U scalar) noexcept;

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DECL VecNum<2, T> operator/(U scalar, const VecNum<2, T>& vec) noexcept;

template<typename U, typename T>
requires(math::numeric_convertible<U, T>)
KA_MATH_DECL VecNum<2, U> vec_cast(const VecNum<2, T>& vec) noexcept;

} // namespace kappa

namespace kappa::math {

template<typename T>
KA_MATH_DECL T length2(const VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL T length(const VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL void normalize_at(VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL VecNum<2, T> normalize(const VecNum<2, T>& vec) noexcept;

template<typename T>
KA_MATH_DECL T dot(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept;

} // namespace kappa::math

#ifndef KA_MATH_VECTOR2_INL
#include "./vector2.inl"
#endif
