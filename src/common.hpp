#pragma once

#include <ntfstl/types.hpp>
#include <shogle/shogle.hpp>
#include <ntfstl/span.hpp>
#include <ntfstl/expected.hpp>

namespace kappa {

using namespace ntf::numdefs;

using shogle::mat4;
using shogle::vec2;
using shogle::vec3;
using shogle::vec4;
using shogle::ivec4;

using ntf::span;
using ntf::cspan;
using shogle::mat4;
using shogle::vec3;
using shogle::vec2;
using shogle::quat;
using shogle::color4;
using shogle::extent3d;

template<typename T>
using expect = ntf::expected<T, std::string_view>;

constexpr u32 GAME_UPS = 60u;

struct vec_span {
  static constexpr u32 INDEX_TOMB = std::numeric_limits<u32>::max();

  u32 idx, count;

  template<typename T>
  span<T> to_span(T* data) const {
    if (empty()) {
      return {};
    }
    return {data+idx, count};
  }

  template<typename T>
  cspan<T> to_cspan(const T* data) const {
    if (empty()) {
      return {};
    }
    return {data+idx, count};
  }

  bool empty() const { return idx == INDEX_TOMB || count == 0u; }
  u32 size() const { return count; }
};

} // namespace kappa
