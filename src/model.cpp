#include "model.hpp"
#include "renderer.hpp"

#define CHECK_ERR_BUFF(_buff) \
if (!_buff) { \
  ntf::logger::error("Failed to create vertex buffer, {}", _buff.error().what()); \
  free_buffs(); \
  return ntf::unexpected{std::string_view{"Failed to create vertex buffer"}}; \
}

model_mesh_provider::model_mesh_provider(mesh_vert_buffs buffs,
                                         ntfr::buffer_t index_buff,
                                         std::vector<mesh_offset>&& mesh_offsets) noexcept :
  _buffs{buffs}, _mesh_offsets{std::move(mesh_offsets)},
  _index_buff{index_buff}, _active_layouts{0u}
{
  // Do not fill inactive attributes
  for (u32 i = 0u; i < _buffs.size(); ++i) {
    if (_buffs[i]) {
      _binds[i].buffer = _buffs[i];
      _binds[i].layout = i;
      ++_active_layouts;
    }
  }
}

model_mesh_provider::model_mesh_provider(model_mesh_provider&& other) noexcept :
  _buffs{std::move(other._buffs)}, _binds{std::move(other._binds)},
  _mesh_offsets{std::move(other._mesh_offsets)}, _index_buff{std::move(other._index_buff)},
  _active_layouts{std::move(other._active_layouts)}
{
  other._index_buff = nullptr;
}

model_mesh_provider& model_mesh_provider::operator=(model_mesh_provider&& other) noexcept {
  _free_buffs();

  _buffs = std::move(other._buffs);
  _binds = std::move(other._binds);
  _mesh_offsets = std::move(other._mesh_offsets);
  _index_buff = std::move(other._index_buff);
  _active_layouts = std::move(other._active_layouts);

  other._index_buff = nullptr;

  return *this;
}

model_mesh_provider::~model_mesh_provider() noexcept { _free_buffs(); }


expect<model_mesh_provider> model_mesh_provider::create(const model_mesh_data& meshes)

