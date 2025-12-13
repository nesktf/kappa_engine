#include "./instance.hpp"

namespace kappa::render {

namespace {

expect<shogle::texture2d> make_tex(shogle::context_view ctx, u32 width, u32 height,
                                   const void* data, shogle::image_format format,
                                   shogle::texture_sampler sampler, u32 mipmaps) {
  const auto make_thing = [&](ntf::weak_cptr<shogle::texture_data> tex_data) {
    shogle::typed_texture_desc desc{
      .format = format,
      .sampler = sampler,
      .addressing = shogle::texture_addressing::repeat,
      .extent = {width, height, 1},
      .layers = 1u,
      .levels = mipmaps,
      .data = tex_data,
    };
    return shogle::texture2d::create(ctx, desc).transform_error(shogle_to_str);
  };

  if (data) {
    const shogle::image_data image{
      .bitmap = data,
      .format = format,
      .alignment = 4u,
      .extent = {width, height, 1},
      .offset = {0, 0, 0},
      .layer = 0u,
      .level = 0u,
    };
    const shogle::texture_data data{
      .images = {image},
      .generate_mipmaps = mipmaps > 1u,
    };
    return make_thing(data);
  } else {
    return make_thing({});
  }
}

expect<std::pair<shogle::texture2d, shogle::framebuffer>> make_fb(shogle::context_view ctx,
                                                                  u32 width, u32 height) {
  using lambda_ret = expect<std::pair<shogle::texture2d, shogle::framebuffer>>;
  const auto make_thing = [&](shogle::texture2d&& tex) -> lambda_ret {
    const shogle::fbo_image image{
      .texture = tex,
      .layer = 0,
      .level = 0,
    };
    const shogle::fbo_image_desc fb_desc{
      .extent = {width, height},
      .viewport = {0, 0, width, height},
      .clear_color = {.3f, .3f, .3f, 1.f},
      .clear_flags = shogle::clear_flag::color_depth,
      .test_buffer = shogle::fbo_buffer::depth24u_stencil8u,
      .images = {image},
    };
    auto fb = shogle::framebuffer::create(ctx, fb_desc);
    if (!fb) {
      return {ntf::unexpect, shogle_to_str(std::move(fb.error()))};
    }
    return {ntf::in_place, std::move(tex), std::move(*fb)};
  };

  return make_tex(ctx, width, height, nullptr, shogle::image_format::rgba8u,
                  shogle::texture_sampler::nearest, 1u)
    .and_then(make_thing);
}

expect<shogle::buffer> make_buffer(shogle::context_view ctx, shogle::buffer_type type, size_t size,
                                   const void* data) {
  using lambda_ret = expect<shogle::buffer>;
  const auto make_thing = [&](ntf::weak_cptr<shogle::buffer_data> buff_data) -> lambda_ret {
    const shogle::buffer_desc desc{
      .type = type,
      .flags = shogle::buffer_flag::dynamic_storage,
      .size = size,
      .data = buff_data,
    };
    return shogle::buffer::create(ctx, desc).transform_error(shogle_to_str);
  };
  if (data) {
    const shogle::buffer_data buff_data{
      .data = data,
      .size = size,
      .offset = 0u,
    };
    return make_thing(buff_data);
  } else {
    return make_thing({});
  }
}

template<u32 tex_extent>
[[maybe_unused]] constexpr auto missing_albedo_bitmap = [] {
  std::array<u8, 4u * tex_extent * tex_extent> out;
  const u8 pixels[]{
    0x00, 0x00, 0x00, 0xFF, // black
    0xFE, 0x00, 0xFE, 0xFF, // pink
    0x00, 0x00, 0x00, 0xFF, // black again
  };

  for (u32 i = 0; i < tex_extent; ++i) {
    const u8* start = i % 2 == 0 ? &pixels[0] : &pixels[4]; // Start row with a different color
    u32 pos = 0;
    for (u32 j = 0; j < tex_extent; ++j) {
      pos = (pos + 4) % 8;
      for (u32 k = 0; k < 4; ++k) {
        out[(4 * i * tex_extent) + (4 * j) + k] = start[pos + k];
      }
    }
  }

  return out;
}();

template<u32 tex_extent = 16u>
expect<shogle::texture2d> make_missing_albedo(shogle::context_view ctx) {
  return make_tex(ctx, tex_extent, tex_extent, missing_albedo_bitmap<tex_extent>.data(),
                  shogle::image_format::rgba8u, shogle::texture_sampler::nearest, 1u);
}

} // namespace

ntf::nullable<render_ctx> g_renderer;

render_ctx::render_ctx(shogle::window&& win_, shogle::context&& ctx_,
                       shogle::texture2d&& missing_tex_, vert_shader_array&& vert_shaders_) :

