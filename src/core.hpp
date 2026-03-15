#pragma once

#include <shogle/core.hpp>
#include <shogle/util/expected.hpp>
#include <shogle/util/logger.hpp>
#include <shogle/util/memory.hpp>
#include <shogle/util/ptr.hpp>

#include <shogle/math/matrix4x4.hpp>
#include <shogle/math/quaternion.hpp>
#include <shogle/math/vector3.hpp>
#include <shogle/math/vector4.hpp>

#ifdef assert
#undef assert
#endif
#define assert(cond, ...) SHOGLE_ASSERT(cond __VA_OPT__(, ) __VA_ARGS__)

#define fn auto

namespace kappa {

using shogle::logger;
extern int g_argc;
extern char** g_argv;

using namespace shogle::numdefs;
using shogle::in_place;

using shogle::nullopt;
using shogle::optional;

using shogle::ptr_view;
using shogle::ref_view;

using shogle::expected;
using shogle::unexpect;

using shogle::s_expect;
using shogle::sv_expect;

using shogle::unique_array;

template<size_t MaxSize>
struct buffer_str {
  static constexpr size_t buffer_size = MaxSize;

  char data[MaxSize];
  size_t len;

  const char* c_str() const noexcept { return data; }

  std::string_view as_view() const noexcept { return {&data[0], len}; }

  size_t copy_from(const char* src, size_t size) {
    std::memset(data, 0, MaxSize);
    len = std::min(MaxSize - 1, size);
    std::memcpy(data, src, len);
    data[MaxSize - 1] = '\0'; // Make sure we have a null terminator
    return len;
  }

  size_t copy_from(const char* src) { return copy_from(src, std::strlen(src)); }

  template<typename... Args>
  size_t format_from(fmt::format_string<Args...> fmt, Args&&... args) {
    std::memset(data, 0, MaxSize);
    const auto res = fmt::format_to_n(data, MaxSize - 1, fmt, std::forward<Args>(args)...);
    len = res.size;
    return res.size;
  }
};

template<size_t MaxSize>
buffer_str<MaxSize> buffer_str_copy(const char* data, size_t len) {
  buffer_str<MaxSize> out;
  out.copy_from(data, len);
  return out;
}

template<size_t MaxSize>
buffer_str<MaxSize> buffer_str_copy(const char* data) {
  buffer_str<MaxSize> out;
  out.copy_from(data);
  return out;
}

template<size_t MaxSize, typename... Args>
buffer_str<MaxSize> buffer_str_fmt(fmt::format_string<Args...> fmt, Args&&... args) {
  buffer_str<MaxSize> out;
  out.format_from(fmt, std::forward<Args>(args)...);
  return out;
}

using buffer_name = buffer_str<128>;
using buffer_path = buffer_str<256>;

template<typename T, size_t MaxSize>
using bs_expect = expected<T, buffer_str<MaxSize>>;

struct array_range {
  static constexpr u32 index_tomb = static_cast<u32>(-1);

  static constexpr array_range null_range() noexcept { return {index_tomb, index_tomb}; }

  u32 start;
  u32 count;
};

template<typename... Fs>
struct overload : Fs... {
  using Fs::operator()...;
};

template<typename... Fs>
overload(Fs...) -> overload<Fs...>;

using shogle::m4f32;
using shogle::qf32;
using shogle::v2f32;
using shogle::v3f32;
using shogle::v4f32;
using v4i32 = shogle::numvec<4, i32>;
using shogle::extent2d;
using shogle::extent3d;
using shogle::span;

using bits32 = u32;
using bits64 = u64;

enum class image_format {
  rgb8u = 0,
  rgba8u,
  rgb16u,
  rgba16u,
  rgb32f,
  rgba32f,
};

constexpr inline size_t image_stride(u32 width, u32 height, image_format format) {
  constexpr auto sizes =
    std::to_array<size_t>({3 * sizeof(u8), 4 * sizeof(u8), 3 * sizeof(u16), 4 * sizeof(u16),
                           3 * sizeof(f32), 4 * sizeof(f32)});
  return sizes[static_cast<size_t>(format) % sizes.size()] * width * height;
}

enum class texture_type {
  diffuse = 0,
  albedo = 0,
  specular,
  normal,
  ambient_occlusion,
  roughness,
  metallic,
};

template<typename T>
fn make_zero_array(size_t count) -> unique_array<T> {
  static_assert(std::is_trivial_v<T>);
  auto ret = shogle::make_array<T>(shogle::uninitialized, count);
  std::memset(ret.data(), 0x00, sizeof(ret[0]) * count);
  return ret;
}

} // namespace kappa
