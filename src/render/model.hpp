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

  struct texture_t {
    buffer_name name;
    texture_handle texture;
    assets::texture_type type;
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
                     rig_t&& rig);

public:
  static s_expect<model3d_renderable> load_model(const assets::model3d_data& data);

public:
  std::string_view name() const;

  span<const mesh_t> meshes() const;

  optional<u32> find_bone_idx(std::string_view name) const;
  span<const bone_t> bones() const;
  span<const m4f32> bone_locals() const;
  span<const m4f32> bone_inv_models() const;

private:
  buffer_name _name;
  unique_array<mesh_t> _meshes;
  unique_array<pipeline_t> _pipelines;
  unique_array<texture_t> _textures;
  rig_t _rig;
};

} // namespace kappa::render
