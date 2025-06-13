#pragma once

#include <shogle/math.hpp>
#include <ntfstl/unique_array.hpp>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <unordered_map>

using namespace ntf::numdefs;

struct vec_span {
  size_t idx, count;
};

class model_data {
public:
  static constexpr size_t INDEX_TOMB = std::numeric_limits<size_t>::max();
  static constexpr size_t BONE_WEIGHT_COUNT = 4u;
  
  struct bone_meta {
    size_t parent;
    std::string name;
  };

  struct armature_meta {
    vec_span bones;
    std::string name;
  };

  struct rig_data {
    std::vector<bone_meta> bones;
    std::vector<ntf::mat4> bone_locals;
    std::vector<ntf::mat4> bone_inv_models;
    std::vector<armature_meta> armatures;
    std::unordered_map<std::string, size_t> bone_registry;
    std::unordered_map<std::string, size_t> armature_registry;
  };

  struct animation_meta {
    double duration;
    double tps;
    vec_span frames;
  };

  struct key_frame_meta {
    size_t bone;
    vec_span pos_keys;
    vec_span rot_Keys;
    vec_span sca_keys;
  };

  template<typename T>
  struct key_frame {
    double timestamp;
    T element;
  };

  struct animation_data {
    std::vector<key_frame_meta> keyframes;
    std::vector<animation_meta> animations;
    std::vector<key_frame<ntf::vec3>> positions;
    std::vector<key_frame<ntf::vec3>> scales;
    std::vector<key_frame<ntf::quat>> rotations;
    std::unordered_map<std::string, size_t> animation_registry;
  };

  using vertex_bones = std::array<size_t, BONE_WEIGHT_COUNT>;
  using vertex_weights = std::array<float, BONE_WEIGHT_COUNT>;

  struct mesh_meta {
    vec_span positions;
    vec_span normals;
    vec_span uvs;
    vec_span tangents;
    vec_span colors;
    vec_span bones;
    vec_span indices;
    std::string name;
    size_t material_idx;
    aiPrimitiveType prim;
  };

  struct mesh_data {
    std::vector<ntf::vec3> positions;
    std::vector<ntf::vec3> normals;
    std::vector<ntf::vec2> uvs;
    std::vector<ntf::vec3> tangents;
    std::vector<ntf::vec3> bitangents;
    std::vector<ntf::color4> colors;
    std::vector<vertex_bones> bones;
    std::vector<vertex_weights> weights;
    std::vector<uint32> indices;
    std::vector<mesh_meta> meshes;
    std::unordered_map<std::string, size_t> mesh_registry;
  };

  struct texture_meta {
    ntf::unique_array<uint8> bitmap;
    std::string name, path;
    aiTextureType tex_type;
    aiTextureMapMode mapping;
    aiTextureOp combine_op;
    aiTextureFlags flags;
  };

  struct material_meta {
    aiShadingMode shading;
    aiBlendMode blending;
    std::string name;
    vec_span textures;
  };

  struct material_data {
    std::vector<texture_meta> textures;
    std::vector<material_meta> materials;
    std::vector<size_t> material_textures;
    std::unordered_map<std::string, size_t> texture_registry;
    std::unordered_map<std::string, size_t> material_registry;
  };

public:
  model_data(mesh_data&& meshes, material_data&& materials,
             rig_data&& rigs, animation_data&& anims) noexcept;

public:
  static ntf::expected<model_data, std::string> load_model(const std::string& path);

private:
  mesh_data _meshes;
  material_data _materials;
  rig_data _rigs;
  animation_data _anims;
};

class assimp_parser {
private:
  using unex_t = ntf::unexpected<std::string_view>;
  using bone_inv_map = std::unordered_map<std::string, ntf::mat4>;

public:
  template<typename T>
  using expect = ntf::expected<T, std::string_view>;

public:
  assimp_parser();

public:
  expect<void> load(const std::string& path);

public:
  expect<void> parse_materials(model_data::material_data& materials);
  expect<void> parse_meshes(model_data::mesh_data& meshes);
  expect<void> parse_rigs(model_data::rig_data& rigs);
  expect<void> parse_animations(model_data::animation_data& anims);

private:
  void _parse_bone_nodes(const bone_inv_map& bone_invs, size_t parent, size_t& bone_count,
                         const aiNode* node, model_data::rig_data& rigs);

private:
  Assimp::Importer _imp;
  std::string _dir;
};
