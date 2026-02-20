#include "./model_internal.hpp"

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

m4f32 asscast(const aiMatrix4x4& mat) {
  // Assimp is row major. The a,b,c,d is the row and the 1,2,3,4 is the column.
  return shogle::math::transpose(std::bit_cast<m4f32>(mat)); // Just transpose it lol
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

using bone_inv_map = std::unordered_map<std::string, m4f32>;

void parse_bone_nodes(const bone_inv_map& bone_invs, i32 parent, i32& bone_idx, const aiNode* node,
                      model3d_data& data) {
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
  auto [_, empl] =
    data._impl->bone_registry.try_emplace(data.bones[bone_idx].name.as_view(), bone_idx);
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

std::pair<bool, std::string_view> parse_rigs(model_allocator& al, model3d_data& data,
                                             const aiScene& scene, std::string_view path) {
  // Store the inverse model matrix for each bone
  bone_inv_map bone_invs;
  for (size_t i = 0; i < scene.mNumMeshes; ++i) {
    const aiMesh* mesh = scene.mMeshes[i];
    for (size_t j = 0; j < mesh->mNumBones; ++j) {
      const aiBone* bone = mesh->mBones[j];
      if (bone_invs.find(bone->mName.C_Str()) != bone_invs.end()) {
        continue;
      }
      auto [_, empl] = bone_invs.try_emplace(bone->mName.C_Str(), asscast(bone->mOffsetMatrix));
      if (!empl) {
        return std::make_pair(false, "Duplicate bones in rig hierarchy");
      }
    }
  }

  // Map root bones to a node in the scene graph
  static constexpr size_t MAX_POSSIBLE_ROOTS = 8;
  const auto* scene_root = scene.mRootNode;
  const aiNode* possible_roots[MAX_POSSIBLE_ROOTS];
  size_t root_count = 0;
  for (const auto& [name, _] : bone_invs) {
    const aiNode* bone_node = scene_root->FindNode(name.c_str());
    // We assume the bone and the node have the same name
    if (bone_invs.find(bone_node->mParent->mName.C_Str()) != bone_invs.end()) {
      continue;
    }
    // Assume all bone nodes with a parent not present in the map is a root node
    if (root_count == MAX_POSSIBLE_ROOTS) {
      MODEL_LOG(warning, "Bone root count out of range at \"{}\"", path);
      break;
    }
    possible_roots[root_count++] = bone_node;
  }

  if (!root_count) {
    return std::make_pair(false, "No root nodes found");
  }
  MODEL_LOG(debug, "Found {} possible bone root(s) at \"{}\"", root_count, path);

  // Create a bone tree from each root node
  auto& reg = data._impl->bone_registry;
  data.bone_count = bone_invs.size();
  data.bones = al.alloc<model3d_data::bone_data>(bone_invs.size());
  data.bone_inv_models = al.alloc<m4f32>(bone_invs.size());
  data.bone_locals = al.alloc<m4f32>(bone_invs.size());
  reg.reserve(bone_invs.size());

  std::vector<model3d_data::armature_data> armatures;
  i32 bone_idx = 0;
  for (size_t i = 0; i < root_count; ++i) {
    const aiNode* root = possible_roots[i];
    // Check for name dupes
    auto bone_it = reg.find(root->mName.C_Str());
    if (bone_it != reg.end()) {
      const size_t bone_index = bone_it->second;
      if (bone_index != array_span::index_tomb) {
        MODEL_LOG(warning, "Bone root \"{}\" already parsed, possible node dupe at \"{}\"",
                  root->mName.C_Str(), path);
        continue;
      }
    }

    const char* armature_name = root->mParent->mName.C_Str();
    const i32 root_idx = bone_idx;
    // Will set -1 as the parent index for the root bone
    parse_bone_nodes(bone_invs, -1, bone_idx, root, data);

    // Make sure the root local transform is its node model transform
    if (!is_identity(data.bone_locals[root_idx] * data.bone_inv_models[root_idx])) {
      MODEL_LOG(warning, "Malformed transform in root \"{}\" at \"{}\", correction applied",
                root->mName.C_Str(), path);
      data.bone_locals[root_idx] = node_model(root->mParent) * data.bone_locals[root_idx];
    }

    auto& armature = armatures.emplace_back();
    armature.name.copy_from(armature_name);
    armature.bones.start = static_cast<u32>(root_idx);
    armature.bones.count = static_cast<u32>(bone_idx - root_idx);
  }
  if (armatures.empty()) {
    data._impl->bone_registry.clear();
    al.dealloc(data.bones, data.bone_count);
    al.dealloc(data.bone_inv_models, data.bone_count);
    al.dealloc(data.bone_locals, data.bone_count);
    return std::make_pair(false, "No valid armatures found in model");
  }
  data.armature_count = armatures.size();
  data.armatures = al.alloc<model3d_data::armature_data>(data.armature_count);
  std::memcpy(data.armatures, armatures.data(), sizeof(armatures[0]) * armatures.size());

  // We didn't know the number of armatures before this so we reserve here
  data._impl->armature_registry.reserve(data.armature_count);
  for (u32 i = 0; i < data.armature_count; ++i) {
    auto& arm = data.armatures[i];
    auto [_, empl] = data._impl->armature_registry.try_emplace(arm.name.as_view(), i++);
    if (!empl) {
      MODEL_LOG(warning, "Armature \"{}\" parsed twice at \"{}\", generating new name for dupe",
                arm.name.as_view(), path);
      decltype(arm.name) old_name;
      old_name.copy_from(arm.name.data, arm.name.len);
      arm.name.format_from("{}_bis", old_name.as_view());
      data._impl->armature_registry.try_emplace(arm.name.as_view(), i);
    }
  }
  return std::make_pair(true, "");
}

bool parse_meshes(model_allocator& al, model3d_data& data, const aiScene& scene) {
  size_t index_count = 0;
  size_t vertex_count = 0;
  size_t face_count = 0;

  {
    size_t pos_count = 0;
    size_t norm_count = 0;
    size_t uv0_count = 0;
    size_t uv1_count = 0;
    size_t tang_count = 0;
    size_t col_count = 0;
    for (size_t i = 0u; i < scene.mNumMeshes; ++i) {
      const aiMesh* mesh = scene.mMeshes[i];
      const size_t verts = mesh->mNumVertices;
      if (!mesh->HasPositions()) {
        return false;
      }

      pos_count += verts;
      if (mesh->HasNormals()) {
        norm_count += verts;
      }
      if (mesh->HasTextureCoords(0)) {
        uv0_count += verts;
      }
      if (mesh->HasTextureCoords(1)) {
        uv1_count += verts;
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

    assert(pos_count > 0);
    data.mesh_pos = al.alloc<v3f32>(pos_count);
    data.mesh_norm = norm_count ? al.alloc<v3f32>(norm_count) : nullptr;
    data.mesh_uv0 = uv0_count ? al.alloc<v2f32>(uv0_count) : nullptr;
    data.mesh_uv1 = uv1_count ? al.alloc<v2f32>(uv1_count) : nullptr;
    data.mesh_tan = tang_count ? al.alloc<v3f32>(tang_count) : nullptr;
    data.mesh_bitan = tang_count ? al.alloc<v3f32>(tang_count) : nullptr;
    data.mesh_col = col_count ? al.alloc<v4f32>(col_count) : nullptr;

    // We need to parse bones before we parse the meshes!!11!1
    if (data.has_armatures()) {
      assert(data.bone_count > 0);
      data.mesh_bones = al.alloc<v4i32>(data.bone_count);
      data.mesh_weights = al.alloc<v4f32>(data.bone_count);
      // Set all parents to -1
      std::memset(data.mesh_bones, 0xFF, sizeof(data.mesh_bones[0]) * data.bone_count);
    } else {
      data.mesh_bones = nullptr;
      data.mesh_weights = nullptr;
    }
  }

  const auto try_place_weight = [&](size_t mesh_start, size_t idx, const aiVertexWeight& weight) {
    const size_t vertex_pos = mesh_start + weight.mVertexId;
    auto& vertex_bone_indices = data.mesh_bones[vertex_pos];
    auto& vertex_bone_weights = data.mesh_weights[vertex_pos];

    assert(vertex_pos < data.vertex_count);
    for (size_t i = 0; i < 4; ++i) {
      if (weight.mWeight == 0.f) {
        return;
      }
      if (vertex_bone_indices[i] == -1) {
        vertex_bone_indices[i] = idx;
        vertex_bone_weights[i] = weight.mWeight;
        return;
      }
    }
    MODEL_LOG(warning, "Bone weigths out of range in vertex {}", vertex_pos);
  };

  data.mesh_count = scene.mNumMeshes;
  data._impl->mesh_registry.reserve(data.mesh_count);
  data.vertex_count = vertex_count;
  data.face_count = face_count;
  data.index_count = index_count;
  for (size_t i = 0; i < scene.mNumMeshes; ++i) {
    const aiMesh* mesh = scene.mMeshes[i];
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
  return true;
}

bool parse_animations(model_allocator& al, model3d_data& data) {}

bool parse_materials(model_allocator& al, model3d_data& data) {}

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
  if (!scene) {
    const char* err = imp.GetErrorString();
    size_t len = std::strlen(err);
    errbuff.copy_from(err, len);
    return {unexpect, errbuff};
  }

  model_allocator al;
  model3d_data out;
  out._impl = new model3d_data::model_internal();

  const auto [has_rigs, rig_err] = parse_rigs(al, out, *scene, path);
  if (!has_rigs) {
    MODEL_LOG(warning, "No rigs found at \"{}\": {}", path, rig_err);
  }
  if (!parse_meshes(al, out, *scene)) {
    if (has_rigs) {
      al.dealloc(out.bones, out.bone_count);
      al.dealloc(out.bone_locals, out.bone_count);
      al.dealloc(out.bone_inv_models, out.bone_count);
      al.dealloc(out.armatures, out.armature_count);
      delete out._impl;
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

  std::memset(out.name.data, 0, sizeof(out.name.data));
  std::memcpy(out.name.data, _impl->model_name.data, _impl->model_name.len);
  std::memset(out.path.data, 0, sizeof(out.path.data));
  std::memcpy(out.path.data, _impl->model_path.data, _impl->model_path.len);
  return {in_place, std::move(out)};
}

model3d_data::model3d_data() noexcept :
    _impl(nullptr), animations(nullptr), textures(nullptr), materials(nullptr),
    armatures(nullptr) {}

void model3d_data::destroy() noexcept {
  if (!_impl) {
    return;
  }
  auto& a = _impl->alloc;

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
  delete _impl;
  _impl = nullptr;
}

optional<u32> model3d_data::find_animation(std::string_view animation_name) noexcept {
  assert(_impl);
  auto it = _impl->anim_registry.find(animation_name);
  if (it == _impl->anim_registry.end()) {
    return nullopt;
  }
  return {in_place, it->second};
}

optional<u32> model3d_data::find_mesh(std::string_view mesh_name) noexcept {
  assert(_impl);
  auto it = _impl->mesh_registry.find(mesh_name);
  if (it == _impl->mesh_registry.end()) {
    return nullopt;
  }
  return {in_place, it->second};
}

optional<u32> model3d_data::find_material(std::string_view material_name) noexcept {
  assert(_impl);
  auto it = _impl->material_registry.find(material_name);
  if (it == _impl->material_registry.end()) {
    return nullopt;
  }
  return {in_place, it->second};
}

optional<u32> model3d_data::find_bone(std::string_view bone_name) noexcept {
  assert(_impl);
  auto it = _impl->bone_registry.find(bone_name);
  if (it == _impl->bone_registry.end()) {
    return nullopt;
  }
  return {in_place, it->second};
}

optional<u32> model3d_data::find_armature(std::string_view armature_name) noexcept {
  assert(_impl);
  auto it = _impl->armature_registry.find(armature_name);
  if (it == _impl->armature_registry.end()) {
    return nullopt;
  }
  return {in_place, it->second};
}

bool model3d_data::has_materials() noexcept {
  return materials != nullptr;
}

bool model3d_data::has_textures() noexcept {
  return textures != nullptr;
}

bool model3d_data::has_armatures() noexcept {
  return armatures != nullptr;
}

bool model3d_data::has_animations() noexcept {
  return animations != nullptr;
}

} // namespace kappa::assets
