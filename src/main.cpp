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

out vec4 frag_color;

void main() {
  frag_color = vec4(0., 0., 1., 1.);
}
)glsl";

struct mesh_t {
  size_t idx;
  size_t vbo;
  size_t ebo;
};
constexpr size_t EBO_TOMB = std::numeric_limits<size_t>::max();

int main() {
  ntf::logger::set_level(ntf::log_level::debug);
  auto cirno_model = model_data::load_model("./res/chiruno/chiruno.gltf");
  if (!cirno_model) {
    ntf::logger::error("{}", cirno_model.error());
    return 1;
  }

  std::vector<ntf::bitmap_data> bitmaps;
  bitmaps.reserve(cirno_model.materials.textures.size());
  for (const auto& tex : cirno_model.materials.textures) {
    const auto& path = cirno_model.materials.paths[tex.index];
    ntf::logger::debug("{}", path);
    auto bitmap = ntf::load_image<uint8>(path).value();
    bitmaps.emplace_back(std::move(bitmap));
  }

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
  auto pipe = ntfr::make_pipeline<ntfr::pnt_vertex>(vert_sh, frag_sh).value();

  auto u_model = pipe.uniform("u_model");
  auto u_proj = pipe.uniform("u_proj");
  auto u_view = pipe.uniform("u_view");

  const auto fb_ratio = 1280.f/720.f;
  auto proj_mat = glm::perspective(glm::radians(45.f), fb_ratio, .1f, 100.f);
  auto view_mat = glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.f, -3.f});
  auto transf = ntf::transform3d<f32>{}
    .pos(0.f, .5f, 0.f).scale(1.f, -1.f, 1.f);

  std::vector<mesh_t> meshes;
  std::vector<ntfr::vertex_buffer> vbos;
  std::vector<ntfr::index_buffer> ebos;
  meshes.reserve(cirno_model.meshes.size());
  vbos.reserve(cirno_model.meshes.size());
  ebos.reserve(cirno_model.meshes.size());

  std::vector<ntfr::texture2d> texs;
  texs.reserve(bitmaps.size());

  for (const auto& bitmap : bitmaps){
    auto tex = ntfr::make_texture2d(ctx, bitmap,
                                    ntfr::texture_sampler::linear,
                                    ntfr::texture_addressing::repeat).value();
    texs.emplace_back(std::move(tex));
  }
  bitmaps.clear();

  for (size_t idx = 0u; const auto& mesh : cirno_model.meshes.data) {
    const auto& mat = cirno_model.materials.data[mesh.material];
    ntf::logger::debug("{} {} {}", mesh.name, mesh.has_indices(), mat.name);
    const ntfr::buffer_data vbo_data {
      .data = mesh.vertex_data(cirno_model.meshes.vertices),
      .size = mesh.vertices_size(),
      .offset = 0u,
    };
    auto vbo = ntfr::vertex_buffer::create(ctx, {
      .flags = ntfr::buffer_flag::dynamic_storage,
      .size = mesh.vertices_size(),
      .data = vbo_data,
    }).value();
    vbos.emplace_back(std::move(vbo));
    size_t vbo_idx = vbos.size()-1u;
    size_t ebo_idx = EBO_TOMB;
    if (mesh.has_indices()) {
      const ntfr::buffer_data ebo_data {
        .data = mesh.index_data(cirno_model.meshes.indices),
        .size = mesh.indices_size(),
        .offset = 0u,
      };
      auto ebo = ntfr::index_buffer::create(ctx, {
        .flags = ntfr::buffer_flag::dynamic_storage,
        .size = mesh.indices_size(),
        .data = ebo_data,
      }).value();
      ebos.emplace_back(std::move(ebo));
      ebo_idx = ebos.size()-1u;
    }
    meshes.emplace_back(idx, vbo_idx, ebo_idx);
    ++idx;
  }

  float t = 0.f;
  ntfr::render_loop(win, ctx, 60u, ntf::overload{
    [&](u32 fdt) {
      t += 1/(float)fdt;
      transf.rot(ntf::vec3{t*M_PIf*.5f, 0.f, 0.f});
    },
    [&](f32 dt, f32 alpha) {
    const ntfr::uniform_const unifs[] = {
      ntfr::format_uniform_const(u_model, transf.world()),
      ntfr::format_uniform_const(u_proj, proj_mat),
      ntfr::format_uniform_const(u_view, view_mat),
    };
    for (const auto& mesh : meshes){
      const auto& vbo = vbos[mesh.vbo];
      NTF_ASSERT(mesh.ebo != EBO_TOMB);
      const auto& ebo = ebos[mesh.ebo];
      const ntfr::render_opts opts {
        .vertex_count = (uint32)cirno_model.meshes.data[mesh.idx].indices.count,
        .vertex_offset = 0,
        .instances = 0,
      };
      ctx.submit_render_command({
        .target = fbo,
        .pipeline = pipe,
        .buffers = {
          .vertex = vbo,
          .index = ebo,
          .shader = {},
        },
        .textures = {},
        .consts = unifs,
        .opts = opts,
        .sort_group = 0u,
        .render_callback = {},
      });
    }
  }});

  return EXIT_SUCCESS;
}
