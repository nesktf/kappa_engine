#pragma once

#include "../core.hpp"

namespace kappa::assets {

enum class image_format {
  rgb8u = 0,
  rgba8u,
  rgb16u,
  rgba16u,
  rgb32f,
  rgba32f,
};

enum class texture_type {
  diffuse = 0,
  albedo = 0,
  specular,
  normal,
  ambient_occlusion,
  roughness,
  metallic,
};

struct texture_data {
public:
  struct texture_internal;

public:
  texture_data() noexcept;

  void destroy() noexcept;

public:
  texture_internal* _impl;
  std::string_view path;
  std::string_view name;
  void* data;
  extent2d extent;
  image_format format;
  texture_type type;
};

class texture_loader {
public:
  struct loader_internal;

  enum load_flags : u32 {
    FLAGS_NONE = 0x0000,
    FLAG_FLIP_Y = 0x0001,
  };

public:
  texture_loader(std::string_view texture_path, std::string_view texture_name,
                 bits32 flags = FLAGS_NONE, optional<texture_type> type = nullopt);

public:
  // Should be only called ONCE, preferably in a threadpool
  // The internal data is destroyed at the end of the function
  bs_expect<texture_data, 256> load();

public:
  bs_expect<texture_data, 256> operator()() { return load(); }

private:
  loader_internal* _impl;
};

struct cubemap_data {
public:
  struct cubemap_internal;

public:
  cubemap_data() noexcept;

  void destroy() noexcept;

public:
  cubemap_internal* _impl;
  std::string_view path;
  std::string_view name;
  std::array<void*, 6> datas;
  extent2d extent;
  image_format format;
  texture_type type;
};

class cubemap_loader {
public:
  struct loader_internal;

  enum load_flags : u32 {
    FLAGS_NONE = 0x0000,
    FLAG_FLIP_Y = 0x0001,
  };

public:
  cubemap_loader(std::string_view cubemap_path, std::string_view cubemap_name,
                 bits32 flags = FLAGS_NONE);

public:
  // Should be only called ONCE, preferably in a threadpool
  // The internal data is destroyed at the end of the function
  bs_expect<texture_data, 256> load();

public:
  bs_expect<texture_data, 256> operator()() { return load(); }

private:
  loader_internal* _impl;
};

} // namespace kappa::assets
