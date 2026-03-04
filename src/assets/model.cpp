#include "./internal.hpp"
#include <assimp/mesh.h>

#define MODEL_LOG(level_, fmt_, ...) \
  ::kappa::logger::level_("[MODEL_IMPORT] " fmt_ __VA_OPT__(, ) __VA_ARGS__)

namespace kappa::assets {

model3d_loader::model3d_loader(std::string_view model_path, std::string_view model_name,
                               bits32 loader_flags) :
    _impl(new model3d_loader::loader_internal()) {
  _impl->importer.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, true);
  _impl->importer.SetPropertyInteger(AI_CONFIG_PP_SBBC_MAX_BONES, 4);
  _impl->model_path.copy_from(model_path.data(), model_path.size());
  _impl->model_name.copy_from(model_name.data(), model_name.size());
  _impl->importer_flags = loader_flags;
}

namespace {

m4f32 asscast(const aiMatrix4x4& mat) {
  // Assimp is row major. The a,b,c,d is the row and the 1,2,3,4 is the column.
  return shogle::math::transpose(std::bit_cast<m4f32>(mat)); // Just transpose it lol
}

using bone_inv_map = std::unordered_map<std::string, m4f32>;

model3d_data::model_internal* initialize_data(const buffer_name& name, const buffer_path& path,
                                              const aiScene& scene, bone_inv_map& bone_invs,
                                              buffer_str<256>& err) {
  if (!scene.mNumMeshes) {
    err.format_from("No meshes in scene");
    return nullptr;
  }

  // TODO: Handle bad_alloc on allocated arrays
  auto ptr = std::make_unique<model3d_data::model_internal>();
  auto& data = *ptr;
  auto& al = ptr->alloc;
  // Copy name & path
  std::memset(data.name.data, 0, sizeof(data.name.data));
  std::memcpy(data.name.data, name.data, name.len);
  data.name.len = name.len;
  std::memset(data.path.data, 0, sizeof(data.path.data));
  std::memcpy(data.path.data, path.data, path.len);
  data.path.len = path.len;

  // First mesh pass. Count blend shapes, bones & meshes.
  data.mesh_count = 0;
  data.blend_shape_count = 0;
  for (size_t i = 0; i < scene.mNumMeshes; ++i) {
    const aiMesh* mesh = scene.mMeshes[i];
    data.blend_shape_count += mesh->mNumAnimMeshes;
    ++data.mesh_count;

    // Store the inverse model matrix for each bone (if any)
    for (size_t j = 0; j < mesh->mNumBones; ++j) {
      const aiBone* bone = mesh->mBones[j];
      if (bone_invs.find(bone->mName.C_Str()) != bone_invs.end()) {
        continue;
      }
      auto [_, empl] = bone_invs.try_emplace(bone->mName.C_Str(), asscast(bone->mOffsetMatrix));
      if (!empl) {
        err.format_from("Duplicate bone \"{}\" in scene", bone->mName.C_Str());
        return nullptr;
      }
    }
  }
  data.meshes = al.alloc<model3d_data::mesh_data>(data.mesh_count);
  std::memset(data.meshes, 0x00, sizeof(data.meshes[0]) * data.mesh_count);
  if (data.blend_shape_count) {
    data.blend_shapes = al.alloc<model3d_data::blend_shape_data>(data.blend_shape_count);
    std::memset(data.blend_shapes, 0x00, sizeof(data.blend_shapes[0]) * data.blend_shape_count);
  } else {
    data.blend_shapes = nullptr;
  }

  static_assert(model3d_data::MAX_MESH_UVS <= AI_MAX_NUMBER_OF_TEXTURECOORDS);
  static_assert(model3d_data::MAX_MESH_COLORS <= AI_MAX_NUMBER_OF_COLOR_SETS);
  // Second mesh pass. Count meshes and blend shapes for each mesh & copy names.
  {
    size_t anim_pos = 0;
    size_t index_count = 0;
    size_t vertex_count = 0, vertex_anim_count = 0;
    size_t normal_count = 0, normal_anim_count = 0;
    size_t tangent_count = 0, tangent_anim_count = 0;
    size_t weight_count = 0;
    size_t uv_counts[model3d_data::MAX_MESH_UVS] = {0};
    size_t uv_anim_counts[model3d_data::MAX_MESH_UVS] = {0};
    size_t color_counts[model3d_data::MAX_MESH_COLORS] = {0};
    size_t color_anim_counts[model3d_data::MAX_MESH_COLORS] = {0};

    for (size_t i = 0; i < scene.mNumMeshes; ++i) {
      const aiMesh* mesh = scene.mMeshes[i];
      const size_t verts = mesh->mNumVertices;
      if (!mesh->HasPositions()) {
        err.format_from("Invalid position data at mesh \"{}\"", mesh->mName.C_Str());
        return nullptr;
      }
      const aiString& mesh_name = mesh->mName;
      if (mesh_name.length) {
        data.meshes[i].name.copy_from(mesh_name.data, mesh_name.length);
      } else {
        data.meshes[i].name.format_from("__mesh{}", i);
      }
      auto [_, empl] = data.mesh_registry.try_emplace(data.meshes[i].name.as_view(), i);
      assert(empl, "Mesh name duplicate");

      vertex_count += verts;
      if (mesh->HasNormals()) {
        normal_count += verts;
      }
      if (mesh->HasTangentsAndBitangents()) {
        tangent_count += verts;
      }
      for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
        if (mesh->HasTextureCoords(uv)) {
          uv_counts[uv] += verts;
        }
      }
      for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
        if (mesh->HasVertexColors(col)) {
          color_counts[col] += verts;
        }
      }
      if (mesh->HasBones()) {
        weight_count += verts;
      }
      for (size_t j = 0; j < mesh->mNumFaces; ++j) {
        index_count += mesh->mFaces[j].mNumIndices;
      }

      if (mesh->mNumAnimMeshes) {
        for (size_t j = 0; j < mesh->mNumAnimMeshes; ++j) {
          const aiAnimMesh* ai_anim = mesh->mAnimMeshes[j];
          const size_t animverts = ai_anim->mNumVertices;
          auto& anim = data.blend_shapes[anim_pos++];
          const aiString& anim_name = ai_anim->mName;
          if (anim_name.length) {
            anim.name.copy_from(anim_name.data, anim_name.length);
          } else {
            anim.name.format_from("__blend_shape{}", anim_pos - 1);
          }
          auto [_, emplb] =
            data.blend_shape_registry.try_emplace(anim.name.as_view(), anim_pos - 1);
          assert(emplb, "Duplicate name in blend shape registry");

          if (!ai_anim->HasPositions()) {
            vertex_anim_count += animverts;
          }
          if (ai_anim->HasNormals()) {
            normal_anim_count += animverts;
          }
          if (ai_anim->HasTangentsAndBitangents()) {
            tangent_anim_count += animverts;
          }
          for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
            if (ai_anim->HasTextureCoords(uv)) {
              uv_anim_counts[uv] += animverts;
            }
          }
          for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
            if (ai_anim->HasVertexColors(col)) {
              color_anim_counts[col] += animverts;
            }
          }
        }
      }
    }

    assert(vertex_count > 0, "No vertices");
    data.mesh_positions = al.alloc<v3f32>(vertex_count);
    data.blend_positions = vertex_anim_count ? al.alloc<v3f32>(vertex_anim_count) : nullptr;

    data.mesh_normals = normal_count ? al.alloc<v3f32>(normal_count) : nullptr;
    data.blend_normals = normal_anim_count ? al.alloc<v3f32>(normal_anim_count) : nullptr;

    for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
      data.mesh_uvs[uv] = uv_counts[uv] ? al.alloc<v2f32>(uv_counts[uv]) : nullptr;
      data.blend_uvs[uv] = uv_anim_counts[uv] ? al.alloc<v2f32>(uv_anim_counts[uv]) : nullptr;
    }
    for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
      data.mesh_colors[col] = color_counts[col] ? al.alloc<v4f32>(color_counts[col]) : nullptr;
      data.blend_colors[col] =
        color_anim_counts[col] ? al.alloc<v4f32>(color_anim_counts[col]) : nullptr;
    }
    if (tangent_count) {
      data.mesh_tangents = al.alloc<v3f32>(tangent_count);
      data.mesh_bitangents = al.alloc<v3f32>(tangent_count);
    } else {
      data.mesh_tangents = nullptr;
      data.mesh_bitangents = nullptr;
    }
    if (tangent_anim_count) {
      data.blend_tangents = al.alloc<v3f32>(tangent_anim_count);
      data.blend_bitangents = al.alloc<v3f32>(tangent_anim_count);
    } else {
      data.blend_tangents = nullptr;
      data.blend_bitangents = nullptr;
    }
    if (!bone_invs.empty() && weight_count) {
      // We need to parse the bone hierarchy before we parse the meshes!!11!1
      data.mesh_bones = al.alloc<v4i32>(weight_count);
      data.mesh_bone_weights = al.alloc<v4f32>(weight_count);
      // Set all parents to -1
      std::memset(data.mesh_bones, 0xFF, sizeof(data.mesh_bones[0]) * weight_count);
      // Set all weights to 0
      std::memset(data.mesh_bone_weights, 0x00, sizeof(data.mesh_bone_weights[0] * weight_count));
    } else {
      data.mesh_bones = nullptr;
      data.mesh_bone_weights = nullptr;
    }
    if (index_count) {
      data.mesh_indices = al.alloc<u32>(index_count);
    } else {
      data.mesh_indices = nullptr;
    }
    data.vertex_count = vertex_count;
    data.index_count = index_count;
  }

  // Preallocate bones
  data.bone_count = bone_invs.size();
  if (data.bone_count) {
    data.bones = al.alloc<model3d_data::bone_data>(data.bone_count);
    data.bone_inv_models = al.alloc<m4f32>(data.bone_count);
    data.bone_locals = al.alloc<m4f32>(data.bone_count);
    data.bone_registry.reserve(data.bone_count); // We fill the registry at parse_bones()
  } else {
    data.bones = nullptr;
    data.bone_inv_models = nullptr;
    data.bone_locals = nullptr;
  }

  // Preallocate animations
  data.animation_count = scene.mNumAnimations;
  if (data.animation_count) {
    size_t keyframe_count = 0;
    size_t pos_count = 0, scale_count = 0, rot_count = 0;
    for (size_t i = 0; i < scene.mNumAnimations; ++i) {
    }
    data.animations = al.alloc<model3d_data::anim_data>(data.animation_count);
    data.anim_registry.reserve(data.animation_count);
  } else {
    data.animations = nullptr;
  }

  return ptr.release();
}

