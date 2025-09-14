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

  std::pair<const void*, u32> vertex_data(u32 attr_idx, u32 mesh_idx) const {
    enum {
      ATTR_POS = 0,
      ATTR_NORM,
      ATTR_UVS,
      ATTR_TANG,
      ATTR_BITANG,

      ATTR_COUNT
    };
    NTF_ASSERT(mesh_idx < meshes.meshes.size());
    if (attr_idx >= ATTR_COUNT) {
      return std::make_pair(nullptr, 0u);
    } 

    const auto& mesh_meta = meshes.meshes[mesh_idx];
    switch (attr_idx) {
      case ATTR_POS: {
        const auto span = mesh_meta.positions.to_cspan(meshes.positions.data());
        return std::make_pair(static_cast<const void*>(span.data()), span.size());
      } break;
      case ATTR_NORM: {
        const auto span = mesh_meta.normals.to_cspan(meshes.normals.data());
        return std::make_pair(static_cast<const void*>(span.data()), span.size());
      } break;
      case ATTR_UVS: {
        const auto span = mesh_meta.uvs.to_cspan(meshes.uvs.data());
        return std::make_pair(static_cast<const void*>(span.data()), span.size());
      } break;
      case ATTR_TANG: {
        const auto span = mesh_meta.tangents.to_cspan(meshes.tangents.data());
        return std::make_pair(static_cast<const void*>(span.data()), span.size());
      } break;
      case ATTR_BITANG: {
        const auto span = mesh_meta.tangents.to_cspan(meshes.bitangents.data());
        return std::make_pair(static_cast<const void*>(span.data()), span.size());
      } break;
      default: break;
    }
    NTF_UNREACHABLE();
  }
};
static_assert(meta::mesh_data_type<static_model_data>);

class static_model3d :
  public model3d_mesh_buffers<static_model_data>,
  public model3d_mesh_textures
{

};

} // namespace kappa
