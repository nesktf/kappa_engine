#pragma once

#include "../renderer/context.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <unordered_map>

namespace kappa::assets {

struct model_rig_data {
  struct bone_meta {
    std::string name;
    u32 parent;
  };

  struct armature_meta {
    std::string name;
    vec_span bones;
  };

  std::vector<mat4> bone_locals;
  std::vector<mat4> bone_inv_models;
  std::vector<bone_meta> bones;
  std::vector<armature_meta> armatures;
  std::unordered_map<std::string_view, u32> bone_registry;
  std::unordered_map<std::string_view, u32> armature_registry;
};

struct model_anim_data {
  template<typename T>
  struct key_frame {
    double timestamp;
    T value;
  };

  struct animation_meta {
    std::string name;
    double duration;
    double tps;
    vec_span frames;
  };

  struct key_frame_meta {
    std::string bone_name;
    vec_span pos_keys;
    vec_span rot_Keys;
    vec_span sca_keys;
  };

  std::vector<key_frame_meta> keyframes;
  std::vector<animation_meta> animations;
  std::vector<key_frame<vec3>> positions;
  std::vector<key_frame<vec3>> scales;
  std::vector<key_frame<quat>> rotations;
  std::unordered_map<std::string_view, u32> animation_registry;
};

struct model_material_data {
  struct texture_meta {
    std::string name;
    std::string path;
    ntf::unique_array<uint8> bitmap;
    extent3d extent;
    shogle::image_format format;
    // aiTextureType tex_type;
    // aiTextureMapMode mapping;
    // aiTextureOp combine_op;
    // aiTextureFlags flags;
  };

  struct material_meta {
    std::string name;
    vec_span textures;
    // aiShadingMode shading;
    // aiBlendMode blending;
  };

  std::vector<texture_meta> textures;
  std::vector<material_meta> materials;
  std::vector<u32> material_textures;
  std::unordered_map<std::string_view, u32> texture_registry;
  std::unordered_map<std::string_view, u32> material_registry;
};

struct model_mesh_data {
  static constexpr size_t VERTEX_BONE_COUNT = 4u;
  using vertex_bones = std::array<u32, VERTEX_BONE_COUNT>;
  using vertex_weights = std::array<float, VERTEX_BONE_COUNT>;

  static constexpr vertex_bones EMPTY_BONE_INDEX = []() {
    vertex_bones arr;
    for (auto& bone : arr) {
      bone = vec_span::INDEX_TOMB;
    }
    return arr;
  }();

  static constexpr vertex_weights EMPTY_BONE_WEIGHT = []() {
    vertex_weights arr;
    for (auto& weight : arr) {
      weight = 0.f;
    }
    return arr;
  }();

  struct mesh_meta {
    vec_span positions;
    vec_span normals;
    vec_span uvs;
    vec_span tangents;
    vec_span colors;
    vec_span bones;
    vec_span indices;
    std::string name;
    std::string material_name;
    u32 face_count;
    // aiPrimitiveType prim;
  };

  std::vector<vec3> positions;
  std::vector<vec3> normals;
  std::vector<vec2> uvs;
  std::vector<vec3> tangents;
  std::vector<vec3> bitangents;
  std::vector<color4> colors;
  std::vector<vertex_bones> bones;
  std::vector<vertex_weights> weights;
  std::vector<uint32> indices;
  std::vector<mesh_meta> meshes;
  std::unordered_map<std::string_view, u32> mesh_registry;
};

class assimp_parser {
private:
  using unex_t = ntf::unexpected<std::string>;
  using bone_inv_map = std::unordered_map<std::string, mat4>;

public:
  // lel
  static constexpr u32 DEFAULT_ASS_FLAGS =
    aiProcess_Triangulate | aiProcess_GenUVCoords | aiProcess_CalcTangentSpace;

public:
  assimp_parser();

public:
  expect<void> load(const std::string& path, uint32 assimp_flags = DEFAULT_ASS_FLAGS);

public:
  expect<void> parse_rigs(model_rig_data& rigs);
  expect<void> parse_animations(model_anim_data& anims);
  expect<void> parse_materials(model_material_data& materials);
  expect<void> parse_meshes(const model_rig_data& rigs, model_mesh_data& meshes,
                            std::string_view model_name);

private:
  void _parse_bone_nodes(const bone_inv_map& bone_invs, u32 parent, u32& bone_count,
                         const aiNode* node, model_rig_data& rigs);

private:
  Assimp::Importer _imp;
  uint32 _flags;
  std::string _dir;
};

class model_textures {
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
  model_textures(ntf::unique_array<texture_t>&& textures,
                 std::unordered_map<std::string_view, u32>&& tex_reg,
                 ntf::unique_array<vec_span>&& mat_spans,
                 ntf::unique_array<u32>&& mat_texes) noexcept;

public:
  static expect<model_textures> create(const model_material_data& materials);

public:
  shogle::texture2d_view find_texture(std::string_view name);

  ntf::optional<u32> find_texture_idx(std::string_view name) const;
  u32 retrieve_material_textures(u32 mat_idx, std::vector<shogle::texture_binding>& texs) const;

private:
  ntf::unique_array<texture_t> _textures;
  std::unordered_map<std::string_view, u32> _tex_reg;
  ntf::unique_array<vec_span> _mat_spans;
  ntf::unique_array<u32> _mat_texes;
};

} // namespace kappa::assets
