#include "assets/model_data.hpp"
#include <ntfstl/logger.hpp>

#define RET_ERR(_msg)             \
  ntf::logger::error("{}", _msg); \
  return unex_t {                 \
    _msg                          \
  }
#define RET_ERR_IF(_cond, _msg) \
  if (_cond) {                  \
    RET_ERR(_msg);              \
  }

namespace kappa {

assimp_parser::assimp_parser() {
  _imp.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, true);
  _imp.SetPropertyInteger(AI_CONFIG_PP_SBBC_MAX_BONES, model_mesh_data::VERTEX_BONE_COUNT);
}

auto assimp_parser::load(const std::string& path, uint32 assimp_flags) -> expect<void> {
  auto dir = shogle::file_dir(path);
  RET_ERR_IF(!dir, "Failed to parse directory path");

  const aiScene* scene = _imp.ReadFile(path.c_str(), assimp_flags);
  RET_ERR_IF(!scene, _imp.GetErrorString());

  _dir = std::move(*dir);
  _flags = assimp_flags;
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
  // the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
  out[0][0] = mat.a1;
  out[1][0] = mat.a2;
  out[2][0] = mat.a3;
  out[3][0] = mat.a4;
  out[0][1] = mat.b1;
  out[1][1] = mat.b2;
  out[2][1] = mat.b3;
  out[3][1] = mat.b4;
  out[0][2] = mat.c1;
  out[1][2] = mat.c2;
  out[2][2] = mat.c3;
  out[3][2] = mat.c4;
  out[0][3] = mat.d1;
  out[1][3] = mat.d2;
  out[2][3] = mat.d3;
  out[3][3] = mat.d4;
  return out;
}

