#include "assets/rigged_model.hpp"
#include "../renderer/shaders.hpp"

#include <ntfstl/logger.hpp>

#define CHECK_ERR_BUFF(_buff)                                                       \
  if (!_buff) {                                                                     \
    ntf::logger::error("Failed to create vertex buffer, {}", _buff.error().what()); \
    free_buffs();                                                                   \
    return ntf::unexpected{std::string_view{"Failed to create vertex buffer"}};     \
  }

namespace kappa {

static const shogle::blend_opts def_blending_opts{
  .mode = shogle::blend_mode::add,
  .src_factor = shogle::blend_factor::src_alpha,
  .dst_factor = shogle::blend_factor::inv_src_alpha,
  .color = {0.f, 0.f, 0.f, 0.f},
};

static const shogle::depth_test_opts def_depth_opts{
  .func = shogle::test_func::less,
  .near_bound = 0.f,
  .far_bound = 1.f,
};

model_rigger::model_rigger(shogle::shader_storage_buffer&& ssbo, ntf::unique_array<bone_t>&& bones,
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
      const u32 idx = bone_vspan.idx + i;

      const auto& bone = rigs.bones[idx];
      const u32 local_parent = bone.parent - bone_vspan.idx;
      if (local_parent != vec_span::INDEX_TOMB) {
        NTF_ASSERT(local_parent < bones.size());
      }
      std::construct_at(bones.data() + i, bone.name, local_parent);
      auto [_, empl] = bone_reg.try_emplace(bones[i].name, i);
      NTF_ASSERT(empl);

      std::construct_at(bone_locals.data() + i, rigs.bone_locals[idx]);
      std::construct_at(bone_inv_models.data() + i, rigs.bone_inv_models[idx]);
    }

    constexpr mat4 identity{1.f};
    ntf::unique_array<mat4> bone_transforms(bone_vspan.size(), identity);
    ntf::unique_array<mat4> transf_cache(bone_vspan.size() * 2u, identity); // for model and locals

    ntf::unique_array<mat4> transf_output(bone_vspan.size(), identity);
    auto ssbo = render::create_ssbo(transf_output.size() * sizeof(mat4), transf_output.data());
    if (!ssbo) {
      return {ntf::unexpect, std::move(ssbo.error())};
    }

    return {ntf::in_place,
            std::move(*ssbo),
            std::move(bones),
            std::move(bone_reg),
            std::move(bone_locals),
            std::move(bone_inv_models),
            std::move(bone_transforms),
            std::move(transf_cache),
            std::move(transf_output)};
  } catch (const std::bad_alloc&) {
    return {ntf::unexpect, "Allocation failure"};
  } catch (...) {
    return {ntf::unexpect, "Unknown error"};
  }
}

static constexpr i32 VERT_MODEL_TRANSFORM_LOC = 1;
static constexpr i32 VERT_SCENE_TRANSFORM_LOC = 2;

static constexpr i32 FRAG_SAMPLER_LOC = 8;

rigged_model3d::rigged_model3d(model3d_mesh_buffers<rigged_model_data>&& meshes,
                               model3d_mesh_textures&& texturer, model_rigger&& rigger,
                               std::vector<model_material_data::material_meta>&& mats,
                               std::unordered_map<std::string_view, u32>&& mat_reg,
                               shogle::pipeline pip, std::vector<u32> mesh_texs,
                               std::string&& name) noexcept :
    model3d_mesh_buffers<rigged_model_data>{std::move(meshes)},
    model3d_mesh_textures{std::move(texturer)}, model_rigger{std::move(rigger)},
    _mats{std::move(mats)}, _mat_reg{std::move(mat_reg)}, _pip{std::move(pip)},
    _mesh_mats{std::move(mesh_texs)}, _name{std::move(name)}, _transf{} {}

expect<rigged_model3d> rigged_model3d::create(data_t&& data) {
  std::vector<shogle::attribute_binding> att_binds;
  const shogle::face_cull_opts cull{
    .mode = shogle::cull_mode::back,
    .front_face = shogle::cull_face::counter_clockwise,
  };
  const render::pipeline_opts pip_opts{
    .tests =
      {
        .stencil_test = nullptr,
        .depth_test = def_depth_opts,
        .scissor_test = nullptr,
        .face_culling = cull,
        .blending = def_blending_opts,
      },
    .primitive = shogle::primitive_mode::triangles,
    .use_aos_bindings = false,
  };
  auto pip = render::make_pipeline(render::VERT_SHADER_RIGGED_MODEL,
                                   render::FRAG_SHADER_RAW_ALBEDO, att_binds, pip_opts);
  if (!pip) {
    return {ntf::unexpect, pip.error()};
  }

  auto mesher = model3d_mesh_buffers<rigged_model_data>::create(data);
  if (!mesher) {
    return {ntf::unexpect, mesher.error()};
  }

  auto texturer = model3d_mesh_textures::create(data.materials);
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
    auto it = data.materials.material_registry.find(mesh.material_name);
    if (it == data.materials.material_registry.end()) {
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

  return {ntf::in_place,
          std::move(*mesher),
          std::move(*texturer),
          std::move(*rigger),
          std::move(data.materials.materials),
          std::move(data.materials.material_registry),
          std::move(*pip),
          std::move(mesh_mats),
          std::move(data.name)};
}

void rigged_model3d::tick() {
  model_rigger::tick_bones(_transf.local());
}

} // namespace kappa
