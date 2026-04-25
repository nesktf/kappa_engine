#include "./internal.hpp"

#include "../util/function.hpp"
#include "../util/logger.hpp"

#define MODEL_LOG(_level, _fmt, ...) \
  ::kappa::log_##_level("[MODEL_IMPORT] " _fmt __VA_OPT__(, ) __VA_ARGS__)

namespace kappa::assets {

namespace {

template<typename T>
constexpr void zeroinit(T* ptr, usize sz) {
  std::memset(ptr, 0x00, sizeof(T) * sz);
};

template<typename T>
void alloc_init(model_allocator& al, T** ptr, usize sz) {
  *ptr = al.alloc<T>(sz);
  zeroinit(*ptr, sz);
}

} // namespace

Model3DLoader::Model3DLoader(std::string_view model_path, std::string_view model_name,
                             const LoadOpts* opts) : _impl(new Model3DLoader::LoaderInternal()) {
  _impl->importer.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, true);
  _impl->importer.SetPropertyInteger(AI_CONFIG_PP_SBBC_MAX_BONES, 4);
  _impl->model_path.copy_from(model_path.data(), model_path.size());
  _impl->model_name.copy_from(model_name.data(), model_name.size());
  _impl->importer_flags = opts ? opts->flags : FLAGS_DEFAULT;
  if (opts && !opts->texture_dir.empty()) {
    _impl->texture_dir.copy_from(opts->texture_dir.data(), opts->texture_dir.size());
  } else {
    auto slash_pos = model_path.find_last_of('/');
    ka_assert(slash_pos != std::string::npos, "Non valid path provided");
    auto model_file_dir = model_path.substr(0, slash_pos);
    _impl->texture_dir.copy_from(model_file_dir.data(), model_file_dir.size());
  }
}

Model3DData::ModelInternal::ModelInternal(const BufferName& name_, const BufferPath& path_) :
    chima(), meshes(nullptr), mesh_count(0), mesh_positions(nullptr), mesh_position_count(0),
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

ran::Mat4f32 asscast(const aiMatrix4x4& mat) {
  // Assimp is row major. The a,b,c,d is the row and the 1,2,3,4 is the column.
  return ran::transpose(std::bit_cast<ran::Mat4f32>(mat)); // Just transpose it lol
}

using bone_inv_map = std::unordered_map<std::string_view, ran::Mat4f32>;

