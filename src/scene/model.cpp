#include "./model.hpp"

namespace kappa::scene {

rigged_model3d::rigged_model3d(u32 model, shogle::shader_storage_buffer&& bone_buffer,
                               ntf::unique_array<mat4>&& bone_transforms,
                               shogle::transform3d<f32> transform) :
    _bone_buffer{std::move(bone_buffer)}, _bone_transforms{std::move(bone_transforms)},
    _transform{transform}, _model{model} {}

u32 rigged_model3d::retrieve_buffer_bindings(std::vector<shogle::shader_binding>& binds) const {
  const shogle::buffer_data data{
    .data = _transf_output.data(),
    .size = _transf_output.size() * sizeof(mat4),
    .offset = 0u,
  };
  [[maybe_unused]] auto ret = _ssbo.upload(data);
  NTF_ASSERT(ret);
  binds.emplace_back(_ssbo.get(), VERT_MODEL_TRANSFORM_LOC, _ssbo.size(), 0u);
  return 1u;
}

void rigged_model3d::update_bones(const mat4& root) {
  const u32 model_offset = _bones.size(); // First elements are locals, the rest models
  NTF_ASSERT(_transf_cache.size() == 2 * model_offset);

  // Populate bone local transforms
  _transf_cache[0] = root * _bone_locals[0] * _bone_transforms[0];
  for (u32 i = 1; i < _bones.size(); ++i) {
    _transf_cache[i] = _bone_locals[i] * _bone_transforms[i];
  }

  // Populate bone model transforms
  _transf_cache[model_offset] = _transf_cache[0]; // Root transform
  for (u32 i = 1; i < _bones.size(); ++i) {
    const u32 parent = _bones[i].parent;
    // Since the bone hierarchy is sorted, we should be able to read the parent model matrix safely
    NTF_ASSERT(parent < model_offset);
    _transf_cache[model_offset + i] = _transf_cache[model_offset + parent] * _transf_cache[i];
  }

  // Prepare shader transform data
  for (u32 i = 0; i < _bones.size(); ++i) {
    _transf_output[i] = _transf_cache[model_offset + i] * _bone_inv_models[i];
  }
}

bool rigged_model3d::set_transform(std::string_view bone, const bone_transform& transf) {
  auto it = _bone_reg.find(bone);
  if (it == _bone_reg.end()) {
    return false;
  }
  NTF_ASSERT(it->second < _bone_transforms.size());

  constexpr vec3 pivot{0.f, 0.f, 0.f};
  _bone_transforms[it->second] =
    shogle::build_trs_matrix(transf.pos, transf.scale, pivot, transf.rot);
  return true;
}

bool rigged_model3d::set_transform(std::string_view bone, const mat4& transf) {
  auto it = _bone_reg.find(bone);
  if (it == _bone_reg.end()) {
    return false;
  }
  NTF_ASSERT(it->second < _bone_transforms.size());

  _bone_transforms[it->second] = transf;
  return true;
}

void rigged_model3d::set_transform(u32 bone, const mat4& transf) {
  NTF_ASSERT(bone < _bone_transforms.size());
  _bone_transforms[bone] = transf;
}

void rigged_model3d::set_transform(u32 bone, const bone_transform& transf) {
  NTF_ASSERT(bone < _bone_transforms.size());
  constexpr vec3 pivot{0.f, 0.f, 0.f};
  _bone_transforms[bone] = shogle::build_trs_matrix(transf.pos, transf.scale, pivot, transf.rot);
}

void rigged_model3d::set_transform(u32 bone, shogle::transform3d<f32>& transf) {
  NTF_ASSERT(bone < _bone_transforms.size());
  _bone_transforms[bone] = transf.local();
}

ntf::optional<u32> rigged_model3d::find_bone(std::string_view name) {
  auto it = _bone_reg.find(name);
  if (it == _bone_reg.end()) {
    return ntf::nullopt;
  }
  NTF_ASSERT(it->second < _bone_transforms.size());
  return it->second;
}

bool rigged_model3d::set_transform(std::string_view bone, shogle::transform3d<f32>& transf) {
  return set_transform(bone, transf.local());
}

void rigged_model3d::apply_animation(const model_anim_data& anims, std::string_view name,
                                     u32 tick) {
  auto it = anims.animation_registry.find(name);
  if (it == anims.animation_registry.end()) {
    return;
  }
  const auto& anim = anims.animations[it->second];
  const f64 t = std::fmod(anim.tps * tick, anim.duration);
}

void entity_registry::update() {
  _rigged_instances.for_each([&](rigged_model3d& model) { model.update_bones(); });
}

u32 entity_registry::retrieve_render_data(const render::scene_render_data& scene,
                                          render::object_render_data& data) {
  const u32 mesh_count = this->mesh_count();
  for (u32 mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {
    auto& mesh = retrieve_mesh_data(mesh_idx, data.meshes);
    mesh.pipeline = _pip.get();

    const i32 sampler = 0;
    const u32 mat_idx = _mesh_mats[mesh_idx];
    mesh.textures.idx = data.textures.size();
    const u32 tex_count =
      model3d_mesh_textures::retrieve_material_textures(mat_idx, data.textures);
    mesh.textures.count = tex_count;

    mesh.uniforms.idx = data.uniforms.size();
    data.uniforms.emplace_back(shogle::format_uniform_const(FRAG_SAMPLER_LOC, sampler));
    mesh.uniforms.count = 1u;

    mesh.bindings.idx = data.bindings.size();
    data.bindings.emplace_back(scene.transform, VERT_SCENE_TRANSFORM_LOC, scene.transform.size(),
                               0u);
    const u32 rigger_bind_count = model_rigger::retrieve_buffer_bindings(data.bindings);
    mesh.bindings.count = rigger_bind_count + 1;
  }
  return mesh_count;
}

} // namespace kappa::scene
