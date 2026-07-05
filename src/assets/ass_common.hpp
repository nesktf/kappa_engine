#pragma once

#include "core.hpp"

namespace kappa::assets {

struct Extent2D {
  u32 width, height;
};

using BufferName = BuffStr<128>;
using BufferPath = BuffStr<256>;

enum class ImageFormat {
  rgb8u = 0,
  rgba8u,
  rgb16u,
  rgba16u,
  rgb32f,
  rgba32f,
};

enum class TextureType {
  albedo = 0,
  diffuse = 0,
  specular,
  normal,
  ambient_occlusion,
};

struct ArrayRange {
  static constexpr u32 index_tomb = (u32)-1;

  static constexpr ArrayRange null_range() noexcept { return {index_tomb, 0}; }

  u32 start;
  u32 count;
};

class AssetErr : public std::exception {
public:
  AssetErr(std::string_view path, std::string_view name, const BuffStr<256>& msg) noexcept :
      _msg(msg) {
    _path.copy_from(path.data(), path.size());
    _name.copy_from(name.data(), name.size());
  }

  template<typename... Args>
  static AssetErr format(std::string_view path, std::string_view name,
                         fmt::format_string<Args...> fmt, Args&&... args) {
    return {path, name, buffer_str_fmt<256>(fmt, std::forward<Args>(args)...)};
  }

public:
  const char* what() const noexcept override { return _msg.c_str(); }

public:
  std::string_view path() const noexcept { return _path.as_view(); }

  std::string_view name() const noexcept { return _name.as_view(); }

  std::string_view msg() const noexcept { return _msg.as_view(); }

private:
  BufferPath _path;
  BufferName _name;
  BuffStr<256> _msg;
};

template<typename T>
using AssExpect = Expected<T, AssetErr>;

} // namespace kappa::assets
