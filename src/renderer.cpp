#include "renderer.hpp"
#include "shaders.hpp"

renderer::renderer(ntfr::window&& win, ntfr::context&& ctx,
                   pipeline_provider&& pip_prov) :
  _win{std::move(win)}, _ctx{std::move(ctx)},
  _pip_prov{std::move(pip_prov)}{}

renderer::handle_t renderer::construct() {
  u32 win_width = 1280;
  u32 win_height = 720;
  const ntfr::win_x11_params x11 {
    .class_name = "cino_anim",
    .instance_name = "cino_anim",
  };
  const ntfr::win_gl_params win_gl {
    .ver_major = 4,
    .ver_minor = 6,
    .swap_interval = 1,
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

  _construct(std::move(win), std::move(ctx), pipeline_provider::create(ctx));
  return {};
}

vertex_stage_props renderer::make_vert_stage(pipeline_provider::vert_type type,
                                             bool aos_bindings)
{
  return _pip_prov.make_vert_stage(type, aos_bindings);
}

pipeline_provider::pipeline_provider(std::array<ntfr::vertex_shader, VERT_COUNT>&& shaders) :
  _vert_shaders{std::move(shaders)} {}

pipeline_provider pipeline_provider::create(ntfr::context_view ctx) {
  auto vert_rigged = *ntfr::vertex_shader::create(ctx, {vert_rigged_model_src});
  auto vert_static = *ntfr::vertex_shader::create(ctx, {vert_static_model_src});
  auto vert_generic = *ntfr::vertex_shader::create(ctx, {vert_generic_model_src});
  auto vert_skybox = *ntfr::vertex_shader::create(ctx, {vert_skybox_src});
  auto vert_sprite = *ntfr::vertex_shader::create(ctx, {vert_sprite_src});
  auto vert_effect = *ntfr::vertex_shader::create(ctx, {vert_effect_src});
  std::array<ntfr::vertex_shader, VERT_COUNT> shaders{
    std::move(vert_rigged), std::move(vert_static), std::move(vert_generic),
    std::move(vert_skybox), std::move(vert_sprite), std::move(vert_effect)
  };
  return pipeline_provider{std::move(shaders)};
}

vertex_stage_props pipeline_provider::make_vert_stage(vert_type type, bool aos_bindings) {
  u32 flags = VERTEX_STAGE_NO_FLAGS;
  std::vector<ntfr::attribute_binding> bindings;

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
    case VERT_RIGGED_MODEL: {
      bindings.reserve(7u);
      // type, location, offset, stride
      bindings.emplace_back(ntfr::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(rig_vertex, pos),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(rig_vertex, norm),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(rig_vertex, uvs),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3,  3u,
                            aos_bindings*offsetof(rig_vertex, tang),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3, 4u,
                            aos_bindings*offsetof(rig_vertex, bitang),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(ntfr::attribute_type::ivec4,  5u,
                            aos_bindings*offsetof(rig_vertex, bones),
                            aos_bindings*sizeof(rig_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec4,  6u,
                            aos_bindings*offsetof(rig_vertex, weights),
                            aos_bindings*sizeof(rig_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS | VERTEX_STAGE_EXPORTS_TANGENTS |
        VERTEX_STAGE_HAS_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_ARRAY;
      break;
    }
    case VERT_STATIC_MODEL: {
      bindings.reserve(5u);
      // type, location, offset, stride
      bindings.emplace_back(ntfr::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(tang_vertex, pos),
                            aos_bindings*sizeof(tang_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(tang_vertex, norm),
                            aos_bindings*sizeof(tang_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(tang_vertex, uvs),
                            aos_bindings*sizeof(tang_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3,  3u,
                            aos_bindings*offsetof(tang_vertex, tang),
                            aos_bindings*sizeof(tang_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3, 4u,
                            aos_bindings*offsetof(tang_vertex, bitang),
                            aos_bindings*sizeof(tang_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS | VERTEX_STAGE_EXPORTS_TANGENTS |
        VERTEX_STAGE_HAS_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_MATRIX;
      break;
    }
    case VERT_GENERIC_MODEL: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(ntfr::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(generic_vertex, pos),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(generic_vertex, norm),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(generic_vertex, uvs),
                            aos_bindings*sizeof(generic_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS |
        VERTEX_STAGE_HAS_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_MATRIX;
      break;
    }
    case VERT_SKYBOX: {
      bindings.reserve(1u);
      bindings.emplace_back(ntfr::attribute_type::vec3, 0u,
                            aos_bindings*offsetof(skybox_vertex, pos),
                            aos_bindings*sizeof(skybox_vertex));
      flags =
        VERTEX_STAGE_HAS_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_NONE;
      break;
    }
    case VERT_SPRITE: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(ntfr::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(generic_vertex, pos),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(generic_vertex, norm),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(generic_vertex, uvs),
                            aos_bindings*sizeof(generic_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS |
        VERTEX_STAGE_HAS_SCENE_TRANSFORMS | VERTEX_STAGE_MODEL_OFFSET;
      break;
    }
    case VERT_EFFECT: {
      bindings.reserve(3u);
      // type, location, offset, stride
      bindings.emplace_back(ntfr::attribute_type::vec3,  0u,
                            aos_bindings*offsetof(generic_vertex, pos),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec3,  1u,
                            aos_bindings*offsetof(generic_vertex, norm),
                            aos_bindings*sizeof(generic_vertex));
      bindings.emplace_back(ntfr::attribute_type::vec2,  2u,
                            aos_bindings*offsetof(generic_vertex, uvs),
                            aos_bindings*sizeof(generic_vertex));
      flags =
        VERTEX_STAGE_EXPORTS_NORMALS;
      break;
    }
    default: NTF_UNREACHABLE();
  }

  return vertex_stage_props{std::move(bindings), _vert_shaders[type], flags};
}