    win{std::move(win_)}, ctx{std::move(ctx_)}, missing_tex{std::move(missing_tex_)},
    vert_shaders{std::move(vert_shaders_)} {}

[[nodiscard]] handle_t initialize() {
  NTF_ASSERT(!g_renderer.has_value());
  u32 win_width = 1280;
  u32 win_height = 720;
  const shogle::win_x11_params x11{
    .class_name = "cino_anim",
    .instance_name = "cino_anim",
  };
  const shogle::win_gl_params win_gl{
    .ver_major = 4,
    .ver_minor = 6,
    .swap_interval = 1,
    .fb_msaa_level = 8,
    .fb_buffer = shogle::fbo_buffer::depth24u_stencil8u,
    .fb_use_alpha = false,
  };
  const shogle::win_params win_params{
    .width = win_width,
    .height = win_height,
    .title = "test",
    .attrib = shogle::win_attrib::decorate | shogle::win_attrib::resizable,
    .renderer_api = shogle::context_api::opengl,
    .platform_params = &x11,
    .renderer_params = &win_gl,
  };
  auto win = shogle::window::create(win_params).value();

  const auto vp = shogle::uvec4{0, 0, win.fb_size()};
  const auto gl_params = shogle::window::make_gl_params(win);
  const color4 fb_color{.3f, .3f, .3f, 1.f};
  const shogle::clear_flag fb_clear{shogle::clear_flag::color_depth};
  const shogle::context_params ctx_params{
    .ctx_params = &gl_params,
    .ctx_api = shogle::context_api::opengl,
    .fb_viewport = vp,
    .fb_clear_flags = fb_clear,
    .fb_clear_color = fb_color,
    .alloc = nullptr,
  };
  auto ctx = shogle::context::create(ctx_params).value();

  auto vert_shaders = initialize_shaders(ctx);
  auto missing_tex = make_missing_albedo(ctx).value();
  g_renderer.emplace(std::move(win), std::move(ctx), std::move(missing_tex),
                     std::move(vert_shaders));
  NTF_ASSERT(g_renderer.has_value());

  return {};
}

handle_t::~handle_t() {
  NTF_ASSERT(g_renderer.has_value());
  g_renderer.reset();
}

expect<shogle::texture2d> create_texture(u32 width, u32 height, const void* data,
                                         shogle::image_format format,
                                         shogle::texture_sampler sampler, u32 mipmaps) {
  NTF_ASSERT(g_renderer.has_value());
  return make_tex(g_renderer->ctx, width, height, data, format, sampler, mipmaps);
}

expect<std::pair<shogle::texture2d, shogle::framebuffer>> create_framebuffer(u32 width,
                                                                             u32 height) {
  NTF_ASSERT(g_renderer.has_value());
  return make_fb(g_renderer->ctx, width, height);
}

shogle::window& window() {
  NTF_ASSERT(g_renderer.has_value());
  return g_renderer->win;
}

shogle::context_view shogle_context() {
  NTF_ASSERT(g_renderer.has_value());
  return g_renderer->ctx;
}

expect<shogle::shader_storage_buffer> create_ssbo(size_t size, const void* data) {
  NTF_ASSERT(g_renderer.has_value());
  return make_buffer(g_renderer->ctx, shogle::buffer_type::shader_storage, size, data)
    .transform([](shogle::buffer&& buffer) -> shogle::shader_storage_buffer {
      return shogle::to_typed<shogle::buffer_type::shader_storage>(std::move(buffer));
    });
}

expect<shogle::uniform_buffer> create_ubo(size_t size, const void* data) {
  NTF_ASSERT(g_renderer.has_value());
  return make_buffer(g_renderer->ctx, shogle::buffer_type::uniform, size, data)
    .transform([](shogle::buffer&& buffer) -> shogle::uniform_buffer {
      return shogle::to_typed<shogle::buffer_type::uniform>(std::move(buffer));
    });
}

expect<shogle::vertex_buffer> create_vbo(size_t size, const void* data) {
  NTF_ASSERT(g_renderer.has_value());
  return make_buffer(g_renderer->ctx, shogle::buffer_type::vertex, size, data)
    .transform([](shogle::buffer&& buffer) -> shogle::vertex_buffer {
      return shogle::to_typed<shogle::buffer_type::vertex>(std::move(buffer));
    });
}

expect<shogle::index_buffer> create_ebo(size_t size, const void* data) {
  NTF_ASSERT(g_renderer.has_value());
  return make_buffer(g_renderer->ctx, shogle::buffer_type::index, size, data)
    .transform([](shogle::buffer&& buffer) -> shogle::index_buffer {
      return shogle::to_typed<shogle::buffer_type::index>(std::move(buffer));
    });
}

expect<shogle::pipeline> make_pipeline(vert_shader_type vert, frag_shader_type frag,
                                       std::vector<shogle::attribute_binding>& bindings,
                                       const pipeline_opts& opts) {
  NTF_ASSERT(g_renderer.has_value());

  u32 vert_flags = VERTEX_STAGE_NO_FLAGS;
  auto vert_stage =
    make_vert_stage(vert, vert_flags, bindings, g_renderer->vert_shaders, opts.use_aos_bindings);

  auto frag_stage = make_frag_stage(g_renderer->ctx, frag, vert_flags);
  if (!frag_stage) {
    return {ntf::unexpect, frag_stage.error()};
  }

  const shogle::shader_t stages[] = {vert_stage, *frag_stage};
  const shogle::pipeline_desc pip_desc{
    .attributes = {bindings.data(), bindings.size()},
    .stages = stages,
    .primitive = opts.primitive,
    .poly_mode = shogle::polygon_mode::fill,
    .poly_width = 1.f,
    .tests = opts.tests,
  };
  return shogle::pipeline::create(g_renderer->ctx, pip_desc).transform_error(shogle_to_str);
}

shogle::framebuffer_view default_fb() {
  NTF_ASSERT(g_renderer.has_value());
  return shogle::framebuffer::get_default(g_renderer->ctx);
}

void render_thing(shogle::framebuffer_view target, u32 sort, const scene_render_data& scene,
                  renderable& obj) {
  NTF_ASSERT(g_renderer.has_value());
  auto& render_data = g_renderer->render_data;

  const u32 mesh_count = obj.retrieve_render_data(scene, render_data);
  if (!mesh_count) {
    return;
  }
  NTF_ASSERT(!render_data.meshes.empty());
  for (const auto& mesh : render_data.meshes) {
    auto tex_span = mesh.textures.to_cspan(render_data.textures.data());
    auto unif_span = mesh.uniforms.to_cspan(render_data.uniforms.data());
    auto bind_span = mesh.bindings.to_cspan(render_data.bindings.data());
    const shogle::buffer_binding buff_bind{
      .vertex = mesh.vertex_buffers,
      .index = mesh.index_buffer,
      .shader = bind_span,
    };
    const shogle::render_opts opts{
      .vertex_count = mesh.vertex_count,
      .vertex_offset = mesh.vertex_offset,
      .index_offset = mesh.index_offset,
      .instances = 0,
    };
    g_renderer->ctx.submit_render_command({
      .target = target,
      .pipeline = mesh.pipeline,
      .buffers = buff_bind,
      .textures = tex_span,
      .consts = unif_span,
      .opts = opts,
      .sort_group = sort + mesh.sort_offset,
      .render_callback = {},
    });
  }
  render_data.meshes.clear();
  render_data.bindings.clear();
  render_data.uniforms.clear();
  render_data.textures.clear();
}

} // namespace kappa::render
