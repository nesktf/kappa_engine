#include "renderable.hpp"

#include <shogle/boilerplate.hpp>

static std::string_view model_vert_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_coords;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_texcoords;
layout (location = 3) in vec3 att_tangents;
layout (location = 4) in vec3 att_bitangents;
layout (location = 5) in ivec4 att_bones;
layout (location = 6) in vec4 att_weights;

out vec2 tex_coord;

uniform mat4 u_model;
uniform mat4 u_proj;
uniform mat4 u_view;

layout(std430, binding = 1) buffer bone_transforms {
  mat4 bone_mat[];
};

const int MAX_BONE_INFLUENCE = 4;

void main() {
  // vec4 total_pos = bone_mat[att_bones] * vec4(att_coords, 1.f);
  vec4 total_pos = vec4(0.f);
  for (int i = 0; i < MAX_BONE_INFLUENCE; ++i){
    if (att_bones[i] == -1) {
      continue;
    }
    vec4 local_pos = bone_mat[att_bones[i]] * vec4(att_coords, 1.f);
    total_pos += local_pos * att_weights[i];
    vec3 local_norm = mat3(bone_mat[att_bones[i]]) * att_normals;
  }

  // gl_Position = u_proj*u_view*u_model*vec4(att_coords, 1.0f);
  gl_Position = u_proj*u_view*u_model*total_pos;
  tex_coord = att_texcoords;
}
)glsl";

static std::string_view model_frag_src = R"glsl(
#version 460 core

in vec2 tex_coord;
out vec4 frag_color;
uniform sampler2D u_sampler;

void main() {
  vec4 out_color = texture(u_sampler, tex_coord);

  if (out_color.a < 0.2) {
    discard;
  }

  frag_color = out_color;
}
)glsl";

rigged_model3d::rigged_model3d(std::string&& name,
                 material_meta&& materials,
                 armature_meta&& armature,
                 render_meta&& rendering,
                 ntf::transform3d<f32> transf) :
  _name{std::move(name)}, _materials{std::move(materials)},
  _armature{std::move(armature)}, _rendering{std::move(rendering)},
  _transf{transf} {}

expect<rigged_model3d> rigged_model3d::create(ntfr::context_view ctx, data_t&& data) {
  auto rigger = model_rigger::create(ctx, data.rigs, data.armature);
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

  auto meshes = model_mesh_provider::create(ctx, data.meshes);
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

  auto vert_sh = ntfr::vertex_shader::create(ctx, {model_vert_src}).value();
  auto frag_sh = ntfr::fragment_shader::create(ctx, {model_frag_src}).value();
  const ntfr::shader_t pip_stages[] = {vert_sh, frag_sh};
  const ntfr::face_cull_opts cull {
    .mode = ntfr::cull_mode::back,
    .front_face = ntfr::cull_face::counter_clockwise,
  };
  auto pip = ntfr::pipeline::create(ctx, {
    .attributes = {pip_attrib.data(), pip_attrib.size()},
    .stages = pip_stages,
    .primitive = ntfr::primitive_mode::triangles,
    .poly_mode = ntfr::polygon_mode::fill,
    .poly_width = 1.f,
    .tests = {
      .stencil_test = nullptr,
      .depth_test = ntfr::def_depth_opts,
      .scissor_test = nullptr,
      .face_culling = cull,
      .blending = ntfr::def_blending_opts,
    },
  });

  auto u_model = pip->uniform("u_model");
  auto u_proj = pip->uniform("u_proj");
  auto u_view = pip->uniform("u_view");
  auto u_sampler = pip->uniform("u_sampler");

  render_meta rendering {
    .pip = std::move(*pip),
    .meshes = std::move(*meshes),
    .u_model = u_model,
    .u_view = u_view,
    .u_proj = u_proj,
    .u_sampler = u_sampler,
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
  _rendering.render_data.clear();
  _rendering.binds.clear();

  _rendering.meshes.retrieve_render_data(_rendering.render_data);

  u32 buff_count = _armature.rigger.retrieve_buffers(_rendering.binds, 0u);
  cspan<ntfr::shader_binding> buff_span{_rendering.binds.data(), buff_count};

  const int32 sampler = 0;
  const ntfr::uniform_const unifs[] = {
    ntfr::format_uniform_const(_rendering.u_model, _transf.world()),
    ntfr::format_uniform_const(_rendering.u_proj, frame.proj),
    ntfr::format_uniform_const(_rendering.u_view, frame.view),
    ntfr::format_uniform_const(_rendering.u_sampler, sampler),
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
    frame.ctx.submit_render_command({
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
