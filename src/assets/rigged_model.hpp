#pragma once

#include "./model_data.hpp"

namespace kappa::assets {

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
  std::string name;
  std::string_view armature;
  model_mesh_data meshes;
  model_material_data materials;
  model_rig_data rigs;
  model_anim_data anims;

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
      ATTR_BONES,
      ATTR_WEIGHTS,

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
      case ATTR_BONES: {
        const auto span = mesh_meta.bones.to_cspan(meshes.bones.data());
        return std::make_pair(static_cast<const void*>(span.data()), span.size());
      } break;
      case ATTR_WEIGHTS: {
        const auto span = mesh_meta.bones.to_cspan(meshes.weights.data());
        return std::make_pair(static_cast<const void*>(span.data()), span.size());
      } break;
      default:
        break;
    }
    NTF_UNREACHABLE();
  }
};

static_assert(meta::mesh_data_type<rigged_model_data>);

struct rigged_model_bone {
  std::string name;
  u32 parent;
};

class rigged_model {
public:
  struct model_rigs {
    ntf::unique_array<rigged_model_bone> bones;
    std::unordered_map<std::string_view, u32> bone_reg;
    ntf::unique_array<mat4> bone_locals;
    ntf::unique_array<mat4> bone_inv_models;
  };

  struct bone_mats {
    ntf::cspan<mat4> locals;
    ntf::cspan<mat4> invs;
    ntf::cspan<rigged_model_bone> bones;
  };

  using data_t = rigged_model_data;

public:
  rigged_model(model_meshes<rigged_model_data>&& meshes, model_textures&& textures,
               model_rigs&& rigs, std::vector<model_material_data::material_meta>&& mats,
               std::unordered_map<std::string_view, u32>&& mat_reg, std::vector<u32>&& mesh_mats,
               shogle::pipeline&& pip, std::string&& name) noexcept;

public:
  static expect<rigged_model> create(rigged_model_data&& data);

public:
  bone_mats bones() const;

  u32 bone_count() const { return static_cast<u32>(_rigs.bones.size()); }

  ntf::optional<u32> find_bone(std::string_view name);

  std::string_view name() const { return _name; }

  shogle::pipeline_view pipeline() const { return _pip.get(); }

  u32 mat_idx(u32 mesh_idx) const {
    NTF_ASSERT(mesh_idx < _mesh_mats.size());
    return _mesh_mats[mesh_idx];
  }

private:
  model_meshes<rigged_model_data> _meshes;
  model_textures _textures;
  model_rigs _rigs;
  std::vector<model_material_data::material_meta> _mats;
  std::unordered_map<std::string_view, u32> _mat_reg;
  std::vector<u32> _mesh_mats;
  shogle::pipeline _pip;
  std::string _name;
};

} // namespace kappa::assets
