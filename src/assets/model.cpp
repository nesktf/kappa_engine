#include "./internal.hpp"

#define MODEL_LOG(level_, fmt_, ...) \
  ::kappa::logger::level_("[MODEL_IMPORT] " fmt_ __VA_OPT__(, ) __VA_ARGS__)

namespace kappa::assets {

namespace {

template<typename T>
constexpr void zeroinit(T* ptr, size_t sz) {
  std::memset(ptr, 0x00, sizeof(T) * sz);
};

template<typename T>
void alloc_init(model_allocator& al, T** ptr, size_t sz) {
  *ptr = al.alloc<T>(sz);
  zeroinit(*ptr, sz);
}

} // namespace

model3d_loader::model3d_loader(std::string_view model_path, std::string_view model_name,
                               shogle::ptr_view<const load_opts> opts) :
    _impl(new model3d_loader::loader_internal()) {
  _impl->importer.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, true);
  _impl->importer.SetPropertyInteger(AI_CONFIG_PP_SBBC_MAX_BONES, 4);
  _impl->model_path.copy_from(model_path.data(), model_path.size());
  _impl->model_name.copy_from(model_name.data(), model_name.size());
  _impl->importer_flags = opts ? opts->flags : FLAGS_DEFAULT;
  if (opts && !opts->texture_dir.empty()) {
    _impl->texture_dir.copy_from(opts->texture_dir.data(), opts->texture_dir.size());
  } else {
    auto slash_pos = model_path.find_last_of('/');
    assert(slash_pos != std::string::npos, "Non valid path provided");
    auto model_file_dir = model_path.substr(0, slash_pos);
    _impl->texture_dir.copy_from(model_file_dir.data(), model_file_dir.size());
  }
}

model3d_data::model_internal::model_internal(const buffer_name& name_, const buffer_path& path_) :
    meshes(nullptr), mesh_count(0), mesh_positions(nullptr), mesh_position_count(0),
    mesh_normals(nullptr), mesh_normal_count(0), mesh_tangents(nullptr), mesh_bitangents(nullptr),
    mesh_tangent_count(0), mesh_bone_indices(nullptr), mesh_bone_weights(nullptr),
    mesh_bone_count(0), mesh_indices(nullptr), mesh_index_count(0), blend_shapes(nullptr),
    blend_shape_count(0), blend_positions(nullptr), blend_position_count(0),
    blend_normals(nullptr), blend_normal_count(0), blend_tangents(nullptr),
    blend_bitangents(nullptr), blend_tangent_count(0),
#if 0
    animations(nullptr), animation_count(0),
    anim_bone_keyframes(nullptr), anim_bone_keyframe_count(0), anim_bone_positions(nullptr),
    anim_bone_position_count(0), anim_bone_scales(nullptr), anim_bone_scale_count(0),
    anim_bone_rotations(nullptr), anim_bone_rotation_count(0),
#endif
    bones(nullptr), bone_locals(nullptr), bone_inv_models(nullptr), bone_count(0),
    textures(nullptr), texture_count(0), materials(nullptr), material_count(0),
    material_textures(nullptr), material_textures_count(0) {
  // Initialize arrays
  zeroinit(&mesh_uvs[0], MAX_MESH_UVS);
  zeroinit(&mesh_uv_count[0], MAX_MESH_UVS);
  zeroinit(&mesh_colors[0], MAX_MESH_COLORS);
  zeroinit(&mesh_color_count[0], MAX_MESH_COLORS);
  zeroinit(&blend_uvs[0], MAX_MESH_UVS);
  zeroinit(&blend_uv_count[0], MAX_MESH_UVS);
  zeroinit(&blend_colors[0], MAX_MESH_COLORS);
  zeroinit(&blend_color_count[0], MAX_MESH_COLORS);

  // Copy name & path
  name.copy_from(name_.data, name_.len);
  path.copy_from(path_.data, path_.len);
}

