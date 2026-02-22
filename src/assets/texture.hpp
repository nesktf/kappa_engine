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
  texture_data(texture_internal& data) noexcept;

  void destroy() noexcept;

public:
  buffer_name& name() const;
  buffer_path& path() const;
  void* data() const;
  extent2d extent() const;
  image_format format() const;
  texture_type type() const;

private:
  texture_internal* _data;
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

} // namespace kappa::assets
