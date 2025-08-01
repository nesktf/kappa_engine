#include "renderer.hpp"
#include "shaders.hpp"

enum vertex_stage_flags {
  VERTEX_STAGE_NO_FLAGS = 0,

  VERTEX_STAGE_SCENE_TRANSFORMS = 1 << 0,

  VERTEX_STAGE_EXPORTS_TANGENTS = 1 << 1,
  VERTEX_STAGE_EXPORTS_NORMALS = 1 << 2,
  VERTEX_STAGE_EXPORTS_CUBEMAP_UVS = 1 << 3,

  VERTEX_STAGE_MODEL_NONE = 0 << 4, // Has no model uniform
  VERTEX_STAGE_MODEL_MATRIX = 1 << 4, // Has a single matrix as model uniform
  VERTEX_STAGE_MODEL_ARRAY = 2 << 4, // Has an array of matrices as model uniform
  VERTEX_STAGE_MODEL_OFFSET = 3 << 4, // Has a matrix and a vec4 with offsets as model uniform
};

enum fragment_stage_flags {
  FRAGMENT_STAGE_NO_FLAGS = 0,

  FRAGMENT_STAGE_TANGENTS = 1 << 0,
  FRAGMENT_STAGE_NORMALS = 1 << 1,

  FRAGMENT_STAGE_SAMPLER_COUNT = 3 << 2,
};

enum fragment_sampler_type {
  FRAGMENT_SAMPLER_ALBEDO = 0, // We use albedo and diffuse interchangeably
  FRAGMENT_SAMPLER_SPECULAR,
  FRAGMENT_SAMPLER_NORMALS,
  FRAGMENT_SAMPLER_DISPLACEMENT,

  FRAGMENT_SAMPLER_COUNT,
};

static shogle::render_expect<shogle::context> make_gl_ctx(
  const shogle::window& win,
  const color4& fb_color = {.3f, .3f, .3f, 1.f},
  shogle::clear_flag fb_clear = shogle::clear_flag::color_depth
) {
  if (win.renderer() != shogle::context_api::opengl) {
    return {ntf::unexpect, shogle::render_error::invalid_handle, "Invalid window context"};
  }
  const auto vp = shogle::uvec4{0, 0, win.fb_size()};
  const auto gl_params = shogle::window::make_gl_params(win);
  auto ctx = shogle::context::create({
    .ctx_params = &gl_params,
    .ctx_api = shogle::context_api::opengl,
    .fb_viewport = vp,
    .fb_clear_flags = fb_clear,
    .fb_clear_color = fb_color,
    .alloc = nullptr,
  });
  if (!ctx) {
    return ntf::unexpected{std::move(ctx.error())};
  }
  return std::move(*ctx);
}

renderer::renderer(shogle::window&& win, shogle::context&& ctx, vert_shader_array&& vert_shaders) :
  _win{std::move(win)}, _ctx{std::move(ctx)}, _vert_shaders{std::move(vert_shaders)} {}

renderer::handle_t renderer::construct() {
  u32 win_width = 1280;
  u32 win_height = 720;
  const shogle::win_x11_params x11 {
    .class_name = "cino_anim",
    .instance_name = "cino_anim",
  };
  const shogle::win_gl_params win_gl {
    .ver_major = 4,
    .ver_minor = 6,
    .swap_interval = 1,
    .fb_msaa_level = 8,
    .fb_buffer = shogle::fbo_buffer::depth24u_stencil8u,
    .fb_use_alpha = false,
  };
  auto win = shogle::window::create({
    .width = win_width,
    .height = win_height,
    .title = "test",
    .attrib = shogle::win_attrib::decorate | shogle::win_attrib::resizable,
    .renderer_api = shogle::context_api::opengl,
    .platform_params = &x11,
    .renderer_params = &win_gl,
  }).value();
  auto ctx = make_gl_ctx(win, {.3f, .3f, .3f, .0f}).value();

  auto vert_rigged = *shogle::vertex_shader::create(ctx, {vert_rigged_model_src});
  auto vert_static = *shogle::vertex_shader::create(ctx, {vert_static_model_src});
  auto vert_generic = *shogle::vertex_shader::create(ctx, {vert_generic_model_src});
  auto vert_skybox = *shogle::vertex_shader::create(ctx, {vert_skybox_src});
  auto vert_sprite = *shogle::vertex_shader::create(ctx, {vert_sprite_src});
  auto vert_effect = *shogle::vertex_shader::create(ctx, {vert_effect_src});
  vert_shader_array shaders {
    std::move(vert_rigged), std::move(vert_static), std::move(vert_generic),
    std::move(vert_skybox), std::move(vert_sprite), std::move(vert_effect)
  };
  _construct(std::move(win), std::move(ctx), std::move(shaders));

  return {};
}