namespace {

m4f32 asscast(const aiMatrix4x4& mat) {
  // Assimp is row major. The a,b,c,d is the row and the 1,2,3,4 is the column.
  return shogle::math::transpose(std::bit_cast<m4f32>(mat)); // Just transpose it lol
}

using bone_inv_map = std::unordered_map<std::string_view, m4f32>;

auto initialize_data(const buffer_name& name, const buffer_path& path, const aiScene& scene,
                     bone_inv_map& bone_invs, buffer_str<256>& err)
  -> std::unique_ptr<model3d_data::model_internal> {
  if (!scene.mNumMeshes) {
    err.format_from("No meshes in scene");
    return nullptr;
  }

  auto ptr = std::make_unique<model3d_data::model_internal>(name, path);
  auto& data = *ptr;
  auto& al = ptr->alloc;
  const auto maybe_alloc = [&]<typename T>(T** ptr, size_t sz) {
    if (sz) {
      alloc_init(al, ptr, sz);
    }
  };

  // First mesh pass. Preallocate blend shapes, bones & meshes.
  for (size_t i = 0; i < scene.mNumMeshes; ++i) {
    const aiMesh* mesh = scene.mMeshes[i];
    data.blend_shape_count += mesh->mNumAnimMeshes;
    ++data.mesh_count;

    // Store the inverse model matrix for each bone (if any)
    for (size_t j = 0; j < mesh->mNumBones; ++j) {
      const aiBone* bone = mesh->mBones[j];
      std::string_view name(bone->mName.data, bone->mName.length);
      if (bone_invs.find(name) != bone_invs.end()) {
        continue;
      }
      auto [_, empl] = bone_invs.try_emplace(name, asscast(bone->mOffsetMatrix));
      if (!empl) {
        err.format_from("Duplicate bone \"{}\" in scene", bone->mName.C_Str());
        return nullptr;
      }
    }
  }
  data.meshes = al.alloc<model3d_data::mesh_data>(data.mesh_count);
  zeroinit(data.meshes, data.mesh_count);
  maybe_alloc(&data.blend_shapes, data.blend_shape_count);

  static_assert(model3d_data::MAX_MESH_UVS <= AI_MAX_NUMBER_OF_TEXTURECOORDS);
  static_assert(model3d_data::MAX_MESH_COLORS <= AI_MAX_NUMBER_OF_COLOR_SETS);
  // Second mesh pass. Count meshes and blend shapes for each mesh,
  // copy names & preallocate vertices
  {
    size_t anim_pos = 0;
    for (size_t i = 0; i < scene.mNumMeshes; ++i) {
      const aiMesh* mesh = scene.mMeshes[i];
      const size_t verts = mesh->mNumVertices;
      if (!mesh->HasPositions()) {
        err.format_from("Invalid position data at mesh \"{}\"", mesh->mName.C_Str());
        return nullptr;
      }
      const aiString& mesh_name = mesh->mName;
      data.meshes[i].name.copy_from(mesh_name.data, mesh_name.length);
      auto [_, empl] = data.mesh_registry.try_emplace(data.meshes[i].name.as_view(), i);
      if (!empl) {
        err.format_from("Duplicate mesh name \"{}\" in model", data.meshes[i].name.as_view());
        return nullptr;
      }

      data.mesh_position_count += verts;
      if (mesh->HasNormals()) {
        data.mesh_normal_count += verts;
      }
      if (mesh->HasTangentsAndBitangents()) {
        data.mesh_tangent_count += verts;
      }
      for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
        if (mesh->HasTextureCoords(uv)) {
          data.mesh_uv_count[uv] += verts;
        }
        if (mesh->HasTextureCoordsName(uv)) {
          // Copy name
          const aiString& name = *mesh->mTextureCoordsNames[uv];
          data.meshes[i].uv_name[uv].copy_from(name.data, name.length);
        }
      }
      for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
        if (mesh->HasVertexColors(col)) {
          data.mesh_color_count[col] += verts;
        }
      }
      if (mesh->HasBones()) {
        data.mesh_bone_count += verts;
      }
      for (size_t j = 0; j < mesh->mNumFaces; ++j) {
        data.mesh_index_count += mesh->mFaces[j].mNumIndices;
      }

      for (size_t j = 0; j < mesh->mNumAnimMeshes; ++j) {
        const aiAnimMesh* ai_anim = mesh->mAnimMeshes[j];
        const size_t animverts = ai_anim->mNumVertices;
        auto& anim = data.blend_shapes[anim_pos++];
        anim.name.copy_from(ai_anim->mName.data, ai_anim->mName.length);

        if (ai_anim->HasPositions()) {
          data.blend_position_count += animverts;
        }
        if (ai_anim->HasNormals()) {
          data.blend_normal_count += animverts;
        }
        if (ai_anim->HasTangentsAndBitangents()) {
          data.blend_tangent_count += animverts;
        }
        for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
          if (ai_anim->HasTextureCoords(uv)) {
            data.blend_uv_count[uv] += animverts;
          }
        }
        for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
          if (ai_anim->HasVertexColors(col)) {
            data.blend_color_count[col] += animverts;
          }
        }
      }
    }

    maybe_alloc(&data.mesh_positions, data.mesh_position_count);
    maybe_alloc(&data.blend_positions, data.blend_position_count);
    maybe_alloc(&data.mesh_indices, data.mesh_index_count);

    maybe_alloc(&data.mesh_normals, data.mesh_normal_count);
    maybe_alloc(&data.blend_normals, data.blend_normal_count);

    maybe_alloc(&data.mesh_tangents, data.mesh_tangent_count);
    maybe_alloc(&data.mesh_bitangents, data.mesh_tangent_count);
    maybe_alloc(&data.blend_tangents, data.blend_tangent_count);
    maybe_alloc(&data.blend_bitangents, data.blend_tangent_count);

    const bool have_bones = !bone_invs.empty();
    if (have_bones && data.mesh_bone_count) {
      data.mesh_bone_indices = al.alloc<v4i32>(data.mesh_bone_count);
      data.mesh_bone_weights = al.alloc<v4f32>(data.mesh_bone_count);
      // Set all parents to NULL (-1)
      std::memset(data.mesh_bone_indices, 0xFF,
                  sizeof(data.mesh_bone_indices[0]) * data.mesh_bone_count);
      // Set all weights to 0
      std::memset(data.mesh_bone_weights, 0x00,
                  sizeof(data.mesh_bone_weights[0] * data.mesh_bone_count));
    }

    for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
      maybe_alloc(data.mesh_uvs + uv, data.mesh_uv_count[uv]);
      maybe_alloc(data.blend_uvs + uv, data.blend_uv_count[uv]);
    }
    for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
      maybe_alloc(data.mesh_colors + col, data.mesh_color_count[col]);
      maybe_alloc(data.blend_colors + col, data.blend_color_count[col]);
    }
  }

