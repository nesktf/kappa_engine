#pragma once

#include <chimatools/chimatools.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <unordered_map>

#include "./model.hpp"
#include "./texture.hpp"

namespace kappa::assets {

struct texture_data::texture_internal {
  texture_internal(chima::context&& chima_, chima::image image_) :
      chima(std::move(chima_)), image(image_), image_destroyer(chima, image) {}

  chima::context chima;
  chima::image image;
  chima::scoped_resource<chima::image> image_destroyer;
  buffer_name name;
  buffer_path path;
  image_format format;
  texture_type type;
};

struct texture_loader::loader_internal {
  buffer_name texture_name;
  buffer_path texture_path;
  bits32 chima_flags;
  optional<texture_type> type;
};

struct model_allocator {
  template<typename T>
  T* alloc(size_t n) {
    return std::allocator<T>().allocate(n);
  }

  template<typename T>
  void dealloc(T* ptr, size_t count) {
    std::allocator<T>().deallocate(ptr, count);
  }
};

struct model3d_data::model_internal {
  model_internal();
  ~model_internal();

  buffer_name name;
  buffer_path path;
  model_allocator alloc;
  std::unordered_map<std::string_view, size_t> mesh_registry;
  std::unordered_map<std::string_view, size_t> blend_shape_registry;
  std::unordered_map<std::string_view, size_t> material_registry;
  std::unordered_map<std::string_view, size_t> texture_registry;
  std::unordered_map<std::string_view, size_t> bone_anim_registry;
  std::unordered_map<std::string_view, size_t> bone_registry;
  std::vector<texture_data> texture_cache;

  mesh_data* meshes;
  v3f32* mesh_positions;
  v3f32* mesh_normals;
  v2f32* mesh_uvs[MAX_MESH_UVS];
  v4f32* mesh_colors[MAX_MESH_COLORS];
  v3f32* mesh_tangents;
  v3f32* mesh_bitangents;
  v4i32* mesh_bones;
  v4f32* mesh_bone_weights;
  u32* mesh_indices;
  size_t vertex_count;
  size_t index_count;
  size_t mesh_count;

  blend_shape_data* blend_shapes;
  v3f32* blend_positions;
  v3f32* blend_normals;
  v2f32* blend_uvs[MAX_MESH_UVS];
  v4f32* blend_colors[MAX_MESH_COLORS];
  v3f32* blend_tangents;
  v3f32* blend_bitangents;
  size_t blend_shape_count;

  anim_data* animations;
  bone_keyframes* anim_bone_keys;
  keyframe_data<v3f32>* anim_positions;
  keyframe_data<v3f32>* anim_scales;
  keyframe_data<qf32>* anim_rotations;
  size_t animation_count;
  size_t anim_bone_key_count;

  bone_data* bones;
  m4f32* bone_locals;
  m4f32* bone_inv_models;
  size_t bone_count;

  texture_data* textures;
  u32* material_textures;
  material_data* materials;
  size_t material_textures_count;
  size_t texture_count;
  size_t material_count;
};

struct model3d_loader::loader_internal {
  Assimp::Importer importer;
  buffer_name model_name;
  buffer_path model_path;
  bits32 importer_flags;
};

} // namespace kappa::assets
