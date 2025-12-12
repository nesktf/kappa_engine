#include "./model.hpp"

namespace kappa::scene {

namespace {

static constexpr i32 VERT_MODEL_TRANSFORM_LOC = 1;
static constexpr i32 VERT_SCENE_TRANSFORM_LOC = 2;

static constexpr i32 FRAG_SAMPLER_LOC = 8;

} // namespace

rigged_model::rigged_model(u32 model, const vec3& pos, real mass,
                           shogle::shader_storage_buffer&& bone_buffer,
                           ntf::unique_array<mat4>&& bone_transforms,
                           shogle::transform3d<f32> transform) :
    _bone_buffer{std::move(bone_buffer)}, _bone_transforms{std::move(bone_transforms)},
    _particle{pos, mass}, _transform{transform}, _model{model} {}

u32 rigged_model::retrieve_buffer_bindings(std::vector<shogle::shader_binding>& binds) const {
  binds.emplace_back(_bone_buffer.get(), VERT_MODEL_TRANSFORM_LOC, _bone_buffer.size(), 0u);
  return 1u;
}

void rigged_model::update_bones(ntf::span<mat4> transform_cache, ntf::cspan<mat4> bone_locals,
                                ntf::cspan<mat4> bone_invs,
                                ntf::cspan<assets::rigged_model_bone> bones) {
  NTF_ASSERT(bone_locals.size() == bones.size());
  NTF_ASSERT(bone_invs.size() == bones.size());
  NTF_ASSERT(_bone_transforms.size() == bones.size());

  const u32 offset = bones.size(); // First elements are locals, the rest models
  NTF_ASSERT(transform_cache.size() == 2 * offset);

  // Populate bone local transforms
  const mat4 root_transform = _transform.local();
  transform_cache[0] = root_transform * bone_locals[0] * _bone_transforms[0];
  for (u32 i = 1; i < bones.size(); ++i) {
    transform_cache[i] = bone_locals[i] * _bone_transforms[i];
  }

  // Populate bone model transforms
  transform_cache[offset] = transform_cache[0]; // Root transform
  for (u32 i = 1; i < bones.size(); ++i) {
    const u32 parent = bones[i].parent;
    // Since the bone hierarchy is sorted, we should be able to read the parent model matrix safely
    NTF_ASSERT(parent < offset);
    transform_cache[offset + i] = transform_cache[offset + parent] * transform_cache[i];
  }

  // Prepare shader transform data
  for (u32 i = 0; i < bones.size(); ++i) {
    _bone_transforms[i] = transform_cache[offset + i] * bone_invs[i];
  }

  // Upload matrices
  const shogle::buffer_data upload_data{
    .data = _bone_transforms.data(),
    .size = _bone_transforms.size() * sizeof(mat4),
    .offset = 0u,
  };
  _bone_buffer.upload(upload_data).value();
}

void entity_registry::update() {
  _rigged_instances.for_each([&](rigged_model& instance) {
    const u32 model_idx = instance.model_idx();
    NTF_ASSERT(model_idx < _rmodels.size());
    const auto& model = _rmodels[model_idx];

    const auto [locals, invs, bones] = model.bones();
    const ntf::span<mat4> cache{_rig_cache.data(), _rig_cache.size()};

    _rig_cache.resize(2 * bones.size());
    instance.update_bones(cache, locals, invs, bones);
  });
}

u32 entity_registry::retrieve_render_data(const render::scene_render_data& scene,
                                          render::object_render_data& data) {
  u32 total_meshes = 0u;
  _rigged_instances.for_each([&](rigged_model& instance) {
    const u32 model_idx = instance.model_idx();
    NTF_ASSERT(model_idx < _rmodels.size());
    const auto& model = _rmodels[model_idx];

    const u32 mesh_count = model.mesh_count();
    for (u32 mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {
      auto& mesh = model.retrieve_mesh_data(mesh_idx, data.meshes);
      mesh.pipeline = model.pipeline();

      const i32 sampler = 0;
      const u32 mat_idx = model.mat_idx(mesh_idx);
      mesh.textures.idx = data.textures.size();
      const u32 tex_count = model.retrieve_material_textures(mat_idx, data.textures);
      mesh.textures.count = tex_count;

      mesh.uniforms.idx = data.uniforms.size();
      data.uniforms.emplace_back(shogle::format_uniform_const(FRAG_SAMPLER_LOC, sampler));
      mesh.uniforms.count = 1u;

      mesh.bindings.idx = data.bindings.size();
      data.bindings.emplace_back(scene.transform, VERT_SCENE_TRANSFORM_LOC, scene.transform.size(),
                                 0u);
      const u32 rigger_bind_count = instance.retrieve_buffer_bindings(data.bindings);
      mesh.bindings.count = rigger_bind_count + 1;
    }
    total_meshes += mesh_count;
  });
  return total_meshes;
}

// bool rigged_model::set_transform(std::string_view bone, const bone_transform& transf) {
//   auto it = _bone_reg.find(bone);
//   if (it == _bone_reg.end()) {
//     return false;
//   }
//   NTF_ASSERT(it->second < _bone_transforms.size());
//
//   constexpr vec3 pivot{0.f, 0.f, 0.f};
//   _bone_transforms[it->second] =
//     shogle::build_trs_matrix(transf.pos, transf.scale, pivot, transf.rot);
//   return true;
// }
//
// bool rigged_model::set_transform(std::string_view bone, const mat4& transf) {
//   auto it = _bone_reg.find(bone);
//   if (it == _bone_reg.end()) {
//     return false;
//   }
//   NTF_ASSERT(it->second < _bone_transforms.size());
//
//   _bone_transforms[it->second] = transf;
//   return true;
// }
//
// void rigged_model::set_transform(u32 bone, const mat4& transf) {
//   NTF_ASSERT(bone < _bone_transforms.size());
//   _bone_transforms[bone] = transf;
// }
//
// void rigged_model::set_transform(u32 bone, const bone_transform& transf) {
//   NTF_ASSERT(bone < _bone_transforms.size());
//   constexpr vec3 pivot{0.f, 0.f, 0.f};
//   _bone_transforms[bone] = shogle::build_trs_matrix(transf.pos, transf.scale, pivot,
//   transf.rot);
// }
//
// void rigged_model::set_transform(u32 bone, shogle::transform3d<f32>& transf) {
//   NTF_ASSERT(bone < _bone_transforms.size());
//   _bone_transforms[bone] = transf.local();
// }
//
// ntf::optional<u32> rigged_model::find_bone(std::string_view name) {
//   auto it = _bone_reg.find(name);
//   if (it == _bone_reg.end()) {
//     return ntf::nullopt;
//   }
//   NTF_ASSERT(it->second < _bone_transforms.size());
//   return it->second;
// }
//
// bool rigged_model::set_transform(std::string_view bone, shogle::transform3d<f32>& transf) {
//   return set_transform(bone, transf.local());
// }

} // namespace kappa::scene