{
  // Assume all models have indices and positions in each mesh
  if (meshes.positions.empty()) {
    return ntf::unexpected{std::string_view{"No position data"}};
  }
  if (meshes.indices.empty()) {
    return ntf::unexpected{std::string_view{"No index data"}};
  }

  auto ctx = renderer::instance().ctx();

  mesh_vert_buffs buffs{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  ntfr::buffer_t ind{nullptr};
  const size_t vertex_elements = meshes.positions.size();;

  auto free_buffs = [&]() {
    for (ntfr::buffer_t buff : buffs) {
      if (buff) {
        ntfr::destroy_buffer(buff);
      }
    }
  };

  auto make_vbuffer = [&](size_t sz) {
    return ntfr::create_buffer(ctx, {
      .type = ntfr::buffer_type::vertex,
      .flags = ntfr::buffer_flag::dynamic_storage,
      .size = vertex_elements*sz,
      .data = nullptr,
    });
  };

  // Prepare each vertex buffer, all of them with the same size
  {
    auto buff = make_vbuffer(sizeof(vec3));
    CHECK_ERR_BUFF(buff);
    buffs[ATTRIB_POS] = *buff;
  } 
  if (!meshes.normals.empty()) {
    auto buff = make_vbuffer(sizeof(vec3));
    CHECK_ERR_BUFF(buff);
    buffs[ATTRIB_NORM] = *buff;
  }
  if (!meshes.uvs.empty()) {
    auto buff = make_vbuffer(sizeof(vec2));
    CHECK_ERR_BUFF(buff);
    buffs[ATTRIB_UVS] = *buff;
  }
  if (!meshes.tangents.empty()) {
    NTF_ASSERT(!meshes.bitangents.empty());

    auto tang_buff = make_vbuffer(sizeof(vec3));
    CHECK_ERR_BUFF(tang_buff);
    buffs[ATTRIB_TANG] = *tang_buff;

    auto btang_buff = make_vbuffer(sizeof(vec3));
    CHECK_ERR_BUFF(btang_buff);
    buffs[ATTRIB_BITANG] = *btang_buff;
  }
  if (!meshes.bones.empty()) {
    NTF_ASSERT(!meshes.weights.empty());

    auto bone_buff = make_vbuffer(sizeof(model_mesh_data::vertex_bones));
    CHECK_ERR_BUFF(bone_buff);
    buffs[ATTRIB_BONES] = *bone_buff;

    auto weight_buff = make_vbuffer(sizeof(model_mesh_data::vertex_weights));
    CHECK_ERR_BUFF(weight_buff);
    buffs[ATTRIB_WEIGHTS] = *weight_buff;
  }

  // Prepare index buffer and upload indices
  std::vector<mesh_offset> mesh_offsets;
  mesh_offsets.reserve(meshes.meshes.size());
  {
    const ntfr::buffer_data idx_data {
      .data = meshes.indices.data(),
      .size = meshes.indices.size()*sizeof(u32),
      .offset = 0u,
    };
    auto ind_buff = ntfr::create_buffer(ctx, {
      .type = ntfr::buffer_type::index,
      .flags = ntfr::buffer_flag::dynamic_storage,
      .size = meshes.indices.size()*sizeof(u32),
      .data = idx_data,
    });
    if (!ind_buff) {
      ntf::logger::error("Failed to create index buffer, {}", ind_buff.error().what());
      return ntf::unexpected{std::string_view{"Failed to create index buffer"}};
    }
    ind = *ind_buff;

    for (const auto& mesh : meshes.meshes) {
      NTF_ASSERT(!mesh.indices.empty());
      mesh_offsets.emplace_back(mesh.indices.idx, mesh.indices.count, 0u);
    }
  }

  // Copy data
  u32 offset = 0u;
  for (size_t i = 0; i < meshes.meshes.size(); ++i) {
    const auto& mesh = meshes.meshes[i];
    NTF_ASSERT(!mesh.positions.empty());

    auto upload_thing = [&](size_t idx, vec_span vspan, const auto& vec) {
      const auto span = vspan.to_cspan(vec.data());
      const ntfr::buffer_data data {
        .data = span.data(),
        .size = span.size_bytes(),
        .offset = offset*sizeof(typename decltype(span)::value_type),
      };
      NTF_ASSERT(buffs[idx]);
      [[maybe_unused]] auto ret = ntfr::buffer_upload(buffs[idx], data);
      NTF_ASSERT(ret);
    };

    upload_thing(ATTRIB_POS, mesh.positions, meshes.positions);
    mesh_offsets[i].vertex_offset = offset;

    if (buffs[ATTRIB_NORM]) {
      upload_thing(ATTRIB_NORM, mesh.normals, meshes.normals);
    }

    if (buffs[ATTRIB_UVS]) {
      upload_thing(ATTRIB_UVS, mesh.uvs, meshes.uvs);
    }

    if (buffs[ATTRIB_TANG]) {
      upload_thing(ATTRIB_TANG, mesh.tangents, meshes.tangents);
      upload_thing(ATTRIB_BITANG, mesh.tangents, meshes.bitangents);
    }

    if (buffs[ATTRIB_BONES]) {
      upload_thing(ATTRIB_BONES, mesh.bones, meshes.bones);
      upload_thing(ATTRIB_WEIGHTS, mesh.bones, meshes.weights);
    }

    offset += mesh.positions.count;
  }

  return expect<model_mesh_provider>{
    ntf::in_place, buffs, ind, std::move(mesh_offsets)
  };
}

void model_mesh_provider::_free_buffs() noexcept {
  if (!_index_buff) {
    return;
  }
  ntfr::destroy_buffer(_index_buff);
  for (ntfr::buffer_t buff : _buffs){
    if (buff) {
      ntfr::destroy_buffer(buff);
    }
  }
}

u32 model_mesh_provider::retrieve_render_data(std::vector<mesh_render_data>& data) {
  for (const auto& offset : _mesh_offsets) {
    cspan<ntfr::vertex_binding> bindings{_binds.data(), _active_layouts};
    data.emplace_back(bindings, _index_buff,
                      offset.index_count, offset.vertex_offset, offset.index_offset, 0u);
  }
  return _mesh_offsets.size();
}

u32 model_mesh_provider::retrieve_bindings(std::vector<ntfr::attribute_binding>& bindings) {
  bindings.emplace_back(ntfr::attribute_type::vec3,  0u, 0u, 0u); // positions
  bindings.emplace_back(ntfr::attribute_type::vec3,  1u, 0u, 0u); // normals
  bindings.emplace_back(ntfr::attribute_type::vec2,  2u, 0u, 0u); // uvs
  bindings.emplace_back(ntfr::attribute_type::vec3,  3u, 0u, 0u); // tangents
  bindings.emplace_back(ntfr::attribute_type::vec3,  4u, 0u, 0u); // bitangents
  bindings.emplace_back(ntfr::attribute_type::ivec4, 5u, 0u, 0u); // bone indices
  bindings.emplace_back(ntfr::attribute_type::vec4,  6u, 0u, 0u); // bone weights
  return (u32)ATTRIB_COUNT;
}

model_rigger::model_rigger(vec_span bones,
                           ntfr::shader_storage_buffer&& ssbo,
                           std::vector<mat4>&& transform_output,
                           std::vector<mat4>&& bone_transforms,
                           std::vector<mat4>&& local_cache,
                           std::vector<mat4>&& model_cache) noexcept :
  _bones{bones},
  _ssbo{std::move(ssbo)}, _transform_output{std::move(transform_output)},
  _bone_transforms{std::move(bone_transforms)},
  _local_cache{std::move(local_cache)}, _model_cache{std::move(model_cache)} {}

expect<model_rigger> model_rigger::create(const model_rig_data& rigs, std::string_view armature)
{
  auto it = rigs.armature_registry.find(armature);
  if (it == rigs.armature_registry.end()) {
    return ntf::unexpected{std::string_view{"Armature not found"}};
  }
  auto ctx = renderer::instance().ctx();

  const u32 idx = it->second;
  NTF_ASSERT(rigs.armatures.size() > idx);
  const vec_span bones = rigs.armatures[idx].bones;

  std::vector<mat4> transform_output;
  std::vector<mat4> bone_transforms;
  std::vector<mat4> local_cache;
  std::vector<mat4> model_cache;

  try {
    const mat4 identity{1.f};
    transform_output.assign(bones.count, identity);
    bone_transforms.assign(bones.count, identity);
    local_cache.assign(bones.count, identity);
    model_cache.assign(bones.count, identity);
  } catch (const std::bad_alloc&) {
    return ntf::unexpected{std::string_view{"Matrix allocation failure"}};
  }

  const ntfr::buffer_data initial_data {
    .data = transform_output.data(),
    .size = bones.count*sizeof(mat4),
    .offset = 0u,
  };
  auto ssbo = ntfr::shader_storage_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = bones.count*sizeof(mat4),
    .data = initial_data,
  });
  if (!ssbo) {
    return ntf::unexpected{std::string_view{"Failed to create ssbo"}};
  }

  return expect<model_rigger> {
    ntf::in_place, bones, std::move(*ssbo),
    std::move(transform_output), std::move(bone_transforms),
    std::move(local_cache), std::move(model_cache)
  };
}

