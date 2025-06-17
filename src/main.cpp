#include <shogle/boilerplate.hpp>
#include "model_data.hpp"
#include "model.hpp"

using namespace ntf::numdefs;

static std::string_view vert_src = R"glsl(
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

static std::string_view frag_src = R"glsl(
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

int main() {
  ntf::logger::set_level(ntf::log_level::debug);

  auto cousine =
    ntf::load_font_atlas<char>("./lib/shogle/demos/res/CousineNerdFont-Regular.ttf").value();

  auto cirno_img = ntf::load_image<ntf::uint8>("./lib/shogle/demos/res/cirno_cpp.jpg").value();

  assimp_parser parser;
  parser.load("./res/chiruno/chiruno.gltf");

  model_rig_data cirno_rigs;
  parser.parse_rigs(cirno_rigs);

  model_material_data cirno_materials;
  parser.parse_materials(cirno_materials);

  model_mesh_data cirno_meshes;
  parser.parse_meshes(cirno_rigs, cirno_meshes);

  // for (const auto& mesh : cirno_meshes.meshes) {
  //   ntf::logger::info("MESH: {}", mesh.name);
  //   const auto idx_span = mesh.bones.to_cspan(cirno_meshes.bones.data());
  //   const auto wei_span = mesh.bones.to_cspan(cirno_meshes.weights.data());
  //   for (u32 i = 0; i < idx_span.size(); ++i) {
  //     ntf::logger::debug("- [{}, {}, {}, {}] -> [{}, {}, {}, {}]",
  //                        idx_span[i][0], idx_span[i][1], idx_span[i][2], idx_span[i][3],
  //                        wei_span[i][0], wei_span[i][1], wei_span[i][2], wei_span[i][3]);
  //   }
  // }

  // for (const auto& [name, idx] : cirno_rigs.bone_registry) {
  //   ntf::logger::info("BONE: {}, IDX: {}", name, idx);
  // }

  u32 win_width = 1280;
  u32 win_height = 720;
  const ntfr::win_x11_params x11 {
    .class_name = "cino_anim",
    .instance_name = "cino_anim",
  };
  const ntfr::win_gl_params win_gl {
    .ver_major = 4,
    .ver_minor = 6,
    .swap_interval = 0,
    .fb_msaa_level = 8,
    .fb_buffer = ntfr::fbo_buffer::depth24u_stencil8u,
    .fb_use_alpha = false,
  };
  auto win = ntfr::window::create({
    .width = win_width,
    .height = win_height,
    .title = "test",
    .attrib = ntfr::win_attrib::decorate | ntfr::win_attrib::resizable,
    .renderer_api = ntfr::context_api::opengl,
    .platform_params = &x11,
    .renderer_params = &win_gl,
  }).value();
  auto ctx = ntfr::make_gl_ctx(win, {.3f, .3f, .3f, .0f}).value();

  ntf::mat4 cam_proj_fnt = glm::ortho(0.f, (float)win_width, 0.f, (float)win_height);
  ntfr::text_buffer text_buffer;
  auto frenderer = ntfr::font_renderer::create(ctx, cam_proj_fnt, std::move(cousine)).value();
  auto sdf_rule = ntfr::sdf_text_rule::create(ctx,
                                             ntf::color3{1.f, 1.f, 1.f}, 0.5f, 0.05f,
                                             ntf::color3{0.f, 0.f, 0.f},
                                             ntf::vec2{-0.005f, -0.005f},
                                             0.62f, 0.05f).value();
  auto quad = ntfr::quad_mesh::create(ctx).value();
  auto cirno_tex = ntfr::make_texture2d(ctx, cirno_img,
                                        ntfr::texture_sampler::nearest,
                                        ntfr::texture_addressing::repeat).value();

  auto vert_src_quad = ntf::file_contents("./lib/shogle/demos/res/shaders/vert_base.vs.glsl").value();
  auto frag_tex_src = ntf::file_contents("./lib/shogle/demos/res/shaders/frag_tex.fs.glsl").value();

  auto vertex = ntfr::vertex_shader::create(ctx, {vert_src_quad}).value();
  auto fragment_tex = ntfr::fragment_shader::create(ctx, {frag_tex_src}).value();
  auto pipe_tex = ntfr::make_pipeline<ntfr::pnt_vertex>(vertex, fragment_tex).value();

  auto u_tex_model = pipe_tex.uniform("model");
  auto u_tex_proj = pipe_tex.uniform("proj");
  auto u_tex_view = pipe_tex.uniform("view");
  auto u_tex_color = pipe_tex.uniform("color");
  auto u_tex_sampler = pipe_tex.uniform("sampler0");

  ntf::mat4 cam_view_quad = glm::translate(glm::mat4{1.f}, glm::vec3{640.f, 360.f, -3.f});
  ntf::mat4 cam_proj_quad = glm::ortho(0.f, (float)win_width, 0.f, (float)win_height, .1f, 100.f);
  auto transf_quad = ntf::transform2d<ntf::float32>{}
    .pos(425.f, -175.f).scale(ntf::vec2{256.f, 256.f});

  auto fbo = ntfr::framebuffer::get_default(ctx);
  bool do_things = true;
  win.set_viewport_callback([&](ntfr::window&, const ntf::extent2d& ext) {
    fbo.viewport({0, 0, ext.x, ext.y});
  });
  win.set_key_press_callback([&](ntfr::window& win, const ntfr::win_key_data& k) {
    if (k.action == ntfr::win_action::press) {
      if (k.key == ntfr::win_key::escape) {
        win.close();
      }
      if (k.key == ntfr::win_key::space) {
        do_things = !do_things;
      }
    }
  });

  std::vector<ntfr::attribute_binding> pip_attrib;
  std::vector<mesh_render_data> model_render_data;
  auto model = model_mesh_provider::create(ctx, cirno_meshes).value();
  model.retrieve_bindings(pip_attrib);
  model.retrieve_render_data(model_render_data);

  auto vert_sh = ntfr::vertex_shader::create(ctx, {vert_src}).value();
  auto frag_sh = ntfr::fragment_shader::create(ctx, {frag_src}).value();
  const ntfr::shader_t pip_stages[] = {vert_sh, frag_sh};
  const ntfr::face_cull_opts cull {
    .mode = ntfr::cull_mode::back,
    .front_face = ntfr::cull_face::counter_clockwise,
  };
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
      .face_culling = cull,
      .blending = ntfr::def_blending_opts,
    },
  }).value();

  auto u_model = pipe.uniform("u_model");
  auto u_proj = pipe.uniform("u_proj");
  auto u_view = pipe.uniform("u_view");
  auto u_sampler = pipe.uniform("u_sampler");

  const auto fb_ratio = (float)win_width/(float)win_height;
  auto proj_mat = glm::perspective(glm::radians(35.f), fb_ratio, .1f, 100.f);
  auto view_mat = glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.f, -3.f});
  auto transf = ntf::transform3d<f32>{}
    .pos(0.f, -1.f, 0.f).scale(1.5f, 1.5f, 1.5f);

  std::vector<u32> mesh_texs;
  mesh_texs.reserve(cirno_meshes.meshes.size());

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

  std::vector<mat4> bone_shader_transforms(cirno_rigs.bones.size()-1, mat4{1.f});
  std::vector<mat4> bone_transforms(cirno_rigs.bones.size()-1, mat4{1.f});

  const ntfr::buffer_data bone_transf_data {
    .data = bone_shader_transforms.data(),
    .size = bone_shader_transforms.size()*sizeof(mat4),
    .offset = 0u,
  };
  auto ssbo = ntfr::shader_storage_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = bone_shader_transforms.size()*sizeof(mat4),
    .data = bone_transf_data,
  }).value();

  for (const auto& mesh : cirno_meshes.meshes) {
    const u32 mat_idx = cirno_materials.material_registry.find(mesh.material_name)->second;
    const auto& mat = cirno_materials.materials[mat_idx];
    u32 tex = 0u;
    if (mat.textures.count == 1){
      tex = cirno_materials.material_textures[mat.textures.idx];
    }
    mesh_texs.emplace_back(tex);
  }

  auto bone_transf = ntf::transform3d<f32>{}
    .scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);

  float t = 0.f, t2 = 0.f;
  double avg_fps{};
  double fps[60] = {0};
  u32 fps_counter{};

  float text_scale = 1.f;
  ntfr::render_loop(win, ctx, 60u, ntf::overload{
    [&](u32 fdt) {
      if (!do_things) {return;}
      t += 1/(float)fdt;
      transf_quad.roll(t*M_PI*.5f);
      transf.rot(ntf::vec3{t*M_PIf*.5f, 0.f, 0.f});
      // bone_transf.rot(ntf::vec3{-t*.5f*M_PIf, 0.f, 0.f});
      bone_transforms[6] = bone_transf.world();

      std::vector<mat4> local_transform(cirno_rigs.bones.size()-1, mat4{1.f});
      std::vector<mat4> model_transform(cirno_rigs.bones.size()-1, mat4{1.f});

      for (u32 i = 0; i < cirno_rigs.bones.size()-1; ++i) {
        local_transform[i] = cirno_rigs.bone_locals[i]*bone_transforms[i];
      }
      model_transform[0] = local_transform[0];

      for (u32 i = 1; i < cirno_rigs.bones.size()-1; ++i) {
        u32 parent = cirno_rigs.bones[i].parent;
        model_transform[i] = model_transform[parent] * local_transform[i];
      }

      for (u32 i = 0; i < cirno_rigs.bones.size()-1; ++i) {
        bone_shader_transforms[i] = model_transform[i]*cirno_rigs.bone_inv_models[i];
      }

      text_scale = 1.3f*std::abs(std::sin(t*M_PIf))+1.f;
    },
    [&](f32 dt, f32 alpha) {
      NTF_UNUSED(alpha);
      t2 += dt;
      if (t2 > 0.016f) {
        fps[fps_counter] = 1/dt;
        fps_counter++;
        t2 = 0.f;
      }
      if (fps_counter > 60) {
        fps_counter = 0;
        avg_fps = 0;
        for (u32 i = 0; i < 60; ++i) {
          avg_fps += fps[i];
        }
        avg_fps /= 60.f;
      }

      ssbo.upload(bone_transf_data);

      const int32 sampler = 0;
      const ntfr::uniform_const unifs[] = {
        ntfr::format_uniform_const(u_model, transf.world()),
        ntfr::format_uniform_const(u_proj, proj_mat),
        ntfr::format_uniform_const(u_view, view_mat),
        ntfr::format_uniform_const(u_sampler, sampler),
      };
      const ntfr::shader_binding ssbo_bind {
        .buffer = ssbo, .binding = 1u, .size = ssbo.size(), .offset = 0u,
      };

      for (size_t i = 0; i < model_render_data.size(); ++i) {
        const auto& mesh = model_render_data[i];
        const u32 tex = mesh_texs[i];
        const ntfr::buffer_binding buff_bind {
          .vertex = mesh.vertex_buffers,
          .index = mesh.index_buffer,
          .shader = {ssbo_bind},
        };
        const ntfr::render_opts opts {
          .vertex_count = mesh.vertex_count,
          .vertex_offset = mesh.vertex_offset,
          .index_offset = mesh.index_offset,
          .instances = 0,
        };
        ctx.submit_render_command({
          .target = fbo,
          .pipeline = pipe,
          .buffers = buff_bind,
          .textures = {texs[tex]},
          .consts = unifs,
          .opts = opts,
          .sort_group = 0u,
          .render_callback = {},
        });
      }

      const ntfr::render_opts quad_opts {
        .vertex_count = 6,
        .vertex_offset = 0,
        .index_offset = 0,
        .instances = 0,
      };
      auto cino_tex_handle = cirno_tex.get();
      const ntfr::uniform_const cino_unifs[] = {
        ntfr::format_uniform_const(u_tex_model, transf_quad.world()),
        ntfr::format_uniform_const(u_tex_proj, cam_proj_quad),
        ntfr::format_uniform_const(u_tex_view, cam_view_quad),
        ntfr::format_uniform_const(u_tex_color, ntf::color4{1.f, 1.f, 1.f, 1.f}),
        ntfr::format_uniform_const(u_tex_sampler, sampler),
      };
      const auto quad_bbind = quad.bindings();
      ctx.submit_render_command({
        .target = fbo,
        .pipeline = pipe_tex,
        .buffers = quad_bbind,
        .textures = {cino_tex_handle},
        .consts = cino_unifs,
        .opts = quad_opts,
        .sort_group = 0u,
        .render_callback = {},
      });

      text_buffer.clear();
      text_buffer.append_fmt(frenderer.glyphs(), 40.f, 100.f, 1.f,
                             "Hello World! ~ze\n{:.2f}fps - {:.2f}ms", avg_fps, 1000/avg_fps);
      text_buffer.append_fmt(frenderer.glyphs(), 20.f, 400.f, text_scale, "(9) -->");
      text_buffer.append_fmt(frenderer.glyphs(), 820.f, 400.f, text_scale, "<-- (9)");
      frenderer.render(quad, fbo, sdf_rule, text_buffer, 1u);
    }
  });

  return EXIT_SUCCESS;
}