#if 0
  // Preallocate animations & keyframes
  data.animation_count = scene.mNumAnimations;
  maybe_alloc(&data.animations, data.animation_count);
  if (data.animation_count) {
    data.bone_anim_registry.reserve(data.animation_count);
    for (size_t i = 0; i < scene.mNumAnimations; ++i) {
      const aiAnimation* ai_anim = scene.mAnimations[i];
      const aiString& anim_name = ai_anim->mName;
      auto& anim = data.animations[i];
      anim.name.copy_from(anim_name.data, anim_name.length);
      auto [_, empl] = data.bone_anim_registry.emplace(anim.name.as_view(), i);
      if (!empl) {
        err.format_from("Duplicate animation name \"{}\" in model", anim.name.as_view());
        return nullptr;
      }

      for (size_t j = 0; j < ai_anim->mNumChannels; ++j) {
        const aiNodeAnim* node = ai_anim->mChannels[j];
        data.anim_bone_position_count += node->mNumPositionKeys;
        data.anim_bone_scale_count += node->mNumScalingKeys;
        data.anim_bone_rotation_count += node->mNumRotationKeys;
      }
      data.anim_bone_keyframe_count += ai_anim->mNumChannels;
    }
  }
  maybe_alloc(&data.anim_bone_keyframes, data.anim_bone_keyframe_count);
  maybe_alloc(&data.anim_bone_positions, data.anim_bone_position_count);
  maybe_alloc(&data.anim_bone_scales, data.anim_bone_scale_count);
  maybe_alloc(&data.anim_bone_rotations, data.anim_bone_rotation_count);
#endif

  // Preallocate bones & matrices
  data.bone_count = bone_invs.size();
  if (data.bone_count) {
    data.bones = al.alloc<model3d_data::bone_data>(data.bone_count);
    zeroinit(data.bones, data.bone_count);
    data.bone_inv_models = al.alloc<m4f32>(data.bone_count);
    data.bone_locals = al.alloc<m4f32>(data.bone_count);
    data.bone_registry.reserve(data.bone_count); // We fill the registry at parse_bones()
  }

  return ptr;
}

bool is_identity(const m4f32& mat) {
  static constexpr m4f32 id(1.f);
  for (size_t i = 0; i < 4; ++i) {
    const auto col_a = mat.column_at(i);
    const auto col_b = id.column_at(i);
    for (size_t j = 0; j < 4; ++j) {
      if (std::abs(col_a[j] - col_b[j]) > shogle::math::epsilon<f32>) {
        return false;
      }
    }
  }
  return true;
}

void parse_bone_nodes(const bone_inv_map& bone_invs, i32 parent, i32& bone_count,
                      const aiNode* node, model3d_data::model_internal& data) {
  assert(node);
  std::string_view name(node->mName.data, node->mName.length);
  const auto& inv_model = bone_invs.at(name);
  const i32 this_bone = bone_count++;

  // Store meta info and transforms
  data.bones[this_bone].name.copy_from(name.data(), name.size());
  data.bones[this_bone].parent = parent;
  data.bone_locals[this_bone] = asscast(node->mTransformation);
  data.bone_inv_models[this_bone] = inv_model;

  // Add bone to the registry. At this point we should never have duplicate bone names.
  data.bone_registry.emplace(data.bones[this_bone].name.as_view(), this_bone);

  // Parse children
  for (size_t i = 0; i < node->mNumChildren; ++i) {
    parse_bone_nodes(bone_invs, this_bone, bone_count, node->mChildren[i], data);
  }
}

m4f32 node_model(const aiNode* node) {
  if (node->mParent == nullptr) {
    return asscast(node->mTransformation);
  }
  return node_model(node->mParent) * asscast(node->mTransformation);
}

void parse_rigs(model3d_data::model_internal& data, const aiScene& scene,
                const bone_inv_map& bone_invs) {
  if (bone_invs.empty()) {
    return;
  }
  const aiNode* root_bone_node = [&]() -> const aiNode* {
    for (const auto& [name, _] : bone_invs) {
      const aiNode* node = scene.mRootNode->FindNode(name.data()); // name is null terminated
      assert(node); // every bone should have a corresponding node
      std::string_view parent_name(node->mParent->mName.data, node->mParent->mName.length);
      // Find the first node with a parent not in the bone map
      // We asume that one is the root bone node
      if (bone_invs.find(parent_name) == bone_invs.end()) {
        return node;
      }
    }
    return nullptr;
  }();
  assert(root_bone_node);
  MODEL_LOG(debug, "Using node \"{}\" as armature root", root_bone_node->mName.C_Str());

  i32 bone_count = 0;
  // Will set -1 as the parent index for the root bone
  parse_bone_nodes(bone_invs, -1, bone_count, root_bone_node, data);

  // Make sure the root local transform is its node model transform
  if (!is_identity(data.bone_locals[0] * data.bone_inv_models[0])) {
    MODEL_LOG(warning, "Malformed transform in bone root at model \"{}\", correction applied",
              data.path.as_view());
    data.bone_locals[0] = node_model(root_bone_node->mParent) * data.bone_locals[0];
  }
}

v3f32 asscast(const aiVector3D& vec) {
  return {vec.x, vec.y, vec.z};
}

v4f32 asscast(const aiColor4D& col) {
  return {col.r, col.g, col.b, col.a};
}

