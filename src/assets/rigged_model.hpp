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
      {.size = sizeof(model_mesh_data::vertex_bones), .name = "rigged_bones"},
      {.size = sizeof(model_mesh_data::vertex_weights), .name = "rigged_weights"},
  }};

  static constexpr std::string_view VERT_SHADER_LAYOUT = R"glsl(
layout (location = 0) in vec3 att_positions;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_uvs;
layout (location = 3) in vec3 att_tangents;
layout (location = 4) in vec3 att_bitangents;
layout (location = 5) in ivec4 att_bones;
layout (location = 6) in vec4 att_weights;
)glsl";

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

  std::pair<const void*, u32> vertex_data(u32 attr_idx, u32 mesh_idx) const;
};

static_assert(meta::mesh_data_type<rigged_model_data>);

class model3d_rig_asset {
private:
  struct bone_t {
    std::string name;
    u32 parent;
  };

public:
  model3d_rig_asset(shogle::shader_storage_buffer&& ssbo, ntf::unique_array<bone_t>&& bones,
                    std::unordered_map<std::string_view, u32>&& bone_reg,
                    ntf::unique_array<mat4>&& bone_locals,
                    ntf::unique_array<mat4>&& bone_inv_models,
                    ntf::unique_array<mat4>&& bone_transforms,
                    ntf::unique_array<mat4>&& transf_cache,
                    ntf::unique_array<mat4>&& transf_output) noexcept;

protected:
  static expect<model3d_rig_asset> create(const model_rig_data& rigs, std::string_view armature);

public:
  ntf::optional<u32> find_bone(std::string_view name);

public:
  void apply_animation(const model_anim_data& anims, std::string_view name, u32 tick);

protected:
  u32 retrieve_buffer_bindings(std::vector<shogle::shader_binding>& binds) const;

private:
  ntf::unique_array<bone_t> _bones;
  std::unordered_map<std::string_view, u32> _bone_reg;
  ntf::unique_array<mat4> _bone_locals;
  ntf::unique_array<mat4> _bone_inv_models;
};

class model3d_rigger {
public:
  struct bone_transform {
    vec3 pos;
    vec3 scale;
    quat rot;
  };

public:
  model3d_rigger(const model3d_rig_asset& asset, shogle::shader_storage_buffer&& bone_ssbo,
                 ntf::unique_array<mat4> bone_transforms, ntf::unique_array<mat4> transf_cache,
                 ntf::unique_array<mat4> transf_output);

public:
  static expect<model3d_rigger> create(const model3d_rig_asset& asset);

public:
  bool set_transform(std::string_view bone, const bone_transform& transf);
  bool set_transform(std::string_view bone, const mat4& transf);
  bool set_transform(std::string_view bone, shogle::transform3d<f32>& transf);

  void set_transform(u32 bone, const bone_transform& transf);
  void set_transform(u32 bone, const mat4& transf);
  void set_transform(u32 bone, shogle::transform3d<f32>& transf);

  void tick_bones(const mat4& root);

public:
  const model3d_rig_asset& asset() const { return *_asset; }

private:
  const model3d_rig_asset* _asset;
  shogle::shader_storage_buffer _bone_ssbo;
  ntf::unique_array<mat4> _bone_transforms;
  ntf::unique_array<mat4> _transf_cache;
  ntf::unique_array<mat4> _transf_output;
};

struct tickable {
  virtual ~tickable() = default;
  virtual void tick() = 0;
};

class rigged_model3d_asset :
    public model3d_mesh_buffers<rigged_model_data>,
    public model3d_mesh_textures,
    public model3d_rig_asset {
public:
  using data_t = rigged_model_data;

public:
  rigged_model3d_asset(model3d_mesh_buffers<rigged_model_data>&& meshes,
                       model3d_mesh_textures&& texturer, model3d_rig_asset&& rigger,
                       std::vector<model_material_data::material_meta>&& mats,
                       std::unordered_map<std::string_view, u32>&& mat_reg, shogle::pipeline pip,
                       std::vector<u32> mesh_mats, std::string&& name) noexcept;

public:
  static expect<rigged_model3d_asset> create(data_t&& data);

public:
  std::string_view name() const { return _name; }

private:
  std::vector<model_material_data::material_meta> _mats;
  std::unordered_map<std::string_view, u32> _mat_reg;
  shogle::pipeline _pip;
  std::vector<u32> _mesh_mats;
  std::string _name;
};

class rigged_model3d : public renderable, public tickable {
public:
  rigged_model3d(const rigged_model3d_asset& asset, model3d_rigger&& rigger);

public:
  void tick() override;
  u32 retrieve_render_data(const scene_render_data& scene, object_render_data& data) override;

public:
  const rigged_model3d_asset& asset() const { return *_asset; }

  shogle::transform3d<f32>& transform() { return _transf; }

private:
  const rigged_model3d_asset* _asset;
  model3d_rigger _rigger;
  shogle::transform3d<f32> _transf;
};

} // namespace kappa
