#pragma once

#include "../core.hpp"

namespace kappa::assets {

struct image_data {
public:
  struct image_internal;

public:
  image_data(image_internal& data) noexcept;

  void destroy() noexcept;

public:
  buffer_name& name() const;
  buffer_path& path() const;
  void* data() const;
  extent2d extent() const;
  image_format format() const;

private:
  image_internal* _data;
};

class image_loader {
public:
  struct loader_internal;

  enum load_flags : u32 {
    FLAGS_NONE = 0x0000,
    FLAG_FLIP_Y = 0x0001,
  };

public:
  image_loader(std::string_view texture_path, std::string_view texture_name,
               bits32 flags = FLAGS_NONE);

public:
  // Should be only called ONCE, preferably in a threadpool
  // The internal data is destroyed at the end of the function
  bs_expect<image_data, 256> load();

public:
  bs_expect<image_data, 256> operator()() { return load(); }

private:
  loader_internal* _impl;
};

} // namespace kappa::assets
