#include <shogle/boilerplate.hpp>
#include "model.hpp"

using namespace ntf::numdefs;

static std::string_view vert_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_coords;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_texcoords;

out vec2 tex_coord;

uniform mat4 u_model;
uniform mat4 u_proj;
uniform mat4 u_view;

void main() {
  gl_Position = u_proj*u_view*u_model*vec4(att_coords, 1.0f);
  tex_coord = att_texcoords;
}
)glsl";

static std::string_view frag_src = R"glsl(
#version 460 core

in vec2 tex_coord;
out vec4 frag_color;
uniform sampler2D u_sampler;

void main() {
  vec4 out_color = texture(u_sampler, tex_coord);

  if (out_color.a < 0.1) {
    discard;
  }

  frag_color = out_color;
}
)glsl";

struct mesh_t {
  size_t pos;
  size_t norm;
  size_t uv;
  size_t ebo;
  u32 indices;
  size_t tex;
};

int main() {
  ntf::logger::set_level(ntf::log_level::debug);

  assimp_parser parser;
  parser.load("./res/chiruno/chiruno.gltf");

  model_rig_data cirno_rigs;
  parser.parse_rigs(cirno_rigs);

  model_material_data cirno_materials;
  parser.parse_materials(cirno_materials);

  model_mesh_data cirno_meshes;
  parser.parse_meshes(cirno_rigs, cirno_meshes);

  auto [win, ctx] = ntfr::make_gl_ctx(1280, 720, "test", 1).value();

  auto fbo = ntfr::framebuffer::get_default(ctx);
  win.set_viewport_callback([&](ntfr::window&, const ntf::extent2d& ext) {
    fbo.viewport({0, 0, ext.x, ext.y});
  });
  win.set_key_press_callback([&](ntfr::window& win, const ntfr::win_key_data& k) {
    if (k.action == ntfr::win_action::press) {
      if (k.key == ntfr::win_key::escape) {
        win.close();
      }
    }
  });

  auto vert_sh = ntfr::vertex_shader::create(ctx, {vert_src}).value();
  auto frag_sh = ntfr::fragment_shader::create(ctx, {frag_src}).value();
  const ntfr::shader_t pip_stages[] = {vert_sh, frag_sh};
  const auto pip_attrib = ntfr::pnt_vertex::soa_binding(0);
  auto pipe = ntfr::pipeline::create(ctx, {
    .attributes = {pip_attrib.data(), pip_attrib.size()},
    .stages = pip_stages,
    .primitive = ntfr::primitive_mode::triangles,
    .poly_mode = ntfr::polygon_mode::fill,
    .poly_width = 1.f,
    .tests = {
      .stencil_test = nullptr,
      .depth_test = ntfr::def_depth_opts,
      .scissor_test = nullptr,
      .face_culling = nullptr,
      .blending = ntfr::def_blending_opts,
    },
  }).value();

  auto u_model = pipe.uniform("u_model");
  auto u_proj = pipe.uniform("u_proj");
  auto u_view = pipe.uniform("u_view");
  auto u_sampler = pipe.uniform("u_sampler");

  const auto fb_ratio = 1280.f/720.f;
  auto proj_mat = glm::perspective(glm::radians(45.f), fb_ratio, .1f, 100.f);
  auto view_mat = glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.f, -3.f});
  auto transf = ntf::transform3d<f32>{}
    .pos(0.f, -1.f, 0.f).scale(1.5f, 1.5f, 1.5f);

  std::vector<mesh_t> meshes;
  std::vector<ntfr::vertex_buffer> vbos;
  std::vector<ntfr::index_buffer> ebos;
  meshes.reserve(cirno_meshes.meshes.size());
  vbos.reserve(cirno_meshes.meshes.size()*3u);
  ebos.reserve(cirno_meshes.meshes.size());

  std::vector<ntfr::texture2d> texs;
  texs.reserve(cirno_materials.textures.size());

  for (const auto& texture : cirno_materials.textures){
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
    texs.emplace_back(std::move(tex));
  }

  for (const auto& mesh : cirno_meshes.meshes) {
    size_t vbos_idx = vbos.size();
    const auto pos_span = mesh.positions.to_cspan(cirno_meshes.positions.data());
    const ntfr::buffer_data pos_data {
      .data = pos_span.data(),
      .size = pos_span.size_bytes(),
      .offset = 0u,
    };
    auto pos_buff = ntfr::vertex_buffer::create(ctx, {
      .flags = {},
      .size = pos_span.size_bytes(),
      .data = pos_data,
    }).value();
    vbos.emplace_back(std::move(pos_buff));

    const auto norm_span = mesh.normals.to_cspan(cirno_meshes.normals.data());
    const ntfr::buffer_data norm_data {
      .data = norm_span.data(),
      .size = norm_span.size_bytes(),
      .offset = 0u,
    };
    auto norm_buff = ntfr::vertex_buffer::create(ctx, {
      .flags = {},
      .size = norm_span.size_bytes(),
      .data = norm_data,
    }).value();
    vbos.emplace_back(std::move(norm_buff));

    const auto uv_span = mesh.uvs.to_cspan(cirno_meshes.uvs.data());
    const ntfr::buffer_data uv_data {
      .data = uv_span.data(),
      .size = uv_span.size_bytes(),
      .offset = 0u,
    };
    auto uv_buff = ntfr::vertex_buffer::create(ctx, {
      .flags = {},
      .size = uv_span.size_bytes(),
      .data = uv_data,
    }).value();
    vbos.emplace_back(std::move(uv_buff));

    size_t ebo_idx = ebos.size();
    const auto ind_span = mesh.indices.to_cspan(cirno_meshes.indices.data());
    const ntfr::buffer_data ind_data {
      .data = ind_span.data(),
      .size = ind_span.size_bytes(),
      .offset = 0u,
    };
    auto ind_buff = ntfr::index_buffer::create(ctx, {
      .flags = {},
      .size = ind_span.size_bytes(),
      .data = ind_data,
    }).value();
    ebos.emplace_back(std::move(ind_buff));

    const u32 mat_idx = cirno_materials.material_registry.find(mesh.material_name)->second;
    const auto& mat = cirno_materials.materials[mat_idx];
    u32 tex = 0u;
    if (mat.textures.count == 1){
      tex = cirno_materials.material_textures[mat.textures.idx];
    }

    meshes.emplace_back(vbos_idx, vbos_idx+1, vbos_idx+2, ebo_idx, ind_span.size(), tex);
  }

  float t = 0.f;
  ntfr::render_loop(win, ctx, 60u, ntf::overload{
    [&](u32 fdt) {
      t += 1/(float)fdt;
      transf.rot(ntf::vec3{t*M_PIf*.5f, 0.f, 0.f});
    },
    [&](f32 dt, f32 alpha) {
      const int32 sampler = 0;
      const ntfr::uniform_const unifs[] = {
        ntfr::format_uniform_const(u_model, transf.world()),
        ntfr::format_uniform_const(u_proj, proj_mat),
        ntfr::format_uniform_const(u_view, view_mat),
        ntfr::format_uniform_const(u_sampler, sampler),
      };
      for (const auto& mesh : meshes) {
        const ntfr::vertex_binding binds[] = {
          {.buffer = vbos[mesh.pos],  .layout = 0u},
          {.buffer = vbos[mesh.norm], .layout = 1u},
          {.buffer = vbos[mesh.uv],   .layout = 2u},
        };
        const ntfr::buffer_binding buff_bind {
          .vertex = binds,
          .index = ebos[mesh.ebo],
          .shader = {},
        };
        const ntfr::render_opts opts {
          .vertex_count = mesh.indices,
          .vertex_offset = 0,
          .instances = 0,
        };
        ctx.submit_render_command({
          .target = fbo,
          .pipeline = pipe,
          .buffers = buff_bind,
          .textures = {texs[mesh.tex]},
          .consts = unifs,
          .opts = opts,
          .sort_group = 0u,
          .render_callback = {},
        });
      }
    }
  });

  return EXIT_SUCCESS;
}