bool is_identity(const m4f32& mat) {
  static constexpr m4f32 id(1.f);
  for (size_t i = 0; i < 4; ++i) {
    const auto col_a = mat.column_at(i);
    const auto col_b = id.column_at(i);
    for (size_t j = 0; j < 4; ++i) {
      if (std::abs(col_a[j] - col_b[j]) > shogle::math::epsilon<f32>) {
        return false;
      }
    }
  }
  return true;
}

v3f32 asscast(const aiVector3D& vec) {
  return {vec.x, vec.y, vec.z};
}

v4f32 asscast(const aiColor4D& col) {
  return {col.r, col.g, col.b, col.a};
}

qf32 asscast(const aiQuaternion& q) {
  return {q.w, q.x, q.y, q.z};
}

void parse_bone_nodes(const bone_inv_map& bone_invs, i32 parent, i32& bone_idx, const aiNode* node,
                      model3d_data::model_internal& data) {
  assert(node);

  auto it = bone_invs.find(node->mName.C_Str());
  assert(it != bone_invs.end());
  const auto& [bone_name, inv_model] = *it;

  // Store meta info and transforms
  data.bones[bone_idx].name.copy_from(bone_name.data(), bone_name.size());
  data.bones[bone_idx].parent = parent;
  data.bone_locals[bone_idx] = asscast(node->mTransformation);
  std::memcpy(data.bone_inv_models + bone_idx, &inv_model, sizeof(inv_model));

  // Add bones to the registry
  auto [_, empl] = data.bone_registry.try_emplace(data.bones[bone_idx].name.as_view(), bone_idx);
  assert(empl); // At this point we should never have duplicate bone names

  // Parse children
  const i32 this_bone_idx = bone_idx++;
  for (u32 i = 0; i < node->mNumChildren; ++i) {
    parse_bone_nodes(bone_invs, this_bone_idx, bone_idx, node->mChildren[i], data);
  }
}

