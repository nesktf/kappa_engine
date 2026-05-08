#include "./glfw.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>

namespace kappa::render {

GLFWContext::GLFWContext(GLFWwindow* win) : _win(win), _vk(nullptr) {}

fn GLFWContext::create(u32 width, u32 height) -> GLFWContext {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  auto res = glfwInit();
  ka_assert(res, "Failed to initialize GLFW");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* win = glfwCreateWindow(width, height, "test", nullptr, nullptr);
  ka_assert(win, "Failed to create GLFW window");
  return {win};
}

fn GLFWContext::fb_resize_fn(GLFWwindow* win, int w, int h) -> void {
  auto& self = *static_cast<GLFWContext*>(glfwGetWindowUserPointer(win));
  self._vk->rebuild_swapchain(VkExtent2D(w, h));
}

namespace {

fn get_extensions() -> Span<const char*> {
  u32 glfw_ext_count = 0;
  const char** glfw_exts_ptr = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
  ka_assert(glfw_ext_count > 0);
  ka_assert(glfw_exts_ptr);
  return {glfw_exts_ptr, glfw_ext_count};
}

} // namespace

fn GLFWContext::bind_vulkan(const VulkanInfo& app_info, const MeshData& mesh_data)
  -> VulkanContext {
  const fn get_extent = [&]() -> VkExtent2D {
    int w, h;
    glfwGetFramebufferSize(_win, &w, &h);
    return {static_cast<u32>(w), static_cast<u32>(h)};
  };

  const render::VulkanSurfaceProvider surface{
    .initial_extent = get_extent(),
    .extensions = get_extensions(),
    .provider_fn = [this](VkInstance vkinst, VkSurfaceKHR* surface,
                          const VkAllocationCallbacks* vkalloc) -> VkResult {
      return glfwCreateWindowSurface(vkinst, _win, vkalloc, surface);
    },
    .imgui_fn =
      [this]() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

        ImGui_ImplGlfw_InitForVulkan(_win, true);
      },
  };
  auto vk = VulkanContext::create(app_info, surface, mesh_data).value();
  _vk = &vk;
  glfwSetWindowUserPointer(_win, this);
  glfwSetFramebufferSizeCallback(_win, fb_resize_fn);
  return vk;
}

fn GLFWContext::destroy() -> void {
  if (_vk) {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }
  glfwDestroyWindow(_win);
  glfwTerminate();
}

fn GLFWContext::start_imgui_frame() -> void {
  ImGui_ImplGlfw_NewFrame();
}

fn GLFWContext::poll_events() -> void {
  glfwPollEvents();
}

fn GLFWContext::should_close() -> bool {
  return glfwWindowShouldClose(_win);
}

} // namespace kappa::render
