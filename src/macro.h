#ifndef KA_MACRO_H_
#define KA_MACRO_H_

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

#define KA_DECL_HANDLE(_typename) typedef struct _typename##_impl* _typename

#ifndef KA_INTERNAL_
#define KA_DECL_OPAQUE(_typename, _size, _align) \
  typedef struct _typename {                     \
    alignas(_align) uint8_t _data[_size];        \
  } _typename
#else
#define KA_DECL_OPAQUE(_typename, _size, _align)       \
  struct _typename;                                    \
  constexpr size_t _opaque_align_##_typename = _align; \
  constexpr size_t _opaque_size_##_typename = _size;
#endif

#define KA_CHECK_OPAQUE(_typename)                                                     \
  static_assert(alignof(_typename) <= _opaque_align_##_typename, "Invalid alignment"); \
  static_assert(sizeof(_typename) == _opaque_size_##_typename, "Invalid size")

#define KA_NO_MOVE(_typename)      \
  _typename(_typename&&) = delete; \
  _typename& operator=(_typename&&) = delete

#define KA_DO_MOVE(_typename)      \
  _typename(_typename&&) noexcept; \
  _typename& operator=(_typename&&) noexcept

#define KA_NO_COPY(_typename)           \
  _typename(const _typename&) = delete; \
  _typename& operator=(const _typename&) = delete

#define KA_SELF_FORWARD(_typename)                                                             \
private:                                                                                       \
  struct create_t {};                                                                          \
  Self self;                                                                                   \
                                                                                               \
public:                                                                                        \
  template<typename... SelfArgs>                                                               \
  _typename(create_t,                                                                          \
            SelfArgs&&... args) noexcept(std::is_nothrow_constructible_v<Self, SelfArgs...>) : \
      self(std::forward<SelfArgs>(args)...) {}

#define KA_C_API

#define KA_VER_MAJ 0
#define KA_VER_MIN 0
#define KA_VER_REV 1

#include <stdalign.h>
#include <stdint.h>

#endif // #ifndef KA_MACRO_H_
