#define KA_MATH_VECTOR2_INL
#include "./vector2.hpp"
#undef KA_MATH_VECTOR2_INL

namespace kappa {

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator=(U scalar) noexcept {
  this->x = static_cast<T>(scalar);
  this->y = static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator+=(U scalar) noexcept {
  this->x += static_cast<T>(scalar);
  this->y += static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator-=(U scalar) noexcept {
  this->x -= static_cast<T>(scalar);
  this->y -= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator*=(U scalar) noexcept {
  this->x *= static_cast<T>(scalar);
  this->y *= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator/=(U scalar) noexcept {
  this->x /= static_cast<T>(scalar);
  this->y /= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator=(const VecNum<2, U>& other) noexcept {
  this->x = static_cast<T>(other.x);
  this->y = static_cast<T>(other.y);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator+=(const VecNum<2, U>& other) noexcept {
  this->x += static_cast<T>(other.x);
  this->y += static_cast<T>(other.y);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator-=(const VecNum<2, U>& other) noexcept {
  this->x -= static_cast<T>(other.x);
  this->y -= static_cast<T>(other.y);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator*=(const VecNum<2, U>& other) noexcept {
  this->x *= static_cast<T>(other.x);
  this->y *= static_cast<T>(other.y);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T>& VecNum<2, T>::operator/=(const VecNum<2, U>& other) noexcept {
  this->x /= static_cast<T>(other.x);
  this->y /= static_cast<T>(other.y);
  return *this;
}

template<typename T>
KA_MATH_DEF VecNum<2, T> operator==(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept {
  return (a.x == b.x) && (a.y == b.y);
}

template<typename T>
KA_MATH_DEF VecNum<2, T> operator!=(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept {
  return (a.x != b.x) || (a.y != b.y);
}

template<typename T>
KA_MATH_DEF VecNum<2, T> operator+(const VecNum<2, T>& vec) noexcept {
  VecNum<2, T> out;
  out.x = +vec.x;
  out.y = +vec.y;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<2, T> operator+(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept {
  VecNum<2, T> out;
  out.x = a.x + b.x;
  out.y = a.y + b.y;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T> operator+(const VecNum<2, T>& vec, U scalar) noexcept {
  VecNum<2, T> out;
  out.x = vec.x + static_cast<T>(scalar);
  out.y = vec.y + static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T> operator+(U scalar, const VecNum<2, T>& vec) noexcept {
  VecNum<2, T> out;
  out.x = static_cast<T>(scalar) * vec.x;
  out.y = static_cast<T>(scalar) + vec.y;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<2, T> operator-(const VecNum<2, T>& vec) noexcept {
  VecNum<2, T> out;
  out.x = -vec.x;
  out.y = -vec.y;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<2, T> operator-(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept {
  VecNum<2, T> out;
  out.x = a.x - b.x;
  out.y = a.y - b.y;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T> operator-(const VecNum<2, T>& vec, U scalar) noexcept {
  VecNum<2, T> out;
  out.x = vec.x - static_cast<T>(scalar);
  out.y = vec.y - static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T> operator-(U scalar, const VecNum<2, T>& vec) noexcept {
  VecNum<2, T> out;
  out.x = static_cast<T>(scalar) - vec.x;
  out.y = static_cast<T>(scalar) - vec.y;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<2, T> operator*(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept {
  VecNum<2, T> out;
  out.x = a.x * b.x;
  out.y = a.y * b.y;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T> operator*(const VecNum<2, T>& vec, U scalar) noexcept {
  VecNum<2, T> out;
  out.x = vec.x * static_cast<T>(scalar);
  out.y = vec.y * static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T> operator*(U scalar, const VecNum<2, T>& vec) noexcept {
  VecNum<2, T> out;
  out.x = static_cast<T>(scalar) * vec.x;
  out.y = static_cast<T>(scalar) * vec.y;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<2, T> operator/(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept {
  VecNum<2, T> out;
  out.x = a.x / b.x;
  out.y = a.y / b.y;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T> operator/(const VecNum<2, T>& vec, U scalar) noexcept {
  VecNum<2, T> out;
  out.x = vec.x / static_cast<T>(scalar);
  out.y = vec.y / static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<2, T> operator/(U scalar, const VecNum<2, T>& vec) noexcept {
  VecNum<2, T> out;
  out.x = static_cast<T>(scalar) / vec.x;
  out.y = static_cast<T>(scalar) / vec.y;
  return out;
}

template<typename U, typename T>
requires(math::numeric_convertible<U, T>)
KA_MATH_DEF VecNum<2, U> vec_cast(const VecNum<2, T>& vec) noexcept {
  return {static_cast<U>(vec.x), static_cast<U>(vec.y)};
}

} // namespace kappa

namespace kappa::math {

template<typename T>
KA_MATH_DEF T length2(const VecNum<2, T>& vec) noexcept {
  return (vec.x * vec.x) + (vec.y * vec.y);
}

template<typename T>
KA_MATH_DEF T length(const VecNum<2, T>& vec) noexcept {
  return std::sqrt((vec.x * vec.x) + (vec.y * vec.y));
}

template<typename T>
KA_MATH_DEF void normalize_at(VecNum<2, T>& vec) noexcept {
  const T len = ::kappa::math::length(vec);
  vec.x /= len;
  vec.y /= len;
}

template<typename T>
KA_MATH_DEF VecNum<2, T> normalize(const VecNum<2, T>& vec) noexcept {
  VecNum<2, T> out;
  const T len = ::kappa::math::length(vec);
  out.x = vec.x / len;
  out.y = vec.y / len;
  return out;
}

template<typename T>
KA_MATH_DEF T dot(const VecNum<2, T>& a, const VecNum<2, T>& b) noexcept {
  return (a.x * b.x) + (a.y * b.y);
}

} // namespace kappa::math
