#include <shogle/boilerplate.hpp>
#include "model.hpp"

using namespace ntf::numdefs;

static std::string_view vert_src = R"glsl(
#version 460 core

layout (location = 0) in vec3 att_coords;
layout (location = 1) in vec3 att_normals;
layout (location = 2) in vec2 att_texcoords;
layout (location = 3) in ivec4 att_bones;
layout (location = 4) in vec4 att_weights;

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

struct mesh_t {
  u32 indices;
  u32 vert_offset;
  u32 indx_offset;
  u32 tex;
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
  // const auto pip_attrib = ntfr::pnt_vertex::soa_binding();
  std::array<ntfr::attribute_binding, 5u> pip_attrib {{
    {.type = ntfr::attribute_type::vec3,  .location = 0u, .offset = 0u, .stride = 0u},
    {.type = ntfr::attribute_type::vec3,  .location = 1u, .offset = 0u, .stride = 0u},
    {.type = ntfr::attribute_type::vec2,  .location = 2u, .offset = 0u, .stride = 0u},
    {.type = ntfr::attribute_type::ivec4, .location = 3u, .offset = 0u, .stride = 0u},
    {.type = ntfr::attribute_type::vec4,  .location = 4u, .offset = 0u, .stride = 0u},
  }};

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

  const auto fb_ratio = 1280.f/720.f;
  auto proj_mat = glm::perspective(glm::radians(30.f), fb_ratio, .1f, 100.f);
  auto view_mat = glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.f, -3.f});
  auto transf = ntf::transform3d<f32>{}
    .pos(0.f, -1.f, 0.f).scale(1.5f, 1.5f, 1.5f);

  std::vector<mesh_t> meshes;
  meshes.reserve(cirno_meshes.meshes.size());

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

  const ntf::span<vec3> pos_span{cirno_meshes.positions.data(), cirno_meshes.positions.size()};
  const ntfr::buffer_data pos_data {
    .data = pos_span.data(),
    .size = pos_span.size_bytes(),
    .offset = 0u,
  };
  auto pos_vbo = ntfr::vertex_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = pos_span.size_bytes(),
    .data = pos_data,
  }).value();

  const ntf::span<vec3> norm_span{cirno_meshes.normals.data(), cirno_meshes.normals.size()};
  const ntfr::buffer_data norm_data {
    .data = norm_span.data(),
    .size = norm_span.size_bytes(),
    .offset = 0u,
  };
  auto norm_vbo = ntfr::vertex_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = norm_span.size_bytes(),
    .data = norm_data,
  }).value();

  const ntf::span<vec2> uv_span{cirno_meshes.uvs.data(), cirno_meshes.uvs.size()};
  const ntfr::buffer_data uv_data {
    .data = uv_span.data(),
    .size = uv_span.size_bytes(),
    .offset = 0u,
  };
  auto uv_vbo = ntfr::vertex_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = uv_span.size_bytes(),
    .data = uv_data,
  }).value();

  const ntf::span<model_mesh_data::vertex_bones> bone_span{cirno_meshes.bones.data(), cirno_meshes.bones.size()};
  const ntfr::buffer_data bone_data {
    .data = bone_span.data(),
    .size = bone_span.size_bytes(),
    .offset = 0u,
  };
  auto bone_vbo = ntfr::vertex_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = bone_span.size_bytes(),
    .data = bone_data,
  }).value();

  const ntf::span<model_mesh_data::vertex_weights> weight_span{cirno_meshes.weights.data(), cirno_meshes.weights.size()};
  const ntfr::buffer_data weight_data {
    .data = weight_span.data(),
    .size = weight_span.size_bytes(),
    .offset = 0u,
  };
  auto weight_vbo = ntfr::vertex_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = weight_span.size_bytes(),
    .data = weight_data,
  }).value();

  const ntf::span<u32> idx_span{cirno_meshes.indices.data(), cirno_meshes.indices.size()};
  const ntfr::buffer_data idx_data {
    .data = idx_span.data(),
    .size = idx_span.size_bytes(),
    .offset = 0u,
  };
  auto ebo = ntfr::index_buffer::create(ctx, {
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = idx_span.size_bytes(),
    .data = idx_data,
  }).value();

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
    const u32 indx_count = mesh.indices.count;
    const u32 indx_offset = mesh.indices.idx;

    // Assumes all meshes have the same vertex count in each attribute
    const u32 vert_offset = mesh.positions.idx;

    const u32 mat_idx = cirno_materials.material_registry.find(mesh.material_name)->second;
    const auto& mat = cirno_materials.materials[mat_idx];
    u32 tex = 0u;
    if (mat.textures.count == 1){
      tex = cirno_materials.material_textures[mat.textures.idx];
    }

    meshes.emplace_back(indx_count, vert_offset, indx_offset, tex);
  }

  auto bone_transf = ntf::transform3d<f32>{}
    .scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);

  float t = 0.f;
  ntfr::render_loop(win, ctx, 60u, ntf::overload{
    [&](u32 fdt) {
      t += 1/(float)fdt;
      transf.rot(ntf::vec3{t*M_PIf*.5f, 0.f, 0.f});
      bone_transf.rot(ntf::vec3{-t*.5f*M_PIf, 0.f, 0.f});
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
    },
    [&](f32 dt, f32 alpha) {
      ssbo.upload(bone_transf_data);

      const int32 sampler = 0;
      const ntfr::uniform_const unifs[] = {
        ntfr::format_uniform_const(u_model, transf.world()),
        ntfr::format_uniform_const(u_proj, proj_mat),
        ntfr::format_uniform_const(u_view, view_mat),
        ntfr::format_uniform_const(u_sampler, sampler),
      };
      const ntfr::vertex_binding binds[] = {
        {.buffer = pos_vbo,    .layout = 0u},
        {.buffer = norm_vbo,   .layout = 1u},
        {.buffer = uv_vbo,     .layout = 2u},
        {.buffer = bone_vbo,   .layout = 3u},
        {.buffer = weight_vbo, .layout = 4u},
      };
      const ntfr::shader_binding ssbo_bind {
        .buffer = ssbo, .binding = 1u, .size = ssbo.size(), .offset = 0u,
      };
      const ntfr::buffer_binding buff_bind {
        .vertex = binds,
        .index = ebo,
        .shader = {ssbo_bind},
      };
      for (const auto& mesh : meshes) {
        const ntfr::render_opts opts {
          .vertex_count = mesh.indices,
          .vertex_offset = mesh.vert_offset,
          .index_offset = mesh.indx_offset,
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
