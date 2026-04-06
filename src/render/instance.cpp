#include <shogle/render/data.hpp>

#include "./internal.hpp"

namespace kappa::render {

namespace {

void update_proj(m4f32& m, f32 vp_w, f32 vp_h) {
  static constexpr f32 FOV = shogle::math::rad(45.f);
  m = shogle::math::perspective(FOV, vp_w / vp_h, 0.01f, 10.f);
}

} // namespace

shogle::nullable<render_context> g_ctx;

render_context::render_context(shogle::gl_context&& gl_, shogle::glfw_win&& win_,
                               shogle::gl_texture&& default_tex_) :
    win(std::move(win_)), gl(std::move(gl_)), imgui(win),
    default_texture(std::move(default_tex_)) {}

shogle::glfw_win& window() {
  assert(g_ctx.has_value());
  return g_ctx->win;
}

void initialize(u32 win_w, u32 win_h) {
  assert(!g_ctx.has_value());
  glfwInit();
  const auto hints = shogle::glfw_gl_hints::make_default(4, 6);
  shogle::glfw_win win(win_w, win_h, "test", hints);
  shogle::gl_context gl(win.surface_provider());

  static constexpr auto missing_tex_data = shogle::make_missing_albedo<8>();
  auto tex = shogle::gl_texture::allocate2d(gl, shogle::gl_texture::TEX_FORMAT_RGBA8,
                                            shogle::extent2d(8, 8), 1, 1,
                                            shogle::gl_texture::MULTISAMPLE_NONE)
               .value();
  const shogle::gl_texture::image_data missing_data{
    .data = missing_tex_data.data(),
    .extent = shogle::extent3d(8, 8, 1),
    .format = shogle::gl_texture::PIXEL_FORMAT_RGBA,
    .datatype = shogle::gl_texture::PIXEL_TYPE_U8,
    .alignment = shogle::gl_texture::ALIGN_4BYTES,
  };
  tex.upload_image(gl, missing_data).value();

  g_ctx.emplace(std::move(gl), std::move(win), std::move(tex));

  update_proj(g_ctx->proj, (f32)win_w, (f32)win_h);

  g_ctx->win.set_viewport_callback([&](auto, const shogle::extent2d& vp) {
    update_proj(g_ctx->proj, (f32)vp.width, (f32)vp.height);
  });
}

void destroy() {
  assert(g_ctx.has_value());
  shogle::gl_texture::deallocate(g_ctx->gl, g_ctx->default_texture);
  g_ctx->textures.for_each(
    [](shogle::gl_texture& tex) { shogle::gl_texture::deallocate(g_ctx->gl, tex); });
  g_ctx->buffers.for_each(
    [](shogle::gl_buffer& buff) { shogle::gl_buffer::deallocate(g_ctx->gl, buff); });
  g_ctx->pipelines.for_each([](pipeline_data& pip) {
    shogle::gl_pipeline::destroy(g_ctx->gl, pip.pipeline);
    shogle::gl_vertex_layout::destroy(g_ctx->gl, pip.layout);
  });
  g_ctx->imgui.destroy();
  g_ctx.reset();
  glfwTerminate();
}

void start_frame() {
  assert(g_ctx.has_value());
  shogle::gl_clear_builder clear_builder;
  const auto frame_clear = clear_builder.set_clear_color(.3f, .3f, .3f, 1.f)
                             .set_clear_flag(shogle::gl_clear_opts::CLEAR_COLOR)
                             .set_clear_flag(shogle::gl_clear_opts::CLEAR_DEPTH)
                             .build();
  g_ctx->gl.start_frame(frame_clear);
  g_ctx->imgui.start_frame();
}

void end_frame() {
  assert(g_ctx.has_value());
  g_ctx->imgui.end_frame();
  g_ctx->gl.end_frame();
}

void submit_render_batch(span<const render_data> batch) {
  assert(g_ctx.has_value());
  shogle::gl_cmd_builder builder;
  static f32 t = 0.f;
  for (const auto& data : batch) {
    auto* pip = g_ctx->pipelines.at_opt((u32)data.pipeline);
    if (!pip) {
      logger::warning("Skipping data with invalid pipeline {}", (u32)data.pipeline);
      continue;
    }

    builder.reset();
    builder.set_vertex_layout(pip->layout);
    builder.set_pipeline(pip->pipeline);
    builder.set_draw_count(data.draw_count);

    for (const auto& binding : data.textures) {
      const auto& tex = is_nil_handle(binding.texture) ? g_ctx->default_texture
                                                       : g_ctx->textures[(u32)binding.texture];
      builder.add_texture(tex, binding.location);
    }
    for (const auto& uniform : data.uniforms) {
      builder.add_uniform(uniform.data, uniform.location, uniform.type);
    }
    m4f32 model(1.f);
    model = shogle::math::translate(model, shogle::vec3(0.f, -1.f, -3.f));
    model = shogle::math::rotate(model, t * shogle::math::pi<f32>, shogle::vec3(0.f, 1.f, 0.f));
    builder.add_uniform(g_ctx->proj, 1);
    builder.add_uniform(model, 2);
    const auto cmd = builder.build();
    g_ctx->gl.submit_immediate_command(cmd);
  }
  t += 0.016;
}

texture_handle create_texture(const texture_create_data& data) {
  assert(g_ctx.has_value());
  auto tex =
    shogle::gl_texture::allocate2d(g_ctx->gl, shogle::gl_texture::TEX_FORMAT_RGBA8, data.extent, 1,
                                   7, shogle::gl_texture::MULTISAMPLE_NONE)
      .transform([](shogle::gl_texture&& tex) -> texture_handle {
        auto slot = g_ctx->textures.insert(std::move(tex));
        return (texture_handle)slot;
      });
  if (!tex) {
    logger::error("Failed to create texture: {}", tex.error().what());
  }
  return tex.value_or(nil_handle<texture_handle>());
}

void destroy_texture(texture_handle texture) {
  assert(g_ctx.has_value());
  if (is_nil_handle(texture)) {
    return;
  }
  if (auto* tex = g_ctx->textures.at_opt((u32)texture)) {
    shogle::gl_texture::deallocate(g_ctx->gl, *tex);
    g_ctx->textures.remove((u32)texture);
  }
}

s_expect<buffer_handle> create_buffer(size_t buffer_size) {
  assert(g_ctx.has_value());

  return shogle::gl_buffer::allocate(g_ctx->gl, buffer_size)
    .transform([](shogle::gl_buffer&& buff) -> buffer_handle {
      auto slot = g_ctx->buffers.insert(std::move(buff));
      return (buffer_handle)slot;
    })
    .transform_error([](shogle::gl_error&& err) -> std::string { return err.what(); });
}

void update_buffer(buffer_handle buffer, const void* data, size_t data_size, size_t data_offset) {
  assert(g_ctx.has_value());
  assert(g_ctx->buffers.has_element((u32)buffer));
  g_ctx->buffers[(u32)buffer].upload_data(g_ctx->gl, data, data_size, data_offset).value();
}

void destroy_buffer(buffer_handle buffer) {
  assert(g_ctx.has_value());
  if (auto* buff = g_ctx->buffers.at_opt((u32)buffer)) {
    shogle::gl_buffer::deallocate(g_ctx->gl, *buff);
    g_ctx->buffers.remove((u32)buffer);
  }
}

} // namespace kappa::render