void parse_meshes(model3d_data::model_internal& data, const aiScene& scene) {
  size_t vertex_pos = 0, vertex_anim_pos = 0;
  size_t normal_pos = 0, normal_anim_pos = 0;
  size_t tangent_pos = 0, tangent_anim_pos = 0;
  size_t bone_pos = 0;
  size_t uv_pos[model3d_data::MAX_MESH_UVS] = {0};
  size_t uv_anim_pos[model3d_data::MAX_MESH_UVS] = {0};
  size_t color_pos[model3d_data::MAX_MESH_COLORS] = {0};
  size_t color_anim_pos[model3d_data::MAX_MESH_COLORS] = {0};
  size_t index_pos = 0;
  size_t shape_pos = 0;

  const auto try_place_weight = [&](i32 bone_idx, const aiVertexWeight& weight) {
    const size_t offset = bone_pos + weight.mVertexId;
    assert(offset < data.mesh_bone_count);

    for (size_t i = 0; i < 4; ++i) {
      if (weight.mWeight == 0.f) {
        return;
      }
      if (data.mesh_bone_indices[offset][i] == -1) {
        data.mesh_bone_indices[offset][i] = bone_idx;
        data.mesh_bone_weights[offset][i] = weight.mWeight;
        return;
      }
    }
    MODEL_LOG(warning, "Bone weights out of range in vertex index {}", vertex_pos);
  };

  const auto do_attrib = [&](bool cond, size_t count, size_t& pos, u32& target_start, auto&& f) {
    if (cond) {
      for (size_t i = 0; i < count; ++i) {
        f(pos + i, i);
      }
      target_start = static_cast<u32>(pos);
      pos += count;
    } else {
      target_start = static_cast<u32>(-1);
    }
  };

  const auto& bone_reg = data.bone_registry;
  for (size_t mesh_idx = 0; mesh_idx < scene.mNumMeshes; ++mesh_idx) {
    const aiMesh* ai_mesh = scene.mMeshes[mesh_idx];
    auto& mesh = data.meshes[mesh_idx];
    mesh.nverts = ai_mesh->mNumVertices;

    // Positions. Always present but use HasPositions for consistency.
    do_attrib(ai_mesh->HasPositions(), mesh.nverts, vertex_pos, mesh.positions_start,
              [&](size_t pos, size_t vert) {
                assert(pos < data.mesh_position_count);
                assert(vert < ai_mesh->mNumVertices);
                data.mesh_positions[pos] = asscast(ai_mesh->mVertices[vert]);
              });

    // Normals
    do_attrib(ai_mesh->HasNormals(), mesh.nverts, normal_pos, mesh.normals_start,
              [&](size_t pos, size_t vert) {
                assert(pos < data.mesh_normal_count);
                assert(vert < ai_mesh->mNumVertices);
                data.mesh_normals[pos] = asscast(ai_mesh->mNormals[vert]);
              });

    // Tangents & bitangents
    do_attrib(ai_mesh->HasTangentsAndBitangents(), mesh.nverts, tangent_pos, mesh.tangents_start,
              [&](size_t pos, size_t vert) {
                assert(pos < data.mesh_tangent_count);
                assert(vert < ai_mesh->mNumVertices);
                data.mesh_tangents[pos] = asscast(ai_mesh->mTangents[vert]);
                data.mesh_bitangents[pos] = asscast(ai_mesh->mBitangents[vert]);
              });

    // UVs
    for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
      do_attrib(ai_mesh->HasTextureCoords(uv), mesh.nverts, uv_pos[uv], mesh.uvs_start[uv],
                [&](size_t pos, size_t vert) {
                  assert(pos < data.mesh_uv_count[uv]);
                  assert(vert < ai_mesh->mNumVertices);
                  const auto& uv_vec = ai_mesh->mTextureCoords[uv][vert];
                  data.mesh_uvs[uv][pos].x = uv_vec.x;
                  data.mesh_uvs[uv][pos].y = uv_vec.y;
                });
    };

    // Colors
    for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
      do_attrib(ai_mesh->HasVertexColors(col), mesh.nverts, color_pos[col], mesh.colors_start[col],
                [&](size_t pos, size_t vert) {
                  assert(pos < data.mesh_color_count[col]);
                  assert(vert < ai_mesh->mNumVertices);
                  data.mesh_colors[col][pos] = asscast(ai_mesh->mColors[col][vert]);
                });
    }

    // Bone indices & weights
    if (!bone_reg.empty() && ai_mesh->HasBones()) {
      // Special iteration here, can't use do_attrib directly
      for (size_t bone = 0; bone < ai_mesh->mNumBones; ++bone) {
        const aiBone* ai_bone = ai_mesh->mBones[bone];
        std::string_view bone_name(ai_bone->mName.data, ai_bone->mName.length);
        // At this point every name *should* be valid
        auto it = bone_reg.find(bone_name);
        if (it == bone_reg.end()) {
          MODEL_LOG(error, "Bone out of hierarchy \"{}\"", bone_name);
          continue;
        }
        const i32 bone_idx = static_cast<i32>(it->second);
        for (size_t weight = 0; weight < ai_bone->mNumWeights; ++weight) {
          try_place_weight(bone_idx, ai_bone->mWeights[weight]);
        }
      }
      mesh.bones_start = static_cast<u32>(bone_pos);
      bone_pos += mesh.nverts;
    } else {
      mesh.bones_start = static_cast<i32>(-1);
    }

    // Face indices
    {
      mesh.face_count = ai_mesh->mNumFaces;
      u32 index_count = 0u;
      for (size_t face_idx = 0; face_idx < ai_mesh->mNumFaces; ++face_idx) {
        const aiFace& face = ai_mesh->mFaces[face_idx];
        for (size_t i = 0; i < face.mNumIndices; ++i) {
          const size_t pos = index_pos + index_count + i;
          assert(pos < data.mesh_index_count);
          data.mesh_indices[pos] = face.mIndices[i];
        }
        index_count += face.mNumIndices;
      }
      mesh.index_start = index_count ? index_pos : (u32)-1;
      mesh.index_count = index_count;
      index_pos += index_count;
    }

    // Blend shapes, almost a duplicate of the mesh data
    if (ai_mesh->mNumAnimMeshes) {
      mesh.blend_start = shape_pos;
      mesh.blend_count = ai_mesh->mNumAnimMeshes;
    } else {
      mesh.blend_start = static_cast<u32>(-1);
    }
    for (size_t j = 0; j < ai_mesh->mNumAnimMeshes; ++j) {
      const aiAnimMesh* ai_anim = ai_mesh->mAnimMeshes[j];
      auto& anim = data.blend_shapes[shape_pos++];
      anim.nverts = ai_anim->mNumVertices;
      anim.weight = ai_anim->mWeight;

      // Positions (can be absent this time)
      do_attrib(ai_anim->HasPositions(), anim.nverts, vertex_anim_pos, anim.positions_start,
                [&](size_t pos, size_t vert) {
                  assert(pos < data.blend_position_count);
                  assert(vert < ai_anim->mNumVertices);
                  data.blend_positions[pos] = asscast(ai_anim->mVertices[vert]);
                });

      // Normals
      do_attrib(ai_anim->HasNormals(), anim.nverts, normal_anim_pos, anim.normals_start,
                [&](size_t pos, size_t vert) {
                  assert(pos < data.blend_normal_count);
                  assert(vert < ai_anim->mNumVertices);
                  data.blend_normals[pos] = asscast(ai_anim->mNormals[vert]);
                });

      // Tangents & Bitangents
      do_attrib(ai_anim->HasTangentsAndBitangents(), anim.nverts, tangent_anim_pos,
                anim.tangents_start, [&](size_t pos, size_t vert) {
                  assert(pos < data.blend_tangent_count);
                  assert(vert < ai_anim->mNumVertices);
                  data.blend_tangents[pos] = asscast(ai_anim->mTangents[vert]);
                  data.blend_bitangents[pos] = asscast(ai_anim->mBitangents[vert]);
                });

      // UVs
      for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
        do_attrib(ai_anim->HasTextureCoords(uv), anim.nverts, uv_anim_pos[uv], anim.uvs_start[uv],
                  [&](size_t pos, size_t vert) {
                    assert(pos < data.blend_uv_count[uv]);
                    assert(vert < ai_anim->mNumVertices);
                    const auto& uv_vec = ai_anim->mTextureCoords[uv][vert];
                    data.blend_uvs[uv][pos].x = uv_vec.x;
                    data.blend_uvs[uv][pos].y = uv_vec.y;
                  });
      }

      // Colors
      for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
        do_attrib(ai_anim->HasVertexColors(col), anim.nverts, color_anim_pos[col],
                  anim.colors_start[col], [&](size_t pos, size_t vert) {
                    assert(pos < data.blend_color_count[col]);
                    assert(vert < ai_anim->mNumVertices);
                    data.blend_colors[col][pos] = asscast(ai_anim->mColors[col][vert]);
                  });
      }
    }

    // Set other props
    mesh.bbox_max = asscast(ai_mesh->mAABB.mMax);
    mesh.bbox_min = asscast(ai_mesh->mAABB.mMin);
    mesh.material_index = ai_mesh->mMaterialIndex;
    mesh.primitive = [&]() -> model3d_data::mesh_primitive {
      switch (ai_mesh->mPrimitiveTypes) {
        case aiPrimitiveType_POINT:
          return model3d_data::MESH_PRIMITIVE_POINT;
        case aiPrimitiveType_LINE:
          return model3d_data::MESH_PRIMITIVE_LINE;
        case aiPrimitiveType_POLYGON:
          return model3d_data::MESH_PRIMITIVE_POLYGON;
        case aiPrimitiveType_TRIANGLE:
          [[fallthrough]];
        default:
          return model3d_data::MESH_PRIMITIVE_TRIANGLE;
      }
    }();
