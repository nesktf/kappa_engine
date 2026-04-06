#define KA_MATH_VECTOR3_INL
#include "./vector3.hpp"
#undef KA_MATH_VECTOR3_INL

namespace kappa {

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator=(U scalar) noexcept {
  this->x = static_cast<T>(scalar);
  this->y = static_cast<T>(scalar);
  this->z = static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator+=(U scalar) noexcept {
  this->x += static_cast<T>(scalar);
  this->y += static_cast<T>(scalar);
  this->z += static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator-=(U scalar) noexcept {
  this->x -= static_cast<T>(scalar);
  this->y -= static_cast<T>(scalar);
  this->z -= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator*=(U scalar) noexcept {
  this->x *= static_cast<T>(scalar);
  this->y *= static_cast<T>(scalar);
  this->z *= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator/=(U scalar) noexcept {
  this->x /= static_cast<T>(scalar);
  this->y /= static_cast<T>(scalar);
  this->z /= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator=(const VecNum<3, U>& other) noexcept {
  this->x = static_cast<T>(other.x);
  this->y = static_cast<T>(other.y);
  this->z = static_cast<T>(other.z);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator+=(const VecNum<3, U>& other) noexcept {
  this->x += static_cast<T>(other.x);
  this->y += static_cast<T>(other.y);
  this->z += static_cast<T>(other.z);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator-=(const VecNum<3, U>& other) noexcept {
  this->x -= static_cast<T>(other.x);
  this->y -= static_cast<T>(other.y);
  this->z -= static_cast<T>(other.z);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator*=(const VecNum<3, U>& other) noexcept {
  this->x *= static_cast<T>(other.x);
  this->y *= static_cast<T>(other.y);
  this->z *= static_cast<T>(other.z);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T>& VecNum<3, T>::operator/=(const VecNum<3, U>& other) noexcept {
  this->x /= static_cast<T>(other.x);
  this->y /= static_cast<T>(other.y);
  this->z /= static_cast<T>(other.z);
  return *this;
}

template<typename T>
KA_MATH_DEF bool operator==(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept {
  return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}

template<typename T>
KA_MATH_DEF bool operator!=(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept {
  return (a.x != b.x) || (a.y != b.y) || (a.z != b.z);
}

template<typename T>
KA_MATH_DEF VecNum<3, T> operator+(const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out;
  out.x = +vec.x;
  out.y = +vec.y;
  out.z = +vec.z;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<3, T> operator+(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept {
  VecNum<3, T> out;
  out.x = a.x + b.x;
  out.y = a.y + b.y;
  out.z = a.z + b.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator+(const VecNum<3, T>& vec, U scalar) noexcept {
  VecNum<3, T> out;
  out.x = vec.x + static_cast<T>(scalar);
  out.y = vec.y + static_cast<T>(scalar);
  out.z = vec.z + static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator+(U scalar, const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out;
  out.x = static_cast<T>(scalar) + vec.x;
  out.y = static_cast<T>(scalar) + vec.y;
  out.z = static_cast<T>(scalar) + vec.z;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<3, T> operator-(const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out;
  out.x = -vec.x;
  out.y = -vec.y;
  out.z = -vec.z;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<3, T> operator-(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept {
  VecNum<3, T> out;
  out.x = a.x - b.x;
  out.y = a.y - b.y;
  out.z = a.z - b.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator-(const VecNum<3, T>& vec, U scalar) noexcept {
  VecNum<3, T> out;
  out.x = vec.x - static_cast<T>(scalar);
  out.y = vec.y - static_cast<T>(scalar);
  out.z = vec.z - static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator-(U scalar, const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out;
  out.x = static_cast<T>(scalar) - vec.x;
  out.y = static_cast<T>(scalar) - vec.y;
  out.z = static_cast<T>(scalar) - vec.z;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<3, T> operator*(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept {
  VecNum<3, T> out;
  out.x = a.x * b.x;
  out.y = a.y * b.y;
  out.z = a.z * b.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator*(const VecNum<3, T>& vec, U scalar) noexcept {
  VecNum<3, T> out;
  out.x = vec.x * static_cast<T>(scalar);
  out.y = vec.y * static_cast<T>(scalar);
  out.z = vec.z * static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator*(U scalar, const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out = vec;
  out.x = static_cast<T>(scalar) * vec.x;
  out.y = static_cast<T>(scalar) * vec.y;
  out.z = static_cast<T>(scalar) * vec.z;
  return out;
}

template<typename T>
KA_MATH_DEF VecNum<3, T> operator/(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept {
  VecNum<3, T> out;
  out.x = a.x / b.x;
  out.y = a.y / b.y;
  out.z = a.z / b.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator/(const VecNum<3, T>& vec, U scalar) noexcept {
  VecNum<3, T> out;
  out.x = vec.x / static_cast<T>(scalar);
  out.y = vec.y / static_cast<T>(scalar);
  out.z = vec.z / static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator/(U scalar, const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out = vec;
  out.x = static_cast<T>(scalar) / vec.x;
  out.y = static_cast<T>(scalar) / vec.y;
  out.z = static_cast<T>(scalar) / vec.z;
  return out;
}

template<typename U, typename T>
requires(math::numeric_convertible<U, T>)
KA_MATH_DEF VecNum<3, U> vec_cast(const VecNum<3, T>& vec) noexcept {
  return {static_cast<U>(vec.x), static_cast<U>(vec.y), static_cast<U>(vec.z)};
}

} // namespace kappa

namespace kappa::math {

template<typename T>
KA_MATH_DEF T length2(const VecNum<3, T>& vec) noexcept {
  return (vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z);
}

template<typename T>
KA_MATH_DEF T length(const VecNum<3, T>& vec) noexcept {
  return ::kappa::math::sqrt((vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z));
}

template<typename T>
KA_MATH_DEF void normalize_at(VecNum<3, T>& vec) noexcept {
  const T len = ::kappa::math::length(vec);
  vec.x /= len;
  vec.y /= len;
  vec.z /= len;
}

template<typename T>
KA_MATH_DEF VecNum<3, T> normalize(const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out;
  const T len = ::kappa::math::length(vec);
  out.x = vec.x / len;
  out.y = vec.y / len;
  out.z = vec.z / len;
  return out;
}

template<typename T>
KA_MATH_DEF T dot(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept {
  return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

template<typename T>
KA_MATH_DEF VecNum<3, T> cross(const VecNum<3, T>& a, const VecNum<3, T>& b) noexcept {
  VecNum<3, T> out;
  out.x = (a.y * b.z) - (b.y * a.z);
  out.y = (a.z * b.x) - (b.z * a.x);
  out.z = (a.x * b.y) - (b.x * a.y);
  return out;
}

template<numeric_type T>
KA_MATH_DEF void gl_to_cartesian_at(VecNum<3, T>& vec) noexcept {
  const T x = vec.x;
  vec.x = vec.z;
  vec.z = vec.y;
  vec.y = x;
}

template<typename T>
KA_MATH_DEF VecNum<3, T> gl_to_cartesian(const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out;
  out.x = vec.z;
  out.y = vec.x;
  out.z = vec.y;
  return out;
}

template<numeric_type T>
KA_MATH_DEF VecNum<3, T> sph_to_cartesian(T rho, T theta, T phi) noexcept {
  VecNum<3, T> out;
  out.x = rho * ::kappa::math::sin(theta) * ::kappa::math::cos(phi);
  out.y = rho * ::kappa::math::sin(theta) * ::kappa::math::sin(phi);
  out.z = rho * ::kappa::math::cos(theta);
  return out;
}

template<numeric_type T>
KA_MATH_DEF void cartesian_to_gl_at(VecNum<3, T>& vec) noexcept {
  const T x = vec.x;
  vec.x = vec.y;
  vec.y = vec.z;
  vec.z = x;
}

template<typename T>
KA_MATH_DEF VecNum<3, T> cartesian_to_gl(const VecNum<3, T>& vec) noexcept {
  VecNum<3, T> out;
  out.x = vec.y;
  out.y = vec.z;
  out.z = vec.x;
  return out;
}

} // namespace kappa::math
