#pragma once

#define SHOGLE_EXPOSE_GLFW 1
#include <shogle/shogle.hpp>

#include <ntfstl/logger.hpp>
#include <ntfstl/optional.hpp>
#include <ntfstl/span.hpp>
#include <ntfstl/threadpool.hpp>
#include <ntfstl/types.hpp>
#include <ntfstl/unique_array.hpp>

#define fn auto

namespace kappa {

using namespace ntf::numdefs;

using shogle::ivec4;
using shogle::mat4;
using shogle::vec2;
using shogle::vec3;
using shogle::vec4;

using ntf::cspan;
using ntf::span;
using shogle::color4;
using shogle::extent3d;
using shogle::mat4;
using shogle::quat;
using shogle::vec2;
using shogle::vec3;

using ntf::logger;

using real = f32;

template<typename T>
using expect = ntf::expected<T, std::string>;

constexpr u32 GAME_UPS = 60u;

inline std::string shogle_to_str(shogle::render_error&& err) {
  return err.what();
}

struct vec_span {
  static constexpr u32 INDEX_TOMB = std::numeric_limits<u32>::max();

  u32 idx, count;

  template<typename T>
  span<T> to_span(T* data) const {
    if (empty()) {
      return {};
    }
    return {data + idx, count};
  }

  template<typename T>
  cspan<T> to_cspan(const T* data) const {
    if (empty()) {
      return {};
    }
    return {data + idx, count};
  }

  bool empty() const { return idx == INDEX_TOMB || count == 0u; }

  u32 size() const { return count; }
};

} // namespace kappa
