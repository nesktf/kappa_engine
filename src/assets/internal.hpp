#pragma once

#include <chimatools/chimatools.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <unordered_map>

#include "./model.hpp"
#include "./texture.hpp"

namespace kappa::assets {

inline image_format parse_chima_format(chima_image_depth depth, u32 channels) {
  assert(channels == 3 || channels == 4);
  switch (depth) {
    case CHIMA_DEPTH_8U: {
      return channels == 4 ? image_format::rgba8u : image_format::rgb8u;
    } break;
    case CHIMA_DEPTH_16U: {
      return channels == 4 ? image_format::rgba16u : image_format::rgb16u;
    } break;
    case CHIMA_DEPTH_32F: {
      return channels == 4 ? image_format::rgba32f : image_format::rgb32f;
    } break;
    default:
      SHOGLE_UNREACHABLE();
  }
};

inline texture_type parse_tex_type_from_name(std::string_view name) {
  SHOGLE_UNUSED(name); // TODO
  return texture_type::albedo;
};

struct image_data::image_internal {
  image_internal(chima::context&& chima_, chima::image image_) :
      chima(std::move(chima_)), image(image_), image_destroyer(chima, image) {}

  chima::context chima;
  chima::image image;
  chima::scoped_resource<chima::image> image_destroyer;
  buffer_name name;
  buffer_path path;
  image_format format;
};

struct image_loader::loader_internal {
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
  model_internal(const buffer_name& name_, const buffer_path& path_);
  ~model_internal();

  buffer_name name;
  buffer_path path;
  model_allocator alloc;
  chima::context chima;
  std::unordered_map<std::string_view, size_t> mesh_registry;
  // std::unordered_map<std::string_view, size_t> blend_shape_registry;
  std::unordered_map<std::string_view, size_t> material_registry;
  std::unordered_map<std::string_view, size_t> texture_registry;
  // std::unordered_map<std::string_view, size_t> bone_anim_registry;
  std::unordered_map<std::string_view, size_t> bone_registry;

  mesh_data* meshes;
  size_t mesh_count;
  v3f32* mesh_positions;
  size_t mesh_position_count;
  v3f32* mesh_normals;
  size_t mesh_normal_count;
  v2f32* mesh_uvs[MAX_MESH_UVS];
  size_t mesh_uv_count[MAX_MESH_UVS];
  v4f32* mesh_colors[MAX_MESH_COLORS];
  size_t mesh_color_count[MAX_MESH_COLORS];
  v3f32* mesh_tangents;
  v3f32* mesh_bitangents;
  size_t mesh_tangent_count;
  v4i32* mesh_bone_indices;
  v4f32* mesh_bone_weights;
  size_t mesh_bone_count;
  u32* mesh_indices;
  size_t mesh_index_count;

  blend_shape_data* blend_shapes;
  size_t blend_shape_count;
  v3f32* blend_positions;
  size_t blend_position_count;
  v3f32* blend_normals;
  size_t blend_normal_count;
  v2f32* blend_uvs[MAX_MESH_UVS];
  size_t blend_uv_count[MAX_MESH_UVS];
  v4f32* blend_colors[MAX_MESH_COLORS];
  size_t blend_color_count[MAX_MESH_COLORS];
  v3f32* blend_tangents;
  v3f32* blend_bitangents;
  size_t blend_tangent_count;

#if 0
  anim_data* animations;
  size_t animation_count;
  bone_keyframes* anim_bone_keyframes;
  size_t anim_bone_keyframe_count;
  keyframe_data<v3f32>* anim_bone_positions;
  size_t anim_bone_position_count;
  keyframe_data<v3f32>* anim_bone_scales;
  size_t anim_bone_scale_count;
  keyframe_data<qf32>* anim_bone_rotations;
  size_t anim_bone_rotation_count;
#endif

  bone_data* bones;
  m4f32* bone_locals;
  m4f32* bone_inv_models;
  size_t bone_count;

  texture_data* textures;
  chima::image* texture_images;
  size_t texture_count;
  material_data* materials;
  size_t material_count;
  u32* material_textures;
  size_t material_textures_count;
};

struct model3d_loader::loader_internal {
  Assimp::Importer importer;
  buffer_name model_name;
  buffer_path model_path;
  buffer_path texture_dir;
  bits32 importer_flags;
};

} // namespace kappa::assets
