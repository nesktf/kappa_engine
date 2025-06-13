#include "model.hpp"
#include <shogle/assets.hpp>
#include <ntfstl/logger.hpp>

using ntf::mat4;
using ntf::vec3;
using ntf::vec2;
using ntf::quat;
using ntf::color4;

#define RET_ERR(_msg) \
  ntf::logger::error("{}", _msg); \
  return unex_t{_msg}
#define RET_ERR_IF(_cond, _msg) \
  if (_cond) { RET_ERR(_msg); }

assimp_parser::assimp_parser() {
  _imp.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, true);
  _imp.SetPropertyBool(AI_CONFIG_PP_SBBC_MAX_BONES, model_data::BONE_WEIGHT_COUNT);
}

auto assimp_parser::load(const std::string& path) -> expect<void> {
  auto dir = ntf::file_dir(path);
  RET_ERR_IF(!dir, "Failed to parse directory path");

  const uint32 assimp_flags =
    aiProcess_FlipUVs | aiProcess_GenUVCoords |
    aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights;
  const aiScene* scene = _imp.ReadFile(path.c_str(), assimp_flags);
  RET_ERR_IF(!scene, _imp.GetErrorString());

  _dir = std::move(*dir);
  return {};
}

static bool is_identity(const mat4& mat) {
  constexpr mat4 id{1.f};
  for (uint32 i = 0; i < 4; ++i) {
    for (uint32 j = 0; j < 4; ++j) {
      if (std::abs(mat[i][j] - id[i][j]) > glm::epsilon<float>()) {
        return false;
      }
    }
  }
  return true;
}

static mat4 asscast(const aiMatrix4x4& mat) {
  mat4 out;
  //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
  out[0][0] = mat.a1; out[1][0] = mat.a2; out[2][0] = mat.a3; out[3][0] = mat.a4;
  out[0][1] = mat.b1; out[1][1] = mat.b2; out[2][1] = mat.b3; out[3][1] = mat.b4;
  out[0][2] = mat.c1; out[1][2] = mat.c2; out[2][2] = mat.c3; out[3][2] = mat.c4;
  out[0][3] = mat.d1; out[1][3] = mat.d2; out[2][3] = mat.d3; out[3][3] = mat.d4;
  return out;
}

static mat4 node_model(const aiNode* node) {
  NTF_ASSERT(node);
  if (node->mParent == nullptr) {
    return asscast(node->mTransformation);
  }
  return node_model(node->mParent)*asscast(node->mTransformation);
}

static vec3 asscast(const aiVector3D& vec) {
  return {vec.x, vec.y, vec.z};
}

static color4 asscast(const aiColor4D& col) {
  return {col.r, col.g, col.b, col.a};
}

static quat asscast(const aiQuaternion& q) {
  return {q.w, q.x, q.y, q.z};
}


void assimp_parser::_parse_bone_nodes(const bone_inv_map& bone_invs,
                                      size_t parent, size_t& bone_count,
                                      const aiNode* node, model_data::rig_data& rigs)
{
  NTF_ASSERT(node);
  size_t node_idx = bone_count;

  auto it = bone_invs.find(node->mName.C_Str());
  NTF_ASSERT(it != bone_invs.end());

  rigs.bones.emplace_back(
    parent,
    node->mName.C_Str()
  );
  rigs.bone_locals.emplace_back(asscast(node->mTransformation));
  rigs.bone_inv_models.emplace_back(it->second);
  // Now we can assign an index to the bone map entry
  rigs.bone_registry.emplace(std::make_pair(it->first, bone_count++));

  for (size_t i = 0; i < node->mNumChildren; ++i) {
    _parse_bone_nodes(bone_invs, node_idx, bone_count, node->mChildren[i], rigs);
  }
}

