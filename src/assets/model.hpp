#pragma once

#include "./texture.hpp"

namespace kappa::assets {

struct model3d_data {
public: // enum mappings from assimp
  enum mesh_blend_method : u32 {
    MESH_BLEND_METHOD_VERTEX_BLEND = 0,
    MESH_BLEND_METHOD_NORMALIZED,
    MESH_BLEND_METHOD_RELATIVE,
  };

  enum mesh_primitive : u32 {
    MESH_PRIMITIVE_POINT = 0,
    MESH_PRIMITIVE_LINE,
    MESH_PRIMITIVE_TRIANGLE,
    MESH_PRIMITIVE_POLYGON,
  };

  enum texture_map_mode : u32 {
    TEXTURE_MAP_MODE_WRAP = 0,
    TEXTURE_MAP_MODE_CLAMP,
    TEXTURE_MAP_MODE_DECAL,
    TEXTURE_MAP_MODE_MIRROR,
  };

  enum material_shading : u32 {
    MATERIAL_SHADING_NONE = 0,
    MATERIAL_SHADING_FLAT,
    MATERIAL_SHADING_PHONG,
    MATERIAL_SHADING_BLINN,
    MATERIAL_SHADING_TOON,
    MATERIAL_SHADING_PBR_BRDF,
  };

  enum anim_behaviour : u32 {
    ANIM_BEHAVIOUR_USE_TRANSFORM = 0, // Use default transform
    ANIM_BEHAVIOUR_CLAMP,             // Clamp animation to the first/last keyframe
    ANIM_BEHAVIOUR_EXTRAPOLATE,       // Extrapolate using the first/last keyframe
    ANIM_BEHAVIOUR_REPEAT,            // Repeat the animation
  };

  enum keyframe_setting : u32 {
    KEYFRAME_SETTING_STEP = 0,
    KEYFRAME_SETTING_LINEAR,
    KEYFRAME_SETTING_SPH_LINEAR,
    KEYFRAME_SETTING_CUBIC_SPLINE,
  };

  static constexpr size_t MAX_MESH_UVS = 2;
  static constexpr size_t MAX_MESH_COLORS = 2;

public:
  template<typename T>
  struct keyframe_data {
    T value;
    f64 timestamp;
    keyframe_setting setting;
  };

  struct anim_data {
    buffer_name name;
    array_range bone_keys;
    f64 ticks, tps;
  };

  struct bone_keyframes {
    u32 bone_index;
    anim_behaviour pre_state, post_state;
    array_range pos_keys;
    array_range rot_keys;
    array_range scale_keys;
  };

  struct blend_shape_data {
    buffer_name name;
    array_range positions;
    array_range normals;
    array_range tangents;
    array_range uvs[MAX_MESH_UVS];
    array_range colors[MAX_MESH_COLORS];
    f32 weight;
  };

  struct mesh_data {
    buffer_name name;
    buffer_name uv_name[MAX_MESH_UVS];
    array_range positions;
    array_range normals;
    array_range tangents;
    array_range uvs[MAX_MESH_UVS];
    array_range colors[MAX_MESH_COLORS];
    array_range bones;
    array_range indices;
    u32 face_count;
    array_range blend_shapes;
    mesh_blend_method blend_method;
    v3f32 bbox_min, bbox_max;
    u32 material_index;
    mesh_primitive primitive;
  };

  struct texture_data {
    buffer_name name;
    buffer_path path;
    void* data;
    extent2d extent;
    image_format format;
    texture_map_mode mapping_mode;
  };

  struct material_data {
    buffer_name name;
    array_range texture_indices;
    material_shading shading_mode;
  };

  struct bone_data {
    buffer_name name;
    i32 parent;
  };

  struct model_internal;

public:
  model3d_data(model_internal& data) noexcept;

  void destroy() noexcept;

public:
  buffer_name& name() const;
  buffer_path& path() const;

  mesh_data& mesh_at(u32 idx) const;
  span<mesh_data> meshes() const;
  span<v3f32> mesh_positions() const;
  span<v3f32> mesh_positions(array_range range) const;
  span<v3f32> mesh_normals() const;
  span<v3f32> mesh_normals(array_range range) const;
  span<v2f32> mesh_uvs(u32 idx) const;
  span<v2f32> mesh_uvs(u32 idx, array_range range) const;
  span<v4f32> mesh_colors() const;
  span<v4f32> mesh_colors(array_range range) const;
  span<v3f32> mesh_tangents() const;
  span<v3f32> mesh_tangents(array_range range) const;
  span<v3f32> mesh_bitangents() const;
  span<v3f32> mesh_bitangents(array_range range) const;
  span<v4i32> mesh_bones() const;
  span<v4i32> mesh_bones(array_range range) const;
  span<v4f32> mesh_bone_weights() const;
  span<v4f32> mesh_bone_weights(array_range range) const;
  span<u32> mesh_indices() const;
  span<u32> mesh_indices(array_range range) const;
  size_t mesh_count() const;

  optional<u32> find_mesh_idx(std::string_view mesh_name) const;

  mesh_data* find_mesh(std::string_view mesh_name) const {
    return find_mesh_idx(mesh_name)
      .transform([this](u32 idx) -> mesh_data* { return &mesh_at(idx); })
      .value_or(nullptr);
  }

