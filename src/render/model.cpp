#include "./model.hpp"
#include "./internal.hpp"

namespace kappa::render {

fn model3d_texture::from_asset(const assets::image_data& asset, texture_type type)
  -> s_expect<model3d_texture> {
  const render::texture_create_data data{
    .data = asset.data(),
    .extent = asset.extent(),
    .format = asset.format(),
  };
  model3d_texture out;
  out.texture = render::create_texture(data);
  out.name.copy_from(asset.name().data, asset.name().len);
  out.type = type;
  out.extent = asset.extent();
  return out;
}

fn model3d_texture::from_model(const assets::model3d_data& model)
  -> s_expect<unique_array<model3d_texture>> {
  const size_t count = model.texture_count();
  if (!count) {
    return {in_place, unique_array<model3d_texture>{}};
  }
  auto out = make_zero_array<model3d_texture>(count);
  size_t tex_pos = 0;
  shogle::scope_end on_err = [&]() {
    for (size_t i = 0; i < tex_pos; ++i) {
      render::destroy_texture(out[i].texture);
    }
  };
  for (const auto& model_tex : model.textures()) {
    const render::texture_create_data data{
      .data = model_tex.data,
      .extent = model_tex.extent,
      .format = model_tex.format,
    };
    auto& out_tex = out[tex_pos++];
    out_tex.extent = model_tex.extent;
    out_tex.type = model_tex.type;
    out_tex.texture = render::create_texture(data);
    out_tex.name.copy_from(model_tex.name.data, model_tex.name.len);
  }
  on_err.disengage();
  return {in_place, std::move(out)};
}

model3d_renderable::model3d_renderable(create_t, const buffer_name& name,
                                       buffer_handle mesh_buffer, unique_array<mesh_t>&& meshes,
                                       unique_array<material_t>&& materials,
                                       unique_array<model3d_texture>&& textures,
                                       optional<rig_t>&& rig) :
    _name(name), _mesh_buffer(mesh_buffer), _meshes(std::move(meshes)),
    _materials(std::move(materials)), _textures(std::move(textures)), _rig(std::move(rig)) {}

model3d_renderable::~model3d_renderable() {
  if (!_meshes.empty()) {
    render::destroy_buffer(_mesh_buffer);
    for (auto& tex : _textures) {
      render::destroy_texture(tex.texture);
    }
    for (auto& mat : _materials) {
      render::destroy_pipeline(mat.pipeline);
    }
  }
}

namespace {

fn prealloc_vertex_buffer(const assets::model3d_data& model, model3d_renderable::mesh_t* meshes,
                          const size_t* indices, size_t index_count) -> buffer_handle {
  size_t size = 0;
  // Allocate, in order:
  // - Positions
  // - Normals
  // - Tangents
  // - Biangents
  // - Bone indices
  // - Bone weights
  // - UVs
  // - Indices
  const auto meshes_data = model.meshes();
  assert(meshes_data.size() == model.mesh_count());
  for (size_t i = 0; i < index_count; ++i) {
    const auto& mesh_data = meshes_data[indices[i]];
    auto& mesh = meshes[i];

    const size_t nverts = mesh_data.nverts;
    mesh.vertex_offset = size;
    size += mesh_data.has_positions() * nverts * sizeof(v3f32);
    size += mesh_data.has_normals() * nverts * sizeof(v3f32);
    size += mesh_data.has_tangents() * 2 * nverts * sizeof(v3f32);
    size += mesh_data.has_bones() * (nverts * sizeof(v4i32) + nverts * sizeof(v4f32));
    size += mesh_data.has_uvs(0) * nverts * sizeof(v2f32);

    if (mesh_data.has_indices()) {
      assert(mesh_data.index_count > 0);
      mesh.index_offset = size;
      mesh.index_count = mesh_data.index_count;
      size += mesh_data.index_count * sizeof(u32);
    }

    mesh.vertex_count = nverts;
  }
  if (!size) {
    return nil_handle<buffer_handle>();
  }
  auto buff = render::create_buffer(size);
  if (!buff) {
    logger::error("MODEL_ALLOC: {}", buff.error());
    return nil_handle<buffer_handle>();
  }
  return *buff;
}

} // namespace

auto model3d_renderable::from_asset(const assets::model3d_data& model, asset_filter_fn filter)
  -> s_expect<model3d_renderable> {
  auto texes = model3d_texture::from_model(model);
  if (!texes) {
    return {unexpect, std::move(texes).error()};
  }

  std::vector<size_t> filtered_meshes;
  const auto meshes_data = model.meshes();
  for (size_t i = 0; i < meshes_data.size(); ++i) {
    if (filter(meshes_data[i])) {
      filtered_meshes.emplace_back(i);
    }
  }
  if (filtered_meshes.empty()) {
    return {unexpect, "Filter returned no meshes"};
  }

  auto meshes = make_zero_array<mesh_t>(filtered_meshes.size());
  auto materials = make_zero_array<material_t>(filtered_meshes.size());
  const auto mesh_buffer =
    prealloc_vertex_buffer(model, meshes.data(), filtered_meshes.data(), filtered_meshes.size());
  if (is_nil_handle(mesh_buffer)) {
    return {unexpect, "Failed to allocate mesh buffer"};
  }

  const auto data_meshes = model.meshes();
  size_t mesh_pos = 0, material_pos = 0;
  shogle::scope_end on_err = [&]() {
    render::destroy_buffer(mesh_buffer);
    for (auto& tex : *texes) {
      render::destroy_texture(tex.texture);
    }
    for (size_t i = 0; i < material_pos; ++i) {
      render::destroy_pipeline(materials[i].pipeline);
    }
  };
  on_err.disengage();

  for (; mesh_pos < filtered_meshes.size(); ++mesh_pos) {
    const auto& mesh_data = data_meshes[filtered_meshes[mesh_pos]];
    if (!filter(mesh_data)) {
      continue;
    }
    auto& mesh = meshes[mesh_pos];
    const size_t nverts = mesh_data.nverts;
    size_t buffer_offset = mesh.vertex_offset;
    logger::debug("MESH ({}): {}", mesh_pos, mesh_data.name.as_view());
    const fn upload_data = [&]<typename T>(span<T> data, bits32 flag) {
      assert(data.size() == nverts);
      mesh.attribute_flags |= flag;
      render::update_buffer(mesh_buffer, data.data(), data.size_bytes(), buffer_offset);
      buffer_offset += data.size_bytes();
    };

    // Upload, in order:
    // - Positions
    // - Normals
    // - Tangents
    // - Biangents
    // - Bone indices
    // - Bone weights
    // - UVs
    // - Indices
    if (mesh_data.has_positions()) {
      logger::debug("POSITONS");
      upload_data(model.mesh_positions(mesh_data.positions()), ATTRIB_FLAG_POSITIONS);
    }
    if (mesh_data.has_normals()) {
      logger::debug("NORMALS");
      upload_data(model.mesh_normals(mesh_data.normals()), ATTRIB_FLAG_NORMALS);
    }
    if (mesh_data.has_tangents()) {
      logger::debug("TANGENTS");
      upload_data(model.mesh_tangents(mesh_data.tangents()), ATTRIB_FLAG_TANGENTS);
      upload_data(model.mesh_bitangents(mesh_data.tangents()), ATTRIB_FLAG_TANGENTS);
    }
    if (mesh_data.has_bones()) {
      logger::debug("BONES");
      upload_data(model.mesh_bone_indices(mesh_data.bones()), ATTRIB_FLAG_BONES);
      upload_data(model.mesh_bone_weights(mesh_data.bones()), ATTRIB_FLAG_BONES);
    }
    if (mesh_data.has_uvs(0)) {
      logger::debug("UVS");
      upload_data(model.mesh_uvs(0, mesh_data.uvs(0)), ATTRIB_FLAG_UV0);
    }
    if (mesh_data.has_indices()) {
      logger::debug("INDICES");
      assert(buffer_offset == mesh.index_offset);
      const auto indices = model.mesh_indices(mesh_data.indices());
      assert(indices.size() == mesh_data.index_count);
      render::update_buffer(mesh_buffer, indices.data(), indices.size_bytes(), buffer_offset);
      buffer_offset += indices.size_bytes();
    }

    const auto tex_indices = model.material_textures(mesh_data.material_index);
    bits32 material_texes{};
    if (texes && !texes->empty()) {
      for (const u32 tex_idx : tex_indices) {
        assert(tex_idx < texes->size());
        material_texes |= flag_from_tex_type((*texes)[tex_idx].type);
      }
    }
    mesh.name.copy_from(mesh_data.name.data, mesh_data.name.len);

    assert(!is_nil_handle(mesh_buffer));
    const pipeline_create_data pip_data{
      .buffer = mesh_buffer,
      .nverts = nverts,
      .vertex_offset = mesh.vertex_offset,
      .index_offset = mesh.index_offset,
      .vertex_attributes = mesh.attribute_flags,
      .material_textures = material_texes,
    };
    auto pip = render::create_pipeline(pip_data);
    if (!pip) {
      return {unexpect, std::move(pip).error()};
    }
    auto& material = materials[material_pos++];
    material.pipeline = *pip;
    material.texture_count = (u32)tex_indices.size();
    material.texture_flags = material_texes;
    std::memcpy(material.textures.data(), tex_indices.data(), tex_indices.size() * sizeof(u32));
  }

  optional<rig_t> rig(nullopt);
  if (model.has_bones()) {
    const auto bones = model.bones();
    rig.emplace();
    rig->bones = shogle::make_array<bone_t>(shogle::uninitialized, bones.size());
    rig->bone_locals = shogle::make_array<m4f32>(shogle::uninitialized, bones.size());
    rig->bone_inv_models = shogle::make_array<m4f32>(shogle::uninitialized, bones.size());
    std::memcpy(rig->bone_locals.data(), model.bone_locals().data(), bones.size() * sizeof(m4f32));
    std::memcpy(rig->bone_inv_models.data(), model.bone_inverse_models().data(),
                bones.size() * sizeof(m4f32));
    for (size_t i = 0; i < bones.size(); ++i) {
      rig->bones[i].parent = bones[i].parent;
      rig->bones[i].name.copy_from(bones[i].name.data, bones[i].name.len);
      rig->bone_map.emplace(rig->bones[i].name.as_view(), static_cast<u32>(i));
    }
  }

  return {in_place,          create_t{},           model.name(),      mesh_buffer,
          std::move(meshes), std::move(materials), std::move(*texes), std::move(rig)};
}

std::string_view model3d_renderable::name() const {
  return _name.as_view();
}

fn model3d_renderable::materials() const -> span<const material_t> {
  return {_materials.data(), _materials.size()};
}

span<const model3d_renderable::mesh_t> model3d_renderable::meshes() const {
  return {_meshes.data(), _meshes.size()};
}

optional<u32> model3d_renderable::find_bone_idx(std::string_view name) const {
  if (!_rig.has_value()) {
    return nullopt;
  }
  if (_rig->bones.empty()) {
    return nullopt;
  }
  auto it = _rig->bone_map.find(name);
  if (it == _rig->bone_map.end()) {
    return nullopt;
  }
  return it->second;
}

span<const model3d_renderable::bone_t> model3d_renderable::bones() const {
  if (!_rig.has_value()) {
    return {};
  }
  return {_rig->bones.data(), _rig->bones.size()};
}

span<const m4f32> model3d_renderable::bone_locals() const {
  if (!_rig.has_value()) {
    return {};
  }
  return {_rig->bone_locals.data(), _rig->bone_locals.size()};
}

span<const m4f32> model3d_renderable::bone_inv_models() const {
  if (!_rig.has_value()) {
    return {};
  }
  return {_rig->bone_inv_models.data(), _rig->bone_inv_models.size()};
}

} // namespace kappa::render