m4f32 node_model(const aiNode* node) {
  if (node->mParent == nullptr) {
    return asscast(node->mTransformation);
  }
  return node_model(node->mParent) * asscast(node->mTransformation);
}

std::pair<bool, std::string_view> parse_rigs(model_allocator& al,
                                             model3d_data::model_internal& data,
                                             const aiScene& scene, std::string_view path) {

  // Create a bone tree from each root node
  {
    auto& reg = data.bone_registry;
    i32 bone_idx = 0;
    // Will set -1 as the parent index for the root bone
    parse_bone_nodes(bone_invs, -1, bone_idx, root_bone_node, data);

    // Make sure the root local transform is its node model transform
    if (!is_identity(data.bone_locals[0] * data.bone_inv_models[0])) {
      MODEL_LOG(warning, "Malformed transform in bone root at \"{}\", correction applied", path);
      data.bone_locals[0] = node_model(root_bone_node->mParent) * data.bone_locals[0];
    }
  }
  return std::make_pair(true, "");
}

bool parse_meshes(model_allocator& al, model3d_data::model_internal& data, const aiScene& scene) {

  const auto& bone_reg = data.bone_registry;

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
    assert(offset < data.vertex_count);

    for (size_t i = 0; i < 4; ++i) {
      if (weight.mWeight == 0.f) {
        return;
      }
      if (data.mesh_bones[offset][i] == -1) {
        data.mesh_bones[offset][i] = bone_idx;
        data.mesh_bone_weights[offset][i] = weight.mWeight;
        return;
      }
    }
    MODEL_LOG(warning, "Bone weights out of range in vertex index {}", vertex_pos);
  };

  for (size_t mesh_idx = 0; mesh_idx < scene.mNumMeshes; ++mesh_idx) {
    const aiMesh* ai_mesh = scene.mMeshes[mesh_idx];
    const u32 nverts = ai_mesh->mNumVertices;
    auto& mesh = data.meshes[mesh_idx];

    // Positions
    {
      for (size_t vert = 0; vert < nverts; ++vert) {
        data.mesh_positions[vertex_pos] = asscast(ai_mesh->mVertices[vert]);
      }
      mesh.positions.start = static_cast<u32>(vertex_pos);
      mesh.positions.count = nverts;
      vertex_pos += nverts;
    }
    // Normals
    if (ai_mesh->HasNormals()) {
      for (size_t vert = 0; vert < nverts; ++vert) {
        data.mesh_normals[normal_pos] = asscast(ai_mesh->mNormals[vert]);
      }
      mesh.normals.start = static_cast<u32>(normal_pos);
      mesh.normals.count = nverts;
      normal_pos += nverts;
    } else {
      mesh.normals = array_range::null_range();
    }
    // Tangents & bitangents
    if (ai_mesh->HasTangentsAndBitangents()) {
      for (size_t vert = 0; vert < nverts; ++vert) {
        data.mesh_tangents[tangent_pos] = asscast(ai_mesh->mTangents[vert]);
        data.mesh_bitangents[tangent_pos] = asscast(ai_mesh->mBitangents[vert]);
      }
      mesh.tangents.start = static_cast<u32>(tangent_pos);
      mesh.tangents.count = nverts;
      tangent_pos += nverts;
    } else {
      mesh.tangents = array_range::null_range();
    }
    // UVs
    for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
      if (ai_mesh->HasTextureCoords(uv)) {
        for (size_t vert = 0; vert < nverts; ++vert) {
          const auto& uv_vec = ai_mesh->mTextureCoords[uv][vert];
          data.mesh_uvs[uv][uv_pos[uv]].x = uv_vec.x;
          data.mesh_uvs[uv][uv_pos[uv]].y = uv_vec.y;
        }
        mesh.uvs[uv].start = static_cast<u32>(uv_pos[uv]);
        mesh.uvs[uv].count = nverts;
        uv_pos[uv] += nverts;
      } else {
        mesh.uvs[uv] = array_range::null_range();
      }
    };
    // Colors
    for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
      if (ai_mesh->HasVertexColors(col)) {
        for (size_t vert = 0; vert < nverts; ++vert) {
          data.mesh_colors[col][color_pos[col]] = asscast(ai_mesh->mColors[col][vert]);
        }
        mesh.colors[col].start = static_cast<u32>(color_pos[col]);
        mesh.colors[col].count = nverts;
        color_pos[col] += nverts;
      } else {
        mesh.colors[col] = array_range::null_range();
      }
    }
    // Bone indices & weigths
    if (!bone_reg.empty() && ai_mesh->HasBones()) {
      for (size_t bone = 0; bone < ai_mesh->mNumBones; ++bone) {
        const aiBone* ai_bone = ai_mesh->mBones[bone];
        auto it = bone_reg.find(ai_bone->mName.C_Str());
        assert(it != bone_reg.end()); // At this point everything *should* be valid
        const i32 bone_idx = static_cast<i32>(it->second);
        for (size_t weight = 0; weight < ai_bone->mNumWeights; ++weight) {
          try_place_weight(bone_idx, ai_bone->mWeights[weight]);
        }
      }
      mesh.bones.start = static_cast<u32>(bone_pos);
      mesh.bones.count = nverts;
      bone_pos += nverts;
    } else {
      mesh.bones = array_range::null_range();
    }
    // Face indices
    mesh.face_count = ai_mesh->mNumFaces;
    if (mesh.face_count > 0) {
      u32 idx_count = 0u;
      for (size_t face_idx = 0; face_idx < ai_mesh->mNumFaces; ++face_idx) {
        const aiFace& face = ai_mesh->mFaces[face_idx];
        for (size_t i = 0; i < face.mNumIndices; ++i) {
          data.mesh_indices[index_pos + idx_count + i] = face.mIndices[i];
        }
        idx_count += face.mNumIndices;
      }
      mesh.indices.start = index_pos;
      mesh.indices.count = idx_count;
      index_pos += idx_count;
    } else {
      mesh.indices = array_range::null_range();
    }

    // Set names
    const aiString& mesh_name = ai_mesh->mName;
    mesh.name.copy_from(mesh_name.data, mesh_name.length);
    for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
      if (ai_mesh->HasTextureCoordsName(uv)) {
        const aiString& name = *ai_mesh->mTextureCoordsNames[uv];
        mesh.uv_name[uv].copy_from(name.data, name.length);
      } else {
        std::memset(&mesh.uv_name[uv], 0x00, sizeof(mesh.uv_name[0])); // No name
      }
    }

    // Blend shapes, almost a duplicate of the mesh data
    for (size_t j = 0; j < ai_mesh->mNumAnimMeshes; ++j) {
      const aiAnimMesh* ai_anim = ai_mesh->mAnimMeshes[j];
      const u32 nanimvert = ai_anim->mNumVertices;
      auto& anim = data.blend_shapes[shape_pos++];

      // Positions (can be optional this time)
      if (ai_anim->HasPositions()) {
        for (size_t vert = 0; vert < nanimvert; ++vert) {
          data.blend_positions[vertex_anim_pos] = asscast(ai_anim->mVertices[vert]);
        }
        anim.positions.start = static_cast<u32>(vertex_anim_pos);
        anim.positions.count = nanimvert;
        vertex_anim_pos += nanimvert;
      } else {
        anim.positions = array_range::null_range();
      }

      // Normals
      if (ai_anim->HasNormals()) {
        for (size_t vert = 0; vert < nanimvert; ++vert) {
          data.blend_normals[normal_anim_pos] = asscast(ai_anim->mNormals[vert]);
        }
        anim.normals.start = static_cast<u32>(normal_anim_pos);
        anim.tangents.count = nanimvert;
        normal_anim_pos += nanimvert;
      } else {
        anim.normals = array_range::null_range();
      }

      // Tangents & Bitangents
      if (ai_anim->HasTangentsAndBitangents()) {
        for (size_t vert = 0; vert < nanimvert; ++vert) {
          data.blend_tangents[tangent_anim_pos] = asscast(ai_anim->mTangents[vert]);
          data.blend_bitangents[tangent_anim_pos] = asscast(ai_anim->mBitangents[vert]);
        }
        anim.tangents.start = static_cast<u32>(tangent_anim_pos);
        anim.tangents.count = nanimvert;
        tangent_anim_pos += nanimvert;
      } else {
        anim.tangents = array_range::null_range();
      }

      // UVs
      for (size_t uv = 0; uv < model3d_data::MAX_MESH_UVS; ++uv) {
        if (ai_anim->HasTextureCoords(uv)) {
          for (size_t vert = 0; vert < nanimvert; ++vert) {
            const auto& uv_vec = ai_anim->mTextureCoords[uv][vert];
            data.blend_uvs[uv][uv_anim_pos[uv]].x = uv_vec.x;
            data.blend_uvs[uv][uv_anim_pos[uv]].y = uv_vec.y;
          }
          anim.uvs[uv].start = static_cast<u32>(uv_anim_pos[uv]);
          anim.uvs[uv].count = nanimvert;
          uv_anim_pos[uv] += nanimvert;
        } else {
          anim.uvs[uv] = array_range::null_range();
        }
      }

      // Colors
      for (size_t col = 0; col < model3d_data::MAX_MESH_COLORS; ++col) {
        if (ai_anim->HasVertexColors(col)) {
          for (size_t vert = 0; vert < nanimvert; ++vert) {
            data.blend_colors[col][color_anim_pos[col]] = asscast(ai_anim->mColors[col][vert]);
          }
          anim.colors[col].start = static_cast<u32>(color_anim_pos[col]);
          anim.colors[col].count = nanimvert;
          color_anim_pos[col] += nanimvert;
        } else {
          anim.colors[col] = array_range::null_range();
        }
      }
    }

    // Set other props
    mesh.bbox_max = asscast(ai_mesh->mAABB.mMax);
    mesh.bbox_min = asscast(ai_mesh->mAABB.mMin);
    mesh.material_index = ai_mesh->mMaterialIndex;
    mesh.primitive = [&]() -> model3d_data::mesh_primitive {
      switch (ai_mesh->mPrimitiveTypes) {
        default:
          return model3d_data::MESH_PRIMITIVE_TRIANGLE;
      };
    }();
  }
  return true;
}

