#include "./render/glfw.hpp"

#include "./util/logger.hpp"

#include <imgui.h>

namespace {

using namespace kappa;

constexpr render::VulkanInfo app_info{
  .app_name = KA_APP_NAME,
  .app_ver = KA_APP_VERSION,
};

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

fn run_engine() -> void {
  auto glfw = render::GLFWContext::create(WINDOW_WIDTH, WINDOW_HEIGHT);
  const DeferFn glfw_defer = [&]() {
    glfw.destroy();
  };
  auto vk = glfw.bind_vulkan(app_info);

  const fn imgui_draw = [&]() {
    glfw.start_frame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    if (ImGui::Begin("background")) {
      auto& effect = vk.get_effect();
      ImGui::Text("Selected effect: %s", effect.name);
      ImGui::SliderInt("Effect Index", &vk.get_effect_idx(), 0, 1);
      ImGui::InputFloat4("data1", effect.data.data1.data());
      ImGui::InputFloat4("data2", effect.data.data2.data());
      ImGui::InputFloat4("data3", effect.data.data3.data());
      ImGui::InputFloat4("data4", effect.data.data4.data());
    }
    ImGui::End();

    ImGui::Render();
  };

  const fn on_render = [&](f64 dt, f64 alpha) {
    (void)dt;
    (void)alpha;

    vk.draw(imgui_draw);
  };

  const fn on_fixed_update = [&](u32 ups) {
    (void)ups;
  };

  render::render_loop<60>(glfw, OverloadFn{on_fixed_update, on_render});
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