#if 0
    mesh.blend_method = [&]() -> model3d_data::mesh_blend_method {
      switch (ai_mesh->mMethod) {
        case aiMorphingMethod_MORPH_RELATIVE:
          return model3d_data::MESH_BLEND_METHOD_RELATIVE;
        case aiMorphingMethod_MORPH_NORMALIZED:
          return model3d_data::MESH_BLEND_METHOD_NORMALIZED;
        case aiMorphingMethod_VERTEX_BLEND:
          [[fallthrough]];
        default:
          return model3d_data::MESH_BLEND_METHOD_VERTEX_BLEND;
      }
    }();
#endif
  }
}

void parse_materials(model3d_data::model_internal& data, const aiScene& scene,
                     const buffer_path& texture_dir) {
  struct tex_set_data {
    aiString filename;
    texture_type type;
  };

  std::vector<tex_set_data> texture_set;
  static constexpr auto parseable_textures = std::to_array({
    aiTextureType_DIFFUSE,
    aiTextureType_NORMALS,
    aiTextureType_SPECULAR,
    aiTextureType_AMBIENT_OCCLUSION,
  });
  const auto texcast = [](aiTextureType type) -> texture_type {
    switch (type) {
      case aiTextureType_SPECULAR:
        return texture_type::specular;
      case aiTextureType_NORMALS:
        return texture_type::normal;
      case aiTextureType_AMBIENT_OCCLUSION:
        return texture_type::ambient_occlusion;
      case aiTextureType_DIFFUSE:
        [[fallthrough]];
      default:
        return texture_type::diffuse;
    }
  };
  const auto parse_material_textures = [&](const aiMaterial& ai_mat, aiTextureType type) {
    const size_t count = ai_mat.GetTextureCount(type);
    for (size_t i = 0; i < count; ++i) {
      aiString filename;
      ai_mat.GetTexture(type, i, &filename);
      std::string_view filename_view(filename.data, filename.length);
      auto it = std::find_if(texture_set.begin(), texture_set.end(),
                             [&](const auto& a) { return a.filename.C_Str() == filename_view; });
      if (it == texture_set.end()) {
        texture_set.emplace_back(filename, texcast(type));
      }
    }
    return count;
  };

  data.material_count = scene.mNumMaterials;
  if (!data.material_count) {
    return;
  }
  auto& al = data.alloc;
  alloc_init(al, &data.materials, data.material_count);
  data.material_registry.reserve(data.material_count);

  for (size_t i = 0; i < data.material_count; ++i) {
    const aiMaterial* ai_mat = scene.mMaterials[i];
    const aiString mat_name = ai_mat->GetName();
    auto& mat = data.materials[i];
    mat.name.copy_from(mat_name.data, mat_name.length);
    data.material_registry.emplace(mat.name.as_view(), i);
    for (aiTextureType type : parseable_textures) {
      data.material_textures_count += parse_material_textures(*ai_mat, type);
    }
  }
  data.texture_count = texture_set.size(); // Should include external and embeded textures
  if (!data.texture_count) {
    return;
  }
  assert(data.material_textures_count > 0);
  alloc_init(al, &data.textures, data.texture_count);
  alloc_init(al, &data.material_textures, data.material_textures_count);
  for (size_t i = 0; i < data.texture_count; ++i) {
    auto& tex = data.textures[i];
    const auto [filename, type] = texture_set[i];
    std::string_view filename_view(filename.data, filename.length);
    tex.name.copy_from(filename.data, filename.length);
    tex.path.format_from("{}/{}", texture_dir.as_view(), filename_view);
    data.texture_registry.emplace(tex.name.as_view(), i); // Name should always be unique
  }
}

} // namespace

