#include "./model.hpp"
#include "./internal.hpp"
#include "instance.hpp"

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
                                       unique_array<mesh_t>&& meshes,
                                       unique_array<material_t>&& materials,
                                       unique_array<model3d_texture>&& textures,
                                       optional<rig_t>&& rig) :
    _name(name), _meshes(std::move(meshes)), _materials(std::move(materials)),
    _textures(std::move(textures)), _rig(std::move(rig)) {}

model3d_renderable::~model3d_renderable() {
  if (!_meshes.empty()) {
    for (auto& mesh : _meshes) {
      render::destroy_buffer(mesh.attribute_buffer);
    }
    for (auto& tex : _textures) {
      render::destroy_texture(tex.texture);
    }
    for (auto& mat : _materials) {
      render::destroy_pipeline(mat.pipeline);
    }
  }
}

auto model3d_renderable::from_asset(const assets::model3d_data& model)
  -> s_expect<model3d_renderable> {
  auto texes = model3d_texture::from_model(model);
  if (!texes) {
    return {unexpect, std::move(texes).error()};
  }

  auto meshes = make_zero_array<mesh_t>(model.mesh_count());
  auto materials = make_zero_array<material_t>(model.mesh_count());

  const auto data_meshes = model.meshes();
  size_t mesh_pos = 0, material_pos = 0;
  shogle::scope_end on_err = [&]() {
    for (auto& tex : *texes) {
      render::destroy_texture(tex.texture);
    }
    for (size_t i = 0; i < mesh_pos; ++i) {
      render::destroy_buffer(meshes[i].attribute_buffer);
    }
    for (size_t i = 0; i < material_pos; ++i) {
      render::destroy_pipeline(materials[i].pipeline);
    }
  };
  on_err.disengage();

  struct mesh_create_data {
    size_t buffer_size;
    size_t nverts;
    const v3f32* positions;
    const v3f32* normals;
    const v2f32* uvs;
    const v3f32* tangents;
    const v3f32* bitangents;
    const v4i32* bone_indices;
    const v4f32* bone_weights;
    size_t index_count;
    const u32* indices;
    bits32 attribs;
  };

  const fn create_mesh = [](mesh_t& mesh, const mesh_create_data& data,
                            std::string_view name) -> s_expect<void> {
    return render::create_buffer(data.buffer_size).transform([&](buffer_handle buffer) {
      size_t offset = 0;

      assert((data.attribs & ATTRIB_FLAG_POSITIONS) && data.positions);
      render::update_buffer(buffer, data.positions, data.nverts * sizeof(v3f32), offset);
      offset += data.nverts * sizeof(v3f32);

      if (data.attribs & ATTRIB_FLAG_NORMALS) {
        assert(data.normals);
        render::update_buffer(buffer, data.normals, data.nverts * sizeof(v3f32), offset);
        offset += data.nverts * sizeof(v3f32);
      }

      if (data.attribs & ATTRIB_FLAG_TANGENTS) {
        assert(data.tangents && data.bitangents);
        render::update_buffer(buffer, data.tangents, data.nverts * sizeof(v3f32), offset);
        offset += data.nverts * sizeof(v3f32);
        render::update_buffer(buffer, data.bitangents, data.nverts * sizeof(v3f32), offset);
        offset += data.nverts * sizeof(v3f32);
      }

      if (data.attribs & ATTRIB_FLAG_BONES) {
        assert(data.bone_indices && data.bone_weights);
        render::update_buffer(buffer, data.bone_indices, data.nverts * sizeof(v4i32), offset);
        offset += data.nverts * sizeof(v4i32);
        render::update_buffer(buffer, data.bone_weights, data.nverts * sizeof(v4f32), offset);
        offset += data.nverts * sizeof(v4f32);
      }

      if (data.attribs & ATTRIB_FLAG_UV0) {
        assert(data.uvs);
        render::update_buffer(buffer, data.uvs, data.nverts * sizeof(v2f32), offset);
        offset += data.nverts * sizeof(v3f32);
      }

      if (data.indices) {
        assert(data.index_count > 0);
        render::update_buffer(buffer, data.indices, data.index_count * sizeof(u32), offset);
        mesh.index_offset = offset;
        mesh.index_count = (u32)data.index_count;
      } else {
        mesh.index_offset = 0;
        mesh.index_count = 0;
      }

      mesh.attribute_buffer = buffer;
      mesh.attribute_flags = data.attribs;
      mesh.vertex_count = (u32)data.nverts;
      mesh.name.copy_from(name.data(), name.size());
    });
  };

  for (; mesh_pos < model.mesh_count(); ++mesh_pos) {
    const auto& mesh = data_meshes[mesh_pos];
    const size_t nverts = mesh.nverts;

    mesh_create_data input{};
    input.nverts = nverts;

    input.positions = model.mesh_positions(mesh.positions()).data();
    input.buffer_size += nverts * sizeof(v3f32);

    if (mesh.has_normals()) {
      input.normals = model.mesh_normals(mesh.normals()).data();
      input.attribs |= ATTRIB_FLAG_NORMALS;
    }
    if (mesh.has_tangents()) {
      input.tangents = model.mesh_tangents(mesh.tangents()).data();
      input.bitangents = model.mesh_bitangents(mesh.tangents()).data();
      input.attribs |= ATTRIB_FLAG_TANGENTS;
    }
    if (mesh.has_bones()) {
      input.bone_indices = model.mesh_bone_indices(mesh.bones()).data();
      input.bone_weights = model.mesh_bone_weights(mesh.bones()).data();
      input.attribs |= ATTRIB_FLAG_BONES;
    }
    if (mesh.has_uvs(0)) {
      input.uvs = model.mesh_uvs(0, mesh.uvs(0)).data();
      input.attribs |= ATTRIB_FLAG_UV0;
    }

    if (mesh.has_indices()) {
      input.indices = model.mesh_indices(mesh.indices()).data();
      input.index_count = mesh.index_count;
    }

    const auto tex_indices = model.material_textures(mesh.material_index);
    bits32 material_texes{};
    if (texes && !texes->empty()) {
      for (const u32 tex_idx : tex_indices) {
        assert(tex_idx < texes->size());
        material_texes |= flag_from_tex_type((*texes)[tex_idx].type);
      }
    }
    const pipeline_create_data pip_data{
      .vertex_attributes = input.attribs,
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

    auto res = create_mesh(meshes[mesh_pos], input, mesh.name.as_view());
    if (!res) {
      return {unexpect, std::move(res).error()};
    }
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

  return {in_place,          create_t{},    model.name(), std::move(meshes), std::move(materials),
          std::move(*texes), std::move(rig)};
}

std::string_view model3d_renderable::name() const {
  return _name.as_view();
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
