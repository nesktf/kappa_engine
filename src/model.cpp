#include "model.hpp"
#include "renderer.hpp"

#define CHECK_ERR_BUFF(_buff) \
if (!_buff) { \
  ntf::logger::error("Failed to create vertex buffer, {}", _buff.error().what()); \
  free_buffs(); \
  return ntf::unexpected{std::string_view{"Failed to create vertex buffer"}}; \
}

model_mesher::model_mesher(vert_buffs buffs,
                           ntfr::index_buffer&& index_buff,
                           ntf::unique_array<mesh_offset>&& mesh_offsets) noexcept :
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

model_mesher::model_mesher(model_mesher&& other) noexcept :
  _buffs{std::move(other._buffs)}, _binds{std::move(other._binds)},
  _mesh_offsets{std::move(other._mesh_offsets)}, _index_buff{std::move(other._index_buff)},
  _active_layouts{std::move(other._active_layouts)}
{
  other._buffs[0] = nullptr;
}

model_mesher& model_mesher::operator=(model_mesher&& other) noexcept {
  _free_buffs();

  _buffs = std::move(other._buffs);
  _binds = std::move(other._binds);
  _mesh_offsets = std::move(other._mesh_offsets);
  _index_buff = std::move(other._index_buff);
  _active_layouts = std::move(other._active_layouts);

  other._buffs[0] = nullptr;

  return *this;
}

model_mesher::~model_mesher() noexcept { _free_buffs(); }