static mat4 node_model(const aiNode* node) {
  NTF_ASSERT(node);
  if (node->mParent == nullptr) {
    return asscast(node->mTransformation);
  }
  return node_model(node->mParent) * asscast(node->mTransformation);
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

auto assimp_parser::parse_materials(model_material_data& mats) -> expect<void> {
  const aiScene* scene = _imp.GetScene();
  RET_ERR_IF(!scene->HasMaterials(), "No materials found");

  std::unordered_map<std::string, u32> parsed_tex;
  const auto stb_flags = shogle::image_load_flags::flip_y;
  shogle::stb_image_loader stb;

  // auto copy_raw_bitmap = [&](span<uint8> bitmap, const aiTexture* tex) -> ntfr::image_format {
  //   const size_t texel_count = tex->mWidth*tex->mHeight;
  //   if (tex->CheckFormat("rgba8888")) {
  //     for (size_t j = 0; j < texel_count; ++j) {
  //       const auto& texel = tex->pcData[j];
  //       const uint8 color[4] = {texel.r, texel.g, texel.b, texel.a};
  //       std::memcpy(bitmap.data()+j, color, 4);
  //     }
  //     return ntfr::image_format::rgba8nu;
  //   } else if (tex->CheckFormat("argb8888")) {
  //     return ntfr::image_format::rgba8nu;
  //   } else if (tex->CheckFormat("rgba5650")) {
  //
  //   } else if (tex->CheckFormat("rgba0010")) {
  //
  //   } else {
  //     return ntfr::image_format::rgba8nu;
  //   }
  // };
  //
  // // Parse embeded images
  // for (size_t i = 0; i < scene->mNumTextures; ++i) {
  //   const aiTexture* ai_tex = scene->mTextures[i];
  //   std::string tex_path = fmt::format("*{}", i);
  //   const char* tex_name = ai_tex->mFilename.C_Str();
  //
  //   if (ai_tex->mHeight == 0) {
  //     // Compressed image
  //     ntf::cspan<uint8> data{reinterpret_cast<uint8*>(ai_tex->pcData), ai_tex->mWidth};
  //     auto parsed = stb.load_image<uint8>(data, stb_flags, 0);
  //     if (!parsed) {
  //       ntf::logger::error("Failed to parse embeded texture: {}", parsed.error().what());
  //       continue;
  //     }
  //     auto [data_ptr, byte_count] = parsed->texels.release();
  //
  //     mats.textures.emplace_back(tex_name, tex_path,
  //                                ntf::unique_array<uint8>{byte_count, data_ptr},
  //                                parsed->extent, parsed->format);
  //   } else {
  //     // Uncompressed image
  //     const size_t byte_count = ai_tex->mWidth*ai_tex->mHeight*4u; // 4 channels
  //     auto bitmap = ntf::unique_array<uint8>::from_size(ntf::uninitialized, byte_count);
  //     auto format = copy_raw_bitmap({bitmap.data(), bitmap.size()}, ai_tex);
  //
  //     extent3d extent{ai_tex->mWidth, ai_tex->mHeight, 1u};
  //
  //     mats.textures.emplace_back(tex_name, tex_path, std::move(bitmap),
  //                                extent, format);
  //   }
  //   auto [_, empl] = parsed_tex.try_emplace(std::move(tex_path), mats.textures.size()-1);
  //   NTF_ASSERT(empl);
  // }

  auto load_textures = [&](u32& tex_count, const aiMaterial* ai_mat, aiTextureType type) {
    size_t count = ai_mat->GetTextureCount(type);
    for (size_t i = 0; i < count; ++i) {
      aiString filename;
      ai_mat->GetTexture(type, i, &filename);

      size_t idx;
      auto it = parsed_tex.find(filename.C_Str());
      if (it == parsed_tex.end()) {
        auto tex_path = fmt::format("{}/{}", _dir, filename.C_Str());
        if (!std::filesystem::exists(tex_path)) {
          ntf::logger::warning("Texture not found in \"{}\"!", tex_path);
          continue;
        }

        auto file_data = shogle::file_data(tex_path);
        if (!file_data) {
          ntf::logger::warning("Failed to load texture \"{}\", {}", tex_path,
                               file_data.error().what());
          continue;
        }
        auto image = stb.load_image<uint8>({file_data->data(), file_data->size()}, stb_flags, 0u);
        if (!image) {
          ntf::logger::warning("Failed to parse texture \"{}\", {}", tex_path,
                               image.error().what());
          continue;
        }
        auto [data_ptr, byte_count] = image->texels.release();
        mats.textures.emplace_back(filename.C_Str(), std::move(tex_path),
                                   ntf::unique_array<uint8>{byte_count, data_ptr}, image->extent,
                                   image->format);
        idx = mats.textures.size() - 1;
        parsed_tex.try_emplace(filename.C_Str(), idx);
      } else {
        idx = it->second;
      }

      mats.material_textures.emplace_back(idx);
      ++tex_count;
    }
    return count;
  };

  for (size_t i = 0; i < scene->mNumMaterials; ++i) {
    const aiMaterial* ai_mat = scene->mMaterials[i];
    const auto mat_name = ai_mat->GetName();

    u32 tex_count = 0u;
    u32 diff_count = load_textures(tex_count, ai_mat, aiTextureType_DIFFUSE);
    ntf::logger::verbose("{} diffuse texture(s) found in material {}", diff_count,
                         mat_name.C_Str());
    const u32 tex_idx = tex_count ? mats.material_textures.size() - 1 : vec_span::INDEX_TOMB;
    const vec_span tex_span{tex_idx, tex_count};

    mats.materials.emplace_back(mat_name.C_Str(), tex_span);
  }

  mats.texture_registry.reserve(mats.textures.size());
  for (size_t i = 0; const auto& tex : mats.textures) {
    mats.texture_registry.try_emplace(tex.name, i++);
  }
  mats.material_registry.reserve(mats.materials.size());
  for (size_t i = 0; const auto& mat : mats.materials) {
    mats.material_registry.try_emplace(mat.name, i++);
  }

  ntf::logger::debug("Parsed {} materials, {} textures", mats.materials.size(),
                     mats.textures.size());

  return {};
}

auto assimp_parser::parse_meshes(const model_rig_data& rigs, model_mesh_data& data,
                                 std::string_view model_name) -> expect<void> {
  const aiScene* scene = _imp.GetScene();

  // Reserve space for all vertex data
  u32 index_count = 0u;
  u32 vertex_count = 0u;
  u32 face_count = 0u;
  {
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

    data.positions.reserve(pos_count);
    data.normals.reserve(norm_count);
    data.uvs.reserve(uv_count);
    data.tangents.reserve(tang_count);
    data.bitangents.reserve(tang_count);
    data.colors.reserve(col_count);
    data.bones.reserve(weigth_count);
    data.weights.reserve(weigth_count);

    data.indices.reserve(index_count);
  }

  auto try_place_weight = [&](size_t mesh_start, size_t idx, const aiVertexWeight& weight) {
    const size_t vertex_pos = mesh_start + weight.mVertexId;
    auto& vertex_bone_indices = data.bones[vertex_pos];
    auto& vertex_bone_weights = data.weights[vertex_pos];

    NTF_ASSERT(vertex_pos < data.weights.size());
    for (size_t i = 0; i < model_mesh_data::VERTEX_BONE_COUNT; ++i) {
      if (weight.mWeight == 0.f) {
        return;
      }
      if (vertex_bone_indices[i] == vec_span::INDEX_TOMB) {
        vertex_bone_indices[i] = idx;
        vertex_bone_weights[i] = weight.mWeight;
        return;
      }
    }
    ntf::logger::warning("Bone weigths out of range in vertex {}", vertex_pos);
  };

  data.meshes.reserve(scene->mNumMeshes);
  data.mesh_registry.reserve(scene->mNumMeshes);
  for (size_t i = 0; i < scene->mNumMeshes; ++i) {
    const aiMesh* mesh = scene->mMeshes[i];
    const size_t nverts = mesh->mNumVertices;
    std::string_view name{mesh->mName.data, mesh->mName.length};
    ntf::logger::warning("{}", name);
    if (name.find(model_name) == std::string::npos) {
      continue;
    }

    // Positions
    vec_span pos{vec_span::INDEX_TOMB, 0u};
    if (mesh->HasPositions()) {
      pos.idx = data.positions.size();
      pos.count = nverts;
      for (size_t j = 0; j < nverts; ++j) {
        data.positions.emplace_back(asscast(mesh->mVertices[j]));
      }
    }

    // Normals
    vec_span norm{vec_span::INDEX_TOMB, 0u};
    if (mesh->HasNormals()) {
      norm.idx = data.normals.size();
      norm.count = nverts;
      for (size_t j = 0; j < nverts; ++j) {
        data.normals.emplace_back(asscast(mesh->mNormals[j]));
      }
    }

    // UVs
    vec_span uv{vec_span::INDEX_TOMB, 0u};
    if (mesh->HasTextureCoords(0)) {
      uv.idx = data.uvs.size();
      uv.count = nverts;
      for (size_t j = 0; j < nverts; ++j) {
        const auto& uv_vec = mesh->mTextureCoords[0][j];
        data.uvs.emplace_back(uv_vec.x, uv_vec.y);
      }
    }

    // Tangents & bitangents
    vec_span tang{vec_span::INDEX_TOMB, 0u};
    if (mesh->HasTangentsAndBitangents()) {
      tang.idx = data.tangents.size();
      tang.count = nverts;
      for (size_t j = 0; j < nverts; ++j) {
        data.tangents.emplace_back(asscast(mesh->mTangents[j]));
        data.bitangents.emplace_back(asscast(mesh->mBitangents[j]));
      }
    }

    // Colors
    vec_span col{vec_span::INDEX_TOMB, 0u};
    if (mesh->HasVertexColors(0)) {
      col.idx = data.colors.size() - nverts;
      col.count = nverts;
      for (size_t j = 0; j < nverts; ++j) {
        data.colors.emplace_back(asscast(mesh->mColors[0][j]));
      }
    }

    // Bone indices & weigths
    vec_span bone{vec_span::INDEX_TOMB, 0u};
    if (!rigs.bone_registry.empty() && mesh->HasBones()) {
      const size_t mesh_start = data.weights.size();
      bone.idx = data.weights.size();
      bone.count = nverts;

      // Fill with empty data
      for (size_t i = 0; i < nverts; ++i) {
        data.bones.emplace_back(model_mesh_data::EMPTY_BONE_INDEX);
        data.weights.emplace_back(model_mesh_data::EMPTY_BONE_WEIGHT);
      }

      for (size_t i = 0; i < mesh->mNumBones; ++i) {
        const aiBone* ai_bone = mesh->mBones[i];
        NTF_ASSERT(rigs.bone_registry.find(ai_bone->mName.C_Str()) != rigs.bone_registry.end());

        const size_t bone_idx = rigs.bone_registry.at(ai_bone->mName.C_Str());
        for (size_t j = 0; j < ai_bone->mNumWeights; ++j) {
          try_place_weight(mesh_start, bone_idx, ai_bone->mWeights[j]);
        }
      }
    }

    // Indices
    vec_span indices{vec_span::INDEX_TOMB, 0u};
    const u32 mesh_faces = mesh->mNumFaces;
    if (mesh->HasFaces()) {
      indices.idx = data.indices.size();
      u32 idx_count = 0u;
      for (size_t j = 0; j < mesh->mNumFaces; ++j) {
        const aiFace& face = mesh->mFaces[j];
        for (size_t k = 0; k < face.mNumIndices; ++k) {
          data.indices.emplace_back(face.mIndices[k]);
        }
        idx_count += face.mNumIndices;
      }
      indices.count = idx_count;
      face_count += mesh_faces;
    }

    const char* mesh_name = mesh->mName.C_Str();
    aiString mat_name = scene->mMaterials[mesh->mMaterialIndex]->GetName();

    data.meshes.emplace_back(pos, norm, uv, tang, col, bone, indices, mesh_name, mat_name.C_Str(),
                             mesh_faces);
  }

  for (u32 i = 0; const auto& mesh : data.meshes) {
    auto [_, empl] = data.mesh_registry.try_emplace(mesh.name, i++);
    NTF_ASSERT(empl);

    ntf::logger::info("{}", mesh.name);
    ntf::logger::info("- pos:  [{} {}]", mesh.positions.idx, mesh.positions.count);
    ntf::logger::info("- norm: [{} {}]", mesh.normals.idx, mesh.normals.count);
    ntf::logger::info("- uvs:  [{} {}]", mesh.uvs.idx, mesh.uvs.count);
    ntf::logger::info("- tang: [{} {}]", mesh.tangents.idx, mesh.tangents.count);
    ntf::logger::info("- bone: [{} {}]", mesh.bones.idx, mesh.bones.count);
    ntf::logger::info("- cols: [{} {}]", mesh.colors.idx, mesh.colors.count);
  }

  ntf::logger::debug("Parsed {} vertices, {} faces, {} indices, {} meshes", vertex_count,
                     data.indices.size(), face_count, data.meshes.size());

  return {};
}

void assimp_parser::_parse_bone_nodes(const bone_inv_map& bone_invs, u32 parent, u32& bone_count,
                                      const aiNode* node, model_rig_data& data) {
  NTF_ASSERT(node);
  u32 node_idx = bone_count;
  ++bone_count;

  auto it = bone_invs.find(node->mName.C_Str());
  NTF_ASSERT(it != bone_invs.end());
  const auto& [bone_name, inv_model] = *it;

  // Store meta info and transforms
  auto& bone = data.bones.emplace_back(bone_name, parent);
  data.bone_locals.emplace_back(asscast(node->mTransformation));
  data.bone_inv_models.emplace_back(inv_model);

  // Add bones to the registry
  auto [_, empl] = data.bone_registry.try_emplace(bone.name, node_idx);
  NTF_ASSERT(empl);

  // Parse children
  // ntf::logger::warning("BONE: [{},{}],  PARENT: {}", bone_name, node_idx, parent);
  // for (u32 i = 0; i < node->mNumChildren; ++i) {
  //   ntf::logger::warning("- {}", node->mChildren[i]->mName.C_Str());
  // }
  for (u32 i = 0; i < node->mNumChildren; ++i) {
    _parse_bone_nodes(bone_invs, node_idx, bone_count, node->mChildren[i], data);
  }
}

auto assimp_parser::parse_rigs(model_rig_data& data) -> expect<void> {
  const aiScene* scene = _imp.GetScene();

  // Store the inverse model matrix for each bone
  bone_inv_map bone_invs;
  for (size_t i = 0; i < scene->mNumMeshes; ++i) {
    const aiMesh* mesh = scene->mMeshes[i];
    for (size_t j = 0; j < mesh->mNumBones; ++j) {
      const aiBone* bone = mesh->mBones[j];
      if (bone_invs.find(bone->mName.C_Str()) != bone_invs.end()) {
        continue;
      }
      auto [_, empl] = bone_invs.try_emplace(bone->mName.C_Str(), asscast(bone->mOffsetMatrix));
      NTF_ASSERT(empl);
    }
  }

  // Map root bones to a node in the scene graph
  const auto* scene_root = scene->mRootNode;
  std::vector<const aiNode*> possible_roots;
  for (const auto& [name, _] : bone_invs) {
    const aiNode* bone_node = scene_root->FindNode(name.c_str());
    // We assume the bone and the node have the same name
    if (bone_invs.find(bone_node->mParent->mName.C_Str()) != bone_invs.end()) {
      continue;
    }
    // Assume all bone nodes with a parent not present in the map is a root node
    possible_roots.emplace_back(bone_node);
  }

  ntf::logger::debug("Found {} possible bone root(s)", possible_roots.size());

  // Create a bone tree from each root node
  u32 bone_count = 0u;
  data.bones.reserve(bone_invs.size()); // Avoid invalidating bone string pointers!!!!
  data.bone_registry.reserve(bone_invs.size());
  for (const aiNode* root : possible_roots) {
    // Check for name dupes
    auto bone_it = data.bone_registry.find(root->mName.C_Str());
    if (bone_it != data.bone_registry.end()) {
      const size_t bone_index = bone_it->second;
      if (bone_index != vec_span::INDEX_TOMB) {
        ntf::logger::warning("Bone root \"{}\" already parsed, possible node dupe",
                             root->mName.C_Str());
        continue;
      }
    }

    if (root->mNumChildren == 0u) {
    }

    const char* armature_name = root->mParent->mName.C_Str();
    u32 root_idx = bone_count;
    // Will set INDEX_TOMB as the parent index for the root bone
    _parse_bone_nodes(bone_invs, vec_span::INDEX_TOMB, bone_count, root, data);

    // Make sure the root local transform is its node model transform
    if (!is_identity(data.bone_locals[root_idx] * data.bone_inv_models[root_idx])) {
      ntf::logger::warning("Malformed transform in root \"{}\", correction applied",
                           root->mName.C_Str());
      data.bone_locals[root_idx] = node_model(root->mParent) * data.bone_locals[root_idx];
    }
    const vec_span armature_span{root_idx, bone_count - root_idx};

    data.armatures.emplace_back(armature_name, armature_span);
  }

  // We don't know the number of armatures before this so we reserve here
  data.armature_registry.reserve(data.armatures.size());
  for (u32 i = 0; auto& arm : data.armatures) {
    auto [_, empl] = data.armature_registry.try_emplace(arm.name, i++);
    if (!empl) {
      ntf::logger::warning("Armature \"{}\" parsed twice, generating new name for dupe", arm.name);
      arm.name.append("_bis");
      data.armature_registry.try_emplace(arm.name, i);
    }
  }

  ntf::logger::debug("Parsed {} armatures, {} bones", data.armatures.size(), data.bones.size());
  return {};
}

auto assimp_parser::parse_animations(model_anim_data& data) -> expect<void> {
  const aiScene* scene = _imp.GetScene();
  RET_ERR_IF(!scene->HasAnimations(), "No animations found");

  // Reserve space
  data.animations.reserve(scene->mNumAnimations);
  data.animation_registry.reserve(scene->mNumAnimations);
  for (size_t i = 0; i < scene->mNumAnimations; ++i) {
    const aiAnimation* anim = scene->mAnimations[i];
    const char* anim_name = anim->mName.C_Str();
    const double anim_tps = anim->mTicksPerSecond;
    const double anim_duration = anim->mDuration;

    // Keyframes
    const u32 kframe_count = anim->mNumChannels;
    const u32 kframe_idx = kframe_count ? data.keyframes.size() : vec_span::INDEX_TOMB;
    const vec_span kframe_span{kframe_idx, kframe_count};
    for (size_t j = 0; j < anim->mNumChannels; ++j) {
      const aiNodeAnim* node_anim = anim->mChannels[j];
      const char* bone_name = node_anim->mNodeName.C_Str();

      // Positions
      const u32 pos_count = node_anim->mNumPositionKeys;
      const u32 pos_idx = pos_count ? data.positions.size() : vec_span::INDEX_TOMB;
      const vec_span pos_span{pos_idx, pos_count};
      for (size_t k = 0; k < pos_count; ++k) {
        const auto& key = node_anim->mPositionKeys[k];
        data.positions.emplace_back(key.mTime, asscast(key.mValue));
      }

      // Rotations
      const u32 rot_count = node_anim->mNumRotationKeys;
      const u32 rot_idx = rot_count ? data.rotations.size() : vec_span::INDEX_TOMB;
      const vec_span rot_span{rot_idx, rot_count};
      for (size_t k = 0; k < rot_count; ++k) {
        const auto& key = node_anim->mRotationKeys[k];
        data.rotations.emplace_back(key.mTime, asscast(key.mValue));
      }

      // Scales
      const u32 sca_count = node_anim->mNumScalingKeys;
      const u32 sca_idx = sca_count ? data.scales.size() : vec_span::INDEX_TOMB;
      const vec_span sca_span{sca_idx, sca_count};
      for (size_t k = 0; k < sca_count; ++k) {
        const auto& key = node_anim->mScalingKeys[k];
        data.scales.emplace_back(key.mTime, asscast(key.mValue));
      }

      data.keyframes.emplace_back(bone_name, pos_span, rot_span, sca_span);
    }

    data.animations.emplace_back(anim_name, anim_duration, anim_tps, kframe_span);
  }

  for (u32 i = 0; const auto& anim : data.animations) {
    auto [_, empl] = data.animation_registry.try_emplace(anim.name, i++);
    NTF_ASSERT(empl);
  }

  ntf::logger::debug("Parsed {} animations, {} keyframes", data.animations.size(),
                     data.keyframes.size());

  return {};
}

model3d_mesh_textures::model3d_mesh_textures(ntf::unique_array<texture_t>&& textures,
                                             std::unordered_map<std::string_view, u32>&& tex_reg,
                                             ntf::unique_array<vec_span>&& mat_spans,
                                             ntf::unique_array<u32>&& mat_texes) noexcept :
    _textures{std::move(textures)},
    _tex_reg{std::move(tex_reg)}, _mat_spans{std::move(mat_spans)},
    _mat_texes{std::move(mat_texes)} {}

template<u32 tex_extent>
[[maybe_unused]] static constexpr auto missing_albedo_bitmap = [] {
  std::array<u8, 4u * tex_extent * tex_extent> out;
  const u8 pixels[]{
    0x00, 0x00, 0x00, 0xFF, // black
    0xFE, 0x00, 0xFE, 0xFF, // pink
    0x00, 0x00, 0x00, 0xFF, // black again
  };

  for (u32 i = 0; i < tex_extent; ++i) {
    const u8* start = i % 2 == 0 ? &pixels[0] : &pixels[4]; // Start row with a different color
    u32 pos = 0;
    for (u32 j = 0; j < tex_extent; ++j) {
      pos = (pos + 4) % 8;
      for (u32 k = 0; k < 4; ++k) {
        out[(4 * i * tex_extent) + (4 * j) + k] = start[pos + k];
      }
    }
  }

  return out;
}();

template<u32 tex_extent = 16u>
static shogle::render_expect<shogle::texture2d> make_missing_albedo(shogle::context_view ctx) {
  const shogle::image_data image{
    .bitmap = missing_albedo_bitmap<tex_extent>.data(),
    .format = shogle::image_format::rgba8u,
    .alignment = 4u,
    .extent = {tex_extent, tex_extent, 1},
    .offset = {0, 0, 0},
    .layer = 0u,
    .level = 0u,
  };
  const shogle::texture_data data{
    .images = {image},
    .generate_mipmaps = false,
  };
  return shogle::texture2d::create(ctx, {
                                          .format = shogle::image_format::rgba8u,
                                          .sampler = shogle::texture_sampler::nearest,
                                          .addressing = shogle::texture_addressing::repeat,
                                          .extent = {tex_extent, tex_extent, 1},
                                          .layers = 1u,
                                          .levels = 1u,
                                          .data = data,
                                        });
}

expect<model3d_mesh_textures> model3d_mesh_textures::create(const model_material_data& mat_data) {
  ntf::unique_array<u32> mat_texes{ntf::uninitialized, mat_data.material_textures.size()};
  for (u32 i = 0u; i < mat_data.material_textures.size(); ++i) {
    mat_texes[i] = mat_data.material_textures[i];
  }
  ntf::unique_array<vec_span> mat_spans{ntf::uninitialized, mat_data.materials.size()};
  for (u32 i = 0u; const auto& mat : mat_data.materials) {
    mat_spans[i++] = mat.textures;
  }

  ntf::unique_array<texture_t> textures{ntf::uninitialized, mat_data.textures.size()};
  std::unordered_map<std::string_view, u32> tex_reg;
  tex_reg.reserve(mat_data.textures.size());
  for (u32 i = 0u; const auto& tex : mat_data.textures) {
    auto tex2d = render::create_texture(tex.extent.x, tex.extent.y, tex.bitmap.data(), tex.format,
                                        shogle::texture_sampler::linear, 7u);
    if (!tex2d) {
      ntf::logger::error("Failed to upload texture \"{}\" ({})", tex.name, i);
      return {ntf::unexpect, "Failed to upload textures"};
    }

    const u32 sampler = render::TEX_SAMPLER_ALBEDO;
    std::construct_at(textures.data() + i, tex.name, std::move(*tex2d), sampler);
    auto [_, empl] = tex_reg.try_emplace(textures[i].name, i);
    NTF_ASSERT(empl);
    ++i;
  }

  return {ntf::in_place, std::move(textures), std::move(tex_reg), std::move(mat_spans),
          std::move(mat_texes)};
}

ntf::optional<u32> model3d_mesh_textures::find_texture_idx(std::string_view name) const {
  auto it = _tex_reg.find(name);
  if (it == _tex_reg.end()) {
    return ntf::nullopt;
  }
  return it->second;
}

shogle::texture2d_view model3d_mesh_textures::find_texture(std::string_view name) {
  auto idx = find_texture_idx(name);
  if (!idx) {
    return {};
  }
  NTF_ASSERT(*idx < _textures.size());
  return _textures[*idx].tex;
}

u32 model3d_mesh_textures::retrieve_material_textures(
  u32 mat_idx, std::vector<shogle::texture_binding>& texs) const {
  NTF_ASSERT(mat_idx < _mat_spans.size());

  const auto tex_span = _mat_spans[mat_idx].to_cspan(_mat_texes.data());
  for (const u32 tex_idx : tex_span) {
    NTF_ASSERT(tex_idx < _textures.size());
    const auto& texture = _textures[tex_idx];
    texs.emplace_back(texture.tex.get(), texture.sampler);
  }

  return tex_span.size();
}

} // namespace kappa
