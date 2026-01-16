#pragma once

#include "./material.hpp"
#include "./model_loader.hpp"

namespace kappa::assets {

static constexpr u32 MAX_MODEL_MATERIALS = 4u;
using mesh_attr_data = std::pair<const void*, u32>;

struct vertex_config {
  size_t size;
  std::string_view name;
};

struct mesh_offset {
  u32 index_offset;
  u32 index_count;
  u32 vertex_offset;
};

template<typename T>
concept mesh_data_type = requires(const T mesh_data, u32 attr_idx, u32 mesh_idx) {
  requires ntf::meta::std_cont<std::remove_const_t<decltype(T::attr_config)>, vertex_config>;
  requires std::convertible_to<std::remove_const_t<decltype(T::attr_count)>, u32>;
  { mesh_data.vertex_count() } -> std::convertible_to<u32>;
  { mesh_data.index_count() } -> std::convertible_to<u32>;
  { mesh_data.mesh_count() } -> std::convertible_to<u32>;
  { mesh_data.mesh_index_range(mesh_idx) } -> std::same_as<vec_span>;
  { mesh_data.vertex_data(attr_idx, mesh_idx) } -> std::same_as<std::pair<const void*, u32>>;
  { mesh_data.index_data() } -> std::same_as<cspan<u32>>;
};

template<u32 AttrCount>
class model_meshes {
public:
  using vert_buffs = std::array<shogle::buffer_t, AttrCount>;
  using vert_binds = std::array<shogle::vertex_binding, AttrCount>;

public:
  model_meshes(vert_buffs buffs, u32 vertex_count, shogle::index_buffer&& idx_buff,
               u32 index_count, ntf::unique_array<mesh_offset>&& offsets);

  NTF_DECLARE_MOVE_ONLY(model_meshes);

public:
  render::mesh_render_data& retrieve_mesh_data(u32 mesh_idx,
                                               std::vector<render::mesh_render_data>& data) const;

public:
  bool has_indices() const { return !_idx_buff.empty(); }

  shogle::vertex_buffer_view vertex_buffer(u32 idx) const;

  shogle::index_buffer_view index_buffer() const { return _idx_buff; }

  cspan<mesh_offset> offsets() const { return {_offsets.data(), _offsets.size()}; }

  u32 mesh_count() const { return _offsets.size(); }

  u32 vertex_count() const { return _vertex_count; }

  u32 index_count() const { return _index_count; }

private:
  void _reset_buffs() noexcept;
  void _free_buffs() noexcept;

private:
  vert_buffs _buffs;
  vert_binds _binds;
  ntf::unique_array<mesh_offset> _offsets;
  shogle::index_buffer _idx_buff;
  u32 _vertex_count, _index_count;
};

template<mesh_data_type MeshData>
fn make_model_meshes(const MeshData& mesh_data) -> expect<model_meshes<MeshData::attr_count>>;

struct rigged_model_data {
public:
  static constexpr std::array<vertex_config, 7> attr_config{{
    {.size = sizeof(vec3), .name = "rigged_position"},
    {.size = sizeof(vec3), .name = "rigged_normal"},
    {.size = sizeof(vec2), .name = "rigged_uv"},
    {.size = sizeof(vec3), .name = "rigged_tangents"},
    {.size = sizeof(vec3), .name = "rigged_bitangents"},
    {.size = sizeof(model_mesh_data::vertex_bones), .name = "rigged_bones"},
    {.size = sizeof(model_mesh_data::vertex_weights), .name = "rigged_weights"},
  }};
  static constexpr u32 attr_count = attr_config.size();

public:
  static fn from_file(const std::string& path, u32 flags) -> expect<rigged_model_data>;

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

static_assert(mesh_data_type<rigged_model_data>);

struct rigged_model {
  struct model_bone {
    std::string name;
    u32 parent;
  };

  struct model_rig {
    ntf::unique_array<model_bone> bones;
    ntf::unique_array<mat4> bone_locals;
    ntf::unique_array<mat4> bone_inv_models;
    std::unordered_map<std::string_view, u32> bone_reg;
  };

  std::string name;
  model_meshes<rigged_model_data::attr_count> meshes;
  std::array<material_handle, MAX_MODEL_MATERIALS> materials;
  model_rig rig;
};

DECLARE_ASSET(asset_tag::rigged_model, rigged_model, rigged_model_handle);

struct static_model_data {
public:
  static constexpr std::array<vertex_config, 5> attr_config{{
    {.size = sizeof(vec3), .name = "static_position"},
    {.size = sizeof(vec3), .name = "static_normal"},
    {.size = sizeof(vec2), .name = "static_uv"},
    {.size = sizeof(vec3), .name = "static_tangents"},
    {.size = sizeof(vec3), .name = "static_bitangents"},
  }};
  static constexpr u32 attr_count = attr_config.size();

public:
  static fn from_file(const std::string& path, u32 flags) -> expect<static_model_data>;

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

static_assert(mesh_data_type<static_model_data>);

struct static_model {
  std::string name;
  model_meshes<static_model_data::attr_count> meshes;
  std::array<material_handle, MAX_MODEL_MATERIALS> materials;
};

DECLARE_ASSET(asset_tag::static_model, static_model, static_model_handle);

} // namespace kappa::assets

#ifndef KAPPA_ASSETS_MODEL_INL
#include "./model.inl"
#endif