expect<model_mesher> model_mesher::create(const model_mesh_data& meshes)
{
  // Assume all models have indices and positions in each mesh
  if (meshes.positions.empty()) {
    return ntf::unexpected{std::string_view{"No position data"}};
  }
  if (meshes.indices.empty()) {
    return ntf::unexpected{std::string_view{"No index data"}};
  }

  auto ctx = renderer::instance().ctx();

  vert_buffs buffs{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
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

  ntf::unique_array<mesh_offset> mesh_offsets{ntf::uninitialized, meshes.meshes.size()};
  for (u32 i = 0u; const auto& mesh : meshes.meshes) {
    NTF_ASSERT(!mesh.indices.empty());
    std::construct_at(mesh_offsets.data()+i,
                      mesh.indices.idx, mesh.indices.count, 0u);
    ++i;
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

  return expect<model_mesher>{
    ntf::in_place, buffs, std::move(*ind), std::move(mesh_offsets)
  };
}

void model_mesher::_free_buffs() noexcept {
  if (!_buffs[0]) {
    return;
  }
  for (ntfr::buffer_t buff : _buffs){
    if (buff) {
      ntfr::destroy_buffer(buff);
    }
  }
}

model_texturer::model_texturer(ntf::unique_array<texture_t>&& textures,
                               std::unordered_map<std::string_view, u32>&& tex_reg,
                               ntf::unique_array<vec_span>&& mat_spans,
                               ntf::unique_array<u32>&& mat_texes) noexcept :
  _textures{std::move(textures)}, _tex_reg{std::move(tex_reg)},
  _mat_spans{std::move(mat_spans)}, _mat_texes{std::move(mat_texes)} {}


template<u32 tex_extent>
[[maybe_unused]] static constexpr auto missing_albedo_bitmap = []{
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

template<u32 tex_extent = 16u>
static ntfr::expect<ntfr::texture2d> make_missing_albedo(ntfr::context_view ctx) {
  const ntfr::image_data image {
    .bitmap = missing_albedo_bitmap<tex_extent>.data(),
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

expect<model_texturer> model_texturer::create(const model_material_data& mat_data) {
  auto ctx = renderer::instance().ctx();

  ntf::unique_array<u32> mat_texes{ntf::uninitialized, mat_data.material_textures.size()};
  for (u32 i = 0u; i < mat_data.material_textures.size(); ++i){
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
    const ntfr::image_data image {
      .bitmap = tex.bitmap.data(),
      .format = tex.format,
      .alignment = 4u,
      .extent = tex.extent,
      .offset = {0, 0, 0},
      .layer = 0u,
      .level = 0u,
    };
    const ntfr::texture_data data {
      .images = {image},
      .generate_mipmaps = true,
    };
    auto tex2d = ntfr::texture2d::create(ctx, {
      .format = ntfr::image_format::rgba8nu,
      .sampler = ntfr::texture_sampler::linear,
      .addressing = ntfr::texture_addressing::repeat,
      .extent = tex.extent,
      .layers = 1u,
      .levels = 7u,
      .data = data,
    });
    if (!tex2d) {
      ntf::logger::error("Failed to upload texture \"{}\" ({})", tex.name, i);
      return {ntf::unexpect, "Failed to upload textures"};
    }

    const u32 sampler = TEX_SAMPLER_ALBEDO;
    std::construct_at(textures.data()+i,
                      tex.name, std::move(*tex2d), sampler);
    auto [_, empl] = tex_reg.try_emplace(textures[i].name, i);
    NTF_ASSERT(empl);
    ++i;
  }

  return {
    ntf::in_place,
    std::move(textures), std::move(tex_reg), std::move(mat_spans), std::move(mat_texes)
  };
}

ntf::optional<u32> model_texturer::find_texture_idx(std::string_view name) const {
  auto it = _tex_reg.find(name);
  if (it == _tex_reg.end()) {
    return ntf::nullopt;
  }
  return it->second;
}

ntfr::texture2d_view model_texturer::find_texture(std::string_view name) {
  auto idx = find_texture_idx(name);
  if (!idx) {
    return {};
  }
  NTF_ASSERT(*idx < _textures.size());
  return _textures[*idx].tex;
}

u32 model_texturer::retrieve_material_textures(u32 mat_idx,
                                                std::vector<ntfr::texture_binding>& texs) const
{
  NTF_ASSERT(mat_idx < _mat_spans.size());

  const auto tex_span = _mat_spans[mat_idx].to_cspan(_mat_texes.data());
  for (const u32 tex_idx : tex_span) {
    NTF_ASSERT(tex_idx < _textures.size());
    const auto& texture = _textures[tex_idx];
    texs.emplace_back(texture.tex.get(), texture.sampler);
  }

  return tex_span.size();
}

mesh_render_data& model_mesher::retrieve_mesh_data(u32 idx,
                                                    std::vector<mesh_render_data>& data)
{
  NTF_ASSERT(idx < _mesh_offsets.size());
  cspan<ntfr::vertex_binding> bindings{_binds.data(), _active_layouts};
  const auto& offset = _mesh_offsets[idx];
  return data.emplace_back(bindings, _index_buff,
                           offset.index_count, offset.vertex_offset, offset.index_offset, 0u);
}

model_rigger::model_rigger(ntfr::shader_storage_buffer&& ssbo,
                           ntf::unique_array<bone_t>&& bones,
                           std::unordered_map<std::string_view, u32>&& bone_reg,
                           ntf::unique_array<mat4>&& bone_locals,
                           ntf::unique_array<mat4>&& bone_inv_models,
                           ntf::unique_array<mat4>&& bone_transforms,
                           ntf::unique_array<mat4>&& transf_cache,
                           ntf::unique_array<mat4>&& transf_output) noexcept :
  _ssbo{std::move(ssbo)}, _bones{std::move(bones)}, _bone_reg{std::move(bone_reg)},
  _bone_locals{std::move(bone_locals)}, _bone_inv_models{std::move(bone_inv_models)},
  _bone_transforms{std::move(bone_transforms)}, _transf_cache{std::move(transf_cache)},
  _transf_output{std::move(transf_output)} {}

expect<model_rigger> model_rigger::create(const model_rig_data& rigs, std::string_view armature) {
  auto it = rigs.armature_registry.find(armature);
  if (it == rigs.armature_registry.end()) {
    return {ntf::unexpect, "Armature not found"};
  }
  auto ctx = renderer::instance().ctx();

  const u32 armature_idx = it->second;
  NTF_ASSERT(rigs.armatures.size() > armature_idx);
  const auto bone_vspan = rigs.armatures[armature_idx].bones;
  try {
    ntf::unique_array<bone_t> bones{ntf::uninitialized, bone_vspan.size()};
    ntf::unique_array<mat4> bone_locals{ntf::uninitialized, bone_vspan.size()};
    ntf::unique_array<mat4> bone_inv_models{ntf::uninitialized, bone_vspan.size()};
    std::unordered_map<std::string_view, u32> bone_reg;
    bone_reg.reserve(bone_vspan.size());
    for (u32 i = 0u; i < bone_vspan.count; ++i) {
      const u32 idx = bone_vspan.idx+i;

      const auto& bone = rigs.bones[idx];
      const u32 local_parent = bone.parent-bone_vspan.idx;
      if (local_parent != vec_span::INDEX_TOMB) {
        NTF_ASSERT(local_parent < bones.size());
      }
      std::construct_at(bones.data()+i,
                        bone.name, local_parent);
      auto [_, empl] = bone_reg.try_emplace(bones[i].name, i);
      NTF_ASSERT(empl);

      std::construct_at(bone_locals.data()+i, rigs.bone_locals[idx]);
      std::construct_at(bone_inv_models.data()+i, rigs.bone_inv_models[idx]);
    }

    constexpr mat4 identity{1.f};
    ntf::unique_array<mat4> bone_transforms{bone_vspan.size(), identity};
    ntf::unique_array<mat4> transf_cache{bone_vspan.size()*2u, identity}; // for model and locals

    ntf::unique_array<mat4> transf_output{bone_vspan.size(), identity};
    const ntfr::buffer_data initial_data {
      .data = transf_output.data(),
      .size = transf_output.size()*sizeof(mat4),
      .offset = 0u,
    };
    auto ssbo = ntfr::shader_storage_buffer::create(ctx, {
      .flags = ntfr::buffer_flag::dynamic_storage,
      .size = transf_output.size()*sizeof(mat4),
      .data = initial_data,
    });
    if (!ssbo) {
      return {ntf::unexpect, ssbo.error().msg()};
    }

    return {
      ntf::in_place,
      std::move(*ssbo), std::move(bones), std::move(bone_reg),
      std::move(bone_locals), std::move(bone_inv_models), std::move(bone_transforms),
      std::move(transf_cache), std::move(transf_output)
    };
  } catch (const std::bad_alloc&) {
    return {ntf::unexpect, "Allocation failure"};
  } catch (...) {
    return {ntf::unexpect, "Unknown error"};
  }
}

void model_rigger::tick_bones(const mat4& root) {
  const u32 model_offset = _bones.size(); // First elements are locals, the rest models
  NTF_ASSERT(_transf_cache.size() == 2*model_offset);
  
  // Populate bone local transforms
  _transf_cache[0] = root*_bone_locals[0]*_bone_transforms[0];
  for (u32 i = 1; i < _bones.size(); ++i) {
    _transf_cache[i] = _bone_locals[i]*_bone_transforms[i];
  }

  // Populate bone model transforms
  _transf_cache[model_offset] = _transf_cache[0]; // Root transform
  for (u32 i = 1; i < _bones.size(); ++i) {
    const u32 parent = _bones[i].parent;
    // Since the bone hierarchy is sorted, we should be able to read the parent model matrix safely
    NTF_ASSERT(parent < model_offset);
    _transf_cache[model_offset+i] = _transf_cache[model_offset+parent]*_transf_cache[i];
  }

  // Prepare shader transform data
  for (u32 i = 0; i < _bones.size(); ++i){
    _transf_output[i] = _transf_cache[model_offset+i]*_bone_inv_models[i];
  }
}

bool model_rigger::set_transform(std::string_view bone, const bone_transform& transf) {
  auto it = _bone_reg.find(bone);
  if (it == _bone_reg.end()) {
    return false;
  }
  NTF_ASSERT(it->second < _bone_transforms.size());

  constexpr vec3 pivot{0.f, 0.f, 0.f};
  _bone_transforms[it->second] = ntf::build_trs_matrix(transf.pos, transf.scale,
                                                       pivot, transf.rot);
  return true;
}

bool model_rigger::set_transform(std::string_view bone, const mat4& transf) {
  auto it = _bone_reg.find(bone);
  if (it == _bone_reg.end()) {
    return false;
  }
  NTF_ASSERT(it->second < _bone_transforms.size());

  _bone_transforms[it->second] = transf;
  return true;
}

void model_rigger::set_transform(u32 bone, const mat4& transf) {
  NTF_ASSERT(bone < _bone_transforms.size());
  _bone_transforms[bone] = transf;
}

void model_rigger::set_transform(u32 bone, const bone_transform& transf) {
  NTF_ASSERT(bone < _bone_transforms.size());
  constexpr vec3 pivot{0.f, 0.f, 0.f};
  _bone_transforms[bone] = ntf::build_trs_matrix(transf.pos, transf.scale,
                                                 pivot, transf.rot);
}

void model_rigger::set_transform(u32 bone, ntf::transform3d<f32>& transf) {
  NTF_ASSERT(bone < _bone_transforms.size());
  _bone_transforms[bone] = transf.local();
}

ntf::optional<u32> model_rigger::find_bone(std::string_view name) {
  auto it = _bone_reg.find(name);
  if (it == _bone_reg.end()) {
    return ntf::nullopt;
  }
  NTF_ASSERT(it->second < _bone_transforms.size());
  return it->second;
}

bool model_rigger::set_transform(std::string_view bone, ntf::transform3d<f32>& transf) {
  return set_transform(bone, transf.local());
}

void model_rigger::apply_animation(const model_anim_data& anims, std::string_view name, u32 tick) {
  auto it = anims.animation_registry.find(name);
  if (it == anims.animation_registry.end()) {
    return;
  }
  const auto& anim = anims.animations[it->second];
  const f64 t = std::fmod(anim.tps*tick, anim.duration);
}

u32 model_rigger::retrieve_buffer_bindings(std::vector<ntfr::shader_binding>& binds) const {
  const ntfr::buffer_data data {
    .data = _transf_output.data(),
    .size = _transf_output.size()*sizeof(mat4),
    .offset = 0u,
  };
  [[maybe_unused]] auto ret = _ssbo.upload(data);
  NTF_ASSERT(ret);
  binds.emplace_back(_ssbo.get(), renderer::VERT_MODEL_TRANSFORM_LOC, _ssbo.size(), 0u);
  return 1u;
}

rigged_model3d::rigged_model3d(model_mesher&& meshes,
                               model_texturer&& texturer,
                               model_rigger&& rigger,
                               std::vector<model_material_data::material_meta>&& mats,
                               std::unordered_map<std::string_view, u32>&& mat_reg,
                               ntfr::pipeline pip,
                               std::vector<u32> mesh_texs,
                               std::string&& name) noexcept :
  model_mesher{std::move(meshes)}, model_texturer{std::move(texturer)},
  model_rigger{std::move(rigger)},
  _mats{std::move(mats)}, _mat_reg{std::move(mat_reg)},
  _pip{std::move(pip)}, _mesh_mats{std::move(mesh_texs)},
  _name{std::move(name)}, _transf{} {}

expect<rigged_model3d> rigged_model3d::create(data_t&& data) {
  auto& r = renderer::instance();

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
  if (!pip) {
    return {ntf::unexpect, pip.error()};
  }

  auto mesher = model_mesher::create(data.meshes);
  if (!mesher) {
    return {ntf::unexpect, mesher.error()};
  }

  auto texturer = model_texturer::create(data.mats);
  if (!texturer) {
    return {ntf::unexpect, texturer.error()};
  }

  auto rigger = model_rigger::create(data.rigs, data.armature);
  if (!rigger) {
    return {ntf::unexpect, rigger.error()};
  }

  std::vector<u32> mesh_mats;
  mesh_mats.reserve(data.meshes.meshes.size());
  for (const auto& mesh : data.meshes.meshes) {
    auto it = data.mats.material_registry.find(mesh.material_name);
    if (it == data.mats.material_registry.end()) {
      mesh_mats.emplace_back(0u);
    }
    mesh_mats.emplace_back(it->second);
    // // Place the first texture only, fix this later
    // const auto& mat = data.mats.materials[it->second];
    // if (mat.textures.count == 1) {
    //   mesh_mats.emplace_back(data.mats.material_textures[mat.textures.idx]);
    // } else {
    //   mesh_mats.emplace_back(0u);
    // }
  }

  return {
    ntf::in_place,
    std::move(*mesher), std::move(*texturer), std::move(*rigger),
    std::move(data.mats.materials), std::move(data.mats.material_registry),
    std::move(*pip), std::move(mesh_mats), std::move(data.name)
  };
}

void rigged_model3d::tick() {
  model_rigger::tick_bones(_transf.local());
}

u32 rigged_model3d::retrieve_render_data(const scene_render_data& scene,
                                         object_render_data& data)
{
  const u32 mesh_count = model_mesher::mesh_count();
  for (u32 mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {
    auto& mesh = retrieve_mesh_data(mesh_idx, data.meshes);
    mesh.pipeline = _pip.get();

    const i32 sampler = 0;
    const u32 mat_idx = _mesh_mats[mesh_idx];
    mesh.textures.idx = data.textures.size();
    const u32 tex_count = model_texturer::retrieve_material_textures(mat_idx, data.textures);
    mesh.textures.count = tex_count;

    mesh.uniforms.idx = data.uniforms.size();
    data.uniforms.emplace_back(ntfr::format_uniform_const(renderer::FRAG_SAMPLER_LOC, sampler));
    mesh.uniforms.count = 1u;

    mesh.bindings.idx = data.bindings.size();
    data.bindings.emplace_back(scene.transform, renderer::VERT_SCENE_TRANSFORM_LOC,
                               scene.transform.size(), 0u);
    const u32 rigger_bind_count = model_rigger::retrieve_buffer_bindings(data.bindings);
    mesh.bindings.count = rigger_bind_count+1;
  }
  return mesh_count;
}