bool parse_bone_animations(model_allocator& al, model3d_data& data) {}

bool parse_materials(model_allocator& al, model3d_data& data) {}

void dealloc_bones(model_allocator& al, model3d_data::model_internal& data) {
  al.dealloc(data.bones, data.bone_count);
  al.dealloc(data.bone_locals, data.bone_count);
  al.dealloc(data.bone_inv_models, data.bone_count);
}

} // namespace

bs_expect<model3d_data, 256> model3d_loader::load() {
  assert(_impl, "model3d_loader use after free");
  const shogle::scope_end defer = [this]() {
    delete _impl;
    _impl = nullptr;
  };
  auto& imp = _impl->importer;
  const auto path = _impl->model_path.as_view();
  const aiScene* scene = imp.ReadFile(_impl->model_path.data, _impl->importer_flags);
  buffer_str<256> errbuff;
  if (!scene || !scene->mNumMeshes) {
    const char* err = imp.GetErrorString();
    size_t len = std::strlen(err);
    errbuff.copy_from(err, len);
    return {unexpect, errbuff};
  }

  model_allocator al;
  const auto [has_rigs, rig_err] = parse_rigs(al, *data, *scene, path);
  if (!has_rigs) {
    MODEL_LOG(warning, "No rigs found at \"{}\": {}", path, rig_err);
  }
  if (!parse_meshes(al, *data, *scene)) {
    if (has_rigs) {
      dealloc_bones(al, *data);
      delete data;
    }
    errbuff.format_from("No valid meshes found at \"{}\"", path);
    MODEL_LOG(error, "{}", errbuff.as_view());
    return {unexpect, errbuff};
  }

  if (!parse_animations(al, out)) {
    MODEL_LOG(warning, "No animations found at \"{}\"", path);
  }
  if (!parse_materials(al, out)) {
    MODEL_LOG(warning, "No materials found at \"{}\"", path);
  }

  return {in_place, std::move(out)};
}

