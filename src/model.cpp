#include "model.hpp"
#include "renderer.hpp"

#define CHECK_ERR_BUFF(_buff) \
if (!_buff) { \
  ntf::logger::error("Failed to create vertex buffer, {}", _buff.error().what()); \
  free_buffs(); \
  return ntf::unexpected{std::string_view{"Failed to create vertex buffer"}}; \
}

model_mesh_provider::model_mesh_provider(mesh_vert_buffs buffs,
                                         ntfr::index_buffer&& index_buff,
                                         std::vector<mesh_offset>&& mesh_offsets) noexcept :
  _buffs{buffs}, _mesh_offsets{std::move(mesh_offsets)},
  _index_buff{std::move(index_buff)}, _active_layouts{0u}
{
  NTF_ASSERT(buffs[0] != nullptr, "Needs a position vertex buffer");
  NTF_ASSERT(!_index_buff.empty(), "Needs an index buffer");
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
  other._buffs[0] = nullptr;
}

model_mesh_provider& model_mesh_provider::operator=(model_mesh_provider&& other) noexcept {
  _free_buffs();

  _buffs = std::move(other._buffs);
  _binds = std::move(other._binds);
  _mesh_offsets = std::move(other._mesh_offsets);
  _index_buff = std::move(other._index_buff);
  _active_layouts = std::move(other._active_layouts);

  other._buffs[0] = nullptr;

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
  const ntfr::buffer_data idx_data {
    .data = meshes.indices.data(),
    .size = meshes.indices.size()*sizeof(u32),
    .offset = 0u,
  };
  auto ind = ntfr::index_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = meshes.indices.size()*sizeof(u32),
    .data = idx_data,
  });
  if (!ind) {
    ntf::logger::error("Failed to create index buffer, {}", ind.error().what());
    return ntf::unexpected{std::string_view{"Failed to create index buffer"}};
  }

  for (const auto& mesh : meshes.meshes) {
    NTF_ASSERT(!mesh.indices.empty());
    mesh_offsets.emplace_back(mesh.indices.idx, mesh.indices.count, 0u);
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
    ntf::in_place, buffs, std::move(*ind), std::move(mesh_offsets)
  };
}

void model_mesh_provider::_free_buffs() noexcept {
  if (!_buffs[0]) {
    return;
  }
  for (ntfr::buffer_t buff : _buffs){
    if (buff) {
      ntfr::destroy_buffer(buff);
    }
  }
}

