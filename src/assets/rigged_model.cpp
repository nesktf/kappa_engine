#include "assets/rigged_model.hpp"
#include "../renderer/shaders.hpp"

#include <ntfstl/logger.hpp>

#define CHECK_ERR_BUFF(_buff)                                                       \
  if (!_buff) {                                                                     \
    ntf::logger::error("Failed to create vertex buffer, {}", _buff.error().what()); \
    free_buffs();                                                                   \
    return ntf::unexpected{std::string_view{"Failed to create vertex buffer"}};     \
  }

namespace kappa::assets {

namespace {

const shogle::blend_opts def_blending_opts{
  .mode = shogle::blend_mode::add,
  .src_factor = shogle::blend_factor::src_alpha,
  .dst_factor = shogle::blend_factor::inv_src_alpha,
  .color = {0.f, 0.f, 0.f, 0.f},
};

const shogle::depth_test_opts def_depth_opts{
  .func = shogle::test_func::less,
  .near_bound = 0.f,
  .far_bound = 1.f,
};

constexpr mat4 IDENTITY_MAT{1.f};

expect<rigged_model3d::rig_data> make_rig_data(model_rig_data& rigs, std::string_view armature) {
  auto it = rigs.armature_registry.find(armature);
  if (it == rigs.armature_registry.end()) {
    return {ntf::unexpect, "Armature not found"};
  }
  const u32 armature_idx = it->second;
  NTF_ASSERT(rigs.armatures.size() > armature_idx);
  const auto bone_vspan = rigs.armatures[armature_idx].bones;
  try {
    ntf::unique_array<rigged_model_bone> bones{ntf::uninitialized, bone_vspan.size()};
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

    // ntf::unique_array<mat4> bone_transforms(bone_vspan.size(), identity);
    // ntf::unique_array<mat4> transf_cache(bone_vspan.size() * 2u,
    //                                      identity); // for model and locals
    // ntf::unique_array<mat4> transf_output(bone_vspan.size(), identity);

    return {ntf::in_place, std::move(bones), std::move(bone_reg), std::move(bone_locals),
            std::move(bone_inv_models)};
  } catch (const std::bad_alloc&) {
    return {ntf::unexpect, "Allocation failure"};
  } catch (...) {
    return {ntf::unexpect, "Unknown error"};
  }
};

} // namespace

rigged_model3d::rigged_model3d(model3d_mesh_buffers<rigged_model_data>&& meshes,
                               model3d_mesh_textures&& texturer, rig_data&& rigs,
                               std::vector<model_material_data::material_meta>&& mats,
                               std::unordered_map<std::string_view, u32>&& mat_reg,
                               std::vector<u32>&& mesh_mats, shogle::pipeline&& pip,
                               std::string&& name) :
    model3d_mesh_buffers<rigged_model_data>{std::move(meshes)},
    model3d_mesh_textures{std::move(texturer)}, _rigs{std::move(rigs)}, _mats{std::move(mats)},
    _mat_reg{std::move(mat_reg)}, _mesh_mats{std::move(mesh_mats)}, _pip{std::move(pip)},
    _name{std::move(name)} {}

expect<rigged_model3d> rigged_model3d::create(rigged_model_data&& data) {
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
    return {ntf::unexpect, std::move(pip.error())};
  }

  auto mesher = model3d_mesh_buffers<rigged_model_data>::create(data);
  if (!mesher) {
    return {ntf::unexpect, std::move(mesher.error())};
  }

  auto texturer = model3d_mesh_textures::create(data.materials);
  if (!texturer) {
    return {ntf::unexpect, std::move(texturer.error())};
  }

  auto rigs = make_rig_data(data.rigs, data.armature);
  if (!rigs) {
    return {ntf::unexpect, std::move(rigs.error())};
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
          std::move(*rigs),
          std::move(data.materials.materials),
          std::move(data.materials.material_registry),
          std::move(mesh_mats),
          std::move(*pip),
          std::move(data.name)};
}

fn rigged_model3d::make_instance_data() const->instance_data {
  const size_t bone_count = _rigs.bones.size();
  const size_t bone_size = bone_count * sizeof(mat4);
  auto ssbo = render::create_ssbo(bone_size, nullptr).value();
  ntf::unique_array<mat4> mats{bone_count, IDENTITY_MAT};
  return {std::move(ssbo), std::move(mats)};
}

} // namespace kappa::assets
