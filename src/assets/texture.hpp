#pragma once

#include "./ass_common.hpp"

namespace kappa::assets {

struct ImageData {
public:
  struct ImageInternal;

public:
  ImageData(ImageInternal& data) noexcept;

  void destroy() noexcept;

public:
  BufferName& name() const;
  BufferPath& path() const;
  void* data() const;
  Extent2D extent() const;
  ImageFormat format() const;

private:
  ImageInternal* _data;
};

class ImageLoader {
public:
  struct LoaderInternal;

  enum LoadFlags : u32 {
    FLAGS_NONE = 0x0000,
    FLAG_FLIP_Y = 0x0001,
  };

public:
  ImageLoader(std::string_view texture_path, std::string_view texture_name,
              bits32 flags = FLAGS_NONE);

public:
  // Should be only called ONCE, preferably in a threadpool
  // The internal data is destroyed at the end of the function
  AssExpect<ImageData> load();

public:
  AssExpect<ImageData> operator()() { return load(); }

private:
  LoaderInternal* _impl;
};

} // namespace kappa::assets
