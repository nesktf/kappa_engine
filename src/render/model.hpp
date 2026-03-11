#pragma once

#include "../assets/model.hpp"
#include "./instance.hpp"

namespace kappa::render {

enum model_attrib_flag : bits32 {
  MODEL_ATTRIB_NONE = 0x0000,
  MODEL_ATTRIB_POSITIONS = 0x0001,
  MODEL_ATTRIB_NORMALS = 0x0002,
  MODEL_ATTRIB_TANGENTS = 0x0004,
  MODEL_ATTRIB_BONES = 0x0008,
  MODEL_ATTRIB_UV0 = 0x0010,
  MODEL_ATTRIB_UV1 = 0x0020,
  MODEL_ATTRIB_COLOR0 = 0x0040,
  MODEL_ATTRIB_COLOR1 = 0x0080,
  MODEL_ATTRIB_ALL = 0xFFFF,
};

struct model3d_texture_data {
public:
  static fn from_asset(const assets::texture_data& asset) -> s_expect<model3d_texture_data>;

public:
  buffer_name name;
  texture_handle texture;
  extent2d extent;
  assets::texture_type type;
};

class model3d_renderable {
public:
  static constexpr size_t MAX_MESH_TEXTURES = 16;

  struct mesh_t {
    buffer_name name;
    buffer_handle vertex_buffer, index_buffer;
    u32 vertex_count, index_count;
    bits32 vertex_attributes;
    u32 pipeline;
    std::array<u32, MAX_MESH_TEXTURES> textures;
    u32 texture_count;
  };

  struct bone_t {
    buffer_name name;
    i32 parent;
  };

  struct pipeline_t {
    pipeline_handle pipeline;
  };

  struct rig_t {
    std::unordered_map<std::string_view, u32> bone_map;
    unique_array<bone_t> bones;
    unique_array<m4f32> bone_locals;
    unique_array<m4f32> bone_inv_models;
  };

private:
  struct create_t {};

public:
  model3d_renderable(create_t, const buffer_name& name, unique_array<mesh_t>&& meshes,
                     unique_array<model3d_texture_data>&& textures, rig_t&& rig);

public:
  static fn load_model(unique_array<model3d_texture_data>&& textures,
                       const assets::model3d_data& data) -> s_expect<model3d_renderable>;

  void destroy();

public:
  fn name() const -> std::string_view;
  fn meshes() const -> span<const mesh_t>;

  fn find_bone_idx(std::string_view name) const -> optional<u32>;
  fn bones() const -> span<const bone_t>;
  fn bone_locals() const -> span<const m4f32>;
  fn bone_inv_models() const -> span<const m4f32>;

private:
  buffer_name _name;
  unique_array<mesh_t> _meshes;
  unique_array<pipeline_t> _pipelines;
  unique_array<model3d_texture_data> _textures;
  rig_t _rig;
};

} // namespace kappa::render
