#include "./internal.hpp"

namespace kappa::render {

namespace {

shogle::nullable<render_context> g_ctx;

void update_proj(m4f32& m, f32 vp_w, f32 vp_h) {
  static constexpr f32 FOV = shogle::math::rad(45.f);
  m = shogle::math::perspective(FOV, vp_w / vp_h, 0.01f, 10.f);
}

} // namespace

render_context::render_context(shogle::gl_context&& gl_, shogle::glfw_win&& win_,
                               shogle::gl_pipeline&& pipeline_) :
    gl(std::move(gl_)), win(std::move(win_)), pipeline(std::move(pipeline_)) {}

shogle::glfw_win& window() {
  assert(g_ctx.has_value());
  return g_ctx->win;
}

void destroy() {
  assert(g_ctx.has_value());
  glfwTerminate();
}

void initialize(u32 win_w, u32 win_h) {
  assert(!g_ctx.has_value());
  glfwInit();
  const auto hints = shogle::glfw_gl_hints::make_default(4, 6);
  shogle::glfw_win win(win_w, win_h, "test", hints);
  shogle::gl_context gl(win.surface_provider());

  auto pip = initialize_pipeline();
  g_ctx.emplace(std::move(gl), std::move(win), std::move(pip).value());

  update_proj(g_ctx->proj, (f32)win_w, (f32)win_h);

  g_ctx->win.set_viewport_callback([&](auto, const shogle::extent2d& vp) {
    update_proj(g_ctx->proj, (f32)vp.width, (f32)vp.height);
  });
}

void start_frame() {
  assert(g_ctx.has_value());
  shogle::gl_clear_builder clear_builder;
  const auto frame_clear = clear_builder.set_clear_color(.3f, .3f, .3f, 1.f)
                             .set_clear_flag(shogle::gl_clear_opts::CLEAR_COLOR)
                             .set_clear_flag(shogle::gl_clear_opts::CLEAR_DEPTH)
                             .build();
  g_ctx->gl.start_frame(frame_clear);
}

void end_frame() {
  assert(g_ctx.has_value());
  g_ctx->gl.end_frame();
}

} // namespace kappa::render