auto assimp_parser::parse_rigs(model_data::rig_data& rigs) -> expect<void> {
  const aiScene* scene = _imp.GetScene();

  bone_inv_map bone_invs;
  for (size_t i = 0; i < scene->mNumMeshes; ++i) {
    const aiMesh* mesh = scene->mMeshes[i];
    for (size_t j = 0; j < mesh->mNumBones; ++j) {
      const auto* bone = mesh->mBones[j];
      if (bone_invs.find(bone->mName.C_Str()) != bone_invs.end()) {
        continue;
      }
      // Store the inverse model matrix for later use
      bone_invs.emplace(
        std::make_pair(bone->mName.C_Str(), asscast(bone->mOffsetMatrix))
      );
    }
  }

  RET_ERR_IF(bone_invs.empty(), "No armatures found in model");

  const auto* scene_root = scene->mRootNode;
  std::vector<const aiNode*> possible_roots;
  for (const auto& [name, _] : bone_invs) {
    const auto* bone_node = scene_root->FindNode(name.c_str());
    if (bone_invs.find(bone_node->mParent->mName.C_Str()) != bone_invs.end()) {
      continue;
    }
    // Assume all bone nodes with a parent not present in the map is a root node
    possible_roots.emplace_back(bone_node);
  }

  ntf::logger::debug("Found {} possible bone root(s)", possible_roots.size());

  size_t bone_count = 0u;
  rigs.bones.reserve(bone_invs.size());

  for (const auto* root : possible_roots) { 
    auto it = rigs.bone_registry.find(root->mName.C_Str());

    if (it != rigs.bone_registry.end()) {
      const size_t bone_index = it->second;
      if (bone_index != model_data::INDEX_TOMB) {
        ntf::logger::warning("Bone root \"{}\" already parsed, possible node dupe",
                             root->mName.C_Str());
        continue;
      }
    }

    size_t root_idx = bone_count;
    // Will set INDEX_TOMB as the parent index for the root bone
    _parse_bone_nodes(bone_invs, model_data::INDEX_TOMB, bone_count, root, rigs);
    if (!is_identity(rigs.bone_locals[root_idx]*rigs.bone_inv_models[root_idx])) {
      ntf::logger::warning("Malformed transform in root \"{}\", correction applied",
                           root->mName.C_Str());
      bones[root_idx].local = node_model(root->mParent)*bones[root_idx].local;
    }

    armatures.emplace_back(vec_span{root_idx, bone_count-root_idx}, root->mParent->mName.C_Str());
  }

  ntf::logger::debug("Parsed {} armatures, {} bones", armatures.size(), bones.size());
}

static void _parse_embeded_textures(const aiScene* scene,
                                    std::vector<model_texture>& texs, texture_list_t& tex_list)
{
  if (!scene->HasTextures()) {
    ntf::logger::debug("No embeded textures found");
    return;
  }

  auto copy_uncompressed_bitmap = [&](ntf::unique_array<uint8>& bitmap, const aiTexture* tex) {
    const size_t texel_count = tex->mWidth*tex->mHeight;
    if (tex->CheckFormat("rgba8888")) {
      for (size_t j = 0; j < texel_count; ++j) {
        const auto& texel = tex->pcData[j];
        const uint8 color[4] = {texel.r, texel.g, texel.b, texel.a};
        std::memcpy(bitmap.data()+j, color, 4);
      }
    } else if (tex->CheckFormat("argb8888")) {

    } else if (tex->CheckFormat("rgba5650")) {

    } else if (tex->CheckFormat("rgba0010")) {

    } else {

    }
  };
  const auto stb_flags = ntf::image_load_flags::flip_y;
  ntf::stb_image_loader stb;

  for (size_t i = 0; i < scene->mNumTextures; ++i) {
    ntf::unique_array<uint8> bitmap;
    const aiTexture* tex = scene->mTextures[i];
    std::string path = fmt::format("*{}", i);
    tex_list.emplace(std::make_pair(path, texs.size()));
    if (tex->mHeight == 0) {
      // Compressed image
      if (!tex->CheckFormat("jpg") || !tex->CheckFormat("png")) {
        // Can't parse this!
        continue;
      }
      ntf::cspan<uint8> data{reinterpret_cast<uint8*>(tex->pcData), tex->mWidth};
      auto parsed_bitmap = stb.load_image<uint8>(data, stb_flags, 0);
      if (!parsed_bitmap) {
        ntf::logger::error("Failed to parse embeded texture: {}", parsed_bitmap.error().what());
        continue;
      }
    } else {
      // Uncompressed image
      const size_t byte_count = tex->mWidth*tex->mHeight*4u; // 4 channels
      bitmap = ntf::unique_array<uint8>::from_size(ntf::uninitialized, byte_count);
      copy_uncompressed_bitmap(bitmap, tex);
    }
    texs.emplace_back(std::move(bitmap), tex->mFilename.C_Str(), path);
  }
}