auto initialize_data(const BufferName& name, const BufferPath& path, const aiScene& scene,
                     bone_inv_map& bone_invs, BuffStr<256>& err)
  -> std::unique_ptr<Model3DData::ModelInternal> {
  if (!scene.mNumMeshes) {
    err.format_from("No meshes in scene");
    return nullptr;
  }

  auto ptr = std::make_unique<Model3DData::ModelInternal>(name, path);
  auto& data = *ptr;
  auto& al = ptr->alloc;
  const auto maybe_alloc = [&]<typename T>(T** ptr, usize sz) {
    if (sz) {
      alloc_init(al, ptr, sz);
    }
  };

  // First mesh pass. Preallocate blend shapes, bones & meshes.
  for (usize i = 0; i < scene.mNumMeshes; ++i) {
    const aiMesh* mesh = scene.mMeshes[i];
    data.blend_shape_count += mesh->mNumAnimMeshes;
    ++data.mesh_count;

    // Store the inverse model matrix for each bone (if any)
    for (usize j = 0; j < mesh->mNumBones; ++j) {
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
  data.meshes = al.alloc<Model3DData::MeshData>(data.mesh_count);
  zeroinit(data.meshes, data.mesh_count);
  maybe_alloc(&data.blend_shapes, data.blend_shape_count);

  static_assert(Model3DData::MAX_MESH_UVS <= AI_MAX_NUMBER_OF_TEXTURECOORDS);
  static_assert(Model3DData::MAX_MESH_COLORS <= AI_MAX_NUMBER_OF_COLOR_SETS);
  // Second mesh pass. Count meshes and blend shapes for each mesh,
  // copy names & preallocate vertices
  {
    usize anim_pos = 0;
    for (usize i = 0; i < scene.mNumMeshes; ++i) {
      const aiMesh* mesh = scene.mMeshes[i];
      const usize verts = mesh->mNumVertices;
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
      for (usize uv = 0; uv < Model3DData::MAX_MESH_UVS; ++uv) {
        if (mesh->HasTextureCoords(uv)) {
          data.mesh_uv_count[uv] += verts;
        }
        if (mesh->HasTextureCoordsName(uv)) {
          // Copy name
          const aiString& name = *mesh->mTextureCoordsNames[uv];
          data.meshes[i].uv_name[uv].copy_from(name.data, name.length);
        }
      }
      for (usize col = 0; col < Model3DData::MAX_MESH_COLORS; ++col) {
        if (mesh->HasVertexColors(col)) {
          data.mesh_color_count[col] += verts;
        }
      }
      if (mesh->HasBones()) {
        data.mesh_bone_count += verts;
      }
      for (usize j = 0; j < mesh->mNumFaces; ++j) {
        data.mesh_index_count += mesh->mFaces[j].mNumIndices;
      }

      for (usize j = 0; j < mesh->mNumAnimMeshes; ++j) {
        const aiAnimMesh* ai_anim = mesh->mAnimMeshes[j];
        const usize animverts = ai_anim->mNumVertices;
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
        for (usize uv = 0; uv < Model3DData::MAX_MESH_UVS; ++uv) {
          if (ai_anim->HasTextureCoords(uv)) {
            data.blend_uv_count[uv] += animverts;
          }
        }
        for (usize col = 0; col < Model3DData::MAX_MESH_COLORS; ++col) {
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
      data.mesh_bone_indices = al.alloc<ran::Vec4s32>(data.mesh_bone_count);
      data.mesh_bone_weights = al.alloc<ran::Vec4f32>(data.mesh_bone_count);
      // Set all parents to NULL (-1)
      std::memset(data.mesh_bone_indices, 0xFF,
                  sizeof(data.mesh_bone_indices[0]) * data.mesh_bone_count);
      // Set all weights to 0
      std::memset(data.mesh_bone_weights, 0x00,
                  sizeof(data.mesh_bone_weights[0] * data.mesh_bone_count));
    }

    for (usize uv = 0; uv < Model3DData::MAX_MESH_UVS; ++uv) {
      maybe_alloc(data.mesh_uvs + uv, data.mesh_uv_count[uv]);
      maybe_alloc(data.blend_uvs + uv, data.blend_uv_count[uv]);
    }
    for (usize col = 0; col < Model3DData::MAX_MESH_COLORS; ++col) {
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
    for (usize i = 0; i < scene.mNumAnimations; ++i) {
      const aiAnimation* ai_anim = scene.mAnimations[i];
      const aiString& anim_name = ai_anim->mName;
      auto& anim = data.animations[i];
      anim.name.copy_from(anim_name.data, anim_name.length);
      auto [_, empl] = data.bone_anim_registry.emplace(anim.name.as_view(), i);
      if (!empl) {
        err.format_from("Duplicate animation name \"{}\" in model", anim.name.as_view());
        return nullptr;
      }

      for (usize j = 0; j < ai_anim->mNumChannels; ++j) {
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
    data.bones = al.alloc<Model3DData::BoneData>(data.bone_count);
    zeroinit(data.bones, data.bone_count);
    data.bone_inv_models = al.alloc<ran::Mat4f32>(data.bone_count);
    data.bone_locals = al.alloc<ran::Mat4f32>(data.bone_count);
    data.bone_registry.reserve(data.bone_count); // We fill the registry at parse_bones()
  }

  return ptr;
}

bool is_identity(const ran::Mat4f32& mat) {
  static constexpr ran::Mat4f32 id(1.f);
  for (usize i = 0; i < 4; ++i) {
    const auto col_a = mat.column_at(i);
    const auto col_b = id.column_at(i);
    for (usize j = 0; j < 4; ++j) {
      if (std::abs(col_a[j] - col_b[j]) > ran::epsilon<f32>) {
        return false;
      }
    }
  }
  return true;
}

void parse_bone_nodes(const bone_inv_map& bone_invs, s32 parent, s32& bone_count,
                      const aiNode* node, Model3DData::ModelInternal& data) {
  assert(node);
  std::string_view name(node->mName.data, node->mName.length);
  const auto& inv_model = bone_invs.at(name);
  const s32 this_bone = bone_count++;

  // Store meta info and transforms
  data.bones[this_bone].name.copy_from(name.data(), name.size());
  data.bones[this_bone].parent = parent;
  data.bone_locals[this_bone] = asscast(node->mTransformation);
  data.bone_inv_models[this_bone] = inv_model;

  // Add bone to the registry. At this point we should never have duplicate bone names.
  data.bone_registry.emplace(data.bones[this_bone].name.as_view(), this_bone);

  // Parse children
  for (usize i = 0; i < node->mNumChildren; ++i) {
    parse_bone_nodes(bone_invs, this_bone, bone_count, node->mChildren[i], data);
  }
}

ran::Mat4f32 node_model(const aiNode* node) {
  if (node->mParent == nullptr) {
    return asscast(node->mTransformation);
  }
  return node_model(node->mParent) * asscast(node->mTransformation);
}

void parse_rigs(Model3DData::ModelInternal& data, const aiScene& scene,
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

  s32 bone_count = 0;
  // Will set -1 as the parent index for the root bone
  parse_bone_nodes(bone_invs, -1, bone_count, root_bone_node, data);

  // Make sure the root local transform is its node model transform
  if (!is_identity(data.bone_locals[0] * data.bone_inv_models[0])) {
    MODEL_LOG(warn, "Malformed transform in bone root at model \"{}\", correction applied",
              data.path.as_view());
    data.bone_locals[0] = node_model(root_bone_node->mParent) * data.bone_locals[0];
  }
}

ran::Vec3f32 asscast(const aiVector3D& vec) {
  return {vec.x, vec.y, vec.z};
}

ran::Vec4f32 asscast(const aiColor4D& col) {
  return {col.r, col.g, col.b, col.a};
}

void parse_meshes(Model3DData::ModelInternal& data, const aiScene& scene) {
  usize vertex_pos = 0, vertex_anim_pos = 0;
  usize normal_pos = 0, normal_anim_pos = 0;
  usize tangent_pos = 0, tangent_anim_pos = 0;
  usize bone_pos = 0;
  usize uv_pos[Model3DData::MAX_MESH_UVS] = {0};
  usize uv_anim_pos[Model3DData::MAX_MESH_UVS] = {0};
  usize color_pos[Model3DData::MAX_MESH_COLORS] = {0};
  usize color_anim_pos[Model3DData::MAX_MESH_COLORS] = {0};
  usize index_pos = 0;
  usize shape_pos = 0;

  const auto try_place_weight = [&](s32 bone_idx, const aiVertexWeight& weight) {
    const usize offset = bone_pos + weight.mVertexId;
    assert(offset < data.mesh_bone_count);

    for (usize i = 0; i < 4; ++i) {
      if (weight.mWeight == 0.f) {
        return;
      }
      if (data.mesh_bone_indices[offset][i] == -1) {
        data.mesh_bone_indices[offset][i] = bone_idx;
        data.mesh_bone_weights[offset][i] = weight.mWeight;
        return;
      }
    }
    MODEL_LOG(warn, "Bone weights out of range in vertex index {}", vertex_pos);
  };

  const auto do_attrib = [&](bool cond, usize count, usize& pos, u32& target_start, auto&& f) {
    if (cond) {
      for (usize i = 0; i < count; ++i) {
        f(pos + i, i);
      }
      target_start = static_cast<u32>(pos);
      pos += count;
    } else {
      target_start = static_cast<u32>(-1);
    }
  };

  const auto& bone_reg = data.bone_registry;
  for (usize mesh_idx = 0; mesh_idx < scene.mNumMeshes; ++mesh_idx) {
    const aiMesh* ai_mesh = scene.mMeshes[mesh_idx];
    auto& mesh = data.meshes[mesh_idx];
    mesh.nverts = ai_mesh->mNumVertices;

    // Positions. Always present but use HasPositions for consistency.
    do_attrib(ai_mesh->HasPositions(), mesh.nverts, vertex_pos, mesh.positions_start,
              [&](usize pos, usize vert) {
                assert(pos < data.mesh_position_count);
                assert(vert < ai_mesh->mNumVertices);
                data.mesh_positions[pos] = asscast(ai_mesh->mVertices[vert]);
              });

    // Normals
    do_attrib(ai_mesh->HasNormals(), mesh.nverts, normal_pos, mesh.normals_start,
              [&](usize pos, usize vert) {
                assert(pos < data.mesh_normal_count);
                assert(vert < ai_mesh->mNumVertices);
                data.mesh_normals[pos] = asscast(ai_mesh->mNormals[vert]);
              });

    // Tangents & bitangents
    do_attrib(ai_mesh->HasTangentsAndBitangents(), mesh.nverts, tangent_pos, mesh.tangents_start,
              [&](usize pos, usize vert) {
                assert(pos < data.mesh_tangent_count);
                assert(vert < ai_mesh->mNumVertices);
                data.mesh_tangents[pos] = asscast(ai_mesh->mTangents[vert]);
                data.mesh_bitangents[pos] = asscast(ai_mesh->mBitangents[vert]);
              });

    // UVs
    for (usize uv = 0; uv < Model3DData::MAX_MESH_UVS; ++uv) {
      do_attrib(ai_mesh->HasTextureCoords(uv), mesh.nverts, uv_pos[uv], mesh.uvs_start[uv],
                [&](usize pos, usize vert) {
                  assert(pos < data.mesh_uv_count[uv]);
                  assert(vert < ai_mesh->mNumVertices);
                  const auto& uv_vec = ai_mesh->mTextureCoords[uv][vert];
                  data.mesh_uvs[uv][pos].x = uv_vec.x;
                  data.mesh_uvs[uv][pos].y = uv_vec.y;
                });
    };

    // Colors
    for (usize col = 0; col < Model3DData::MAX_MESH_COLORS; ++col) {
      do_attrib(ai_mesh->HasVertexColors(col), mesh.nverts, color_pos[col], mesh.colors_start[col],
                [&](usize pos, usize vert) {
                  assert(pos < data.mesh_color_count[col]);
                  assert(vert < ai_mesh->mNumVertices);
                  data.mesh_colors[col][pos] = asscast(ai_mesh->mColors[col][vert]);
                });
    }

    // Bone indices & weights
    if (!bone_reg.empty() && ai_mesh->HasBones()) {
      // Special iteration here, can't use do_attrib directly
      for (usize bone = 0; bone < ai_mesh->mNumBones; ++bone) {
        const aiBone* ai_bone = ai_mesh->mBones[bone];
        std::string_view bone_name(ai_bone->mName.data, ai_bone->mName.length);
        // At this point every name *should* be valid
        auto it = bone_reg.find(bone_name);
        if (it == bone_reg.end()) {
          MODEL_LOG(error, "Bone out of hierarchy \"{}\"", bone_name);
          continue;
        }
        const s32 bone_idx = static_cast<s32>(it->second);
        for (usize weight = 0; weight < ai_bone->mNumWeights; ++weight) {
          try_place_weight(bone_idx, ai_bone->mWeights[weight]);
        }
      }
      mesh.bones_start = static_cast<u32>(bone_pos);
      bone_pos += mesh.nverts;
    } else {
      mesh.bones_start = static_cast<s32>(-1);
    }

    // Face indices
    {
      mesh.face_count = ai_mesh->mNumFaces;
      u32 index_count = 0u;
      for (usize face_idx = 0; face_idx < ai_mesh->mNumFaces; ++face_idx) {
        const aiFace& face = ai_mesh->mFaces[face_idx];
        for (usize i = 0; i < face.mNumIndices; ++i) {
          const usize pos = index_pos + index_count + i;
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
    for (usize j = 0; j < ai_mesh->mNumAnimMeshes; ++j) {
      const aiAnimMesh* ai_anim = ai_mesh->mAnimMeshes[j];
      auto& anim = data.blend_shapes[shape_pos++];
      anim.nverts = ai_anim->mNumVertices;
      anim.weight = ai_anim->mWeight;

      // Positions (can be absent this time)
      do_attrib(ai_anim->HasPositions(), anim.nverts, vertex_anim_pos, anim.positions_start,
                [&](usize pos, usize vert) {
                  assert(pos < data.blend_position_count);
                  assert(vert < ai_anim->mNumVertices);
                  data.blend_positions[pos] = asscast(ai_anim->mVertices[vert]);
                });

      // Normals
      do_attrib(ai_anim->HasNormals(), anim.nverts, normal_anim_pos, anim.normals_start,
                [&](usize pos, usize vert) {
                  assert(pos < data.blend_normal_count);
                  assert(vert < ai_anim->mNumVertices);
                  data.blend_normals[pos] = asscast(ai_anim->mNormals[vert]);
                });

      // Tangents & Bitangents
      do_attrib(ai_anim->HasTangentsAndBitangents(), anim.nverts, tangent_anim_pos,
                anim.tangents_start, [&](usize pos, usize vert) {
                  assert(pos < data.blend_tangent_count);
                  assert(vert < ai_anim->mNumVertices);
                  data.blend_tangents[pos] = asscast(ai_anim->mTangents[vert]);
                  data.blend_bitangents[pos] = asscast(ai_anim->mBitangents[vert]);
                });

      // UVs
      for (usize uv = 0; uv < Model3DData::MAX_MESH_UVS; ++uv) {
        do_attrib(ai_anim->HasTextureCoords(uv), anim.nverts, uv_anim_pos[uv], anim.uvs_start[uv],
                  [&](usize pos, usize vert) {
                    assert(pos < data.blend_uv_count[uv]);
                    assert(vert < ai_anim->mNumVertices);
                    const auto& uv_vec = ai_anim->mTextureCoords[uv][vert];
                    data.blend_uvs[uv][pos].x = uv_vec.x;
                    data.blend_uvs[uv][pos].y = uv_vec.y;
                  });
      }

      // Colors
      for (usize col = 0; col < Model3DData::MAX_MESH_COLORS; ++col) {
        do_attrib(ai_anim->HasVertexColors(col), anim.nverts, color_anim_pos[col],
                  anim.colors_start[col], [&](usize pos, usize vert) {
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
    mesh.primitive = [&]() -> Model3DData::MeshPrimitive {
      switch (ai_mesh->mPrimitiveTypes) {
        case aiPrimitiveType_POINT:
          return Model3DData::MESH_PRIMITIVE_POINT;
        case aiPrimitiveType_LINE:
          return Model3DData::MESH_PRIMITIVE_LINE;
        case aiPrimitiveType_POLYGON:
          return Model3DData::MESH_PRIMITIVE_POLYGON;
        case aiPrimitiveType_TRIANGLE:
          [[fallthrough]];
        default:
          return Model3DData::MESH_PRIMITIVE_TRIANGLE;
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

bool parse_materials(Model3DData::ModelInternal& data, const aiScene& scene,
                     const BufferPath& texture_dir, BuffStr<256>& err) {
  struct tex_set_data {
    aiString filename;
    TextureType type;
  };

  std::vector<tex_set_data> texture_set;
  std::vector<u32> material_textures;
  static constexpr auto parseable_textures = std::to_array({
    aiTextureType_DIFFUSE,
    aiTextureType_NORMALS,
    aiTextureType_SPECULAR,
    aiTextureType_AMBIENT_OCCLUSION,
  });
  const auto texcast = [](aiTextureType type) -> TextureType {
    switch (type) {
      case aiTextureType_SPECULAR:
        return TextureType::specular;
      case aiTextureType_NORMALS:
        return TextureType::normal;
      case aiTextureType_AMBIENT_OCCLUSION:
        return TextureType::ambient_occlusion;
      case aiTextureType_DIFFUSE:
        [[fallthrough]];
      default:
        return TextureType::diffuse;
    }
  };
  const auto parse_material_textures = [&](const aiMaterial& ai_mat, aiTextureType type) {
    const usize count = ai_mat.GetTextureCount(type);
    for (usize i = 0; i < count; ++i) {
      aiString filename;
      ai_mat.GetTexture(type, i, &filename);
      std::string_view filename_view(filename.data, filename.length);
      auto it = std::find_if(texture_set.begin(), texture_set.end(),
                             [&](const auto& a) { return a.filename.C_Str() == filename_view; });
      if (it != texture_set.end()) {
        u32 tex_pos = (u32)std::distance(texture_set.begin(), it);
        material_textures.emplace_back(tex_pos);
      } else {
        u32 tex_pos = (u32)texture_set.size();
        texture_set.emplace_back(filename, texcast(type));
        material_textures.emplace_back(tex_pos);
      }
    }
    return count;
  };

  data.material_count = scene.mNumMaterials;
  if (!data.material_count) {
    err.format_from("No materials in model");
    return false;
  }
  auto& al = data.alloc;
  alloc_init(al, &data.materials, data.material_count);
  data.material_registry.reserve(data.material_count);

  for (usize i = 0; i < data.material_count; ++i) {
    const aiMaterial* ai_mat = scene.mMaterials[i];
    const aiString mat_name = ai_mat->GetName();
    auto& mat = data.materials[i];
    mat.name.copy_from(mat_name.data, mat_name.length);
    data.material_registry.emplace(mat.name.as_view(), i);

    u32 material_texture_count = 0;
    for (aiTextureType type : parseable_textures) {
      material_texture_count += parse_material_textures(*ai_mat, type);
    }
    if (material_texture_count) {
      mat.texture_indices.count = material_texture_count;
      mat.texture_indices.start = ((u32)material_textures.size()) - material_texture_count;
    } else {
      mat.texture_indices = ArrayRange::null_range();
    }
  }
  data.texture_count = texture_set.size(); // Should include external and embeded textures
  if (!data.texture_count) {
    return true;
  }
  // Load images
  alloc_init(al, &data.textures, data.texture_count);
  alloc_init(al, &data.texture_images, data.texture_count);
  usize image_idx = 0;
  DeferFn image_err_cleanup = [&]() {
    for (usize i = 0; i < image_idx; ++i) {
      chima::image::destroy(data.chima, data.texture_images[i]);
    }
  };

  static constexpr chima_image_depth image_depth = CHIMA_DEPTH_8U;
  chima::error chimaerr;
  for (; image_idx < data.texture_count; ++image_idx) {
    auto& tex = data.textures[image_idx];
    auto& img = data.texture_images[image_idx];
    const auto [filename, type] = texture_set[image_idx];
    std::string_view filename_view(filename.data, filename.length);
    tex.name.copy_from(filename.data, filename.length);
    tex.path.format_from("{}/{}", texture_dir.as_view(), filename_view);
    data.texture_registry.emplace(tex.name.as_view(), image_idx); // Name should always be unique
    auto image = chima::image::load(data.chima, image_depth, tex.path.c_str(), &chimaerr);
    if (!image) {
      err.format_from("Failed to load image at \"{}\", {}", tex.path.as_view(), chimaerr.what());
      MODEL_LOG(error, "{}", err.as_view());
      return false;
    }
    img = *image;
    tex.data = img.data();
    const auto [w, h] = img.extent();
    tex.extent.width = w;
    tex.extent.height = h;
    tex.format = parse_chima_format(image_depth, img.channels());
    tex.type = texture_set[image_idx].type;
  }
  image_err_cleanup.disengage();

  // Copy material -> texture map
  alloc_init(al, &data.material_textures, material_textures.size());
  data.material_textures_count = material_textures.size();
  std::memcpy(data.material_textures, material_textures.data(),
              material_textures.size() * sizeof(material_textures[0]));
  return true;
}

} // namespace

AssExpect<Model3DData> Model3DLoader::load() {
  ka_assert(_impl, "model3d_loader use after free");
  const DeferFn defer = [this]() {
    delete _impl;
    _impl = nullptr;
  };
  const auto assimpflags = [flags = _impl->importer_flags]() -> bits32 {
    bits32 out = 0;
    if (flags & FLAG_TRIANGULATE) {
      out |= aiProcess_Triangulate;
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

  BuffStr<256> err;
  const fn unex = [&]() -> AssExpect<Model3DData> {
    return {unexpect,
            AssetErr(_impl->model_path.as_view(), _impl->model_name.as_view(), std::move(err))};
  };

  try {
    const aiScene* scene = _impl->importer.ReadFile(_impl->model_path.c_str(), assimpflags);
    if (!scene || !scene->mNumMeshes) {
      err.format_from("ASSIMP error: {}", _impl->importer.GetErrorString());
      MODEL_LOG(error, "{}", err.as_view());
      return unex();
    }

    bone_inv_map bone_invs;
    auto data = initialize_data(_impl->model_name, _impl->model_path, *scene, bone_invs, err);
    if (!data) {
      return unex();
    }

    parse_rigs(*data, *scene, bone_invs);
    parse_meshes(*data, *scene);
    if (!parse_materials(*data, *scene, _impl->texture_dir, err)) {
      return unex();
    }
    // TODO: Parse animations
    return {in_place, *data.release()};
  } catch (const std::bad_alloc&) {
    err.format_from("Model allocation failure");
  } catch (const std::exception& ex) {
    err.format_from("{}", ex.what());
  } catch (...) {
    err.format_from("Unkown error");
  }
  return unex();
}

Model3DData::ModelInternal::~ModelInternal() {
#define DEALLOC(ptr, sz) \
  if (ptr)               \
  alloc.dealloc(ptr, sz)

  for (usize i = 0; i < MAX_MESH_COLORS; ++i) {
    DEALLOC(mesh_colors[i], mesh_color_count[i]);
    DEALLOC(blend_colors[i], blend_color_count[i]);
  }
  for (usize i = 0; i < MAX_MESH_UVS; ++i) {
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

  for (usize i = 0; i < texture_count; ++i) {
    chima::image::destroy(chima, texture_images[i]);
  }
  DEALLOC(textures, texture_count);
  DEALLOC(texture_images, texture_count);
  DEALLOC(material_textures, material_textures_count);
  DEALLOC(materials, material_count);

#undef DEALLOC
}

Model3DData::Model3DData(ModelInternal& data) noexcept : _data(&data) {}

void Model3DData::destroy() noexcept {
  if (!_data) {
    return;
  }
  delete _data;
  _data = nullptr;
}

#define CHECK_DATA ka_assert(_data, "model3d_data use after free");

namespace {

usize cap_range(usize max, usize count) {
  return std::min(count, max - count);
}

template<typename T>
Span<T> datarange(T* data, usize data_sz, ArrayRange range) {
  if (range.start == (u32)-1) {
    return Span<T>{};
  } else {
    assert(range.start != (u32)-1 && range.count);
    return Span<T>{data + range.start, cap_range(data_sz, range.count)};
  }
}

template<typename T>
Span<T> datarange(T* data, usize data_sz) {
  return Span<T>{data, data_sz};
}

Optional<usize> find_map_idx(auto&& map, std::string_view name) {
  auto it = map.find(name);
  if (it == map.end()) {
    return nullopt;
  }
  return {in_place, it->second};
}

} // namespace

BufferName& Model3DData::name() const {
  CHECK_DATA;
  return _data->name;
}

BufferPath& Model3DData::path() const {
  CHECK_DATA;
  return _data->path;
}

Model3DData::MeshData& Model3DData::mesh_at(usize idx) const {
  CHECK_DATA;
  assert(idx < _data->mesh_count);
  return _data->meshes[idx];
}

Span<Model3DData::MeshData> Model3DData::meshes() const {
  CHECK_DATA;
  return datarange(_data->meshes, _data->mesh_count);
}

Span<ran::Vec3f32> Model3DData::mesh_positions() const {
  CHECK_DATA;
  return datarange(_data->mesh_positions, _data->mesh_position_count);
}

Span<ran::Vec3f32> Model3DData::mesh_positions(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->mesh_positions, _data->mesh_position_count, range);
}

Span<ran::Vec3f32> Model3DData::mesh_normals() const {
  CHECK_DATA;
  return datarange(_data->mesh_normals, _data->mesh_normal_count);
}

Span<ran::Vec3f32> Model3DData::mesh_normals(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->mesh_normals, _data->mesh_normal_count, range);
}

Span<ran::Vec2f32> Model3DData::mesh_uvs(usize idx) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_UVS);
  return datarange(_data->mesh_uvs[idx], _data->mesh_uv_count[idx]);
}

Span<ran::Vec2f32> Model3DData::mesh_uvs(usize idx, ArrayRange range) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_UVS);
  return datarange(_data->mesh_uvs[idx], _data->mesh_uv_count[idx], range);
}

Span<ran::Vec4f32> Model3DData::mesh_colors(usize idx) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_COLORS);
  return datarange(_data->mesh_colors[idx], _data->mesh_color_count[idx]);
}

Span<ran::Vec4f32> Model3DData::mesh_colors(usize idx, ArrayRange range) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_COLORS);
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_colors[idx], _data->mesh_color_count[idx], range);
}

Span<ran::Vec3f32> Model3DData::mesh_tangents() const {
  CHECK_DATA;
  return datarange(_data->mesh_tangents, _data->mesh_tangent_count);
}

Span<ran::Vec3f32> Model3DData::mesh_tangents(ArrayRange range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_tangents, _data->mesh_tangent_count, range);
}

Span<ran::Vec3f32> Model3DData::mesh_bitangents() const {
  CHECK_DATA;
  return datarange(_data->mesh_bitangents, _data->mesh_tangent_count);
}

Span<ran::Vec3f32> Model3DData::mesh_bitangents(ArrayRange range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_bitangents, _data->mesh_tangent_count, range);
}

Span<ran::Vec4s32> Model3DData::mesh_bone_indices() const {
  CHECK_DATA;
  return datarange(_data->mesh_bone_indices, _data->mesh_bone_count);
}

Span<ran::Vec4s32> Model3DData::mesh_bone_indices(ArrayRange range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_bone_indices, _data->mesh_bone_count, range);
}

Span<ran::Vec4f32> Model3DData::mesh_bone_weights() const {
  CHECK_DATA;
  return datarange(_data->mesh_bone_weights, _data->mesh_bone_count);
}

Span<ran::Vec4f32> Model3DData::mesh_bone_weights(ArrayRange range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_bone_weights, _data->mesh_bone_count, range);
}

Span<u32> Model3DData::mesh_indices() const {
  CHECK_DATA;
  return datarange(_data->mesh_indices, _data->mesh_index_count);
}

Span<u32> Model3DData::mesh_indices(ArrayRange range) const {
  CHECK_DATA;
  assert(range.start != (u32)-1 && range.count);
  return datarange(_data->mesh_indices, _data->mesh_index_count, range);
}

usize Model3DData::mesh_count() const {
  CHECK_DATA;
  return _data->mesh_count;
}

Model3DData::BlendShapeData& Model3DData::blend_shape_at(usize idx) const {
  CHECK_DATA;
  assert(idx < _data->blend_shape_count);
  return _data->blend_shapes[idx];
}

Span<Model3DData::BlendShapeData> Model3DData::blend_shapes() const {
  CHECK_DATA;
  return datarange(_data->blend_shapes, _data->blend_shape_count);
}

Span<ran::Vec3f32> Model3DData::blend_positions() const {
  CHECK_DATA;
  return datarange(_data->blend_positions, _data->blend_position_count);
}

Span<ran::Vec3f32> Model3DData::blend_positions(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->blend_positions, _data->blend_position_count, range);
}

Span<ran::Vec3f32> Model3DData::blend_normals() const {
  CHECK_DATA;
  return datarange(_data->blend_normals, _data->blend_normal_count);
}

Span<ran::Vec3f32> Model3DData::blend_normals(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->blend_normals, _data->blend_normal_count, range);
}

Span<ran::Vec2f32> Model3DData::blend_uvs(usize idx) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_UVS);
  return datarange(_data->blend_uvs[idx], _data->blend_uv_count[idx]);
}

Span<ran::Vec2f32> Model3DData::blend_uvs(usize idx, ArrayRange range) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_UVS);
  return datarange(_data->blend_uvs[idx], _data->blend_uv_count[idx], range);
}

