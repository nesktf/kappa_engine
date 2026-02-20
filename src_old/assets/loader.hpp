#pragma once

#include "assets/rigged_model.hpp"

#define DECLARE_ASSET(type_, opts_type_)                                                          \
  template<>                                                                                      \
  struct asset_type_mapper<asset_tag::type_> {                                                    \
    using type = type_;                                                                           \
  };                                                                                              \
  template<>                                                                                      \
  struct asset_opt_mapper<type_> {                                                                \
    using type = opts_type_;                                                                      \
  };                                                                                              \
  template<>                                                                                      \
  struct asset_tag_mapper<type_> : public std::integral_constant<asset_tag, asset_tag::type_> {}; \
  static_assert(asset_type<type_>)

namespace kappa::assets {

struct handle_t {
  ~handle_t();
};

[[nodiscard]] handle_t initialize();

enum class asset_tag : u32 {
  static_model = 0,
  rigged_model,
  texture,
};

template<asset_tag>
struct asset_type_mapper {};

template<asset_tag Tag>
using asset_type_mapper_t = asset_type_mapper<Tag>::type;

template<typename>
struct asset_opt_mapper {};

template<typename T>
using asset_opt_mapper_t = asset_opt_mapper<T>::type;

template<typename>
struct asset_tag_mapper {};

template<typename T>
constexpr auto asset_tag_mapper_v = asset_tag_mapper<T>::value;

template<typename T>
concept asset_type = requires() {
  typename asset_opt_mapper_t<T>;
  requires std::same_as<T, std::remove_cv_t<asset_type_mapper_t<asset_tag_mapper_v<T>>>>;
};

struct rigged_model_opts {
  u32 flags;
  std::string_view armature;
};

DECLARE_ASSET(rigged_model, rigged_model_opts);

void clear_assets();
void handle_requests();

void do_destroy_asset(u64 handle, asset_tag tag);
void* do_get_asset(u64 handle, asset_tag tag);

class asset_handle {
public:
  asset_handle(u64 handle, asset_tag tag) noexcept : _handle{handle}, _tag{tag} {}

public:
  template<asset_type T>
  T& as() const {
    NTF_ASSERT(asset_tag_mapper_v<T> == _tag);
    auto* ptr = static_cast<T*>(do_get_asset(value(), tag()));
    NTF_ASSERT(ptr);
    return *ptr;
  }

  template<asset_type T>
  T* as_opt() const {
    if (asset_tag_mapper_v<T> != tag()) {
      return nullptr;
    }
    return static_cast<T*>(do_get_asset(value(), tag()));
  }

public:
  u32 value() const noexcept { return _handle; }

  asset_tag tag() const noexcept { return _tag; }

private:
  u64 _handle;
  asset_tag _tag;
};

ntf::optional<asset_handle> do_find_asset(const std::string& name, asset_tag tag);

template<typename T>
class typed_handle {
public:
  explicit typed_handle(asset_handle entry) : _handle{entry.value()} {
    if (entry.tag() != tag()) {
      throw std::invalid_argument{"Invalid asset entry"};
    }
  }

  typed_handle(ntf::unchecked_t, u64 handle) noexcept : _handle{handle} {}

public:
  static ntf::optional<typed_handle> from_opaque(asset_handle asset) {
    if (asset.tag() != asset_tag_mapper_v<T>) {
      return {ntf::nullopt};
    }
    return {ntf::in_place, ntf::unchecked, asset.value()};
  }

  static typed_handle from_opaque_unchecked(asset_handle asset) {
    NTF_ASSERT(asset.tag() == asset_tag_mapper_v<T>);
    return {ntf::unchecked, asset.value()};
  }

public:
  T& get() const {
    auto* ptr = static_cast<T*>(do_get_asset(value(), tag()));
    NTF_ASSERT(ptr);
    return *ptr;
  }

  T* get_opt() const noexcept { return static_cast<T*>(value(), tag()); }

  T* operator->() const { return &get(); }

  T& operator*() const { return get(); }

public:
  u64 value() const noexcept { return _handle; }

  asset_tag tag() const noexcept { return asset_tag_mapper_v<T>; }

private:
  u64 _handle;
};

static constexpr size_t MAX_CB_SIZE = 4 * sizeof(void*);
using asset_callback = ntf::inplace_function<void(expect<asset_handle>), MAX_CB_SIZE>;

void do_request_asset(const std::string& name, const std::string& path, asset_tag tag,
                      asset_callback callback, const void* opts);

template<asset_type T, typename F>
requires(std::is_invocable_r_v<void, F, expect<typed_handle<T>>> && sizeof(F) <= MAX_CB_SIZE)
void request_asset(const std::string& name, const std::string& path,
                   const asset_opt_mapper_t<T>& opts, F&& func) {
  static constexpr auto tag = asset_tag_mapper_v<T>;
  do_request_asset(
    name, path, tag,
    [func = std::forward<F>(func)](expect<asset_handle> h) {
      std::invoke(func, std::move(h).transform(typed_handle<T>::from_opaque_unchecked));
    },
    &opts);
}

template<asset_type T>
ntf::optional<typed_handle<T>> find_asset(const std::string& name) {
  return do_find_asset(name, asset_tag_mapper_v<T>)
    .transform(typed_handle<T>::from_opaque_unchecked);
}

template<asset_type T>
void destroy_asset(typed_handle<T> asset) {
  do_destroy_asset(asset.value(), asset.tag());
}

using rigged_model_handle = typed_handle<rigged_model>;

} // namespace kappa::assets
