#pragma once

#include "model_data.hpp"
#include "renderer.hpp"

class model_mesh_provider {
public:
  enum attr_idx {
    ATTRIB_POS = 0,
    ATTRIB_NORM,
    ATTRIB_UVS,
    ATTRIB_TANG,
    ATTRIB_BITANG,
    ATTRIB_BONES,
    ATTRIB_WEIGHTS,

    ATTRIB_COUNT,
  };

  using mesh_vert_buffs = std::array<ntfr::buffer_t, ATTRIB_COUNT>;
  using mesh_vert_binds = std::array<ntfr::vertex_binding, ATTRIB_COUNT>;

private:
  struct mesh_offset {
    u32 index_offset;
    u32 index_count;
    u32 vertex_offset;
  };

public:
  model_mesh_provider(mesh_vert_buffs buffs, ntfr::index_buffer&& index_buff,
                      std::vector<mesh_offset>&& mesh_offsets) noexcept;

  ~model_mesh_provider() noexcept;

  model_mesh_provider(const model_mesh_provider&) = delete;
  model_mesh_provider(model_mesh_provider&& other) noexcept;

public:
  model_mesh_provider& operator=(const model_mesh_provider&) = delete;
  model_mesh_provider& operator=(model_mesh_provider&&) noexcept;

public:
  static expect<model_mesh_provider> create(const model_mesh_data& meshes);

public:
  mesh_render_data& retrieve_mesh_data(u32 idx, std::vector<mesh_render_data>& data);
  u32 mesh_count() const { return _mesh_offsets.size(); }

private:
  void _free_buffs() noexcept;

private:
  mesh_vert_buffs _buffs;
  mesh_vert_binds _binds;
  std::vector<mesh_offset> _mesh_offsets;
  ntfr::index_buffer _index_buff;
  u32 _active_layouts;
};

class model_rigger {
public:
  struct bone_transform {
    vec3 pos;
    vec3 scale;
    quat rot;
  };

public:
  model_rigger(vec_span bones,
               ntfr::shader_storage_buffer&& ssbo,
               std::vector<mat4>&& transform_output, std::vector<mat4>&& bone_transforms,
               std::vector<mat4>&& local_cache, std::vector<mat4>&& model_cache) noexcept;

public:
  static expect<model_rigger> create(const model_rig_data& rigs, std::string_view armature);

public:
  void set_root_transform(const mat4& transf);
  void set_transform(const model_rig_data& rigs,
                     std::string_view bone, const bone_transform& transf);
  void set_transform(const model_rig_data& rigs,
                     std::string_view bone, const mat4& transf);
  void tick(const model_rig_data& rigs);

public:
  u32 retrieve_buffer_bindings(std::vector<ntfr::shader_binding>& binds);

private:
  vec_span _bones;
  ntfr::shader_storage_buffer _ssbo;
  std::vector<mat4> _transform_output;
  std::vector<mat4> _bone_transforms;
  std::vector<mat4> _local_cache;
  std::vector<mat4> _model_cache;
};

struct tickable {
  virtual ~tickable() = default;
  virtual void tick() = 0;
};

class rigged_model3d : public renderable, public tickable {
public:
  struct data_t {
    std::string name;
    std::string_view armature;
    model_rig_data rigs;
    model_anim_data anims;
    model_material_data mats;
    model_mesh_data meshes;
  };

  struct texture_t {
    std::string name;
    ntfr::texture2d tex;
  };

  struct material_meta {
    std::vector<texture_t> textures;
    std::unordered_map<std::string_view, u32> texture_registry;
    std::vector<model_material_data::material_meta> materials;
    std::unordered_map<std::string_view, u32> material_registry;
    std::vector<u32> material_textures;
  };

  struct armature_meta {
    model_anim_data anims;
    model_rig_data rigs;
    model_rigger rigger;
  };

  struct render_meta {
    ntfr::pipeline pip;
    model_mesh_provider meshes;
    std::vector<u32> mesh_texs;
  };

public:
  rigged_model3d(std::string&& name,
                 material_meta&& materials,
                 armature_meta&& armature,
                 render_meta&& rendering,
                 ntf::transform3d<f32> transf);

public:
  static expect<rigged_model3d> create(data_t&& data);

public:
  void tick() override;
  u32 retrieve_render_data(const scene_render_data& scene, object_render_data& data) override;

public:
  ntf::transform3d<f32>& transform() { return _transf; }
  std::string_view name() const { return _name; }

  void set_bone_transform(std::string_view name, const model_rigger::bone_transform& transf);
  void set_bone_transform(std::string_view name, const mat4& transf);
  void set_bone_transform(std::string_view name, ntf::transform3d<f32>& transf);

private:
  std::string _name;
  material_meta _materials;
  armature_meta _armature;
  render_meta _rendering;
  ntf::transform3d<f32> _transf;
};
