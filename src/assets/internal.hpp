#pragma once

#include "./ass_common.hpp"

#include "./model.hpp"
#include "./texture.hpp"

#include <chimatools/chimatools.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <unordered_map>

namespace kappa::assets {

inline ImageFormat parse_chima_format(chima_image_depth depth, u32 channels) {
  ka_assert(channels == 3 || channels == 4);
  switch (depth) {
    case CHIMA_DEPTH_8U: {
      return channels == 4 ? ImageFormat::rgba8u : ImageFormat::rgb8u;
    } break;
    case CHIMA_DEPTH_16U: {
      return channels == 4 ? ImageFormat::rgba16u : ImageFormat::rgb16u;
    } break;
    case CHIMA_DEPTH_32F: {
      return channels == 4 ? ImageFormat::rgba32f : ImageFormat::rgb32f;
    } break;
    default:
      KA_UNREACHABLE();
  }
};

inline TextureType parse_tex_type_from_name(std::string_view name) {
  KA_UNUSED(name); // TODO
  return TextureType::albedo;
};

struct ImageData::ImageInternal {
  ImageInternal(chima::context&& chima_, chima::image image_) :
      chima(std::move(chima_)), image(image_), image_destroyer(chima, image) {}

  chima::context chima;
  chima::image image;
  chima::scoped_resource<chima::image> image_destroyer;
  BufferName name;
  BufferPath path;
  ImageFormat format;
};

struct ImageLoader::LoaderInternal {
  BufferName texture_name;
  BufferPath texture_path;
  bits32 chima_flags;
  Optional<TextureType> type;
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

struct Model3DData::ModelInternal {
  ModelInternal(const BufferName& name_, const BufferPath& path_);
  ~ModelInternal();

  BufferName name;
  BufferPath path;
  model_allocator alloc;
  chima::context chima;
  std::unordered_map<std::string_view, size_t> mesh_registry;
  // std::unordered_map<std::string_view, size_t> blend_shape_registry;
  std::unordered_map<std::string_view, size_t> material_registry;
  std::unordered_map<std::string_view, size_t> texture_registry;
  // std::unordered_map<std::string_view, size_t> bone_anim_registry;
  std::unordered_map<std::string_view, size_t> bone_registry;

  MeshData* meshes;
  size_t mesh_count;
  ran::Vec3f32* mesh_positions;
  size_t mesh_position_count;
  ran::Vec3f32* mesh_normals;
  size_t mesh_normal_count;
  ran::Vec2f32* mesh_uvs[MAX_MESH_UVS];
  size_t mesh_uv_count[MAX_MESH_UVS];
  ran::Vec4f32* mesh_colors[MAX_MESH_COLORS];
  size_t mesh_color_count[MAX_MESH_COLORS];
  ran::Vec3f32* mesh_tangents;
  ran::Vec3f32* mesh_bitangents;
  size_t mesh_tangent_count;
  ran::Vec4s32* mesh_bone_indices;
  ran::Vec4f32* mesh_bone_weights;
  size_t mesh_bone_count;
  u32* mesh_indices;
  size_t mesh_index_count;

  BlendShapeData* blend_shapes;
  size_t blend_shape_count;
  ran::Vec3f32* blend_positions;
  size_t blend_position_count;
  ran::Vec3f32* blend_normals;
  size_t blend_normal_count;
  ran::Vec2f32* blend_uvs[MAX_MESH_UVS];
  size_t blend_uv_count[MAX_MESH_UVS];
  ran::Vec4f32* blend_colors[MAX_MESH_COLORS];
  size_t blend_color_count[MAX_MESH_COLORS];
  ran::Vec3f32* blend_tangents;
  ran::Vec3f32* blend_bitangents;
  size_t blend_tangent_count;

#if 0
  anim_data* animations;
  size_t animation_count;
  bone_keyframes* anim_bone_keyframes;
  size_t anim_bone_keyframe_count;
  keyframe_data<ran::Vec3f32>* anim_bone_positions;
  size_t anim_bone_position_count;
  keyframe_data<ran::Vec3f32>* anim_bone_scales;
  size_t anim_bone_scale_count;
  keyframe_data<qf32>* anim_bone_rotations;
  size_t anim_bone_rotation_count;
#endif

  BoneData* bones;
  ran::Mat4f32* bone_locals;
  ran::Mat4f32* bone_inv_models;
  size_t bone_count;

  TextureData* textures;
  chima::image* texture_images;
  size_t texture_count;
  MaterialData* materials;
  size_t material_count;
  u32* material_textures;
  size_t material_textures_count;
};

struct Model3DLoader::LoaderInternal {
  Assimp::Importer importer;
  BufferName model_name;
  BufferPath model_path;
  BufferPath texture_dir;
  bits32 importer_flags;
};

} // namespace kappa::assets
