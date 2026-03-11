#include "./model.hpp"
#include "./internal.hpp"

namespace kappa::render {

namespace {

enum material_tex_flag : bits32 {
  MATERIAL_TEX_NONE = 0x0000,
  MATERIAL_TEX_DIFFUSE = 0x0001,
  MATERIAL_TEX_ALBEDO = 0x0001,
  MATERIAL_TEX_SPECULAR = 0x0002,
  MATERIAL_TEX_NORMAL = 0x0004,
  MATERIAL_TEX_AO = 0x0008,
  MATERIAL_TEX_ROUGHNESS = 0x0010,
  MATERIAL_TEX_METALLIC = 0x0020,
};

auto collect_textures(const assets::model3d_data& data)
  -> unique_array<model3d_renderable::texture_t> {}

struct mesh_create_data {
  size_t vertex_count;
  const v3f32* positions;
  const v3f32* normals;
  const v2f32* uvs;
  const v3f32* tangents;
  const v3f32* bitangents;
  const v4i32* bone_indices;
  const v4f32* bone_weights;
  size_t index_count;
  const u32* indices;
  bits32 texture_bits;
  span<texture_create_data> textures;
  assets::model3d_data::material_shading shading;
};

s_expect<void> create_mesh(model3d_renderable::mesh_t& mesh, const mesh_create_data& data) {}

void destroy_mesh(model3d_renderable::mesh_t& mesh) {}

auto collect_meshes(span<model3d_renderable::texture_t> texes, const assets::model3d_data& data)
  -> unique_array<model3d_renderable::mesh_t> {
  static_assert(std::is_trivial_v<model3d_renderable::mesh_t>);
  auto meshes =
    shogle::make_array<model3d_renderable::mesh_t>(shogle::mem::uninitialized, data.mesh_count());
  std::memset(meshes.data(), 0x00, sizeof(meshes[0]) * data.mesh_count());
  const auto data_meshes = data.meshes();
  size_t mesh_pos = 0;
  shogle::scope_end mesh_err_cleanup = [&]() {
    for (size_t i = 0; i < mesh_pos; ++i) {
      destroy_mesh(meshes[i]);
    }
  };

  for (; mesh_pos < data.mesh_count(); ++mesh_pos) {
    const auto& mesh = data_meshes[mesh_pos];
    mesh_create_data input{};
    input.vertex_count = mesh.nverts;
    input.positions = data.mesh_positions(mesh.positions()).data();
    if (mesh.has_uvs(0)) {
      input.uvs = data.mesh_uvs(0, mesh.uvs(0)).data();
    }
    if (mesh.has_normals()) {
      input.normals = data.mesh_normals(mesh.normals()).data();
    }
    if (mesh.has_tangents()) {
      input.tangents = data.mesh_tangents(mesh.tangents()).data();
      input.bitangents = data.mesh_bitangents(mesh.tangents()).data();
    }
    if (mesh.has_bones()) {
      input.bone_indices = data.mesh_bone_indices(mesh.bones()).data();
      input.bone_weights = data.mesh_bone_weights(mesh.bones()).data();
    }

    if (mesh.has_indices()) {
      input.indices = data.mesh_indices(mesh.indices()).data();
      input.index_count = mesh.index_count;
    }

    const auto texes = data.material_textures(mesh.material_index);
    for (const auto& tex : texes) {
    }

    auto res = create_mesh(meshes[mesh_pos], input);
    if (!res) {
      return {};
    }
  }
  mesh_err_cleanup.disengage();
  return meshes;
}

} // namespace

model3d_renderable::model3d_renderable(create_t, const buffer_name& name,
                                       unique_array<mesh_t>&& meshes, rig_t&& rig) :
    _name(name), _meshes(std::move(meshes)), _rig(std::move(rig)) {}

s_expect<model3d_renderable> model3d_renderable::load_model(const assets::model3d_data& data) {
  auto texes = collect_textures(data);
  auto meshes = collect_meshes({texes.data(), texes.size()}, data);

  rig_t rig{};
  if (data.has_bones()) {
    const auto bones = data.bones();
    rig.bones = shogle::make_array<bone_t>(shogle::mem::uninitialized, bones.size());
    rig.bone_locals = shogle::make_array<m4f32>(shogle::mem::uninitialized, bones.size());
    rig.bone_inv_models = shogle::make_array<m4f32>(shogle::mem::uninitialized, bones.size());
    std::memcpy(rig.bone_locals.data(), data.bone_locals().data(), bones.size() * sizeof(m4f32));
    std::memcpy(rig.bone_inv_models.data(), data.bone_inverse_models().data(),
                bones.size() * sizeof(m4f32));
    for (size_t i = 0; i < bones.size(); ++i) {
      rig.bones[i].parent = bones[i].parent;
      rig.bones[i].name.copy_from(bones[i].name.data, bones[i].name.len);
      rig.bone_map.emplace(rig.bones[i].name.as_view(), static_cast<u32>(i));
    }
  }

  return {in_place, create_t{}, data.name(), std::move(meshes), std::move(rig)};
}

std::string_view model3d_renderable::name() const {
  return _name.as_view();
}

span<const model3d_renderable::mesh_t> model3d_renderable::meshes() const {
  return {_meshes.data(), _meshes.size()};
}

optional<u32> model3d_renderable::find_bone_idx(std::string_view name) const {
  if (_rig.bones.empty()) {
    return nullopt;
  }
  auto it = _rig.bone_map.find(name);
  if (it == _rig.bone_map.end()) {
    return nullopt;
  }
  return it->second;
}

span<const model3d_renderable::bone_t> model3d_renderable::bones() const {
  return {_rig.bones.data(), _rig.bones.size()};
}

span<const m4f32> model3d_renderable::bone_locals() const {
  return {_rig.bone_locals.data(), _rig.bone_locals.size()};
}

span<const m4f32> model3d_renderable::bone_inv_models() const {
  return {_rig.bone_inv_models.data(), _rig.bone_inv_models.size()};
}

} // namespace kappa::render
