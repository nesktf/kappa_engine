#pragma once

#include "./texture.hpp"

namespace kappa::assets {

struct model3d_data {
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

  struct mesh_data {
    buffer_name name;
    bits32 props;
    u32 vertex_count;
    u32 element_count;
    u32 material_idx;
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
  v3f32* mesh_pos;
  v3f32* mesh_norm;
  v2f32* mesh_uv0;
  v2f32* mesh_uv1;
  v4f32* mesh_col;
  v3f32* mesh_tan;
  v3f32* mesh_bitan;
  v4i32* mesh_bones;
  v4f32* mesh_weights;
  u32* mesh_indices;
  size_t index_count;
  size_t mesh_count;
  size_t vertex_count;
  size_t face_count;

  anim_data* animations;
  anim_keyframe* anim_keyframes;
  size_t animation_count;
  keyframe_data<v3f32>* anim_positions;
  keyframe_data<v3f32>* anim_scales;
  keyframe_data<qf32>* anim_rotations;
  size_t anim_keyframe_count;

  texture_data* textures;
  size_t texture_count;
  u32* material_textures;
  size_t material_textures_count;
  material_data* materials;
  size_t material_count;

  armature_data* armatures;
  size_t armature_count;
  bone_data* bones;
  size_t bone_count;
  m4f32* bone_locals;
  m4f32* bone_inv_models;
};

class model3d_loader {
private:
  struct loader_internal;

public:
  model3d_loader(std::string_view model_path, std::string_view model_name, bits32 load_flags);

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