Span<ran::Vec4f32> Model3DData::blend_colors(usize idx) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_COLORS);
  return datarange(_data->blend_colors[idx], _data->blend_color_count[idx]);
}

Span<ran::Vec4f32> Model3DData::blend_colors(usize idx, ArrayRange range) const {
  CHECK_DATA;
  assert(idx < MAX_MESH_COLORS);
  return datarange(_data->blend_colors[idx], _data->blend_color_count[idx], range);
}

Span<ran::Vec3f32> Model3DData::blend_tangents() const {
  CHECK_DATA;
  return datarange(_data->blend_tangents, _data->blend_tangent_count);
}

Span<ran::Vec3f32> Model3DData::blend_tangents(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->blend_tangents, _data->blend_tangent_count, range);
}

Span<ran::Vec3f32> Model3DData::blend_bitangents() const {
  CHECK_DATA;
  return datarange(_data->blend_bitangents, _data->blend_tangent_count);
}

Span<ran::Vec3f32> Model3DData::blend_bitangents(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->blend_bitangents, _data->blend_tangent_count, range);
}

usize Model3DData::blend_shape_count() const {
  CHECK_DATA;
  return _data->blend_shape_count;
}

Model3DData::BoneData& Model3DData::bone_at(usize idx) const {
  CHECK_DATA;
  assert(idx < _data->bone_count);
  return _data->bones[idx];
}

