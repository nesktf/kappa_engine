#include "./model.hpp"

namespace kappa::scene {

namespace {

constexpr i32 VERT_MODEL_TRANSFORM_LOC = 1;
constexpr i32 VERT_SCENE_TRANSFORM_LOC = 2;
constexpr mat4 IDENTITY_MAT{1.f};

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

void rigged_model::update_bones(ntf::span<mat4> cache, ntf::cspan<mat4> bone_locals,
                                ntf::cspan<mat4> bone_invs,
                                ntf::cspan<assets::rigged_model_bone> bones) {
  NTF_ASSERT(bone_locals.size() == bones.size());
  NTF_ASSERT(bone_invs.size() == bones.size());
  NTF_ASSERT(_bone_transforms.size() == bones.size());

  NTF_ASSERT(cache.size() == 3 * bones.size());
  const u32 model_offset = bones.size();
  const u32 output_offset = 2 * bones.size();

  ntf::span<mat4> locals{cache.data(), bones.size()};
  ntf::span<mat4> models{cache.data() + model_offset, bones.size()};
  ntf::span<mat4> output{cache.data() + output_offset, bones.size()};

  // Populate bone local transforms
  const mat4 root_transform = _transform.local();
  locals[0] = root_transform * bone_locals[0] * _bone_transforms[0];
  for (u32 i = 1; i < bones.size(); ++i) {
    locals[i] = bone_locals[i] * _bone_transforms[i];
  }

  // Populate bone model transforms
  models[0] = cache[0]; // Root transform
  for (u32 i = 1; i < bones.size(); ++i) {
    const u32 parent = bones[i].parent;
    // Since the bone hierarchy is sorted, we should be able to read the parent model matrix safely
    NTF_ASSERT(parent < bones.size());
    models[i] = models[parent] * locals[i];
  }

  // Prepare shader transform data
  for (u32 i = 0; i < bones.size(); ++i) {
    output[i] = models[i] * bone_invs[i];
  }
  const shogle::buffer_data upload_data{
    .data = output.data(),
    .size = output.size_bytes(),
    .offset = 0u,
  };
  _bone_buffer.upload(upload_data).value();
}

void entity_registry::update() {
  _forces.update_forces(1.f / GAME_UPS, [&](u64 particle, u32 tag) -> physics::particle_entity& {
    NTF_UNUSED(tag);
    const auto ent = ent_handle::from_u64(particle);
    return _rigged_instances.at(ent).particle();
  });

  for (auto& [instance, _] : _rigged_instances) {
    instance.particle().integrate(1.f / GAME_UPS);
    const auto model_idx = static_cast<assets::asset_bundle::rmodel_idx>(instance.model_idx());
    const auto& model = _bundle.get_rmodel(model_idx);

    const auto [locals, invs, bones] = model.bones();
    _rig_cache.resize(3 * bones.size(), IDENTITY_MAT);
    const ntf::span<mat4> cache{_rig_cache.data(), _rig_cache.size()};
    instance.update_bones(cache, locals, invs, bones);
  };
}

u32 entity_registry::retrieve_render_data(const render::scene_render_data& scene,
                                          render::object_render_data& render_data) {
  u32 total_meshes = 0u;
  for (auto& [instance, handle] : _rigged_instances) {
    const auto model_idx = static_cast<assets::asset_bundle::rmodel_idx>(instance.model_idx());
    const auto& model = _bundle.get_rmodel(model_idx);

    render_data.bindings.emplace_back(scene.transform, VERT_SCENE_TRANSFORM_LOC,
                                      scene.transform.size(), 0u);
    const u32 rigger_bind_count = instance.retrieve_buffer_bindings(render_data.bindings);
    const u32 rigger_bind_idx = render_data.bindings.size() - rigger_bind_count;
    total_meshes += model.retrieve_model_data(render_data, {rigger_bind_count, rigger_bind_idx});
  }
  return total_meshes;
}

namespace {

struct instance_buffs {
  shogle::shader_storage_buffer bone_buffer;
  ntf::unique_array<mat4> bone_transforms;
};

fn make_instance_buffers(u32 bone_count) -> instance_buffs {
  const size_t bone_size = bone_count * sizeof(mat4);
  auto ssbo = render::create_ssbo(bone_size, nullptr).value();
  ntf::unique_array<mat4> mats(bone_count, IDENTITY_MAT);
  return {std::move(ssbo), std::move(mats)};
}

} // namespace

fn entity_registry::add_entity(u32 model_idx, const vec3& pos, real mass) -> ent_handle {
  const auto& model = _bundle.get_rmodel(static_cast<assets::asset_bundle::rmodel_idx>(model_idx));
  auto transform = shogle::transform3d<f32>{}.pos(pos).scale(1.f, 1.f, 1.f);
  auto [bone_buffer, bone_transforms] = make_instance_buffers(model.bone_count());
  return _rigged_instances.emplace(model_idx, pos, mass, std::move(bone_buffer),
                                   std::move(bone_transforms), transform);
}

} // namespace kappa::scene
