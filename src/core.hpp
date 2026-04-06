#pragma once

#define KA_NOOP         (void)0
#define KA_UNUSED(expr) (void)(true ? KA_NOOP : ((void)(expr)))

#define KA_APPLY_VA_ARGS_(M, args) M args
#define KA_APPLY_VA_ARGS(M, ...)   KA_APPLY_VA_ARGS_(M, (__VA_ARGS__))

#define KA_NARG_(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                 _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, \
                 ...)                                                                            \
  _33

#define KA_EMPTY_MACRO
#define KA_NARG(...)                                                                            \
  KA_APPLY_VA_ARGS(KA_NARG_, KA_EMPTY_MACRO __VA_OPT__(, ) __VA_ARGS__, 32, 31, 30, 29, 28, 27, \
                   26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, \
                   6, 5, 4, 3, 2, 1, 0, KA_EMPTY_MACRO)

#define KA_JOIN__(lhs, rhs) lhs##rhs
#define KA_JOIN_(lhs, rhs)  KA_JOIN__(lhs, rhs)
#define KA_JOIN(lhs, rhs)   KA_JOIN_(lhs, rhs)

#define KA_STRINGIFY_(a) #a
#define KA_STRINGIFY(a)  KA_STRINGIFY_(a)

#define KA_LIKELY(_arg)   __builtin_expect(!!(_arg), !0)
#define KA_UNLIKELY(_arg) __builtin_expect(!!(_arg), 0)
#define KA_UNREACHABLE()  __builtin_unreachable()

#define ka_assert_2(_cond, _msg)                                                                  \
  do {                                                                                            \
    if (KA_UNLIKELY(!(_cond))) {                                                                  \
      ::kappa::impl::assert_failure(KA_STRINGIFY(_cond), __FILE__, __PRETTY_FUNCTION__, __LINE__, \
                                    _msg);                                                        \
    }                                                                                             \
  } while (0)
#define ka_assert_1(_cond) ka_assert_2(_cond, nullptr)
#define ka_assert(...)     KA_APPLY_VA_ARGS(KA_JOIN(ka_assert_, KA_NARG(__VA_ARGS__)), __VA_ARGS__)

#define ka_panic(_msg)                                                      \
  do {                                                                      \
    ::kappa::impl::on_panic(__FILE__, __PRETTY_FUNCTION__, __LINE__, _msg); \
  } while (0)
#define ka_todo(_msg) ka_panic("TODO: " _msg)

#if defined(_MSC_VER)
#define KA_INLINE   __forceinline
#define KA_NOINLINE __declspec((noinline))
#elif defined(__GNUC__) || defined(__MINGW32__)
#define KA_INLINE   inline __attribute__((__always_inline__))
#define KA_NOINLINE __attribute__((__noinline__))
#elif defined(__clang__)
#define KA_INLINE   __forceinline__
#define KA_NOINLINE __noinline__
#else
#define KA_INLINE inline
#define KA_NOINLINE
#endif

#ifdef __cpp_exceptions
#define KA_THROW(_obj) throw _obj
#else
#define KA_THROW(_obj) ka_panic("Thrown exception " KA_STRINGIFY(_obj))
#endif
#define KA_THROW_IF(_cond, _obj) \
  do {                           \
    if (_cond) {                 \
      KA_THROW(_obj);            \
    }                            \
  } while (0)

#define fn auto

#include <cassert>
#include <cstdint>

namespace kappa {

namespace impl {

[[noreturn]] void assert_failure(const char* cond, const char* file, const char* func, int line,
                                 const char* msg);

[[noreturn]] void on_panic(const char* file, const char* func, int line, const char* msg);

} // namespace impl

using usize = std::size_t;
using ptrdiff_t = std::ptrdiff_t;
using uintptr_t = std::uintptr_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using f32 = float;
using f64 = double;

using bits32 = u32;
using bits64 = u64;

struct Extent2D {
  u32 width, height;
};

struct Extent3D {
  u32 width, height, depth;
};

template<typename T>
struct RectanglePos {
  T x, y;
  T width, height;
};

template<typename T>
struct CirclePos {
  T x, y;
  T radius;
};

} // namespace kappa
