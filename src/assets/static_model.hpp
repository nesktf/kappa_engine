#pragma once

#include "./model_data.hpp"

namespace kappa {

struct static_model_data {
public:
  static constexpr std::array<vertex_config, 5> VERT_CONFIG{{
      {.size = sizeof(vec3), .name = "static_position"},
      {.size = sizeof(vec3), .name = "static_normal"},
      {.size = sizeof(vec2), .name = "static_uv"},
      {.size = sizeof(vec3), .name = "static_tangents"},
      {.size = sizeof(vec3), .name = "static_bitangents"},
  }};

  static constexpr std::string_view VERT_SHADER_LAYOUT = R"glsl(
layout (location = 0) in vec3 att_positions;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_uvs;
layout (location = 3) in vec3 att_tangents;
layout (location = 4) in vec3 att_bitangents;
)glsl";

public:
  std::string name;
  model_mesh_data meshes;
  model_material_data materials;

public:
  u32 vertex_count() const { return meshes.positions.size(); }

  u32 index_count() const { return meshes.indices.size(); }

  u32 mesh_count() const { return meshes.meshes.size(); }

  vec_span mesh_index_range(u32 mesh_idx) const {
    NTF_ASSERT(mesh_idx < meshes.meshes.size());
    return meshes.meshes[mesh_idx].indices;
  }

  cspan<u32> index_data() const { return {meshes.indices.data(), meshes.indices.size()}; }

  std::pair<const void*, u32> vertex_data(u32 attr_idx, u32 mesh_idx) const;
};

static_assert(meta::mesh_data_type<static_model_data>);

class static_model3d :
    public model3d_mesh_buffers<static_model_data>,
    public model3d_mesh_textures {};

} // namespace kappa
