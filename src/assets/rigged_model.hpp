#pragma once

#include "./model_data.hpp"

namespace kappa {

struct rigged_model_data {
public:
  static constexpr std::array<vertex_config, 7> VERT_CONFIG{{
    {.size = sizeof(vec3), .name = "rigged_position"},
    {.size = sizeof(vec3), .name = "rigged_normal"},
    {.size = sizeof(vec2), .name = "rigged_uv"},
    {.size = sizeof(vec3), .name = "rigged_tangents"},
    {.size = sizeof(vec3), .name = "rigged_bitangents"},
    {.size = sizeof(model_mesh_data::vertex_bones),   .name = "rigged_bones"},
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

  std::pair<const void*, u32> vertex_data(u32 attr_idx, u32 mesh_idx) const {
    enum {
      ATTR_POS = 0,
      ATTR_NORM,
      ATTR_UVS,
      ATTR_TANG,
      ATTR_BITANG,
      ATTR_BONES,
      ATTR_WEIGHTS,
    };
    NTF_ASSERT(mesh_idx < meshes.meshes.size());
    if (attr_idx > ATTR_WEIGHTS) {
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
      default: break;
    }
    NTF_UNREACHABLE();
  }
  cspan<u32> index_data() const { return {meshes.indices.data(), meshes.indices.size()}; }
};
static_assert(meta::mesh_data_type<rigged_model_data>);

class rigged_model_texturer {
private:
  struct texture_t {
    std::string name;
    shogle::texture2d tex;
    u32 sampler;
  };
  struct material_t {
    std::string name;
    vec_span textures;
  };

public:
  rigged_model_texturer(ntf::unique_array<texture_t>&& textures,
                 std::unordered_map<std::string_view, u32>&& tex_reg,
                 ntf::unique_array<vec_span>&& mat_spans,
                 ntf::unique_array<u32>&& mat_texes) noexcept;

protected:
  static expect<rigged_model_texturer> create(const model_material_data& materials);

public:
  shogle::texture2d_view find_texture(std::string_view name);

protected:
  ntf::optional<u32> find_texture_idx(std::string_view name) const;
  u32 retrieve_material_textures(u32 mat_idx, std::vector<shogle::texture_binding>& texs) const;

private:
  ntf::unique_array<texture_t> _textures;
  std::unordered_map<std::string_view, u32> _tex_reg;
  ntf::unique_array<vec_span> _mat_spans;
  ntf::unique_array<u32> _mat_texes;
};

class model_rigger {
private:
  struct bone_t {
    std::string name;
    u32 parent;
  };

public:
  struct bone_transform {
    vec3 pos;
    vec3 scale;
    quat rot;
  };

public:
  model_rigger(shogle::shader_storage_buffer&& ssbo,
               ntf::unique_array<bone_t>&& bones,
               std::unordered_map<std::string_view, u32>&& bone_reg,
               ntf::unique_array<mat4>&& bone_locals,
               ntf::unique_array<mat4>&& bone_inv_models,
               ntf::unique_array<mat4>&& bone_transforms,
               ntf::unique_array<mat4>&& transf_cache,
               ntf::unique_array<mat4>&& transf_output) noexcept;

protected:
  static expect<model_rigger> create(const model_rig_data& rigs, std::string_view armature);

public:
  bool set_transform(std::string_view bone, const bone_transform& transf);
  bool set_transform(std::string_view bone, const mat4& transf);
  bool set_transform(std::string_view bone, shogle::transform3d<f32>& transf);
  
  void set_transform(u32 bone, const bone_transform& transf);
  void set_transform(u32 bone, const mat4& transf);
  void set_transform(u32 bone, shogle::transform3d<f32>& transf);

  ntf::optional<u32> find_bone(std::string_view name);

public:
  void apply_animation(const model_anim_data& anims, std::string_view name, u32 tick);

protected:
  void tick_bones(const mat4& root);
  u32 retrieve_buffer_bindings(std::vector<shogle::shader_binding>& binds) const;

private:
  shogle::shader_storage_buffer _ssbo;
  ntf::unique_array<bone_t> _bones;
  std::unordered_map<std::string_view, u32> _bone_reg;
  ntf::unique_array<mat4> _bone_locals;
  ntf::unique_array<mat4> _bone_inv_models;
  ntf::unique_array<mat4> _bone_transforms;
  ntf::unique_array<mat4> _transf_cache;
  ntf::unique_array<mat4> _transf_output;
};

struct tickable {
  virtual ~tickable() = default;
  virtual void tick() = 0;
};

class rigged_model3d : public mesh_buffers<rigged_model_data>,
                       public rigged_model_texturer, public model_rigger,
                       public renderable, public tickable {
public:
  using data_t = rigged_model_data;

public:
  rigged_model3d(mesh_buffers<rigged_model_data>&& meshes,
                 rigged_model_texturer&& texturer,
                 model_rigger&& rigger,
                 std::vector<model_material_data::material_meta>&& mats,
                 std::unordered_map<std::string_view, u32>&& mat_reg,
                 shogle::pipeline pip,
                 std::vector<u32> mesh_mats,
                 std::string&& name) noexcept;

public:
  static expect<rigged_model3d> create(data_t&& data);

public:
  void tick() override;
  u32 retrieve_render_data(const scene_render_data& scene, object_render_data& data) override;

public:
  shogle::transform3d<f32>& transform() { return _transf; }
  std::string_view name() const { return _name; }

private:
  std::vector<model_material_data::material_meta> _mats;
  std::unordered_map<std::string_view, u32> _mat_reg;
  shogle::pipeline _pip;
  std::vector<u32> _mesh_mats;
  std::string _name;
  shogle::transform3d<f32> _transf;
};

} // namespace kappa