bs_expect<model3d_data, 256> model3d_loader::load() {
  assert(_impl, "model3d_loader use after free");
  const shogle::scope_end defer = [this]() {
    delete _impl;
    _impl = nullptr;
  };
  bs_expect<model3d_data, 256> ret(unexpect);
  const auto assimpflags = [flags = _impl->importer_flags]() -> bits32 {
    bits32 out = 0;
    if (flags & FLAG_TRIANGULATE) {
      out |= aiProcess_Triangulate;
    }
    if (flags & FLAG_EMBED_TEXTURES) {
      out |= aiProcess_EmbedTextures;
    }
    if (flags & FLAG_GEN_TANGENTS) {
      out |= aiProcess_CalcTangentSpace;
    }
    if (flags & FLAG_GEN_UVS) {
      out |= aiProcess_GenUVCoords;
    }
    if (flags & FLAG_GEN_NORMALS) {
      out |= aiProcess_GenNormals;
    }
    return out;
  }();

  try {
    const aiScene* scene = _impl->importer.ReadFile(_impl->model_path.c_str(), assimpflags);
    if (!scene || !scene->mNumMeshes) {
      ret.error().format_from("ASSIMP error: {}", _impl->importer.GetErrorString());
      return ret;
    }

    bone_inv_map bone_invs;
    auto data =
      initialize_data(_impl->model_name, _impl->model_path, *scene, bone_invs, ret.error());
    if (!data) {
      return ret; // initialize_data fills the error string
    }

    parse_rigs(*data, *scene, bone_invs);
    parse_meshes(*data, *scene);
    parse_materials(*data, *scene, _impl->texture_dir);
    // TODO: Parse animations
    ret.emplace(*data.release());
  } catch (const std::bad_alloc&) {
    ret.error().format_from("Model allocation failure");
  } catch (const std::exception& ex) {
    ret.error().format_from("{}", ex.what());
  } catch (...) {
    ret.error().format_from("Unkown error");
  }
  return ret;
}

model3d_data::model_internal::~model_internal() {
#define DEALLOC(ptr, sz) \
  if (ptr)               \
  alloc.dealloc(ptr, sz)

  for (size_t i = 0; i < MAX_MESH_COLORS; ++i) {
    DEALLOC(mesh_colors[i], mesh_color_count[i]);
    DEALLOC(blend_colors[i], blend_color_count[i]);
  }
  for (size_t i = 0; i < MAX_MESH_UVS; ++i) {
    DEALLOC(mesh_uvs[i], mesh_uv_count[i]);
    DEALLOC(blend_uvs[i], blend_uv_count[i]);
  }

  DEALLOC(mesh_bone_indices, mesh_bone_count);
  DEALLOC(mesh_bone_weights, mesh_bone_count);

  DEALLOC(mesh_tangents, mesh_tangent_count);
  DEALLOC(mesh_bitangents, mesh_tangent_count);
  DEALLOC(blend_tangents, blend_tangent_count);
  DEALLOC(blend_bitangents, blend_tangent_count);

  DEALLOC(mesh_normals, mesh_normal_count);
  DEALLOC(blend_normals, blend_normal_count);

  DEALLOC(mesh_positions, mesh_position_count);
  DEALLOC(blend_positions, blend_position_count);

  DEALLOC(meshes, mesh_count);
  DEALLOC(mesh_indices, mesh_index_count);
  DEALLOC(blend_shapes, blend_shape_count);

  DEALLOC(bones, bone_count);
  DEALLOC(bone_locals, bone_count);
  DEALLOC(bone_inv_models, bone_count);

  for (size_t i = 0; i < texture_count; ++i) {
    if (!textures[i].data) {
      continue;
    }
    const size_t stride =
      image_stride(textures[i].extent.width, textures[i].extent.height, textures[i].format);
    switch (textures[i].format) {
      case image_format::rgb8u:
        [[fallthrough]];
      case image_format::rgba8u: {
        DEALLOC((u8*)textures[i].data, stride);
      } break;
      case image_format::rgb16u:
        [[fallthrough]];
      case image_format::rgba16u: {
        DEALLOC((u16*)textures[i].data, stride);
      } break;
      case image_format::rgb32f:
        [[fallthrough]];
      case image_format::rgba32f: {
        DEALLOC((f32*)textures[i].data, stride);
      } break;
    }
  }
  DEALLOC(textures, texture_count);
  DEALLOC(material_textures, material_textures_count);
  DEALLOC(materials, material_count);

#undef DEALLOC
}