shogle::vertex_shader_view renderer::_make_vert_stage(vert_shader_type type, u32& flags,
                                                    std::vector<shogle::attribute_binding>& bindings,
                                                    bool aos_bindings)
{
  struct rig_vertex {
    vec3 pos;
    vec3 norm;
    vec2 uvs;
    vec3 tang;
    vec3 bitang;
    ivec4 bones;
    vec4 weights;
  };

  struct tang_vertex {
    vec3 pos;
    vec3 norm;
    vec2 uvs;
    vec3 tang;
    vec3 bitang;
  };

  struct generic_vertex {
    vec3 pos;
    vec3 norm;
    vec2 uvs;
  };

  struct skybox_vertex {
    vec3 pos;
  };

  switch (type) {
    case VERT_SHADER_RIGGED_MODEL: {
      bindings.reserve(7u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(rig_vertex, pos),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(rig_vertex, norm),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(rig_vertex, uvs),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3,  3u,
                            aos_bindings*offsetof(rig_vertex, tang),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 4u,
                            aos_bindings*offsetof(rig_vertex, bitang),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::ivec4,  5u,
                            aos_bindings*offsetof(rig_vertex, bones),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(shogle::attribute_type::vec4,  6u,
                            aos_bindings*offsetof(rig_vertex, weights),
                            aos_bindings*sizeof(rig_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS | VERTEX_STAGE_EXPORTS_TANGENTS |
        VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_ARRAY;
      break;
    }
    case VERT_SHADER_STATIC_MODEL: {
      bindings.reserve(5u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(tang_vertex, pos),
                            aos_bindings*sizeof(tang_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(tang_vertex, norm),
                            aos_bindings*sizeof(tang_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(tang_vertex, uvs),
                            aos_bindings*sizeof(tang_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3,  3u,
                            aos_bindings*offsetof(tang_vertex, tang),
                            aos_bindings*sizeof(tang_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3, 4u,
                            aos_bindings*offsetof(tang_vertex, bitang),
                            aos_bindings*sizeof(tang_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS | VERTEX_STAGE_EXPORTS_TANGENTS |
        VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_MATRIX;
      break;
    }
    case VERT_SHADER_GENERIC_MODEL: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(generic_vertex, pos),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(generic_vertex, norm),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(generic_vertex, uvs),
                            aos_bindings*sizeof(generic_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS |
        VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_MATRIX;
      break;
    }
    case VERT_SHADER_SKYBOX: {
      bindings.reserve(1u);
      bindings.emplace_back(shogle::attribute_type::vec3, 0u,
                            aos_bindings*offsetof(skybox_vertex, pos),
                            aos_bindings*sizeof(skybox_vertex));
      flags =
        VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_NONE;
      break;
    }
    case VERT_SHADER_SPRITE: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(generic_vertex, pos),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(generic_vertex, norm),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(generic_vertex, uvs),
                            aos_bindings*sizeof(generic_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS |
        VERTEX_STAGE_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_OFFSET;
      break;
    }
    case VERT_SHADER_EFFECT: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(shogle::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(generic_vertex, pos),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(generic_vertex, norm),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(shogle::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(generic_vertex, uvs),
                            aos_bindings*sizeof(generic_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS;
      break;
    }
    default: NTF_UNREACHABLE();
  }

  return _vert_shaders[type];
}

expect<shogle::pipeline> renderer::make_pipeline(vert_shader_type vert, frag_shader_type frag,
                                               std::vector<shogle::attribute_binding>& bindings,
                                               const pipeline_opts& opts)
{
  auto ctx = renderer::instance().ctx();

  u32 vert_flags = VERTEX_STAGE_NO_FLAGS;
  auto vert_stage = _make_vert_stage(vert, vert_flags, bindings, opts.use_aos_bindings);

  auto frag_stage = _make_frag_stage(frag, vert_flags);
  if (!frag_stage) {
    return {ntf::unexpect, frag_stage.error()};
  }

  const shogle::shader_t stages[] = {vert_stage, *frag_stage};
  return shogle::pipeline::create(ctx, {
    .attributes = {bindings.data(), bindings.size()},
    .stages = stages,
    .primitive = opts.primitive,
    .poly_mode = shogle::polygon_mode::fill,
    .poly_width = 1.f,
    .tests = opts.tests,
  })
  .transform_error([](shogle::render_error&& err) -> std::string_view {
    return err.msg();
  });
}

expect<shogle::fragment_shader> renderer::_make_frag_stage(frag_shader_type type, u32 vert_flags) {
  auto ctx = renderer::instance().ctx();
  std::vector<shogle::string_view> srcs;
  srcs.emplace_back(frag_header_base_src);
  srcs.emplace_back(frag_tangents_base_src);
  srcs.emplace_back(frag_raw_albedo_src);

  return shogle::fragment_shader::create(ctx, {srcs.data(), srcs.size()})
  .transform_error([](shogle::render_error&& err) -> std::string_view {
    return err.msg();
  });
}

void renderer::render(shogle::framebuffer_view target, u32 sort,
                      const scene_render_data& scene, renderable& obj) {
  const u32 mesh_count = obj.retrieve_render_data(scene, _render_data);
  if (!mesh_count) {
    return;
  }
  NTF_ASSERT(mesh_count && !_render_data.meshes.empty());
  for (const auto& mesh : _render_data.meshes) {
    auto tex_span = mesh.textures.to_cspan(_render_data.textures.data());
    auto unif_span = mesh.uniforms.to_cspan(_render_data.uniforms.data());
    auto bind_span = mesh.bindings.to_cspan(_render_data.bindings.data());
    const shogle::buffer_binding buff_bind {
      .vertex = mesh.vertex_buffers,
      .index = mesh.index_buffer,
      .shader = bind_span,
    };
    const shogle::render_opts opts {
      .vertex_count = mesh.vertex_count,
      .vertex_offset = mesh.vertex_offset,
      .index_offset = mesh.index_offset,
      .instances = 0,
    };
    _ctx.submit_render_command({
      .target = target,
      .pipeline = mesh.pipeline,
      .buffers = buff_bind,
      .textures = tex_span,
      .consts = unif_span,
      .opts = opts,
      .sort_group = sort+mesh.sort_offset,
      .render_callback = {},
    });
  }
  _render_data.meshes.clear();
  _render_data.bindings.clear();
  _render_data.uniforms.clear();
  _render_data.textures.clear();
}
