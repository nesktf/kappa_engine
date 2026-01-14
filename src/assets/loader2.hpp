#pragma once

#include "../core.hpp"
#include "./model_data.hpp"
#include <chimatools/chimatools.hpp>

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
  fn as() const -> T& {
    NTF_ASSERT(asset_tag_mapper_v<T> == _tag);
    auto* ptr = static_cast<T*>(do_get_asset(value(), tag()));
    NTF_ASSERT(ptr);
    return *ptr;
  }

  template<asset_type T>
  fn as_opt() const -> T* {
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

struct sound {
  std::string name;
  ntf::unique_array<u32> samples; // ?
};

DECLARE_ASSET(asset_tag::sound, sound, sound_handle);

struct texture {
  std::string name;
  shogle::texture2d texture;
};

DECLARE_ASSET(asset_tag::texture, texture, texture_handle);

struct spritesheet {
  std::string name;
  texture_handle texture;
  ntf::unique_array<chima_sprite> sprites;
  ntf::unique_array<chima_sprite_anim> anims;
};

DECLARE_ASSET(asset_tag::spritesheet, spritesheet, spritesheet_handle);

enum material_texture_type {
  MATERIAL_TEX_ALBEDO = 0,
  MATERIAL_TEX_COUNT,
};

struct material {
  std::string name;
  shogle::pipeline pipeline;
  std::array<texture_handle, MATERIAL_TEX_COUNT> textures;
};

DECLARE_ASSET(asset_tag::material, material, material_handle);

static constexpr u32 MAX_MODEL_MATERIALS = 4u;
using mesh_attr_data = std::pair<const void*, u32>;

struct rigged_model_data {
public:
  static constexpr std::array<vertex_config, 7> VERT_CONFIG{{
    {.size = sizeof(vec3), .name = "rigged_position"},
    {.size = sizeof(vec3), .name = "rigged_normal"},
    {.size = sizeof(vec2), .name = "rigged_uv"},
    {.size = sizeof(vec3), .name = "rigged_tangents"},
    {.size = sizeof(vec3), .name = "rigged_bitangents"},
    {.size = sizeof(model_mesh_data::vertex_bones), .name = "rigged_bones"},
    {.size = sizeof(model_mesh_data::vertex_weights), .name = "rigged_weights"},
  }};

public:
  fn vertex_count() const -> u32;
  fn index_count() const -> u32;
  fn mesh_count() const -> u32;
  fn mesh_index_range(u32 mesh_idx) const -> vec_span;
  fn index_data() const -> cspan<u32>;
  fn vertex_data(u32 atrr_idx, u32 mesh_idx) const -> mesh_attr_data;

public:
  std::string name;
  std::string armature;
  model_mesh_data meshes;
  model_material_data materials;
  model_rig_data rigs;
  model_anim_data anims;
};

static_assert(meta::mesh_data_type<rigged_model_data>);

struct rigged_model {
  struct model_bone {
    std::string name;
    u32 parent;
  };

  std::string name;
  model_meshes<rigged_model_data> meshes;
  std::array<material_handle, MAX_MODEL_MATERIALS> materials;
  ntf::unique_array<model_bone> bones;
  ntf::unique_array<mat4> bone_locals;
  ntf::unique_array<mat4> bone_inv_models;
  std::unordered_map<std::string_view, u32> bone_reg;
};

DECLARE_ASSET(asset_tag::rigged_model, rigged_model, rigged_model_handle);

struct static_model_data {
public:
  static constexpr std::array<vertex_config, 5> VERT_CONFIG{{
    {.size = sizeof(vec3), .name = "static_position"},
    {.size = sizeof(vec3), .name = "static_normal"},
    {.size = sizeof(vec2), .name = "static_uv"},
    {.size = sizeof(vec3), .name = "static_tangents"},
    {.size = sizeof(vec3), .name = "static_bitangents"},
  }};

public:
  fn vertex_count() const -> u32;
  fn index_count() const -> u32;
  fn mesh_count() const -> u32;
  fn mesh_index_range(u32 mesh_idx) const -> vec_span;
  fn index_data() const -> cspan<u32>;
  fn vertex_data(u32 atrr_idx, u32 mesh_idx) const -> mesh_attr_data;

public:
  std::string name;
  model_mesh_data meshes;
  model_material_data materials;
};

struct static_model {
  std::string name;
  model_meshes<static_model_data> meshes;
  std::array<material_handle, MAX_MODEL_MATERIALS> materials;
};

DECLARE_ASSET(asset_tag::static_model, static_model, static_model_handle);

} // namespace kappa::assets
