#include "./render/context.hpp"
#include "./render/glfw.hpp"

#include "./util/logger.hpp"

namespace {

using namespace kappa;

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

fn run_engine() -> void {
  auto glfw = render::GLFWContext::create(WINDOW_WIDTH, WINDOW_HEIGHT);
  const DeferFn glfw_defer = [&]() {
    glfw.destroy();
  };
  auto vk = render::RenderContext::create(glfw).value();
  const DeferFn rctx_defer = [&]() {
    vk.destroy();
  };
  render::render_loop<60>(glfw, vk);
}

} // namespace

int kappa::g_argc;
char** kappa::g_argv;

int main(int argc, char* argv[]) {
  g_argc = argc;
  g_argv = argv;
  try {
    run_engine();
  } catch (const std::exception& ex) {
    log_error(" {}", ex.what());
  }
}