void assimp_parser::parse_materials(std::vector<model_material>& materials,
                                    std::vector<model_texture>& textures,
                                    texture_list_t& tex_list)
{
  const aiScene* scene = _imp.GetScene();
  if (!scene->HasMaterials()) {
    ntf::logger::debug("No materials found");
    return;
  }

  _parse_embeded_textures(scene, textures, tex_list);
  for (size_t i = 0; i < scene->mNumMaterials; ++i) {
    const aiMaterial* mat = scene->mMaterials[i];
    // mat->mProperties[0]
  }

}

static std::pair<u32, u32> _reserve_vertices(model_vertex_data& vertices, const aiScene* scene) {
  u32 vertex_count = 0u;
  u32 index_count = 0u;

  size_t pos_count = 0u;
  size_t norm_count = 0u;
  size_t uv_count = 0u;
  size_t tang_count = 0u;
  size_t col_count = 0u;
  size_t weigth_count = 0u;
  for (size_t i = 0u; i < scene->mNumMeshes; ++i) {
    const aiMesh* mesh = scene->mMeshes[i];
    const size_t verts = mesh->mNumVertices;

    if (mesh->HasPositions()) { 
      pos_count += verts;
    }
    if (mesh->HasNormals()) {
      norm_count += verts;
    }
    if (mesh->HasTextureCoords(0)) {
      uv_count += verts;
    }
    if (mesh->HasTangentsAndBitangents()) {
      tang_count += verts;
    }
    if (mesh->HasVertexColors(0)) {
      col_count += verts;
    }

    for (size_t j = 0; j < mesh->mNumFaces; ++j) {
      index_count += mesh->mFaces[j].mNumIndices;
    }
    vertex_count += verts;
  }

  vertices.positions.reserve(pos_count);
  vertices.normals.reserve(norm_count);
  vertices.uvs.reserve(uv_count);
  vertices.tangents.reserve(tang_count);
  vertices.bitangents.reserve(tang_count);
  vertices.colors.reserve(col_count);
  vertices.bones.reserve(weigth_count);
  vertices.weights.reserve(weigth_count);

  return std::make_pair(vertex_count, index_count);
}

static void _parse_weights(const bone_list_t& bones, const aiMesh* mesh,
                           std::vector<vertex_weights>& weights,
                           std::vector<vertex_bones>& indices)
{
  NTF_ASSERT(!bones.empty());
  const size_t mesh_pos = weights.size();
  const size_t nverts = mesh->mNumVertices;

  constexpr auto empty_index = []() -> vertex_bones {
    vertex_bones idx;
    for (auto& bone : idx) {
      bone = INDEX_TOMB;
    }
    return idx;
  }();
  constexpr auto empty_weight = []() -> vertex_weights {
    vertex_weights wgth;
    for (auto& weight : wgth) {
      weight = 0.f;
    }
    return wgth;
  }();

  for (size_t i = 0; i < nverts; ++i) {
    indices.emplace_back(empty_index);
    weights.emplace_back(empty_weight);
  }

  auto try_place_weight = [&](size_t idx, const aiVertexWeight& weight) {
    const size_t vertex_pos = mesh_pos+weight.mVertexId;
    auto& vertex_bone_indices = indices[vertex_pos];
    auto& vertex_bone_weights = weights[vertex_pos];

    NTF_ASSERT(vertex_pos < weights.size());
    for (size_t i = 0; i < BONE_WEIGHT_COUNT; ++i) {
      if (weight.mWeight == 0.f) {
        return;
      }
      if (vertex_bone_indices[i] == INDEX_TOMB) {
        vertex_bone_indices[i] = idx;
        vertex_bone_weights[i] = weight.mWeight;
        return;
      }
    }
    ntf::logger::warning("Bone weigths out of range in vertex {}", vertex_pos);
  };

  for (size_t i = 0; i < mesh->mNumBones; ++i) {
    const aiBone* bone = mesh->mBones[i];
    NTF_ASSERT(bones.find(bone->mName.C_Str()) != bones.end());
    const size_t idx = bones.at(bone->mName.C_Str()).first;
    for (size_t j = 0; j < bone->mNumWeights; ++j) {
      try_place_weight(idx, bone->mWeights[j]);
    }
  }
}

