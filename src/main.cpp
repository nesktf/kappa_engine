#include "./render/glfw.hpp"

#include "./util/logger.hpp"

#include <imgui.h>

#include <ranmath/ran.hpp>

#define KA_APP_NAME    "Kappa"
#define KA_APP_VERSION VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)

namespace {

using namespace kappa;

struct ComputeConstants {
  ran::Vec4f32 data1;
  ran::Vec4f32 data2;
  ran::Vec4f32 data3;
  ran::Vec4f32 data4;
};

struct Vertex {
  ran::Vec3f32 pos;
  f32 uv_x;
  ran::Vec3f32 normal;
  f32 uv_y;
  ran::Vec4f32 color;
};

struct ComputeEffect {
  const char* name;
  render::VulkanCompute compute;
  ComputeConstants data;
};

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

constexpr auto rect_verts = std::to_array<Vertex>({
  {
    {.5f, -.5f, 0.f},
    0.f,
    {0.f, 1.f, 0.f},
    1.f,
    {0.f, 1.f, 0.f, 1.f},
  },
  {
    {.5f, .5f, 0.f},
    0.f,
    {0.f, 1.f, 0.f},
    1.f,
    {.5f, .5f, .5f, 1.f},
  },
  {
    {-.5f, -.5f, 0.f},
    0.f,
    {0.f, 1.f, 0.f},
    1.f,
    {1.f, 0.f, 0.f, 1.f},
  },
  {
    {-.5f, .5f, 0.f},
    0.f,
    {0.f, 1.f, 0.f},
    1.f,
    {0.f, 1.f, 0.f, 1.f},
  },
});
constexpr auto rect_indices = std::to_array<u32>({0, 1, 2, 2, 1, 3});

fn run_engine() -> void {
  auto glfw = render::GLFWContext::create(WINDOW_WIDTH, WINDOW_HEIGHT);
  const DeferFn glfw_defer = [&]() {
    glfw.destroy();
  };
  auto vk = glfw.bind_vulkan(app_info, mesh_data);

  const fn imgui_draw = [&]() {
    glfw.start_imgui_frame();
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

  auto transform = ran::Mat4f32::identity();
  const fn on_render = [&](f64 dt, f64 alpha) {
    KA_UNUSED(dt);
    KA_UNUSED(alpha);

    vk.draw(imgui_draw, transform);
  };

  const fn on_fixed_update = [&](u32 ups) {
    KA_UNUSED(ups);
    static constexpr auto delta = ran::pi<f32> / 60.f;
    transform = ran::rotate(transform, delta, ran::Vec3f32(0.f, 0.f, 1.f));
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