model_internal::model_internal() {}

model_internal::~model_internal() {
  a.dealloc(meshes, mesh_count);
  a.dealloc(mesh_pos, vertex_count);
  a.dealloc(mesh_norm, vertex_count);
  a.dealloc(mesh_uv0, vertex_count);
  a.dealloc(mesh_uv1, vertex_count);
  a.dealloc(mesh_col, vertex_count);
  a.dealloc(mesh_tan, vertex_count);
  a.dealloc(mesh_bitan, vertex_count);
  a.dealloc(mesh_bones, vertex_count);
  a.dealloc(mesh_weights, vertex_count);
  a.dealloc(mesh_indices, vertex_count);
  if (has_materials()) {
    a.dealloc(materials, material_count);
    if (has_textures()) {
      a.dealloc(textures, texture_count);
    }
  }
  if (has_animations()) {
    a.dealloc(animations, animation_count);
    a.dealloc(anim_keyframes, animation_count);
    a.dealloc(anim_positions, anim_keyframe_count);
    a.dealloc(anim_scales, anim_keyframe_count);
    a.dealloc(anim_rotations, anim_keyframe_count);
  }
  if (has_armatures()) {
    a.dealloc(armatures, armature_count);
    a.dealloc(bones, bone_count);
    a.dealloc(bone_locals, bone_count);
    a.dealloc(bone_inv_models, bone_count);
  }
}

