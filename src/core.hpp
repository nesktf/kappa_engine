#pragma once

#include "./macro.h"

#include <cassert>

namespace kappa {

namespace impl {

[[noreturn]] void assert_failure(const char* cond, const char* file, const char* func, int line,
                                 const char* msg);

[[noreturn]] void on_panic(const char* file, const char* func, int line, const char* msg);

} // namespace impl

extern int g_argc;
extern char** g_argv;

using usize = std::size_t;
using ptrdiff_t = std::ptrdiff_t;
using uintptr_t = uintptr_t;

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

} // namespace kappa
