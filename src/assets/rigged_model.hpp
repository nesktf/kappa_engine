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
  static constexpr bool USE_INDICES = true;

public:
  model_mesh_data meshes;
  model_material_data materials;
  model_rig_data rigs;
  model_anim_data anims;

public:
  size_t vertex_count() const { return meshes.positions.size(); }
  size_t index_count() const { return meshes.indices.size(); }
  std::pair<const void*, size_t> get_data_ptr(size_t idx) const {
    enum {
      IDX_POS = 0,
      IDX_NORM,
      IDX_UVS,
      IDX_TANG,
      IDX_BITANG,
      IDX_BONES,
      IDX_WEIGHTS,
      IDX_INDICES,
    };
    NTF_ASSERT(idx <= IDX_INDICES);

    return std::make_pair(nullptr, 0u);
  }
};
static_assert(meta::mesh_data_type<rigged_model_data>);

class rigged_model_mesher {
protected:
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

public:
  using vert_buffs = std::array<shogle::buffer_t, ATTRIB_COUNT>;
  using vert_binds = std::array<shogle::vertex_binding, ATTRIB_COUNT>;

private:
  struct mesh_offset {
    u32 index_offset;
    u32 index_count;
    u32 vertex_offset;
  };

public:
  rigged_model_mesher(vert_buffs buffs, shogle::index_buffer&& index_buff,
               ntf::unique_array<mesh_offset>&& mesh_offsets) noexcept;

  ~rigged_model_mesher() noexcept;

  rigged_model_mesher(const rigged_model_mesher&) = delete;
  rigged_model_mesher(rigged_model_mesher&& other) noexcept;

public:
  rigged_model_mesher& operator=(const rigged_model_mesher&) = delete;
  rigged_model_mesher& operator=(rigged_model_mesher&&) noexcept;

protected:
  static expect<rigged_model_mesher> create(const model_mesh_data& meshes);

protected:
  mesh_render_data& retrieve_mesh_data(u32 idx, std::vector<mesh_render_data>& data);
  u32 mesh_count() const { return _mesh_offsets.size(); }

private:
  void _free_buffs() noexcept;

private:
  vert_buffs _buffs;
  vert_binds _binds;
  ntf::unique_array<mesh_offset> _mesh_offsets;
  shogle::index_buffer _index_buff;
  u32 _active_layouts;
};

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

class rigged_model3d : public rigged_model_mesher, public rigged_model_texturer, public model_rigger,
                       public renderable, public tickable {
public:
  struct data_t {
    std::string name;
    std::string_view armature;
    model_rig_data rigs;
    model_anim_data anims;
    model_material_data mats;
    model_mesh_data meshes;
  };

public:
  rigged_model3d(rigged_model_mesher&& meshes,
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
