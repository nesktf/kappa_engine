#define KAPPA_ASSETS_COMMON_INL
#include "./common.hpp"
#undef KAPPA_ASSETS_COMMON_INL

namespace kappa::assets {

template<asset_type T>
fn asset_handle::as() const -> T& {
  NTF_ASSERT(asset_tag_mapper_v<T> == _tag);
  auto* ptr = static_cast<T*>(do_get_asset(value(), tag()));
  NTF_ASSERT(ptr);
  return *ptr;
}

template<asset_type T>
fn asset_handle::as_opt() const -> T* {
  if (asset_tag_mapper_v<T> != tag()) {
    return nullptr;
  }
  return static_cast<T*>(do_get_asset(value(), tag()));
}

template<typename T>
typed_handle<T>::typed_handle(asset_handle entry) : _handle(entry.value()) {
  if (entry.tag() != tag()) {
    throw std::invalid_argument{"Invalid asset entry"};
  }
}

template<typename T>
fn typed_handle<T>::from_opaque(asset_handle asset) -> ntf::optional<typed_handle> {
  if (asset.tag() != asset_tag_mapper_v<T>) {
    return {ntf::nullopt};
  }
  return {ntf::in_place, ntf::unchecked, asset.value()};
}

template<typename T>
fn typed_handle<T>::from_opaque_unchecked(asset_handle asset) -> typed_handle {
  NTF_ASSERT(asset.tag() == asset_tag_mapper_v<T>);
  return {ntf::unchecked, asset.value()};
}

template<typename T>
fn typed_handle<T>::get() const -> T& {
  auto* ptr = static_cast<T*>(do_get_asset(value(), tag()));
  NTF_ASSERT(ptr);
  return *ptr;
}

template<typename T>
fn typed_handle<T>::get_opt() const noexcept -> T* {
  return static_cast<T*>(value(), tag());
}

} // namespace kappa::assets
