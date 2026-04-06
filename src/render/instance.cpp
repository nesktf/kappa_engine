#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "./instance.hpp"
#include "./vk_context.hpp"

#include <GLFW/glfw3.h>

#define KA_VER_MAJ 0
#define KA_VER_MIN 0
#define KA_VER_REV 1

#define KA_APP_NAME       "Kappa"
#define KA_VULKAN_VERSION VK_API_VERSION_1_3
#define KA_APP_VERSION    VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)
#define KA_ENGINE_NAME    KA_APP_NAME " render engine"
#define KA_ENGINE_VERSION VK_MAKE_VERSION(KA_VER_MAJ, KA_VER_MIN, KA_VER_REV)

namespace kappa::render {

namespace {

struct Window {
  GLFWwindow* handle;
};

Nullable<Window> g_win;

fn init_glfw(u32 win_w, u32 win_h) -> void {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  if (!glfwInit()) {
    ka_panic("Failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* win = glfwCreateWindow(win_w, win_h, "test", nullptr, nullptr);
  if (!win) {
    ka_panic("Failed to create GLFW window");
  }
  g_win.emplace(win);
}

fn destroy_glfw() -> void {
  ka_assert(g_win.has_value());
  glfwDestroyWindow(g_win->handle);
  glfwTerminate();
}

fn glfw_make_surface(VkInstance vk, VkSurfaceKHR* surface) -> bool {
  return !glfwCreateWindowSurface(vk, g_win->handle, nullptr, surface);
}

fn glfw_extent() -> Extent2D {
  int w, h;
  glfwGetFramebufferSize(g_win->handle, &w, &h);
  return {static_cast<u32>(w), static_cast<u32>(h)};
}

} // namespace

namespace {

// Load vkCreateDebugUtilsMessengerEXT, since is an extension and is not loaded by default
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

// Do the same with vkDestroyDebugUtilsMessengerEXT
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func) {
    func(instance, debugMessenger, pAllocator);
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity, VkDebugUtilsMessageTypeFlagsEXT msg_type,
  const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
  KA_UNUSED(user_data);
  // auto& messenger = *static_cast<debug_messenger*>(user_data);

  const char* kind;
  switch (msg_type) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
      kind = "GENERAL";
      break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
      kind = "VALIDATION";
      break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
      kind = "PERFORMANCE";
      break;
    default:
      kind = "UNKNOWN";
      break;
  }

  switch (msg_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      log_verbose("[VK_LAYERS][{}] {}", kind, callback_data->pMessage);
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      log_info("[VK_LAYERS][{}] {}", kind, callback_data->pMessage);
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      log_warn("[VK_LAYERS][{}] {}", kind, callback_data->pMessage);
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      log_error("[VK_LAYERS][{}] {}", kind, callback_data->pMessage);
      break;
    default:
      log_verbose("[VK_LAYERS][{}] {}", kind, callback_data->pMessage);
      break;
  }

  return VK_FALSE; // Should the callback abort the call that triggered the message?
}

fn make_vk_instance(Span<const char*> extensions)
  -> std::pair<VkInstance, VkDebugUtilsMessengerEXT> {
#ifndef NDEBUG
  const fn check_layer_support = [&]() -> bool {
    u32 layer_count{};
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    Vec<VkLayerProperties> avail;
    avail.resize(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, avail.data());

    for (const char* layer : validation_layers) {
      bool found{false};
      for (const auto& props : avail) {
        if (std::strcmp(layer, props.layerName) == 0) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
    }

    return true;
  };

  if (!check_layer_support()) {
    ka_panic("Validation layers not available");
  }
#endif

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // struct type
  // app_info.pNext = nullptr; // No extensions, not needed if default constructed
  app_info.pApplicationName = KA_APP_NAME;
  app_info.applicationVersion = KA_APP_VERSION;
  app_info.pEngineName = KA_ENGINE_NAME;
  app_info.engineVersion = KA_ENGINE_VERSION;
  app_info.apiVersion = KA_VULKAN_VERSION;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  u32 ext_count{0};
  vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
  Vec<VkExtensionProperties> exts;
  exts.resize(ext_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());
  log_verbose("{} vulkan extensions available", ext_count);
  for (const auto& ext : exts) {
    log_verbose(" - {}", ext.extensionName);
  }

  bool ext_avail = true;
  for (const char* extname : extensions) {
    bool found = false;
    for (const auto& props : exts) {
      if (std::strcmp(extname, props.extensionName) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      ext_avail = false;
      break;
    }
  }

  if (!ext_avail) {
    ka_panic("Failed to find the required vulkan extensions");
  }

  // Pass the windowing system extensions
  create_info.enabledExtensionCount = static_cast<u32>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();

  VkDebugUtilsMessengerEXT messenger{};
#ifndef NDEBUG
  // Setup the debug messenger
  VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
  messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messenger_info.pfnUserCallback = debug_callback;
  messenger_info.pUserData = nullptr;

  // Which global validation layers to enable
  create_info.enabledLayerCount = static_cast<u32>(validation_layers.size());
  create_info.ppEnabledLayerNames = validation_layers.data();

  // Enable a debug messenger for vkCreateInstance and vkDestroyInstance
  create_info.pNext = static_cast<const void*>(&messenger_info);
#else
  create_info.enabledLayerCount = 0;
  create_info.pNext = nullptr;
#endif
  VkInstance instance;
  VK_ASSERT(vkCreateInstance(&create_info, nullptr, &instance));

#ifndef NDEBUG
  // Create the debug messenger
  VK_ASSERT(CreateDebugUtilsMessengerEXT(instance, &messenger_info, nullptr, &messenger));
#endif

  VK_LOG(debug, "Vulkan instance initialized");
  return std::make_pair(instance, messenger);
}

fn init_vulkan() {
  u32 glfw_ext_count = 0;
  const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
  ka_assert(glfw_ext_count > 0);
  ka_assert(glfw_exts);

  VK_LOG(debug, "GLFW Vulkan extensions:");
  for (usize i = 0; i < glfw_ext_count; ++i) {
    VK_LOG(debug, "{}", glfw_exts[i]);
  }

  const auto [vk, messenger] = make_vk_instance({glfw_exts, glfw_ext_count});
  const fn make_allocator = [&](VkInstance vk, const GraphicsDevice& device) {
    VmaAllocatorCreateInfo vma_info{};
    vma_info.instance = vk;
    vma_info.device = device;
    vma_info.physicalDevice = device.physical_device();
    vma_info.vulkanApiVersion = KA_VULKAN_VERSION;
    vma_info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT |
                     VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VmaAllocator vmalloc;
    VK_ASSERT(vmaCreateAllocator(&vma_info, &vmalloc));
    return vmalloc;
  };

  const fn create_cmdpool = [&](const GraphicsDevice& device) -> RenderContext::CommandPool {
    // Commands in vulkan are recorded in a command buffer
    // instead of being executed directly using function calls
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    const auto queue_families = device.queue_families();
    // The pool will be used for drawing commands only
    pool.queueFamilyIndex = queue_families.graphics;

    // All command buffers can be rerecorded individually
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool graphics_pool;
    VK_ASSERT(vkCreateCommandPool(device, &pool, nullptr, &graphics_pool));

    VkCommandPool transfer_pool;
    pool.queueFamilyIndex = queue_families.transfer;
    VK_ASSERT(vkCreateCommandPool(device, &pool, nullptr, &transfer_pool));

    // Command buffer allocation
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> graphics_cmdbufs;

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = graphics_pool;
    alloc.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    // If the command buffer can be submited directly but not called from other buffers
    // or cannot be submitted directly but can be called from pirmary buffers
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_ASSERT(vkAllocateCommandBuffers(device, &alloc, graphics_cmdbufs.data()));

    VkCommandBuffer transfer_command_buffer;
    alloc.commandPool = transfer_pool;
    alloc.commandBufferCount = 1;
    VK_ASSERT(vkAllocateCommandBuffers(device, &alloc, &transfer_command_buffer));
    return {graphics_pool, transfer_pool, graphics_cmdbufs, transfer_command_buffer};
  };

  const fn create_sync = [&](const GraphicsDevice& device) -> RenderContext::SyncObjects {
    // Steps for drawing a frame:
    // 1. Wait for the previous frame to finish
    // 2. Acquire an image from the swapchain
    // 3. Record a command buffer which draws an scene onto that image
    // 4. Submit the recorded command buffer (and execute it)
    // 5. Present the swapchain image

    // Steps 2, 4 and 5 hapen asynchronously on the GPU, so some
    // synchronization primitives are needed: semaphores and fences
    // - Semaphores are for synchronizing the GPU (Blocks between queue commands)
    // - Fences are for synchronizing the CPU (Blocks until some queue command finishes)

    // Semaphores should be used for swapchain operations (because they happen on the GPU)
    // Fences are used when we're waiting for the previous frame to end before drawing again, so
    // we don't overwrite the current contents of the command buffer while the GPU is using it

    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    // Create the fence signaled, since there is no previous frame on the first frame
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    RenderContext::SyncObjects objs;
    u32 i = 0;
    for (; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      auto& image_sem = objs.image_avail_sem[i];
      auto& render_sem = objs.render_end_sem[i];
      auto& in_flight_fen = objs.in_flight_fences[i];
      VK_ASSERT(vkCreateSemaphore(device, &semaphore, nullptr, &image_sem));
      VK_ASSERT(vkCreateSemaphore(device, &semaphore, nullptr, &render_sem))
      VK_ASSERT(vkCreateFence(device, &fence, nullptr, &in_flight_fen));
    }
    return objs;
  };

  VkSurfaceKHR surface{};
  if (!glfw_make_surface(vk, &surface)) {
    ka_panic("Failed to create window surface");
  }

  GraphicsDevice device(vk, surface);

  const auto [swapchain_w, swapchain_h] = glfw_extent();
  const VkExtent2D swapchain_extent{.width = swapchain_w, .height = swapchain_h};
  WindowSwapchain swapchain(device, surface, swapchain_extent, VK_NULL_HANDLE);
}

fn destroy_vulkan() -> void {
  ka_assert(g_ctx.has_value());
  g_ctx->destroy();
}

} // namespace

fn initialize(u32 win_w, u32 win_h) -> void {
  init_glfw(win_w, win_h);
  init_vulkan();
}

fn destroy() -> void {
  ka_assert(g_ctx.has_value());
  destroy_vulkan();
  destroy_glfw();
}

fn vk_error_string(VkResult res) noexcept -> std::string_view {
#define VKSTR(code) \
  case code:        \
    return #code;
  switch (res) {
    VKSTR(VK_NOT_READY);
    VKSTR(VK_TIMEOUT);
    VKSTR(VK_EVENT_SET);
    VKSTR(VK_EVENT_RESET);
    VKSTR(VK_INCOMPLETE);
    VKSTR(VK_ERROR_OUT_OF_HOST_MEMORY);
    VKSTR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    VKSTR(VK_ERROR_INITIALIZATION_FAILED);
    VKSTR(VK_ERROR_DEVICE_LOST);
    VKSTR(VK_ERROR_MEMORY_MAP_FAILED);
    VKSTR(VK_ERROR_LAYER_NOT_PRESENT);
    VKSTR(VK_ERROR_EXTENSION_NOT_PRESENT);
    VKSTR(VK_ERROR_FEATURE_NOT_PRESENT);
    VKSTR(VK_ERROR_INCOMPATIBLE_DRIVER);
    VKSTR(VK_ERROR_TOO_MANY_OBJECTS);
    VKSTR(VK_ERROR_FORMAT_NOT_SUPPORTED);
    VKSTR(VK_ERROR_SURFACE_LOST_KHR);
    VKSTR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    VKSTR(VK_SUBOPTIMAL_KHR);
    VKSTR(VK_ERROR_OUT_OF_DATE_KHR);
    VKSTR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    VKSTR(VK_ERROR_VALIDATION_FAILED_EXT);
    VKSTR(VK_ERROR_INVALID_SHADER_NV);
    default:
      return "UNKNOWN_ERROR";
  }
#undef VKSTR
};

fn start_frame() -> void {}

fn end_frame() -> void {}

fn device_wait() -> void {}

fn flag_dirty_framebufer() -> void {}

fn record_command(const IndexedDrawCmd& cmd) -> void {}

fn create_pipeline(const LayoutInfo& layout, std::string_view vert, std::string_view frag)
  -> ResHandle {}

fn destroy_pipeline(ResHandle pipeline) -> void {}

fn create_buffer(VkBufferType type, usize size, bits32 flags) -> ResHandle {}

fn upload_buffer_data(ResHandle buffer, const void* data, usize size, usize offset) -> void {}

fn destroy_buffer(ResHandle buffer) -> void {}

} // namespace kappa::render

#if 0
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

#endif
