#pragma once

#include <ntfstl/types.hpp>
#include <shogle/math.hpp>
#include <ntfstl/span.hpp>
#include <ntfstl/expected.hpp>

using namespace ntf::numdefs;

using ntf::mat4;
using ntf::vec2;
using ntf::vec3;
using ntf::vec4;
using ntf::ivec4;

using ntf::span;
using ntf::cspan;
using ntf::mat4;
using ntf::vec3;
using ntf::vec2;
using ntf::quat;
using ntf::color4;
using ntf::extent3d;

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
};