mesh_render_data& model_mesh_provider::retrieve_mesh_data(u32 idx,
                                                          std::vector<mesh_render_data>& data) {
  NTF_ASSERT(idx < _mesh_offsets.size());
  cspan<ntfr::vertex_binding> bindings{_binds.data(), _active_layouts};
  const auto& offset = _mesh_offsets[idx];
  return data.emplace_back(bindings, _index_buff,
                           offset.index_count, offset.vertex_offset, offset.index_offset, 0u);
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

u32 model_rigger::retrieve_buffer_bindings(std::vector<ntfr::shader_binding>& binds) {
  const ntfr::buffer_data data {
    .data = _transform_output.data(),
    .size = _bones.count*sizeof(mat4),
    .offset = 0u,
  };
  [[maybe_unused]] auto ret = _ssbo.upload(data);
  NTF_ASSERT(ret);
  binds.emplace_back(_ssbo.get(), renderer::VERT_MODEL_TRANSFORM_LOC, _ssbo.size(), 0u);
  return 1u;
}

rigged_model3d::rigged_model3d(std::string&& name,
                               material_meta&& materials,
                               armature_meta&& armature,
                               render_meta&& rendering,
                               ntf::transform3d<f32> transf) :
  _name{std::move(name)}, _materials{std::move(materials)},
  _armature{std::move(armature)}, _rendering{std::move(rendering)},
  _transf{transf} {}

template<u32 tex_extent = 32u>
static ntfr::expect<ntfr::texture2d> make_missing_albedo(ntfr::context_view ctx) {
  static constexpr auto bitmap = [](){
    std::array<u8, 4u*tex_extent*tex_extent> out;
    const u8 pixels[] {
      0x00, 0x00, 0x00, 0xFF, // black
      0xFE, 0x00, 0xFE, 0xFF, // pink
      0x00, 0x00, 0x00, 0xFF, // black again
    };

    for (u32 i = 0; i < tex_extent; ++i) {
      const u8* start = i%2 == 0 ? &pixels[0] : &pixels[4]; // Start row with a different color
      u32 pos = 0;
      for (u32 j = 0; j < tex_extent; ++j) {
        pos = (pos + 4) % 8;
        for (u32 k = 0; k < 4; ++k) {
          out[(4*i*tex_extent)+(4*j)+k] = start[pos+k];
        }
      }
    }

    return out;
  }();
  const ntfr::image_data image {
    .bitmap = bitmap.data(),
    .format = ntfr::image_format::rgba8nu,
    .alignment = 4u,
    .extent = {tex_extent, tex_extent, 1},
    .offset = {0, 0, 0},
    .layer = 0u,
    .level = 0u,
  };
  const ntfr::texture_data data {
    .images = {image},
    .generate_mipmaps = false,
  };
  return ntfr::texture2d::create(ctx, {
    .format = ntfr::image_format::rgba8nu,
    .sampler = ntfr::texture_sampler::nearest,
    .addressing = ntfr::texture_addressing::repeat,
    .extent = {tex_extent, tex_extent, 1},
    .layers = 1u,
    .levels = 1u,
    .data = data,
  });
}

expect<rigged_model3d> rigged_model3d::create(data_t&& data) {
  auto& r = renderer::instance();
  auto ctx = r.ctx();

  auto rigger = model_rigger::create(data.rigs, data.armature);
  if (!rigger) {
    return ntf::unexpected{std::move(rigger.error())};
  }
  armature_meta armature {
    .anims = std::move(data.anims),
    .rigs = std::move(data.rigs),
    .rigger = std::move(*rigger),
  };

  std::vector<texture_t> texs;
  texs.reserve(data.mats.textures.size());
  std::unordered_map<std::string_view, u32> tex_reg;
  tex_reg.reserve(data.mats.textures.size());

  for (auto& texture : data.mats.textures) {
    const ntfr::image_data images {
      .bitmap = texture.bitmap.data(),
      .format = texture.format,
      .alignment = 4u,
      .extent = texture.extent,
      .offset = {0, 0, 0},
      .layer = 0u,
      .level = 0u,
    };
    const ntfr::texture_data data {
      .images = {images},
      .generate_mipmaps = true,
    };
    auto tex = ntfr::texture2d::create(ctx, {
      .format = ntfr::image_format::rgba8nu,
      .sampler = ntfr::texture_sampler::linear,
      .addressing = ntfr::texture_addressing::repeat,
      .extent = texture.extent,
      .layers = 1u,
      .levels = 7u,
      .data = data,
    }).value();
    texs.emplace_back(std::move(texture.name), std::move(tex));
    // texs.emplace_back(std::move(texture.name), make_missing_albedo(ctx).value());
  }
  for (u32 i = 0; const auto& texture : texs) {
    auto [_, empl] = tex_reg.try_emplace(texture.name, i++);
    NTF_ASSERT(empl);
  }

  material_meta materials {
    .textures = std::move(texs),
    .texture_registry = std::move(tex_reg),
    .materials = std::move(data.mats.materials),
    .material_registry = std::move(data.mats.material_registry),
    .material_textures = std::move(data.mats.material_textures),
  };

  auto meshes = model_mesh_provider::create(data.meshes);
  if (!meshes) {
    return ntf::unexpected{std::move(meshes.error())};
  }

  std::vector<u32> mesh_texs;
  mesh_texs.reserve(data.meshes.meshes.size());
  for (const auto& mesh : data.meshes.meshes) {
    auto it = materials.material_registry.find(mesh.material_name);
    if (it == materials.material_registry.end()) {
      mesh_texs.emplace_back(0u);
    }
    // Place the first texture only, fix this later
    const auto& mat = materials.materials[it->second];
    if (mat.textures.count == 1) {
      mesh_texs.emplace_back(materials.material_textures[mat.textures.idx]);
    } else {
      mesh_texs.emplace_back(0u);
    }
  }

  std::vector<ntfr::attribute_binding> att_binds;
  const ntfr::face_cull_opts cull {
    .mode = ntfr::cull_mode::back,
    .front_face = ntfr::cull_face::counter_clockwise,
  };
  const pipeline_opts pip_opts {
    .tests = {
      .stencil_test = nullptr,
      .depth_test = ntfr::def_depth_opts,
      .scissor_test = nullptr,
      .face_culling = cull,
      .blending = ntfr::def_blending_opts,
    },
    .primitive = ntfr::primitive_mode::triangles,
    .use_aos_bindings = false,
  };
  auto pip = r.make_pipeline(VERT_SHADER_RIGGED_MODEL, FRAG_SHADER_RAW_ALBEDO,
                             att_binds, pip_opts);

  render_meta rendering {
    .pip = std::move(*pip),
    .meshes = std::move(*meshes),
    .mesh_texs = std::move(mesh_texs),
  };

  return expect<rigged_model3d>{
    ntf::in_place, std::move(data.name),
    std::move(materials), std::move(armature), std::move(rendering),
    ntf::transform3d<f32>{}.pos(0.f, 0.f, 0.f).scale(1.f, 1.f, 1.f)
  };
}

void rigged_model3d::tick() {
  _armature.rigger.set_root_transform(_transf.world());
  _armature.rigger.tick(_armature.rigs);
}

void rigged_model3d::set_bone_transform(std::string_view name,
                                        const model_rigger::bone_transform& transf)
{
  _armature.rigger.set_transform(_armature.rigs, name, transf);
}

void rigged_model3d::set_bone_transform(std::string_view name, const mat4& transf) {
  _armature.rigger.set_transform(_armature.rigs, name, transf);
}

void rigged_model3d::set_bone_transform(std::string_view name, ntf::transform3d<f32>& transf) {
  set_bone_transform(name, transf.local());
}

u32 rigged_model3d::retrieve_render_data(const scene_render_data& scene,
                                         object_render_data& data)
{
  const u32 mesh_count = _rendering.meshes.mesh_count();
  for (u32 mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {
    auto& mesh = _rendering.meshes.retrieve_mesh_data(mesh_idx, data.meshes);
    mesh.pipeline = _rendering.pip.get();

    const u32 tex = _rendering.mesh_texs[mesh_idx];
    mesh.textures.idx = data.textures.size();
    data.textures.emplace_back(_materials.textures[tex].tex);
    mesh.textures.count = 1u;

    mesh.uniforms.idx = data.uniforms.size();
    const i32 sampler = 0;
    data.uniforms.emplace_back(ntfr::format_uniform_const(renderer::FRAG_SAMPLER_LOC, sampler));
    mesh.uniforms.count = 1u;

    mesh.bindings.idx = data.bindings.size();
    data.bindings.emplace_back(scene.transform, renderer::VERT_SCENE_TRANSFORM_LOC,
                               scene.transform.size(), 0u);
    const u32 rigger_bind_count = _armature.rigger.retrieve_buffer_bindings(data.bindings);
    mesh.bindings.count = rigger_bind_count+1;
  }
  return mesh_count;
}