void assimp_parser::parse_meshes(model_vertex_data& vertices, std::vector<model_mesh>& meshes,
                                 const bone_list_t& bones)
{
  const aiScene* scene = _imp.GetScene();
  auto [vert_count, indx_count] = _reserve_vertices(vertices, scene);

  for (size_t i = 0; i < scene->mNumMeshes; ++i) {
    const aiMesh* mesh = scene->mMeshes[i];
    const size_t nverts = mesh->mNumVertices;

    vec_span pos;
    if (mesh->HasPositions()) {
      for(size_t j = 0; j < nverts; ++j) {
        vertices.positions.emplace_back(asscast(mesh->mVertices[j]));
      }
      pos.idx = vertices.positions.size()-nverts;
      pos.count = nverts;
    } else {
      pos.idx = INDEX_TOMB;
    }

    vec_span norm;
    if (mesh->HasNormals()) {
      for (size_t j = 0; j < nverts; ++j) {
        vertices.normals.emplace_back(asscast(mesh->mNormals[j]));
      }
      norm.idx = vertices.normals.size()-nverts;
      norm.count = nverts;
    } else {
      norm.idx = INDEX_TOMB;
    }

    vec_span uv;
    if (mesh->HasTextureCoords(0)) {
      for (size_t j = 0; j < nverts; ++j) {
        const auto& uv_vec = mesh->mTextureCoords[0][j];
        vertices.uvs.emplace_back(uv_vec.x, uv_vec.y);
      }
      uv.idx = vertices.uvs.size()-nverts;
      uv.count = nverts;
    } else {
      uv.idx = INDEX_TOMB;
    }

    vec_span tang;
    if (mesh->HasTangentsAndBitangents()) {
      for (size_t j = 0; j < nverts; ++j) {
        vertices.tangents.emplace_back(asscast(mesh->mTangents[j]));
        vertices.bitangents.emplace_back(asscast(mesh->mBitangents[j]));
      }
      tang.idx = vertices.tangents.size()-nverts;
      tang.count = nverts;
    } else {
      tang.idx = INDEX_TOMB;
    }

    vec_span col;
    if (mesh->HasVertexColors(0)) {
      for (size_t j = 0; j < nverts; ++j) {
        vertices.colors.emplace_back(asscast(mesh->mColors[0][j]));
      }
      col.idx = vertices.colors.size()-nverts;
      col.count = nverts;
    } else {
      col.idx = INDEX_TOMB;
    }

    vec_span bone;
    if (!bones.empty() && mesh->HasBones()) {
      _parse_weights(bones, mesh, vertices.weights, vertices.bones);
      bone.idx = vertices.weights.size()-nverts;
      bone.count = nverts;
    } else {
      bone.idx = INDEX_TOMB;
    }

    std::vector<uint32> indices;
    if (mesh->HasFaces()) {
      for (size_t j = 0; j < mesh->mNumFaces; ++j) {
        const aiFace& face = mesh->mFaces[j];
        indices.reserve(face.mNumIndices);
        for (size_t k = 0; k < face.mNumIndices; ++k) {
          indices.emplace_back(face.mIndices[k]);
        }
      }
    }

    std::string name = mesh->mName.C_Str();

    meshes.emplace_back(pos, norm, uv, tang, col, bone, std::move(indices),
                        std::move(name), mesh->mMaterialIndex);
  }
  ntf::logger::debug("Parsed {} vertices, {} indices, {} mesh(es)",
                     vert_count, indx_count, scene->mNumMeshes);
}

model_data::model_data(model_vertex_data&& vertices, std::vector<model_mesh>&& meshes,
                       std::vector<model_armature>&& armatures, std::vector<model_bone>&& bones,
                       std::vector<model_texture>&& textures,
                       std::vector<model_material>&& materials) noexcept :
  _vertices{std::move(vertices)}, _meshes{std::move(meshes)},
  _armatures{std::move(armatures)}, _bones{std::move(bones)},
  _textures{std::move(textures)}, _materials{std::move(materials)} {}

ntf::expected<model_data, std::string> model_data::load_model(const std::string& path) {
  assimp_parser parser;

  std::string err;
  if (!parser.load(path, err)) {
    return ntf::unexpected{std::move(err)};
  }

  model_vertex_data vertices;
  std::vector<model_mesh> meshes;
  std::vector<model_armature> armatures;
  std::vector<model_bone> bones;
  std::vector<model_texture> textures;
  std::vector<model_material> materials;

  bone_list_t bone_list;
  parser.parse_armatures(armatures, bones, bone_list);

  texture_list_t tex_list;
  parser.parse_materials(materials, textures, tex_list);

  parser.parse_meshes(vertices, meshes, bone_list);

  return ntf::expected<model_data, std::string>{
    ntf::in_place, std::move(vertices), std::move(meshes),
    std::move(armatures), std::move(bones),
    std::move(textures), std::move(materials)
  };
}
