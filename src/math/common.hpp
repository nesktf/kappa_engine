#pragma once

#include "../core.hpp"

#include <complex>

#define KA_MATH_DECL constexpr
#define KA_MATH_DEF  constexpr KA_INLINE

#define KA_MATH_DECLARE_VECTOR_SPECIAL_MEMBERS(type_)          \
  constexpr type_() noexcept = default;                        \
  constexpr type_(const type_&) noexcept = default;            \
  constexpr type_(type_&&) noexcept = default;                 \
  constexpr type_& operator=(const type_&) noexcept = default; \
  constexpr type_& operator=(type_&&) noexcept = default

namespace kappa::math {

template<typename T>
concept numeric_type = std::integral<T> || std::floating_point<T>;

template<typename From, typename To>
concept numeric_convertible = numeric_type<From> && std::convertible_to<From, To>;

template<numeric_type T>
static constexpr T pi = static_cast<T>(3.14159265358979323846);

template<numeric_type T>
static constexpr T e = static_cast<T>(2.7182818284590452354);

template<std::floating_point T>
static constexpr T epsilon = std::numeric_limits<T>::epsilon();

template<numeric_type T>
KA_MATH_DECL T sqrt(T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T rsqrt(T x) noexcept;

KA_MATH_DECL f32 qrsqrt(float x) noexcept;

template<numeric_type T>
KA_MATH_DECL T cos(T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T sin(T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T tan(T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T acos(T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T asin(T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T atan(T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T atan2(T y, T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T rad(T degs) noexcept;

template<numeric_type T>
KA_MATH_DECL T deg(T rads) noexcept;

template<numeric_type T>
KA_MATH_DECL T epsilon_err(T a, T b) noexcept;

template<numeric_type T>
KA_MATH_DECL T abs(T x) noexcept;

template<numeric_type T>
KA_MATH_DECL T clamp(T x, T min, T max) noexcept;

template<numeric_type T>
KA_MATH_DECL T max(T a, T b) noexcept;

template<numeric_type T>
KA_MATH_DECL T min(T a, T b) noexcept;

template<numeric_type TL, numeric_type TR>
KA_MATH_DECL auto periodic_add(const TL& a, const TR& b, decltype(a + b) min,
                               decltype(a + b) max) noexcept;

template<std::floating_point T>
KA_MATH_DECL bool fequal(T a, T b) noexcept;

} // namespace kappa::math

namespace kappa {

// Forward declarations for vector types
template<u32 N, math::numeric_type T>
struct VecNum;

template<u32 N, u32 M, math::numeric_type T>
struct Mat;

template<math::numeric_type T>
struct Quat;

using Cmplxf32 = std::complex<f32>;
using Cmplxf64 = std::complex<f64>;

using Vec2f32 = VecNum<2, f32>;
using Vec3f32 = VecNum<3, f32>;
using Vec4f32 = VecNum<4, f32>;
using Vec2f64 = VecNum<2, f64>;
using Vec3f64 = VecNum<3, f64>;
using Vec4f64 = VecNum<4, f64>;

using Vec2s32 = VecNum<2, s32>;
using Vec3s32 = VecNum<3, s32>;
using Vec4s32 = VecNum<4, s32>;
using Vec2u32 = VecNum<2, u32>;
using Vec3u32 = VecNum<3, u32>;
using Vec4u32 = VecNum<4, u32>;

using Mat3f32 = Mat<4, 4, f32>;
using Mat4f32 = Mat<4, 4, f32>;
using Mat3f64 = Mat<4, 4, f64>;
using Mat4f64 = Mat<4, 4, f64>;

using Quatf32 = Quat<f32>;
using Quatf64 = Quat<f64>;

} // namespace kappa

#ifndef KA_MATH_COMMON_INL
#include "./common.inl"
#endif