model3d_data::model3d_data() noexcept :
    _impl(nullptr), animations(nullptr), textures(nullptr), materials(nullptr),
    armatures(nullptr) {}

void model3d_data::destroy() noexcept {
  if (!_data) {
    return;
  }
  delete _data;
  _data = nullptr;
}

namespace {

optional<u32> find_map_idx(auto&& map, std::string_view name) {
  auto it = map.find(name);
  if (it == map.end()) {
    return nullopt;
  }
  return {in_place, it->second};
}

} // namespace

optional<u32> model3d_data::find_mesh(std::string_view mesh_name) noexcept {
  assert(_data, "model3d_data use after free");
  return find_map_idx(_data->mesh_registry, mesh_name);
}

optional<u32> model3d_data::find_animation_idx(std::string_view animation_name) noexcept {
  assert(_data, "model3d_data use after free");
  return find_map_idx(_data->anim_registry, animation_name);
}

optional<u32> model3d_data::find_bone_idx(std::string_view bone_name) noexcept {
  assert(_data, "model3d_data use after free");
  return find_map_idx(_data->bone_registry, bone_name);
}

optional<u32> model3d_data::find_armature(std::string_view armature_name) noexcept {
  assert(_data, "model3d_data use after free");
  return find_map_idx(_data->armature_registry, armature_name);
}

optional<u32> model3d_data::find_material_idx(std::string_view material_name) noexcept {
  assert(_data, "model3d_data use after free");
  return find_map_idx(_data->material_registry, material_name);
}

optional<u32> model3d_data::find_texture_idx(std::string_view material_name) noexcept {
  assert(_data, "model3d_data use after free");
  return find_map_idx(_data->texture_registry, material_name);
}

} // namespace kappa::assets