Span<Model3DData::BoneData> Model3DData::bones() const {
  CHECK_DATA;
  return datarange(_data->bones, _data->bone_count);
}

Span<Model3DData::BoneData> Model3DData::bones(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->bones, _data->bone_count, range);
}

Span<ran::Mat4f32> Model3DData::bone_locals() const {
  CHECK_DATA;
  return datarange(_data->bone_locals, _data->bone_count);
}

Span<ran::Mat4f32> Model3DData::bone_locals(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->bone_locals, _data->bone_count, range);
}

Span<ran::Mat4f32> Model3DData::bone_inverse_models() const {
  CHECK_DATA;
  return datarange(_data->bone_inv_models, _data->bone_count);
}

Span<ran::Mat4f32> Model3DData::bone_inverse_models(ArrayRange range) const {
  CHECK_DATA;
  return datarange(_data->bone_inv_models, _data->bone_count, range);
}

usize Model3DData::bone_count() const {
  CHECK_DATA;
  return _data->bone_count;
}

Model3DData::MaterialData& Model3DData::material_at(usize idx) const {
  CHECK_DATA;
  assert(idx < _data->material_count);
  return _data->materials[idx];
}

Span<Model3DData::MaterialData> Model3DData::materials() const {
  CHECK_DATA;
  return datarange(_data->materials, _data->material_count);
}

