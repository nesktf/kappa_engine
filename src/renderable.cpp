#include "renderable.hpp"
#include "renderer.hpp"

#include <shogle/boilerplate.hpp>

rigged_model3d::rigged_model3d(std::string&& name,
                               material_meta&& materials,
                               armature_meta&& armature,
                               render_meta&& rendering,
                               ntf::transform3d<f32> transf) :
  _name{std::move(name)}, _materials{std::move(materials)},
  _armature{std::move(armature)}, _rendering{std::move(rendering)},
  _transf{transf} {}

expect<rigged_model3d> rigged_model3d::create(data_t&& data) {
  auto& r = renderer::instance();
  auto ctx = r.ctx();

  auto rigger = model_rigger::create(data.rigs, data.armature);
  if (!rigger) {
    return ntf::unexpected{std::move(rigger.error())};
  }
  armature_meta armature {
    .anims = std::move(data.anims),
    .rigs = std::move(data.rigs),
    .rigger = std::move(*rigger),
  };

  std::vector<texture_t> texs;
  texs.reserve(data.mats.textures.size());
  std::unordered_map<std::string_view, u32> tex_reg;
  tex_reg.reserve(data.mats.textures.size());

  for (auto& texture : data.mats.textures) {
    const ntfr::image_data images {
      .bitmap = texture.bitmap.data(),
      .format = texture.format,
      .alignment = 4u,
      .extent = texture.extent,
      .offset = {0, 0, 0},
      .layer = 0u,
      .level = 0u,
    };
    const ntfr::texture_data data {
      .images = {images},
      .generate_mipmaps = true,
    };
    auto tex = ntfr::texture2d::create(ctx, {
      .format = ntfr::image_format::rgba8nu,
      .sampler = ntfr::texture_sampler::linear,
      .addressing = ntfr::texture_addressing::repeat,
      .extent = texture.extent,
      .layers = 1u,
      .levels = 7u,
      .data = data,
    }).value();
    texs.emplace_back(std::move(texture.name), std::move(tex));
  }
  for (u32 i = 0; const auto& texture : texs) {
    auto [_, empl] = tex_reg.try_emplace(texture.name, i++);
    NTF_ASSERT(empl);
  }

  material_meta materials {
    .textures = std::move(texs),
    .texture_registry = std::move(tex_reg),
    .materials = std::move(data.mats.materials),
    .material_registry = std::move(data.mats.material_registry),
    .material_textures = std::move(data.mats.material_textures),
  };

  auto meshes = model_mesh_provider::create(data.meshes);
  if (!meshes) {
    return ntf::unexpected{std::move(meshes.error())};
  }
  std::vector<ntfr::attribute_binding> pip_attrib;
  meshes->retrieve_bindings(pip_attrib);

  std::vector<u32> mesh_texs;
  mesh_texs.reserve(data.meshes.meshes.size());
  for (const auto& mesh : data.meshes.meshes) {
    auto it = materials.material_registry.find(mesh.material_name);
    if (it == materials.material_registry.end()) {
      mesh_texs.emplace_back(0u);
    }
    // Place the first texture only, fix this later
    const auto& mat = materials.materials[it->second];
    if (mat.textures.count == 1) {
      mesh_texs.emplace_back(materials.material_textures[mat.textures.idx]);
    } else {
      mesh_texs.emplace_back(0u);
    }
  }

  std::vector<ntfr::attribute_binding> att_binds;
  const ntfr::face_cull_opts cull {
    .mode = ntfr::cull_mode::back,
    .front_face = ntfr::cull_face::counter_clockwise,
  };
  const pipeline_opts pip_opts {
    .tests = {
      .stencil_test = nullptr,
      .depth_test = ntfr::def_depth_opts,
      .scissor_test = nullptr,
      .face_culling = cull,
      .blending = ntfr::def_blending_opts,
    },
    .primitive = ntfr::primitive_mode::triangles,
    .use_aos_bindings = false,
  };
  auto pip = r.make_pipeline(VERT_SHADER_RIGGED_MODEL, FRAG_SHADER_RAW_ALBEDO,
                             att_binds, pip_opts);

  render_meta rendering {
    .pip = std::move(*pip),
    .meshes = std::move(*meshes),
    .render_data = {},
    .binds = {},
    .mesh_texs = std::move(mesh_texs),
  };

  return expect<rigged_model3d>{
    ntf::in_place, std::move(data.name),
    std::move(materials), std::move(armature), std::move(rendering),
    ntf::transform3d<f32>{}.pos(0.f, 0.f, 0.f).scale(1.f, 1.f, 1.f)
  };
}

void rigged_model3d::tick() {
  _armature.rigger.set_root_transform(_transf.world());
  _armature.rigger.tick(_armature.rigs);
}

void rigged_model3d::set_bone_transform(std::string_view name,
                                        const model_rigger::bone_transform& transf)
{
  _armature.rigger.set_transform(_armature.rigs, name, transf);
}

void rigged_model3d::set_bone_transform(std::string_view name, const mat4& transf) {
  _armature.rigger.set_transform(_armature.rigs, name, transf);
}

void rigged_model3d::set_bone_transform(std::string_view name, ntf::transform3d<f32>& transf) {
  set_bone_transform(name, transf.local());
}

void rigged_model3d::render(const game_frame& frame) {
  auto ctx = renderer::instance().ctx();

  _rendering.render_data.clear();
  _rendering.binds.clear();

  _rendering.meshes.retrieve_render_data(_rendering.render_data);

  u32 buff_count = _armature.rigger.retrieve_buffers(_rendering.binds, frame.stransf);
  cspan<ntfr::shader_binding> buff_span{_rendering.binds.data(), buff_count};

  const int32 sampler = 0;
  const u32 u_albedo = 8u;
  const ntfr::uniform_const unifs[] = {
    ntfr::format_uniform_const(u_albedo, sampler),
  };


  for (u32 i = 0; i < _rendering.render_data.size(); ++i) {
    const auto& mesh = _rendering.render_data[i];
    const u32 tex = _rendering.mesh_texs[i];
    const ntfr::buffer_binding buff_bind {
      .vertex = mesh.vertex_buffers,
      .index = mesh.index_buffer,
      .shader = buff_span,
    };
    const ntfr::render_opts opts {
      .vertex_count = mesh.vertex_count,
      .vertex_offset = mesh.vertex_offset,
      .index_offset = mesh.index_offset,
      .instances = 0,
    };
    ctx.submit_render_command({
      .target = frame.fbo,
      .pipeline = _rendering.pip,
      .buffers = buff_bind,
      .textures = {_materials.textures[tex].tex},
      .consts = unifs,
      .opts = opts,
      .sort_group = 0u,
      .render_callback = {},
    });
  }
}