model3d_data::model3d_data(model_internal& data) noexcept : _data(&data) {}

void model3d_data::destroy() noexcept {
  if (!_data) {
    return;
  }
  delete _data;
  _data = nullptr;
}

fn collect_model_texture_items(std::vector<model_texture_item>& items, const model3d_data& model)
  -> u32 {
  u32 count = 0;
  for (const auto& mesh : model.meshes()) {
    const auto texes = model.material_textures(mesh.material_index);
    for (const auto& tex : texes) {
      auto it = std::find_if(items.begin(), items.end(), [&](const auto& t) {
        return t.path.as_view() == tex.path.as_view();
      });
      if (it != items.end()) {
        continue;
      }
      items.emplace_back(tex.path, tex.type);
      ++count;
    }
  }
  return count;
}

#define CHECK_DATA assert(_data, "model3d_data use after free");

namespace {

template<typename T>
span<T> datarange(T* data, size_t data_sz, array_range range) {
  assert(range.start != (u32)-1 && range.count);
  return span<T>{data + range.start, std::min((size_t)range.count, data_sz - range.count)};
}

template<typename T>
span<T> datarange(T* data, size_t data_sz) {
  return span<T>{data, data_sz};
}

optional<size_t> find_map_idx(auto&& map, std::string_view name) {
  auto it = map.find(name);
  if (it == map.end()) {
    return nullopt;
  }
  return {in_place, it->second};
}

} // namespace

buffer_name& model3d_data::name() const {
  CHECK_DATA;
  return _data->name;
}

buffer_path& model3d_data::path() const {
  CHECK_DATA;
  return _data->path;
}

model3d_data::mesh_data& model3d_data::mesh_at(size_t idx) const {
  CHECK_DATA;
  assert(idx < _data->mesh_count);
  return _data->meshes[idx];
}

span<model3d_data::mesh_data> model3d_data::meshes() const {
  CHECK_DATA;
  return datarange(_data->meshes, _data->mesh_count);
}

span<v3f32> model3d_data::mesh_positions() const {
  CHECK_DATA;
  return datarange(_data->mesh_positions, _data->mesh_position_count);
}

span<v3f32> model3d_data::mesh_positions(array_range range) const {
  CHECK_DATA;
  return datarange(_data->mesh_positions, _data->mesh_position_count, range);
}

span<v3f32> model3d_data::mesh_normals() const {
  CHECK_DATA;
  return datarange(_data->mesh_normals, _data->mesh_normal_count);
}

span<v3f32> model3d_data::mesh_normals(array_range range) const {
  CHECK_DATA;
  return datarange(_data->mesh_normals, _data->mesh_normal_count, range);
}

span<v2f32> model3d_data::mesh_uvs(size_t idx) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_UVS);
  return datarange(_data->mesh_uvs[idx], _data->mesh_uv_count[idx]);
}

span<v2f32> model3d_data::mesh_uvs(size_t idx, array_range range) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_UVS);
  return datarange(_data->mesh_uvs[idx], _data->mesh_uv_count[idx], range);
}

span<v4f32> model3d_data::mesh_colors(size_t idx) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_COLORS);
  return datarange(_data->mesh_colors[idx], _data->mesh_color_count[idx]);
}

span<v4f32> model3d_data::mesh_colors(size_t idx, array_range range) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_COLORS);
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_colors[idx], _data->mesh_color_count[idx], range);
}

span<v3f32> model3d_data::mesh_tangents() const {
  CHECK_DATA;
  return datarange(_data->mesh_tangents, _data->mesh_tangent_count);
}

span<v3f32> model3d_data::mesh_tangents(array_range range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_tangents, _data->mesh_tangent_count, range);
}

span<v3f32> model3d_data::mesh_bitangents() const {
  CHECK_DATA;
  return datarange(_data->mesh_bitangents, _data->mesh_tangent_count);
}

span<v3f32> model3d_data::mesh_bitangents(array_range range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_bitangents, _data->mesh_tangent_count, range);
}

span<v4i32> model3d_data::mesh_bone_indices() const {
  CHECK_DATA;
  return datarange(_data->mesh_bone_indices, _data->mesh_bone_count);
}

span<v4i32> model3d_data::mesh_bone_indices(array_range range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_bone_indices, _data->mesh_bone_count, range);
}

span<v4f32> model3d_data::mesh_bone_weights() const {
  CHECK_DATA;
  return datarange(_data->mesh_bone_weights, _data->mesh_bone_count);
}

span<v4f32> model3d_data::mesh_bone_weights(array_range range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_bone_weights, _data->mesh_bone_count, range);
}

span<u32> model3d_data::mesh_indices() const {
  CHECK_DATA;
  return datarange(_data->mesh_indices, _data->mesh_index_count);
}

span<u32> model3d_data::mesh_indices(array_range range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_indices, _data->mesh_index_count, range);
}

size_t model3d_data::mesh_count() const {
  CHECK_DATA;
  return _data->mesh_count;
}

model3d_data::blend_shape_data& model3d_data::blend_shape_at(size_t idx) const {
  CHECK_DATA;
  assert(idx < _data->blend_shape_count);
  return _data->blend_shapes[idx];
}