usize Model3DData::material_count() const {
  CHECK_DATA;
  return _data->material_count;
}

Model3DData::TextureData& Model3DData::texture_at(usize idx) const {
  CHECK_DATA;
  assert(idx < _data->texture_count);
  return _data->textures[idx];
}

Span<Model3DData::TextureData> Model3DData::textures() const {
  CHECK_DATA;
  return datarange(_data->textures, _data->texture_count);
}

usize Model3DData::texture_count() const {
  CHECK_DATA;
  return _data->texture_count;
}

Span<u32> Model3DData::material_textures(usize idx) const {
  CHECK_DATA;
  assert(idx < _data->material_count);
  const auto& mat = _data->materials[idx];
  return datarange(_data->material_textures, _data->material_count, mat.texture_indices);
#if 0
  assert(idx < _data->material_count);
  auto& material = _data->materials[idx];
  if (material.texture_indices.start == (u32)-1 || !material.texture_indices.count) {
    return Span<texture_data>{}; // empty span
  }

  const auto map_range =
    datarange(_data->material_textures, _data->material_textures_count, material.texture_indices);
  assert(!map_range.empty());
  _data->texture_cache.clear();
  _data->texture_cache.resize(map_range.size());

  for (usize i = 0; i < map_range.size(); ++i) {
    _data->texture_cache[i] = _data->textures[map_range[i]];
  }

  return {_data->texture_cache.data(), _data->texture_cache.size()};
#endif
}

Optional<usize> Model3DData::find_mesh_idx(std::string_view mesh_name) const {
  CHECK_DATA;
  return find_map_idx(_data->mesh_registry, mesh_name);
}

Optional<usize> Model3DData::find_bone_idx(std::string_view bone_name) const {
  CHECK_DATA;
  return find_map_idx(_data->bone_registry, bone_name);
}

Optional<usize> Model3DData::find_material_idx(std::string_view material_name) const {
  CHECK_DATA;
  return find_map_idx(_data->material_registry, material_name);
}

Optional<usize> Model3DData::find_texture_idx(std::string_view texture_name) const {
  CHECK_DATA;
  return find_map_idx(_data->texture_registry, texture_name);
}

} // namespace kappa::assets
