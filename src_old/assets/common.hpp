#pragma once

#include "../core.hpp"

#define DECLARE_ASSET(tag_, type_, handle_)                                           \
  template<>                                                                          \
  struct asset_type_mapper<tag_> {                                                    \
    using type = type_;                                                               \
  };                                                                                  \
  template<>                                                                          \
  struct asset_tag_mapper<type_> : public std::integral_constant<asset_tag, tag_> {}; \
  static_assert(asset_type<type_>);                                                   \
  using handle_ = typed_handle<type_>

namespace kappa::assets {

enum class asset_tag {
  texture = 0,
  sound,
  material,
  spritesheet,
  rigged_model,
  static_model,
};

template<asset_tag>
struct asset_type_mapper {};

template<asset_tag Tag>
using asset_type_mapper_t = asset_type_mapper<Tag>::type;

template<typename>
struct asset_tag_mapper {};

template<typename T>
constexpr auto asset_tag_mapper_v = asset_tag_mapper<T>::value;

template<typename T>
concept asset_type = requires() {
  requires std::same_as<T, std::remove_cv_t<asset_type_mapper_t<asset_tag_mapper_v<T>>>>;
};

void do_destroy_asset(u64 handle, asset_tag tag);
void* do_get_asset(u64 handle, asset_tag tag);

class asset_handle {
public:
  asset_handle(u64 handle, asset_tag tag) noexcept : _handle{handle}, _tag{tag} {}

public:
  template<asset_type T>
  fn as() const -> T&;

  template<asset_type T>
  fn as_opt() const -> T*;

  u32 value() const noexcept { return _handle; }

  asset_tag tag() const noexcept { return _tag; }

private:
  u64 _handle;
  asset_tag _tag;
};

template<typename T>
class typed_handle {
public:
  explicit typed_handle(asset_handle entry);

  typed_handle(ntf::unchecked_t, u64 handle) noexcept : _handle{handle} {}

public:
  static ntf::optional<typed_handle> from_opaque(asset_handle asset);
  static typed_handle from_opaque_unchecked(asset_handle asset);

public:
  T& get() const;
  T* get_opt() const noexcept;

  T* operator->() const { return &get(); }

  T& operator*() const { return get(); }

  u64 value() const noexcept { return _handle; }

  asset_tag tag() const noexcept { return asset_tag_mapper_v<T>; }

private:
  u64 _handle;
};

} // namespace kappa::assets

#ifndef KAPPA_ASSETS_COMMON_INL
#include "./common.inl"
#endif
