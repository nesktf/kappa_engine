#include "./internal.hpp"

#define TEX_LOG(_level, _fmt, ...) \
  ::kappa::log_##_level("[TEXTURE_IMPORTER] " _fmt __VA_OPT__(, ) __VA_ARGS__)

namespace kappa::assets {

ImageLoader::ImageLoader(std::string_view texture_path, std::string_view texture_name, u32 flags) :
    _impl(new ImageLoader::LoaderInternal()) {
  _impl->texture_path.copy_from(texture_path.data(), texture_path.size());
  _impl->texture_name.copy_from(texture_name.data(), texture_name.size());
  _impl->chima_flags = flags;
}

AssExpect<ImageData> ImageLoader::load() {
  ka_assert(_impl, "texture_loader use after free");
  const DeferFn defer = [this]() {
    delete _impl;
    _impl = nullptr;
  };

  chima::error chimaerr;
  auto chima = chima::context::create(nullptr, &chimaerr);
  if (!chima) {
    auto err = AssetErr::format(_impl->texture_path.as_view(), _impl->texture_name.as_view(),
                                "Failed to create loading context, {}", chimaerr.what());
    TEX_LOG(error, "{}", err.msg());
    return {unexpect, std::move(err)};
  }
  if (_impl->chima_flags & FLAG_FLIP_Y) {
    chima->set_flip_y(true);
  }

  auto image = chima::image::load(*chima, CHIMA_DEPTH_8U, _impl->texture_path.c_str(), &chimaerr);
  if (!image) {
    auto err = AssetErr::format(_impl->texture_path.as_view(), _impl->texture_name.as_view(),
                                "Failed to load image at, {}", chimaerr.what());
    TEX_LOG(error, "{}", err.msg());
    return {unexpect, std::move(err)};
  }

  try {
    auto* ptr = new ImageData::ImageInternal(*std::move(chima), *image);
    std::memcpy(ptr->name.data, _impl->texture_name.data, sizeof(ptr->name.data));
    ptr->name.len = _impl->texture_name.len;
    std::memcpy(ptr->path.data, _impl->texture_path.data, sizeof(ptr->path.data));
    ptr->path.len = _impl->texture_path.len;
    ptr->format = parse_chima_format(image->get().depth, image->get().channels);

    return {in_place, *ptr};
  } catch (const std::bad_alloc&) {
    chima::image::destroy(*chima, *image);
    auto err = AssetErr::format(_impl->texture_path.as_view(), _impl->texture_name.as_view(),
                                "Failed to allocate texture internals");
    TEX_LOG(error, "{}", err.msg());
    return {unexpect, std::move(err)};
  }
}

ImageData::ImageData(ImageInternal& data) noexcept : _data(&data) {}

void ImageData::destroy() noexcept {
  if (!_data) {
    return;
  }
  delete _data;
  _data = nullptr;
}

BufferName& ImageData::name() const {
  ka_assert(_data, "texture_data use after free");
  return _data->name;
}

BufferPath& ImageData::path() const {
  ka_assert(_data, "texture_data use after free");
  return _data->path;
}

void* ImageData::data() const {
  ka_assert(_data, "texture_data use after free");
  return _data->image.data();
}

Extent2D ImageData::extent() const {
  ka_assert(_data, "texture_data use after free");
  return {_data->image.get().extent.width, _data->image.get().extent.height};
}

ImageFormat ImageData::format() const {
  ka_assert(_data, "texture_data use after free");
  return _data->format;
}

} // namespace kappa::assets
