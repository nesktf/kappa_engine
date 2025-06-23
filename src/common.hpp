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
