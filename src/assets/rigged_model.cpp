#include "assets/rigged_model.hpp"

#include <ntfstl/logger.hpp>

#define CHECK_ERR_BUFF(_buff) \
if (!_buff) { \
  ntf::logger::error("Failed to create vertex buffer, {}", _buff.error().what()); \
  free_buffs(); \
  return ntf::unexpected{std::string_view{"Failed to create vertex buffer"}}; \
}

namespace kappa {

static const shogle::blend_opts def_blending_opts {
  .mode = shogle::blend_mode::add,
  .src_factor = shogle::blend_factor::src_alpha,
  .dst_factor = shogle::blend_factor::inv_src_alpha,
  .color = {0.f, 0.f, 0.f, 0.f},
};

static const shogle::depth_test_opts def_depth_opts {
  .func = shogle::test_func::less,
  .near_bound = 0.f,
  .far_bound = 1.f,
};

model_rigger::model_rigger(shogle::shader_storage_buffer&& ssbo,
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
    ntf::unique_array<mat4> bone_transforms(bone_vspan.size(), identity);
    ntf::unique_array<mat4> transf_cache(bone_vspan.size()*2u, identity); // for model and locals

    ntf::unique_array<mat4> transf_output(bone_vspan.size(), identity);
    const shogle::buffer_data initial_data {
      .data = transf_output.data(),
      .size = transf_output.size()*sizeof(mat4),
      .offset = 0u,
    };
    auto ssbo = shogle::shader_storage_buffer::create(ctx, {
      .flags = shogle::buffer_flag::dynamic_storage,
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
  _bone_transforms[it->second] = shogle::build_trs_matrix(transf.pos, transf.scale,
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
  _bone_transforms[bone] = shogle::build_trs_matrix(transf.pos, transf.scale,
                                                 pivot, transf.rot);
}

void model_rigger::set_transform(u32 bone, shogle::transform3d<f32>& transf) {
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

bool model_rigger::set_transform(std::string_view bone, shogle::transform3d<f32>& transf) {
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

u32 model_rigger::retrieve_buffer_bindings(std::vector<shogle::shader_binding>& binds) const {
  const shogle::buffer_data data {
    .data = _transf_output.data(),
    .size = _transf_output.size()*sizeof(mat4),
    .offset = 0u,
  };
  [[maybe_unused]] auto ret = _ssbo.upload(data);
  NTF_ASSERT(ret);
  binds.emplace_back(_ssbo.get(), renderer::VERT_MODEL_TRANSFORM_LOC, _ssbo.size(), 0u);
  return 1u;
}

rigged_model3d::rigged_model3d(model3d_mesh_buffers<rigged_model_data>&& meshes,
                               model3d_mesh_textures&& texturer,
                               model_rigger&& rigger,
                               std::vector<model_material_data::material_meta>&& mats,
                               std::unordered_map<std::string_view, u32>&& mat_reg,
                               shogle::pipeline pip,
                               std::vector<u32> mesh_texs,
                               std::string&& name) noexcept :
  model3d_mesh_buffers<rigged_model_data>{std::move(meshes)},
  model3d_mesh_textures{std::move(texturer)},
  model_rigger{std::move(rigger)},
  _mats{std::move(mats)}, _mat_reg{std::move(mat_reg)},
  _pip{std::move(pip)}, _mesh_mats{std::move(mesh_texs)},
  _name{std::move(name)}, _transf{} {}

expect<rigged_model3d> rigged_model3d::create(data_t&& data) {
  auto& r = renderer::instance();

  std::vector<shogle::attribute_binding> att_binds;
  const shogle::face_cull_opts cull {
    .mode = shogle::cull_mode::back,
    .front_face = shogle::cull_face::counter_clockwise,
  };
  const pipeline_opts pip_opts {
    .tests = {
      .stencil_test = nullptr,
      .depth_test = def_depth_opts,
      .scissor_test = nullptr,
      .face_culling = cull,
      .blending = def_blending_opts,
    },
    .primitive = shogle::primitive_mode::triangles,
    .use_aos_bindings = false,
  };
  auto pip = r.make_pipeline(VERT_SHADER_RIGGED_MODEL, FRAG_SHADER_RAW_ALBEDO,
                             att_binds, pip_opts);
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

  return {
    ntf::in_place,
    std::move(*mesher), std::move(*texturer), std::move(*rigger),
    std::move(data.materials.materials), std::move(data.materials.material_registry),
    std::move(*pip), std::move(mesh_mats), std::move(data.name)
  };
}

void rigged_model3d::tick() {
  model_rigger::tick_bones(_transf.local());
}

u32 rigged_model3d::retrieve_render_data(const scene_render_data& scene,
                                         object_render_data& data)
{
  const u32 mesh_count = this->mesh_count();
  for (u32 mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {
    auto& mesh = retrieve_mesh_data(mesh_idx, data.meshes);
    mesh.pipeline = _pip.get();

    const i32 sampler = 0;
    const u32 mat_idx = _mesh_mats[mesh_idx];
    mesh.textures.idx = data.textures.size();
    const u32 tex_count = model3d_mesh_textures::retrieve_material_textures(mat_idx, data.textures);
    mesh.textures.count = tex_count;

    mesh.uniforms.idx = data.uniforms.size();
    data.uniforms.emplace_back(shogle::format_uniform_const(renderer::FRAG_SAMPLER_LOC, sampler));
    mesh.uniforms.count = 1u;

    mesh.bindings.idx = data.bindings.size();
    data.bindings.emplace_back(scene.transform, renderer::VERT_SCENE_TRANSFORM_LOC,
                               scene.transform.size(), 0u);
    const u32 rigger_bind_count = model_rigger::retrieve_buffer_bindings(data.bindings);
    mesh.bindings.count = rigger_bind_count+1;
  }
  return mesh_count;
}

} // namespace kapp
