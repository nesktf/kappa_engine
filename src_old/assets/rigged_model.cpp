#include "assets/rigged_model.hpp"
#include "../renderer/shaders.hpp"

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

const shogle::face_cull_opts def_pip_cull{
  .mode = shogle::cull_mode::back,
  .front_face = shogle::cull_face::counter_clockwise,
};

const render::pipeline_opts pip_opts{
  .tests =
    {
      .stencil_test = nullptr,
      .depth_test = def_depth_opts,
      .scissor_test = nullptr,
      .face_culling = def_pip_cull,
      .blending = def_blending_opts,
    },
  .primitive = shogle::primitive_mode::triangles,
  .use_aos_bindings = false,
};

rigged_model::model_rigs make_model_rigs(model_rig_data& rigs, u32 armature_idx) {
  NTF_ASSERT(armature_idx < rigs.armatures.size());
  const auto bone_vspan = rigs.armatures[armature_idx].bones;
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

  return {std::move(bones), std::move(bone_reg), std::move(bone_locals),
          std::move(bone_inv_models)};
};

} // namespace

rigged_model::rigged_model(model_meshes<rigged_model_data>&& meshes, model_textures&& textures,
                           model_rigs&& rigs,
                           std::vector<model_material_data::material_meta>&& mats,
                           std::unordered_map<std::string_view, u32>&& mat_reg,
                           std::vector<u32>&& mesh_mats, shogle::pipeline&& pip,
                           std::string&& name) noexcept :
    _meshes{std::move(meshes)}, _textures{std::move(textures)}, _rigs{std::move(rigs)},
    _mats{std::move(mats)}, _mat_reg{std::move(mat_reg)}, _mesh_mats{std::move(mesh_mats)},
    _pip{std::move(pip)}, _name{std::move(name)} {}

expect<rigged_model> rigged_model::create(rigged_model_data&& data) {
  std::vector<shogle::attribute_binding> att_binds;
  auto pip = render::make_pipeline(render::VERT_SHADER_RIGGED_MODEL,
                                   render::FRAG_SHADER_RAW_ALBEDO, att_binds, pip_opts);
  if (!pip) {
    return {ntf::unexpect, std::move(pip.error())};
  }

  auto mesher = model_meshes<rigged_model_data>::create(data);
  if (!mesher) {
    return {ntf::unexpect, std::move(mesher.error())};
  }

  auto texturer = model_textures::create(data.materials);
  if (!texturer) {
    return {ntf::unexpect, std::move(texturer.error())};
  }

  try {
    auto& all_rigs = data.rigs;
    auto it = all_rigs.armature_registry.find(data.armature);
    if (it == all_rigs.armature_registry.end()) {
      return {ntf::unexpect, fmt::format("Armature \"{}\" not found", data.armature)};
    }
    const u32 armature_idx = it->second;
    auto rigs = make_model_rigs(all_rigs, armature_idx);

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
            std::move(rigs),
            std::move(data.materials.materials),
            std::move(data.materials.material_registry),
            std::move(mesh_mats),
            std::move(*pip),
            std::move(data.name)};
  } catch (const std::bad_alloc&) {
    return {ntf::unexpect, "Allocation failure"};
  } catch (...) {
    return {ntf::unexpect, "Unknown error"};
  }
}

fn rigged_model::bones() const -> bone_mats {
  ntf::cspan<mat4> locals{_rigs.bone_locals.data(), _rigs.bone_locals.size()};
  ntf::cspan<mat4> invs{_rigs.bone_inv_models.data(), _rigs.bone_inv_models.size()};
  ntf::cspan<rigged_model_bone> bones{_rigs.bones.data(), _rigs.bones.size()};
  return {locals, invs, bones};
}

u32 rigged_model::retrieve_model_data(render::object_render_data& render_data,
                                      vec_span rigger_bind) const {
  static constexpr i32 FRAG_SAMPLER_LOC = 8;
  const u32 mesh_count = _meshes.mesh_count();
  for (u32 mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {
    auto& mesh = _meshes.retrieve_mesh_data(mesh_idx, render_data.meshes);
    mesh.pipeline = _pip.get();

    const i32 sampler = 0;
    NTF_ASSERT(mesh_idx < _mesh_mats.size());
    const u32 mat_idx = _mesh_mats[mesh_idx];
    mesh.textures.idx = render_data.textures.size();
    const u32 tex_count = _textures.retrieve_material_textures(mat_idx, render_data.textures);
    mesh.textures.count = tex_count;

    render_data.uniforms.emplace_back(shogle::format_uniform_const(FRAG_SAMPLER_LOC, sampler));
    mesh.uniforms.count = 1u;
    mesh.uniforms.idx = render_data.uniforms.size() - 1u;

    mesh.bindings = rigger_bind;
  }
  return mesh_count;
}

} // namespace kappa::assets
