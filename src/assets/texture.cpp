#include "./internal.hpp"

#define TEX_LOG(level_, fmt_, ...) \
  ::kappa::logger::level_("[TEXTURE_IMPORTER] " fmt_ __VA_OPT__(, ) __VA_ARGS__)

namespace kappa::assets {

texture_loader::texture_loader(std::string_view texture_path, std::string_view texture_name,
                               bits32 flags, optional<texture_type> type) :
    _impl(new texture_loader::loader_internal()) {
  _impl->texture_path.copy_from(texture_path.data(), texture_path.size());
  _impl->texture_name.copy_from(texture_name.data(), texture_name.size());
  _impl->chima_flags = flags;
  _impl->type = type;
}

namespace {

image_format parse_chima_format(chima_image_depth depth, u32 channels) {
  assert(channels == 3 || channels == 4);
  switch (depth) {
    case CHIMA_DEPTH_8U: {
      return channels == 4 ? image_format::rgba8u : image_format::rgb8u;
    } break;
    case CHIMA_DEPTH_16U: {
      return channels == 4 ? image_format::rgba16u : image_format::rgb16u;
    } break;
    case CHIMA_DEPTH_32F: {
      return channels == 4 ? image_format::rgba32f : image_format::rgb32f;
    } break;
    default:
      SHOGLE_UNREACHABLE();
  }
};

texture_type parse_type_from_name(std::string_view name) {
  SHOGLE_UNUSED(name); // TODO
  return texture_type::albedo;
};

} // namespace

bs_expect<texture_data, 256> texture_loader::load() {
  assert(_impl, "texture_loader use after free");
  const shogle::scope_end defer = [this]() {
    delete _impl;
    _impl = nullptr;
  };

  chima::error err;
  auto chima = chima::context::create(nullptr, &err);
  if (!chima) {
    auto errstr = buffer_str_fmt<256>("Failed to create loading context for \"{}\", {}",
                                      _impl->texture_path.as_view(), err.what());
    TEX_LOG(error, "{}", errstr.as_view());
    return {unexpect, std::move(errstr)};
  }
  if (_impl->chima_flags & FLAG_FLIP_Y) {
    chima->set_flip_y(true);
  }

  auto image = chima::image::load(*chima, CHIMA_DEPTH_8U, _impl->texture_path.c_str(), &err);
  if (!image) {
    auto errstr = buffer_str_fmt<256>("Failed to load image at \"{}\", {}",
                                      _impl->texture_path.as_view(), err.what());
    TEX_LOG(error, "{}", errstr.as_view());
    return {unexpect, std::move(errstr)};
  }

  texture_data out;
  out.data = image->get().data;
  out.extent.width = image->get().extent.width;
  out.extent.height = image->get().extent.height;
  out.format = parse_chima_format(image->get().depth, image->get().channels);
  out.type = _impl->type ? *_impl->type : parse_type_from_name(_impl->texture_name.as_view());
  try {
    auto* ptr = new texture_data::texture_internal(*std::move(chima), *image);
    std::memcpy(ptr->name_data, _impl->texture_name.data, sizeof(ptr->name_data));
    out.name = {ptr->name_data, _impl->texture_name.len};
    std::memcpy(ptr->path_data, _impl->texture_path.data, sizeof(ptr->path_data));
    out.path = {ptr->path_data, _impl->texture_path.len};
    out._impl = ptr;
  } catch (const std::bad_alloc&) {
    chima::image::destroy(*chima, *image);
    auto errstr = buffer_str_fmt<256>("Failed to allocate texture internals for \"{}\"",
                                      _impl->texture_path.as_view());
    TEX_LOG(error, "{}", errstr.as_view());
    return {unexpect, std::move(errstr)};
  }
  return {in_place, std::move(out)};
}

texture_data::texture_data() noexcept : _impl(nullptr) {}

void texture_data::destroy() noexcept {
  if (!_impl) {
    return;
  }
  delete _impl;
  _impl = nullptr;
}

} // namespace kappa::assets
