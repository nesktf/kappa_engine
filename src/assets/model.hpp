#pragma once

#include "./texture.hpp"

namespace kappa::assets {

struct model3d_data {
public:
  enum mesh_morph_method {
    MESH_MORPH_METHOD_VERTEX_BLEND = 0x0001,
    MESH_MORPH_METHOD_NORMALIZED = 0x0002,
    MESH_MORPH_METHOD_RELATIVE = 0x0004,
  };

  enum mesh_primitive {
    MESH_PRIMITIVE_POINT = 0x0001,
    MESH_PRIMITIVE_LINE = 0x0002,
    MESH_PRIMITIVE_TRIANGLE = 0x0004,
    MESH_PRIMITIVE_POLYGON = 0x0008,
  };

public:
  template<typename T>
  struct keyframe_data {
    T value;
    f64 timestamp;
  };

  struct anim_data {
    buffer_name name;
    array_span frames;
    f64 duration;
    f64 tps;
  };

  struct anim_keyframe {
    buffer_name bone_name;
    array_span pos_keys;
    array_span rot_keys;
    array_span scale_keys;
  };

  struct blend_shape_data {
    buffer_name name;
    array_span positions;
    array_span normals;
    array_span uvs0;
    array_span uvs1;
    array_span colors;
    array_span tangents;
    u32 vertex_count;
    f32 weight;
  };

  struct mesh_data {
    buffer_name name;
    buffer_name mat_name;
    buffer_name uv0_name;
    buffer_name uv1_name;
    v3f32 bbox_max;
    v3f32 bbox_min;
    array_span positions;
    array_span normals;
    array_span uvs0;
    array_span uvs1;
    array_span colors;
    array_span tangents;
    array_span bones;
    array_span indices;
    u32 vertex_count;
    u32 face_count;
    mesh_morph_method morph_method;
    mesh_primitive primitive;
  };

  struct texture_data {
    buffer_name name;
    buffer_path path;
    void* data;
    extent2d extent;
    image_format format;
  };

  struct material_data {
    buffer_name name;
    array_span textures;
    bits32 props;
  };

  struct bone_data {
    buffer_name name;
    i32 parent;
  };

  struct armature_data {
    buffer_name name;
    array_span bones;
  };

  struct model_internal;

public:
  model3d_data() noexcept;

  void destroy() noexcept;

public:
  optional<u32> find_animation(std::string_view animation_name) noexcept;
  optional<u32> find_mesh(std::string_view mesh_name) noexcept;
  optional<u32> find_material(std::string_view material_name) noexcept;
  optional<u32> find_bone(std::string_view bone_name) noexcept;
  optional<u32> find_armature(std::string_view armature_name) noexcept;

  bool has_materials() noexcept;
  bool has_textures() noexcept;
  bool has_armatures() noexcept;
  bool has_animations() noexcept;

public:
  buffer_name name;
  buffer_path path;
  model_internal* _impl;

  mesh_data* meshes;
  v3f32* mesh_positions;
  v3f32* mesh_normals;
  v2f32* mesh_uvs0;
  v2f32* mesh_uvs1;
  v4f32* mesh_colors;
  v3f32* mesh_tangents;
  v3f32* mesh_bitangents;
  v4i32* mesh_bones;
  v4f32* mesh_bone_weights;
  u32* mesh_indices;
  u32 index_count;
  u32 mesh_count;
  u32 vertex_count;
  u32 face_count;

  blend_shape_data* blend_shapes;
  v3f32* blend_positions;
  v3f32* blend_normals;
  v3f32* blend_uvs0;
  v3f32* blend_uvs1;
  v3f32* blend_colors;
  v3f32* blend_tangents;
  v3f32* blend_bitangents;

  anim_data* animations;
  anim_keyframe* anim_keyframes;
  size_t animation_count;
  keyframe_data<v3f32>* anim_positions;
  keyframe_data<v3f32>* anim_scales;
  keyframe_data<qf32>* anim_rotations;
  size_t anim_keyframe_count;

  texture_data* textures;
  u32 texture_count;
  u32* material_textures;
  u32 material_textures_count;
  material_data* materials;
  u32 material_count;

  armature_data* armatures;
  u32 armature_count;
  bone_data* bones;
  u32 bone_count;
  m4f32* bone_locals;
  m4f32* bone_inv_models;
};

class model3d_loader {
private:
  struct loader_internal;

public:
  enum load_flags {
    FLAGS_NONE = 0x0000,
  };

public:
  model3d_loader(std::string_view model_path, std::string_view model_name,
                 bits32 load_flags = FLAGS_NONE);

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