void model_rigger::tick(const model_rig_data& rigs) {
  for (auto& local : _local_cache){
    local = mat4{1.f};
  }
  for (auto& model : _model_cache) {
    model = mat4{1.f};
  }

  // Populate bone local transforms
  for (u32 i = 0; i < _bones.count; ++i) {
    _local_cache[i] = rigs.bone_locals[_bones.idx+i]*_bone_transforms[i];
  }

  // Populate bone model transforms
  _model_cache[0] = _local_cache[0]; // Root transform
  for (u32 i = 1; i < _bones.count; ++i) {
    u32 parent = rigs.bones[_bones.idx+i].parent;
    _model_cache[i] = _model_cache[parent] * _local_cache[i];
  }

  // Prepare shader transform data
  for (u32 i = 0; i < _bones.count; ++i){
    _transform_output[i] = _model_cache[i]*rigs.bone_inv_models[_bones.idx+i];
  }
}

void model_rigger::set_root_transform(const mat4& transf) {
  _bone_transforms[0u] = transf;
}

void model_rigger::set_transform(const model_rig_data& rigs,
                                 std::string_view bone, const bone_transform& transf)
{
  auto it = rigs.bone_registry.find(bone);
  NTF_ASSERT(it != rigs.bone_registry.end());
  u32 local_pos = it->second - _bones.idx;
  NTF_ASSERT(local_pos < _bone_transforms.size());

  constexpr vec3 pivot{0.f, 0.f, 0.f};
  _bone_transforms[local_pos] = ntf::build_trs_matrix(transf.pos, transf.scale,
                                                      pivot, transf.rot);
}

void model_rigger::set_transform(const model_rig_data& rigs,
                                 std::string_view bone, const mat4& transf)
{
  auto it = rigs.bone_registry.find(bone);
  NTF_ASSERT(it != rigs.bone_registry.end());
  u32 local_pos = it->second - _bones.idx;
  NTF_ASSERT(local_pos < _bone_transforms.size());

  _bone_transforms[local_pos] = transf;
}

u32 model_rigger::retrieve_buffers(std::vector<ntfr::shader_binding>& binds,
                                   ntfr::uniform_buffer_view stransf) {
  const ntfr::buffer_data data {
    .data = _transform_output.data(),
    .size = _bones.count*sizeof(mat4),
    .offset = 0u,
  };
  [[maybe_unused]] auto ret = _ssbo.upload(data);
  NTF_ASSERT(ret);
  NTF_ASSERT(!stransf.empty());
  binds.emplace_back(_ssbo.get(), 1u, _ssbo.size(), 0u);
  binds.emplace_back(stransf.get(), 2u, 2*sizeof(mat4), 0u);
  return 2u;
}