  blend_shape_data& blend_shape_at(u32 idx) const;
  span<blend_shape_data> blend_shapes() const;
  span<blend_shape_data> blend_shapes(array_range range) const;
  span<v3f32> blend_positions() const;
  span<v3f32> blend_positions(array_range range) const;
  span<v3f32> blend_normals() const;
  span<v3f32> blend_normals(array_range range) const;
  span<v2f32> blend_uvs(u32 idx) const;
  span<v2f32> blend_uvs(u32 idx, array_range range) const;
  span<v4f32> blend_colors(u32 idx) const;
  span<v4f32> blend_colors(u32 idx, array_range range) const;
  span<v3f32> blend_tangents() const;
  span<v3f32> blend_tangents(array_range range) const;
  span<v3f32> blend_bitangents() const;
  span<v3f32> blend_bitangents(array_range range) const;
  u32 blend_shape_count() const;

  bool has_blend_shapes() const { return (blend_shape_count() > 0); }

  anim_data& animation_at(u32 idx) const;
  span<anim_data> animations() const;
  span<anim_keyframe> keyframes() const;
  span<anim_keyframe> keyframes(array_range range) const;
  span<keyframe_data<v3f32>> anim_positions() const;
  span<keyframe_data<v3f32>> anim_positions(array_range range) const;
  span<keyframe_data<v3f32>> anim_scales() const;
  span<keyframe_data<v3f32>> anim_scales(array_range range) const;
  span<keyframe_data<qf32>> anim_rotations() const;
  span<keyframe_data<qf32>> anim_rotations(array_range range) const;
  u32 animation_count() const;

  bool has_animations() const { return (animation_count() > 0); }

  optional<u32> find_animation_idx(std::string_view animation_name) const;

  anim_data* find_animation(std::string_view animation_name) const {
    return find_animation_idx(animation_name)
      .transform([this](u32 idx) -> anim_data* { return &animation_at(idx); })
      .value_or(nullptr);
  }

  armature_data& armature_at(u32 idx) const;
  span<armature_data> armatures() const;
  bone_data& bone_at(u32 idx) const;
  span<bone_data> bones() const;
  span<bone_data> bones(array_range range) const;
  span<m4f32> bone_locals() const;
  span<m4f32> bone_locals(array_range range) const;
  span<m4f32> bone_inverse_models() const;
  span<m4f32> bone_inverse_models(array_range range) const;
  u32 bone_count() const;
  u32 armature_count() const;

  bool has_armatures() const { return (armature_count() > 0); }

  bool has_bones() const { return (bone_count() > 0); }

  optional<u32> find_armature_idx(std::string_view armature_name) const;

  armature_data* find_armature(std::string_view armature_name) const {
    return find_armature_idx(armature_name)
      .transform([this](u32 idx) -> armature_data* { return &armature_at(idx); })
      .value_or(nullptr);
  }

  optional<u32> find_bone_idx(std::string_view bone_name) const;

  bone_data* find_bone(std::string_view bone_name) const {
    return find_bone_idx(bone_name)
      .transform([this](u32 idx) -> bone_data* { return &bone_at(idx); })
      .value_or(nullptr);
  }

  material_data& material_at(u32 idx) const;
  span<material_data> materials() const;
  span<texture_data> material_textures(u32 idx) const;
  texture_data& texture_at(u32 idx) const;
  span<texture_data> textures() const;

  u32 texture_count() const;
  u32 material_count() const;

  bool has_materials() const { return (material_count() > 0); }

  bool has_textures() const { return (texture_count() > 0); }

  optional<u32> find_material_idx(std::string_view material_name) const;

  material_data* find_material(std::string_view material_name) const {
    return find_material_idx(material_name)
      .transform([this](u32 idx) -> material_data* { return &material_at(idx); })
      .value_or(nullptr);
  }

  optional<u32> find_texture_idx(std::string_view texture_name) const;

  texture_data* find_texture(std::string_view texture_name) const {
    return find_texture_idx(texture_name)
      .transform([this](u32 idx) -> texture_data* { return &texture_at(idx); })
      .value_or(nullptr);
  }

private:
  model_internal* _data;
};

class model3d_loader {
private:
  struct loader_internal;

public:
  enum load_flags {
    FLAGS_NONE = 0x00000000,
    FLAG_TRIANGULATE = 0x00000001,
    FLAG_EMBED_TEXTURES = 0x00000002,
    FLAG_CALC_TANGENTS = 0x00000004,
    FLAG_GEN_UVS = 0x00000008,
  };

  static constexpr bits32 FLAGS_DEFAULT = FLAG_TRIANGULATE | FLAG_CALC_TANGENTS | FLAG_GEN_UVS;

public:
  model3d_loader(std::string_view model_path, std::string_view model_name,
                 bits32 load_flags = FLAGS_DEFAULT);

public:
  // Should be only called ONCE, preferably in a threadpool
  // The internal data is destroyed at the end of the function
  bs_expect<model3d_data, 256> load();

public:
  bs_expect<model3d_data, 256> operator()() { return load(); }

private:
  loader_internal* _impl;
};

} // namespace kappa::assets