span<model3d_data::blend_shape_data> model3d_data::blend_shapes() const {
  CHECK_DATA;
  return datarange(_data->blend_shapes, _data->blend_shape_count);
}

span<v3f32> model3d_data::blend_positions() const {
  CHECK_DATA;
  return datarange(_data->blend_positions, _data->blend_position_count);
}

span<v3f32> model3d_data::blend_positions(array_range range) const {
  CHECK_DATA;
  return datarange(_data->blend_positions, _data->blend_position_count, range);
}

span<v3f32> model3d_data::blend_normals() const {
  CHECK_DATA;
  return datarange(_data->blend_normals, _data->blend_normal_count);
}

span<v3f32> model3d_data::blend_normals(array_range range) const {
  CHECK_DATA;
  return datarange(_data->blend_normals, _data->blend_normal_count, range);
}

span<v2f32> model3d_data::blend_uvs(size_t idx) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_UVS);
  return datarange(_data->blend_uvs[idx], _data->blend_uv_count[idx]);
}

span<v2f32> model3d_data::blend_uvs(size_t idx, array_range range) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_UVS);
  return datarange(_data->blend_uvs[idx], _data->blend_uv_count[idx], range);
}

span<v4f32> model3d_data::blend_colors(size_t idx) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_COLORS);
  return datarange(_data->blend_colors[idx], _data->blend_color_count[idx]);
}

span<v4f32> model3d_data::blend_colors(size_t idx, array_range range) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_COLORS);
  return datarange(_data->blend_colors[idx], _data->blend_color_count[idx], range);
}

span<v3f32> model3d_data::blend_tangents() const {
  CHECK_DATA;
  return datarange(_data->blend_tangents, _data->blend_tangent_count);
}

span<v3f32> model3d_data::blend_tangents(array_range range) const {
  CHECK_DATA;
  return datarange(_data->blend_tangents, _data->blend_tangent_count, range);
}

span<v3f32> model3d_data::blend_bitangents() const {
  CHECK_DATA;
  return datarange(_data->blend_bitangents, _data->blend_tangent_count);
}

span<v3f32> model3d_data::blend_bitangents(array_range range) const {
  CHECK_DATA;
  return datarange(_data->blend_bitangents, _data->blend_tangent_count, range);
}

size_t model3d_data::blend_shape_count() const {
  CHECK_DATA;
  return _data->blend_shape_count;
}

model3d_data::bone_data& model3d_data::bone_at(size_t idx) const {
  CHECK_DATA;
  assert(idx < _data->bone_count);
  return _data->bones[idx];
}

span<model3d_data::bone_data> model3d_data::bones() const {
  CHECK_DATA;
  return datarange(_data->bones, _data->bone_count);
}

span<model3d_data::bone_data> model3d_data::bones(array_range range) const {
  CHECK_DATA;
  return datarange(_data->bones, _data->bone_count, range);
}

span<m4f32> model3d_data::bone_locals() const {
  CHECK_DATA;
  return datarange(_data->bone_locals, _data->bone_count);
}

span<m4f32> model3d_data::bone_locals(array_range range) const {
  CHECK_DATA;
  return datarange(_data->bone_locals, _data->bone_count, range);
}

span<m4f32> model3d_data::bone_inverse_models() const {
  CHECK_DATA;
  return datarange(_data->bone_inv_models, _data->bone_count);
}

span<m4f32> model3d_data::bone_inverse_models(array_range range) const {
  CHECK_DATA;
  return datarange(_data->bone_inv_models, _data->bone_count, range);
}

size_t model3d_data::bone_count() const {
  CHECK_DATA;
  return _data->bone_count;
}

model3d_data::material_data& model3d_data::material_at(size_t idx) const {
  CHECK_DATA;
  assert(idx < _data->material_count);
  return _data->materials[idx];
}

span<model3d_data::material_data> model3d_data::materials() const {
  CHECK_DATA;
  return datarange(_data->materials, _data->material_count);
}

size_t model3d_data::material_count() const {
  CHECK_DATA;
  return _data->material_count;
}

model3d_data::texture_data& model3d_data::texture_at(size_t idx) const {
  CHECK_DATA;
  assert(idx < _data->texture_count);
  return _data->textures[idx];
}

span<model3d_data::texture_data> model3d_data::textures() const {
  CHECK_DATA;
  return datarange(_data->textures, _data->texture_count);
}

size_t model3d_data::texture_count() const {
  CHECK_DATA;
  return _data->texture_count;
}

span<model3d_data::texture_data> model3d_data::material_textures(size_t idx) const {
  CHECK_DATA;
  assert(idx < _data->material_count);
  auto& material = _data->materials[idx];
  if (material.texture_indices.start == (u32)-1 || !material.texture_indices.count) {
    return span<texture_data>{}; // empty span
  }
  return datarange(_data->textures, _data->texture_count, material.texture_indices);
}

optional<size_t> model3d_data::find_mesh_idx(std::string_view mesh_name) const {
  CHECK_DATA;
  return find_map_idx(_data->mesh_registry, mesh_name);
}

optional<size_t> model3d_data::find_bone_idx(std::string_view bone_name) const {
  CHECK_DATA;
  return find_map_idx(_data->bone_registry, bone_name);
}

optional<size_t> model3d_data::find_material_idx(std::string_view material_name) const {
  CHECK_DATA;
  return find_map_idx(_data->material_registry, material_name);
}

optional<size_t> model3d_data::find_texture_idx(std::string_view texture_name) const {
  CHECK_DATA;
  return find_map_idx(_data->texture_registry, texture_name);
}

} // namespace kappa::assets
