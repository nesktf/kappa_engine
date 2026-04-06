#define KA_MATH_QUATERNION_INL
#include "./quaternion.hpp"
#undef KA_MATH_QUATERNION_INL

#include "./vector3.hpp"
#include "./vector4.hpp"

#include "./matrix3x3.hpp"
#include "./matrix4x4.hpp"

namespace kappa {

template<typename U, typename T>
requires(math::numeric_convertible<U, T>)
KA_MATH_DEF Quat<U> vec_cast(const Quat<T>& q) noexcept {
  return {static_cast<U>(q.w), static_cast<U>(q.x), static_cast<U>(q.y), static_cast<U>(q.z)};
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator=(U scalar) noexcept {
  this->w = static_cast<T>(scalar);
  this->x = static_cast<T>(scalar);
  this->y = static_cast<T>(scalar);
  this->z = static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator+=(U scalar) noexcept {
  this->w += static_cast<T>(scalar);
  this->x += static_cast<T>(scalar);
  this->y += static_cast<T>(scalar);
  this->z += static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator-=(U scalar) noexcept {
  this->w -= static_cast<T>(scalar);
  this->x -= static_cast<T>(scalar);
  this->y -= static_cast<T>(scalar);
  this->z -= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator*=(U scalar) noexcept {
  this->w *= static_cast<T>(scalar);
  this->x *= static_cast<T>(scalar);
  this->y *= static_cast<T>(scalar);
  this->z *= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator/=(U scalar) noexcept {
  this->w /= static_cast<T>(scalar);
  this->x /= static_cast<T>(scalar);
  this->y /= static_cast<T>(scalar);
  this->z /= static_cast<T>(scalar);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator=(const Quat<U>& other) noexcept {
  this->w = static_cast<T>(other.w);
  this->x = static_cast<T>(other.x);
  this->y = static_cast<T>(other.y);
  this->z = static_cast<T>(other.z);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator+=(const Quat<U>& other) noexcept {
  this->w += static_cast<T>(other.w);
  this->x += static_cast<T>(other.x);
  this->y += static_cast<T>(other.y);
  this->z += static_cast<T>(other.z);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator-=(const Quat<U>& other) noexcept {
  this->w -= static_cast<T>(other.w);
  this->x -= static_cast<T>(other.x);
  this->y -= static_cast<T>(other.y);
  this->z -= static_cast<T>(other.z);
  return *this;
}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>& Quat<T>::operator*=(const Quat<U>& other) noexcept {
  const auto q = ::kappa::vec_cast<T>(other);
  const Quat<T> self = *this;
  this->w = (self.w * q.w) - (self.x * q.x) - (self.y * q.y) - (self.z * q.z);
  this->x = (self.w * q.x) + (self.x * q.w) + (self.y * q.z) - (self.z * q.y);
  this->y = (self.w * q.y) + (self.y * q.w) + (self.z * q.x) - (self.x * q.z);
  this->z = (self.w * q.z) + (self.z * q.w) + (self.x * q.y) - (self.y * q.x);
  return *this;
}

template<typename T>
KA_MATH_DEF bool operator==(const Quat<T>& a, const Quat<T>& b) noexcept {
  return (a.w == b.w) && (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}

template<typename T>
KA_MATH_DEF bool operator!=(const Quat<T>& a, const Quat<T>& b) noexcept {
  return (a.w != b.w) || (a.x != b.x) || (a.y != b.y) || (a.z != b.z);
}

template<typename T>
KA_MATH_DEF Quat<T> operator+(const Quat<T>& q) noexcept {
  Quat<T> out;
  out.w = +q.w;
  out.x = +q.x;
  out.y = +q.y;
  out.z = +q.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator+(const Quat<T>& a, const Quat<U>& b) noexcept {
  Quat<T> out;
  out.w = a.w + static_cast<T>(b.w);
  out.x = a.x + static_cast<T>(b.x);
  out.y = a.y + static_cast<T>(b.y);
  out.z = a.z + static_cast<T>(b.z);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator+(const Quat<T>& q, U scalar) noexcept {
  Quat<T> out;
  out.w = q.w + static_cast<T>(scalar);
  out.x = q.x + static_cast<T>(scalar);
  out.y = q.y + static_cast<T>(scalar);
  out.z = q.z + static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator+(U scalar, const Quat<T>& q) noexcept {
  Quat<T> out;
  out.w = static_cast<T>(scalar) + q.w;
  out.x = static_cast<T>(scalar) + q.x;
  out.y = static_cast<T>(scalar) + q.y;
  out.z = static_cast<T>(scalar) + q.z;
  return out;
}

template<typename T>
KA_MATH_DEF Quat<T> operator-(const Quat<T>& q) noexcept {
  Quat<T> out;
  out.w = -q.w;
  out.x = -q.x;
  out.y = -q.y;
  out.z = -q.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator-(const Quat<T>& a, const Quat<U>& b) noexcept {
  Quat<T> out;
  out.w = a.w - static_cast<T>(b.w);
  out.x = a.x - static_cast<T>(b.x);
  out.y = a.y - static_cast<T>(b.y);
  out.z = a.z - static_cast<T>(b.z);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator-(const Quat<T>& q, U scalar) noexcept {
  Quat<T> out;
  out.w = q.w - static_cast<T>(scalar);
  out.x = q.x - static_cast<T>(scalar);
  out.y = q.y - static_cast<T>(scalar);
  out.z = q.z - static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator-(U scalar, const Quat<T>& q) noexcept {
  Quat<T> out;
  out.w = static_cast<T>(scalar) - q.w;
  out.x = static_cast<T>(scalar) - q.x;
  out.y = static_cast<T>(scalar) - q.y;
  out.z = static_cast<T>(scalar) - q.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator*(const Quat<T>& a, const Quat<U>& b) noexcept {
  const auto q = ::kappa::vec_cast<T>(b);
  Quat<T> out;
  out.w = (a.w * q.w) - (a.x * q.x) - (a.y * q.y) - (a.z * q.z);
  out.x = (a.w * q.x) + (a.x * q.w) + (a.y * q.z) - (a.z * q.y);
  out.y = (a.w * q.y) + (a.y * q.w) + (a.z * q.x) - (a.x * q.z);
  out.z = (a.w * q.z) + (a.z * q.w) + (a.x * q.y) - (a.y * q.x);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator*(const Quat<T>& q, const VecNum<3, U>& v) noexcept {
  const auto vt = ::kappa::vec_cast<T>(v);
  const VecNum<3, T> qv = ::kappa::math::to_vec3(q);
  const VecNum<3, T> c1 = ::kappa::math::cross(qv, vt);
  const VecNum<3, T> c2 = ::kappa::math::cross(qv, c1);
  return vt + (((c1 * q.w) + c2) * T(2));
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator*(const Quat<T>& q, const VecNum<4, U>& v) noexcept {
  const VecNum<3, U> v3{v.x, v.y, v.z};
  const VecNum<3, T> vq = q * ::kappa::vec_cast<T>(v3);
  return VecNum<4, T>{vq.x, vq.y, vq.z, static_cast<T>(v.w)};
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator*(const Quat<T>& q, U scalar) noexcept {
  Quat<T> out;
  out.w = q.w * static_cast<T>(scalar);
  out.x = q.x * static_cast<T>(scalar);
  out.y = q.y * static_cast<T>(scalar);
  out.z = q.z * static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator*(U scalar, const Quat<T>& q) noexcept {
  Quat<T> out;
  out.w = static_cast<T>(scalar) * q.w;
  out.x = static_cast<T>(scalar) * q.x;
  out.y = static_cast<T>(scalar) * q.y;
  out.z = static_cast<T>(scalar) * q.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator/(const Quat<T>& q, U scalar) noexcept {
  Quat<T> out;
  out.w = q.w / static_cast<T>(scalar);
  out.x = q.x / static_cast<T>(scalar);
  out.y = q.y / static_cast<T>(scalar);
  out.z = q.z / static_cast<T>(scalar);
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> operator/(U scalar, const Quat<T>& q) noexcept {
  Quat<T> out;
  out.w = static_cast<T>(scalar) / q.w;
  out.x = static_cast<T>(scalar) / q.x;
  out.y = static_cast<T>(scalar) / q.y;
  out.z = static_cast<T>(scalar) / q.z;
  return out;
}

} // namespace kappa

namespace kappa::math {

template<typename T>
KA_MATH_DEF Quat<T> conjugate(const Quat<T>& q) noexcept {
  Quat<T> out;
  out.w = q.w;
  out.x = -q.x;
  out.y = -q.y;
  out.z = -q.z;
  return out;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF T dot(const Quat<T>& a, const Quat<U>& b) noexcept {
  const T w = a.w * static_cast<T>(b.w);
  const T x = a.x * static_cast<T>(b.x);
  const T y = a.y * static_cast<T>(b.y);
  const T z = a.z * static_cast<T>(b.z);
  return (w + x + y + z);
}

template<typename T>
KA_MATH_DEF Quat<T> inverse(const Quat<T>& q) noexcept {
  const T d = ::kappa::math::dot(q, q);
  Quat<T> out;
  out.w = q.w / d;
  out.x = -q.x / d;
  out.y = -q.y / d;
  out.z = -q.z / d;
  return out;
}

template<typename T>
KA_MATH_DEF Mat<3, 3, T> to_mat3(const Quat<T>& q) noexcept {
  const T qxx = q.x * q.x;
  const T qyy = q.y * q.y;
  const T qzz = q.z * q.z;
  const T qxz = q.x * q.z;
  const T qxy = q.x * q.y;
  const T qyz = q.y * q.z;
  const T qwx = q.w * q.x;
  const T qwy = q.w * q.y;
  const T qwz = q.w * q.z;

  Mat<3, 3, T> out;
  out.x1 = T(1) - (T(2) * (qyy + qzz));
  out.y1 = T(2) * (qxy + qwz);
  out.z1 = T(2) * (qxz - qwy);
  out.x2 = T(2) * (qxy - qwz);
  out.y2 = T(1) - (T(2) * (qxx + qzz));
  out.z2 = T(2) * (qyz + qwx);
  out.x3 = T(2) * (qxz + qwy);
  out.y3 = T(2) * (qyz - qwx);
  out.z3 = T(1) - (T(2) * (qxx + qyy));
  return out;
}

template<typename T>
KA_MATH_DEF Mat<4, 4, T> to_mat4(const Quat<T>& q) noexcept {
  const T qxx = q.x * q.x;
  const T qyy = q.y * q.y;
  const T qzz = q.z * q.z;
  const T qxz = q.x * q.z;
  const T qxy = q.x * q.y;
  const T qyz = q.y * q.z;
  const T qwx = q.w * q.x;
  const T qwy = q.w * q.y;
  const T qwz = q.w * q.z;

  Mat<4, 4, T> out;
  out.x1 = T(1) - (T(2) * (qyy + qzz));
  out.y1 = T(2) * (qxy + qwz);
  out.z1 = T(2) * (qxz - qwy);
  out.w1 = T(0);
  out.x2 = T(2) * (qxy - qwz);
  out.y2 = T(1) - (T(2) * (qxx + qzz));
  out.z2 = T(2) * (qyz + qwx);
  out.w2 = T(0);
  out.x3 = T(2) * (qxz + qwy);
  out.y3 = T(2) * (qyz - qwx);
  out.z3 = T(1) - (T(2) * (qxx + qyy));
  out.w3 = T(0);
  out.x4 = T(0);
  out.y4 = T(0);
  out.z4 = T(0);
  out.w4 = T(1);
  return out;
}

template<typename T>
KA_MATH_DEF Quat<T> to_quat(const Mat<3, 3, T>& m) noexcept {
  const auto [biggest, idx] = [](auto&& m) -> std::pair<T, u32> {
    const T x2m1 = m.x1 - m.x2 - m.x3;
    const T y2m1 = m.y2 - m.x1 - m.z3;
    const T z2m1 = m.z3 - m.x1 - m.y2;
    const T w2m1 = m.x1 + m.y2 + m.z3;
    u32 idx = 0;
    T biggest = w2m1;
    if (x2m1 > biggest) {
      biggest = x2m1;
      idx = 1;
    }
    if (y2m1 > biggest) {
      biggest = y2m1;
      idx = 2;
    }
    if (z2m1 > biggest) {
      biggest = z2m1;
      idx = 3;
    }
    return {::kappa::math::sqrt(biggest + T(1)) * T(0.5), idx};
  }(m);

  const T mult = T(0.25) / biggest;
  const T y3z2m = (m.y3 - m.z2) * mult;
  const T z1x3m = (m.z1 - m.x3) * mult;
  const T y2x1m = (m.y2 - m.x1) * mult;
  const T y2x2p = (m.y2 + m.x1) * mult;
  const T z1x3p = (m.z1 + m.x3) * mult;
  const T y3z2p = (m.y3 + m.z2) * mult;
  Quat<T> out[] = {
    {biggest, y3z2m, z1x3m, y2x1m},
    {y3z2m, biggest, y2x2p, z1x3p},
    {z1x3m, y2x2p, biggest, y3z2p},
    {y2x1m, z1x3p, y3z2p, biggest},
  };
  return out[idx];
}

template<typename T>
KA_MATH_DEF Quat<T> to_quat(const Mat<4, 4, T>& m) noexcept {
  return ::kappa::math::to_quat(::kappa::math::to_mat3(m));
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T> to_quat(T angle, const VecNum<3, U>& axis) noexcept {
  const VecNum<3, U> norm = ::kappa::math::normalize(axis);
  Quat<T> out;
  out.w = ::kappa::math::cos(angle * T(0.5));
  out.x = ::kappa::math::sin(angle * T(0.5)) * axis.x;
  out.y = ::kappa::math::sin(angle * T(0.5)) * axis.y;
  out.z = ::kappa::math::sin(angle * T(0.5)) * axis.z;
  return out;
}

template<typename T>
KA_MATH_DEF Quat<T> to_quat(const VecNum<3, T>& euler) noexcept {
  const T cx = ::kappa::math::cos(euler.x * T(0.5));
  const T cy = ::kappa::math::cos(euler.y * T(0.5));
  const T cz = ::kappa::math::cos(euler.z * T(0.5));
  const T sx = ::kappa::math::sin(euler.x * T(0.5));
  const T sy = ::kappa::math::sin(euler.y * T(0.5));
  const T sz = ::kappa::math::sin(euler.z * T(0.5));

  Quat<T> out;
  out.w = (cx * cy * cz) + (sx * sy * sz);
  out.x = (sx * cy * cz) - (cx * sy * sz);
  out.y = (cx * sy * cz) + (sx * cy * sz);
  out.z = (cx * cy * sz) - (sx * sy * cz);
  return out;
}

template<typename T>
KA_MATH_DEF T roll(const Quat<T>& q) noexcept {
  const T y = T(2) * ((q.x * q.y) + (q.w * q.z));
  const T x = (q.w * q.w) + (q.x * q.x) - (q.y * q.y) - (q.z * q.z);
  return ::kappa::math::atan2(y, x);
}

template<typename T>
KA_MATH_DEF T pitch(const Quat<T>& q) noexcept {
  const T y = T(2) * ((q.y * q.z) + (q.w * q.x));
  const T x = (q.w * q.w) - (q.x * q.x) - (q.y * q.y) + (q.z * q.z);

  const bool singularity = [](auto&& x, auto&& y) -> bool {
    if constexpr (std::floating_point<T>) {
      return ::kappa::math::fequal(x, T(0)) && ::kappa::math::fequal(y, T(0));
    } else {
      return (x == T(0)) && (y == T(0));
    }
  }(x, y);

  return singularity ? (T(2) * ::kappa::math::atan2(q.x, q.w)) : ::kappa::math::atan2(y, x);
}

template<typename T>
KA_MATH_DEF T yaw(const Quat<T>& q) noexcept {
  const T v = T(-2) * ((q.x * q.z) - (q.w * q.y));
  return ::kappa::math::asin(::kappa::math::clamp(v, T(-1), T(1)));
}

template<typename T>
KA_MATH_DEF VecNum<3, T> to_euler(const Quat<T>& q) noexcept {
  VecNum<3, T> out;
  out.x = ::kappa::math::pitch(q);
  out.y = ::kappa::math::yaw(q);
  out.z = ::kappa::math::roll(q);
  return out;
}

} // namespace kappa::math

namespace kappa {

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>::Quat(const VecNum<3, U>& euler) noexcept :
    Quat(::kappa::math::to_quat(euler)) {}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>::Quat(const Mat<3, 3, U>& m) noexcept : Quat(::kappa::math::to_quat(m)) {}

template<math::numeric_type T>
template<math::numeric_convertible<T> U>
KA_MATH_DEF Quat<T>::Quat(const Mat<4, 4, U>& m) noexcept : Quat(::kappa::math::to_quat(m)) {}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<3, T> operator*(const VecNum<3, U>& v, const Quat<T>& q) noexcept {
  return ::kappa::math::inverse(q) * v;
}

template<typename T, math::numeric_convertible<T> U>
KA_MATH_DEF VecNum<4, T> operator*(const VecNum<4, U>& v, const Quat<T>& q) noexcept {
  return ::kappa::math::inverse(q) * v;
}

} // namespace kappa
