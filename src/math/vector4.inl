#define KA_MATH_VECTOR4_INL
#include "./vector4.hpp"
#undef KA_MATH_VECTOR4_INL

namespace kappa {

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator=(U scalar) noexcept {
  this->x = static_cast<T>(scalar);
  this->y = static_cast<T>(scalar);
  this->z = static_cast<T>(scalar);
  this->w = static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator+=(U scalar) noexcept {
  this->x += static_cast<T>(scalar);
  this->y += static_cast<T>(scalar);
  this->z += static_cast<T>(scalar);
  this->w += static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator-=(U scalar) noexcept {
  this->x -= static_cast<T>(scalar);
  this->y -= static_cast<T>(scalar);
  this->z -= static_cast<T>(scalar);
  this->w -= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator*=(U scalar) noexcept {
  this->x *= static_cast<T>(scalar);
  this->y *= static_cast<T>(scalar);
  this->z *= static_cast<T>(scalar);
  this->w *= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator/=(U scalar) noexcept {
  this->x /= static_cast<T>(scalar);
  this->y /= static_cast<T>(scalar);
  this->z /= static_cast<T>(scalar);
  this->w /= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator=(const VecNum<4, U>& other) noexcept {
  this->x = static_cast<T>(other.x);
  this->y = static_cast<T>(other.y);
  this->z = static_cast<T>(other.z);
  this->w = static_cast<T>(other.w);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator+=(const VecNum<4, U>& other) noexcept {
  this->x += static_cast<T>(other.x);
  this->y += static_cast<T>(other.y);
  this->z += static_cast<T>(other.z);
  this->w += static_cast<T>(other.w);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator-=(const VecNum<4, U>& other) noexcept {
  this->x -= static_cast<T>(other.x);
  this->y -= static_cast<T>(other.y);
  this->z -= static_cast<T>(other.z);
  this->w -= static_cast<T>(other.w);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator*=(const VecNum<4, U>& other) noexcept {
  this->x *= static_cast<T>(other.x);
  this->y *= static_cast<T>(other.y);
  this->z *= static_cast<T>(other.z);
  this->w *= static_cast<T>(other.w);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T>& VecNum<4, T>::operator/=(const VecNum<4, U>& other) noexcept {
  this->x /= static_cast<T>(other.x);
  this->y /= static_cast<T>(other.y);
  this->z /= static_cast<T>(other.z);
  this->w /= static_cast<T>(other.w);
  return *this;
}

template<typename T>
KA_MATH_DEF bool operator==(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept {
  return (a.x == b.x) && (a.y == b.y) && (a.z == b.z) && (a.w == b.w);
}

template<typename T>
KA_MATH_DEF bool operator!=(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept {
  return (a.x != b.x) || (a.y != b.y) || (a.z != b.z) || (a.w != b.w);
}

template<typename T>
KA_MATH_DEF VecNum<4, T> operator+(const VecNum<4, T>& vec) noexcept {
  VecNum<4, T> out;
  out.x = +vec.x;
  out.y = +vec.y;
  out.z = +vec.z;
  out.w = +vec.w;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<4, T> operator+(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept {
  VecNum<4, T> out;
  out.x = a.x + b.x;
  out.y = a.y + b.y;
  out.z = a.z + b.z;
  out.w = a.w + b.w;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator+(const VecNum<4, T>& vec, U scalar) noexcept {
  VecNum<4, T> out;
  out.x = vec.x + static_cast<T>(scalar);
  out.y = vec.y + static_cast<T>(scalar);
  out.z = vec.z + static_cast<T>(scalar);
  out.w = vec.w + static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator+(U scalar, const VecNum<4, T>& vec) noexcept {
  VecNum<4, T> out;
  out.x = static_cast<T>(scalar) + vec.x;
  out.y = static_cast<T>(scalar) + vec.y;
  out.z = static_cast<T>(scalar) + vec.z;
  out.w = static_cast<T>(scalar) + vec.w;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<4, T> operator-(const VecNum<4, T>& vec) noexcept {
  VecNum<4, T> out;
  out.x = -vec.x;
  out.y = -vec.y;
  out.z = -vec.z;
  out.w = -vec.w;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<4, T> operator-(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept {
  VecNum<4, T> out;
  out.x = a.x - b.x;
  out.y = a.y - b.y;
  out.z = a.z - b.z;
  out.w = a.w - b.w;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator-(const VecNum<4, T>& vec, U scalar) noexcept {
  VecNum<4, T> out;
  out.x = vec.x - static_cast<T>(scalar);
  out.y = vec.y - static_cast<T>(scalar);
  out.z = vec.z - static_cast<T>(scalar);
  out.w = vec.w - static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator-(U scalar, const VecNum<4, T>& vec) noexcept {
  VecNum<4, T> out;
  out.x = static_cast<T>(scalar) - vec.x;
  out.y = static_cast<T>(scalar) - vec.y;
  out.z = static_cast<T>(scalar) - vec.z;
  out.w = static_cast<T>(scalar) - vec.w;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<4, T> operator*(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept {
  VecNum<4, T> out;
  out.x = a.x * b.x;
  out.y = a.y * b.y;
  out.z = a.z * b.z;
  out.w = a.w * b.w;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator*(const VecNum<4, T>& vec, U scalar) noexcept {
  VecNum<4, T> out;
  out.x = vec.x * static_cast<T>(scalar);
  out.y = vec.y * static_cast<T>(scalar);
  out.z = vec.z * static_cast<T>(scalar);
  out.w = vec.w * static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator*(U scalar, const VecNum<4, T>& vec) noexcept {
  VecNum<4, T> out;
  out.x = static_cast<T>(scalar) * vec.x;
  out.y = static_cast<T>(scalar) * vec.y;
  out.z = static_cast<T>(scalar) * vec.z;
  out.w = static_cast<T>(scalar) * vec.w;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<4, T> operator/(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept {
  VecNum<4, T> out;
  out.x = a.x / b.x;
  out.y = a.y / b.y;
  out.z = a.z / b.z;
  out.w = a.w / b.w;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator/(const VecNum<4, T>& vec, U scalar) noexcept {
  VecNum<4, T> out;
  out.x = vec.x / static_cast<T>(scalar);
  out.y = vec.y / static_cast<T>(scalar);
  out.z = vec.z / static_cast<T>(scalar);
  out.w = vec.w / static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator/(U scalar, const VecNum<4, T>& vec) noexcept {
  VecNum<4, T> out;
  out.x = static_cast<T>(scalar) / vec.x;
  out.y = static_cast<T>(scalar) / vec.y;
  out.z = static_cast<T>(scalar) / vec.z;
  out.w = static_cast<T>(scalar) / vec.w;
  return out;
}

template<typename U, typename T>
requires(math::numeric_convertible<U, T>)
KA_MATH_DEF VecNum<4, U> vec_cast(const VecNum<4, T>& vec) noexcept {
  return {static_cast<U>(vec.x), static_cast<U>(vec.y), static_cast<U>(vec.z),
          static_cast<U>(vec.z)};
}

} // namespace kappa

namespace kappa::math {

template<typename T>
KA_MATH_DEF T length2(const VecNum<4, T>& vec) noexcept {
  return (vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z) + (vec.w * vec.w);
}

template<typename T>
KA_MATH_DEF T length(const VecNum<4, T>& vec) noexcept {
  return std::sqrt((vec.x * vec.c) + (vec.y * vec.y) + (vec.z * vec.z) + (vec.w * vec.w));
}

template<typename T>
KA_MATH_DEF void normalize_at(VecNum<4, T>& vec) noexcept {
  const T len = ::kappa::math::length(vec);
  vec.x /= len;
  vec.y /= len;
  vec.z /= len;
  vec.w /= len;
}

template<typename T>
KA_MATH_DEF VecNum<4, T> normalize(const VecNum<4, T>& vec) noexcept {
  VecNum<4, T> out;
  const T len = ::kappa::math::length(vec);
  out.x = vec.x / len;
  out.y = vec.y / len;
  out.z = vec.z / len;
  out.w = vec.w / len;
  return out;
}

template<typename T>
KA_MATH_DEF T dot(const VecNum<4, T>& a, const VecNum<4, T>& b) noexcept {
  return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
}

} // namespace kappa::math
