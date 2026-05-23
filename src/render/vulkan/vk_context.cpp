#include "./vk_context.hpp"

#include "./vk_private.hpp"

namespace kappa::render {

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

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity, VkDebugUtilsMessageTypeFlagsEXT msg_type,
  const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
  auto& ctx = *static_cast<VkContext_Impl*>(user_data);
  KA_UNUSED(ctx);

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

fn init_vk_instance(VkInstance* vk, VkDebugUtilsMessengerEXT* messenger, void* messenger_user,
                    const char* app_name, u32 app_ver, Span<const char*> extensions)
  -> VkExpect<void> {

#ifndef NDEBUG
  const fn check_layer_support = [&]() -> bool {
    u32 layer_count{};
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    Vec<VkLayerProperties> avail;
    avail.resize(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, avail.data());

    for (const char* layer : validation_layers) {
      bool found = false;
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
    return {unexpect, VK_ERROR_LAYER_NOT_PRESENT};
  }
#endif

  auto vkapp_info = vkmk_zero<VkApplicationInfo>();
  // app_info.pNext = nullptr; // No extensions, not needed if default constructed
  vkapp_info.pApplicationName = app_name;
  vkapp_info.applicationVersion = app_ver;
  vkapp_info.pEngineName = KA_ENGINE_NAME;
  vkapp_info.engineVersion = KA_ENGINE_VER;
  vkapp_info.apiVersion = KA_VULKAN_VERSION;

  auto create_info = vkmk_zero<VkInstanceCreateInfo>();
  create_info.pApplicationInfo = &vkapp_info;

  u32 ext_count{0};
  vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
  Vec<VkExtensionProperties> exts;
  exts.resize(ext_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());
  KA_VK_LOG(debug, "Vulkan extensions available ({}):", ext_count);
  for (const auto& ext : exts) {
    KA_VK_LOG(debug, "- {}", ext.extensionName);
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
    return {unexpect, VK_ERROR_EXTENSION_NOT_PRESENT};
  }

  // Pass the windowing system extensions
  create_info.enabledExtensionCount = static_cast<u32>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();

#ifndef NDEBUG
  // Setup the debug messenger
  auto messenger_info = vkmk_zero<VkDebugUtilsMessengerCreateInfoEXT>();
  messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messenger_info.pfnUserCallback = debug_callback;
  messenger_info.pUserData = messenger_user;

  // Which global validation layers to enable
  create_info.enabledLayerCount = static_cast<u32>(validation_layers.size());
  create_info.ppEnabledLayerNames = validation_layers.data();

  // Enable a debug messenger for vkCreateInstance and vkDestroyInstance
  create_info.pNext = static_cast<const void*>(&messenger_info);
#else
  create_info.enabledLayerCount = 0;
  create_info.pNext = nullptr;
  KA_UNUSED(messenger_user);
#endif

  KA_VK_UNEX(vkCreateInstance(&create_info, vkalloc, vk));

#ifndef NDEBUG
  DeferFn instance_err = [&]() {
    vkDestroyInstance(*vk, vkalloc);
  };
  KA_VK_UNEX(CreateDebugUtilsMessengerEXT(*vk, &messenger_info, vkalloc, messenger));
  instance_err.disengage();
#endif
  KA_VK_LOG(debug, "Vulkan instance initialized");

  return {};
}

fn make_swapchain_args(VkContextDevice& dev, VkSurfaceKHR surface, VkExtent2D surface_extent)
  -> VkSwapchainArgs {
  VkSwapchainArgs swargs{};
  swargs.device = dev.device();
  swargs.physical_device = dev.physical_device();
  swargs.surface = surface;
  swargs.surface_extent = surface_extent;
  swargs.surface_formats = dev.surface_formats();
  swargs.surface_present_modes = dev.surface_present_modes();
  const auto [graphics, present, _] = dev.queues();
  swargs.graphics_queue = graphics;
  swargs.present_queue = present;
  return swargs;
}

fn init_imdraw_data(VkContext_Impl::ImDrawData& data, VkDevice device, u32 graphics_queue)
  -> VkExpect<void> {
  const auto cmdpool_info =
    vkmk_cmdpool_info(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphics_queue);
  KA_VK_UNEX(vkCreateCommandPool(device, &cmdpool_info, vkalloc, &data.cmdpool));
  DeferFn pool_defer = [&]() {
    vkDestroyCommandPool(device, data.cmdpool, vkalloc);
  };

  const auto cmd_alloc_info =
    vkmk_cmdbuf_alloc_info(data.cmdpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  KA_VK_UNEX(vkAllocateCommandBuffers(device, &cmd_alloc_info, &data.cmdbuf));

  const auto fence_create_info = vkmk_fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
  KA_VK_UNEX(vkCreateFence(device, &fence_create_info, vkalloc, &data.fence));

  pool_defer.disengage();
  return {};
}

} // namespace

VkContext_Impl::VkContext_Impl(VkInstance vk_, VkDebugUtilsMessengerEXT messenger_,
                               VmaAllocator vmalloc_, VkSurfaceKHR surface_,
                               VkContextDevice&& device_, VkSwapchain&& swapchain_,
                               VkFrameData&& framedata_, ImDrawData&& imdrawdata_,
                               VkDelQueue&& delqueue_,
                               Optional<VkUpdateSurfExtFn>&& update_surf_) :
    vk(vk_), messenger(messenger_), vmalloc(vmalloc_), surface(surface_),
    imdrawdata(std::move(imdrawdata_)), device(std::move(device_)),
    swapchain(std::move(swapchain_)), framedata(std::move(framedata_)),
    delqueue(std::move(delqueue_)), update_surf(std::move(update_surf_)) {}

VkContext_Impl::~VkContext_Impl() {
  device.wait_idle();
  swapchain.destroy(device.device());
  delqueue.flush();
}

fn VkContext::create(const VkContextArgs& args) -> VkMsgExpect<VkContext> {
#define KA_VK_UNEX_CODE(_func, _msg)                \
  if (auto _vkret = (_func); _vkret.has_error()) {  \
    return {unexpect, _msg, _vkret.error().code()}; \
  }

#define KA_VK_UNEX_CODE_RET(_name, _func, _msg)    \
  auto _name = (_func);                            \
  if (_name.has_error()) {                         \
    return {unexpect, _msg, _name.error().code()}; \
  }

  auto surface_data = args.surface.value();
  Vec<const char*> extensions;
  u32 extension_count = 1;
  if (!surface_data.extensions.empty()) {
    extension_count += (u32)surface_data.extensions.size();
    extensions.reserve(extension_count);
    KA_VK_LOG(debug, "Required Vulkan surface extensions ({}):", surface_data.extensions.size());
    for (usize i = 0; i < surface_data.extensions.size(); ++i) {
      KA_VK_LOG(debug, "- {}", surface_data.extensions[i]);
      extensions.push_back(surface_data.extensions[i]);
    }
  } else {
    KA_VK_LOG(warn, "No Vulkan extensions provided for surface creation");
  }
#ifndef NDEBUG
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  VkDelQueue delqueue;
  DeferFn delqueue_err = [&]() {
    delqueue.flush();
  };

  auto* ctx = std::allocator<VkContext_Impl>().allocate(1);
  DeferFn ctx_err = [&]() {
    std::allocator<VkContext_Impl>().deallocate(ctx, 1);
  };
  VkInstance vk = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
  KA_VK_UNEX_CODE(init_vk_instance(&vk, &messenger, ctx, args.app_name, args.app_ver,
                                   {extensions.data(), extensions.size()}),
                  "Failed to initialize vulkan context");
  delqueue.enqueue(vk);
#ifndef NDEBUG
  delqueue.enqueue(messenger, vk);
#endif

  const fn create_surface = [&](VkInstance vk, VkSurfaceKHR* surf) -> VkExpect<void> {
    const auto res = surface_data.create(vk, surf, vkalloc);
    if (res != VK_SUCCESS || surf == VK_NULL_HANDLE) {
      return {unexpect, res == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : res};
    }
    return {};
  };

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  KA_VK_UNEX_CODE(create_surface(vk, &surface), "Failed to create surface");
  delqueue.enqueue(surface, vk);
  const auto swapchain_extent = surface_data.swapchain_extent;

  KA_VK_UNEX_CODE_RET(device, VkContextDevice::create(vk, surface), "Failed to create device");
  device->add_to_delqueue(delqueue);

  KA_VK_UNEX_CODE_RET(vmalloc,
                      vk_create_vma_alloc(vk, device->device(), device->physical_device()),
                      "Failed to create buffer allocator");
  delqueue.enqueue((VkMemAllocator)*vmalloc, device->device());
  const u32 graphics_queue = device->queues().graphics;

  const auto swargs = make_swapchain_args(*device, surface, swapchain_extent);
  KA_VK_UNEX_CODE_RET(swapchain, VkSwapchain::create(swargs), "Failed to create swapchain");
  DeferFn swapchain_err = [&]() {
    swapchain->destroy(device->device());
  };

  KA_VK_UNEX_CODE_RET(framedata, VkFrameData::create(device->device(), graphics_queue),
                      "Failed to create frame data");
  framedata->add_to_delqueue(delqueue);

  VkContext_Impl::ImDrawData imdraw{};
  KA_VK_UNEX_CODE(init_imdraw_data(imdraw, device->device(), graphics_queue),
                  "Failed to initialize immediate draw data");
  delqueue.enqueue(imdraw.cmdpool, device->device());
  delqueue.enqueue(imdraw.fence, device->device());

  new (ctx) VkContext_Impl(vk, messenger, *vmalloc, surface, *std::move(device),
                           *std::move(swapchain), *std::move(framedata), std::move(imdraw),
                           std::move(delqueue), std::move(surface_data.update_extent));

  swapchain_err.disengage();
  ctx_err.disengage();
  delqueue_err.disengage();

  return {in_place, create_t(), ctx};
#undef KA_VK_UNEX_CODE
#undef KA_VK_UNEX_CODE_RET
}

fn VkContext::destroy(VkContext_Impl* ctx) noexcept -> void {
  if (!ctx) {
    return;
  }
  std::destroy_at(ctx);
  std::allocator<VkContext_Impl>().deallocate(ctx, 1);
}

fn VkContext::device() -> VkDevice {
  return _vk->device.device();
}

fn VkContext::physical_device() -> VkPhysicalDevice {
  return _vk->device.physical_device();
}

fn VkContext::allocator() -> VkMemAllocator {
  return (VkMemAllocator)_vk->vmalloc;
}

fn vk_rebuild_swapchain(VkContext_Impl& vk, VkExtent2D extent) -> VkExpect<void> {
  const auto swargs = make_swapchain_args(vk.device, vk.surface, extent);
  return VkSwapchain::create(swargs, vk.swapchain.swapchain())
    .transform([&](VkSwapchain&& swapchain) {
      vk.swapchain.destroy(vk.device.device());
      vk.swapchain = std::move(swapchain);
    });
}

fn vk_draw_frame_fn(VkContext_Impl& vk, VkFrameFn func) -> VkMsgExpect<void> {
  auto& frame = vk.framedata.curr_frame();
  const auto cmd = frame.cmdbuf;
  const auto device = vk.device.device();
  const auto graphics_queue = vk.device.graphics_queue();
  const auto present_queue = vk.device.present_queue();
  auto swapchain = vk.swapchain.swapchain();

  const fn rebuild_swapchain = [&]() -> VkMsgExpect<void> {
    VkExtent2D new_extent{};
    (*vk.update_surf)(&new_extent);
    if (auto res = vk_rebuild_swapchain(vk, new_extent); !res) {
      return {unexpect, "Failed to rebuild swapchain", res.error().code()};
    }
    swapchain = vk.swapchain.swapchain();
    return {};
  };

  // Wait for the previous rendering commands to finish
  KA_VK_ASSERT(vkWaitForFences(device, 1, &frame.render_fen, true, 1000000000));
  KA_VK_ASSERT(vkResetFences(device, 1, &frame.render_fen));

  // Acquire swapchain image. Will signal the swapchain semaphore when we acquire an image.
  VkResult ret = VK_SUCCESS;
  ret = vkAcquireNextImageKHR(device, swapchain, 1000000000, frame.swapchain_sem, nullptr,
                              &frame.swapchain_idx);
  if (ret == VK_ERROR_OUT_OF_DATE_KHR) {
    if (vk.update_surf) {
      if (auto res = rebuild_swapchain(); !res) {
        return res;
      }
    }
  } else if (ret != VK_SUCCESS && ret != VK_SUBOPTIMAL_KHR) {
    return {unexpect, "Failed to acquire swapchain image", ret};
  }
  const auto swapchain_image = vk.swapchain.images()[frame.swapchain_idx];
  const auto swapchain_view = vk.swapchain.image_views()[frame.swapchain_idx];
  const auto swapchain_extent = vk.swapchain.extent();

  //  Initialize command buffer
  KA_VK_ASSERT(vkResetCommandBuffer(cmd, 0));
  const auto cmd_begin_info = vkmk_cmdbuf_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  KA_VK_ASSERT(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  const VkFrameContext framedata{
    .cmd = cmd,
    .swapchain_image = swapchain_image,
    .swapchain_view = swapchain_view,
    .swapchain_extent = swapchain_extent,
  };
  func(framedata);

  //  Finalize command buffer
  KA_VK_ASSERT(vkEndCommandBuffer(cmd));

  // Wait until the swapchain semaphore is signaled (when we acquire an image)
  const auto wait_info = vkmk_semaphore_submit_info(
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame.swapchain_sem);

  // Signal the render semaphore when the commands finish
  const auto signal_info =
    vkmk_semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.render_sem);

  const auto cmdinfo = vkmk_command_buffer_submit_info(cmd);
  const auto submit = vkmk_submit_info(cmdinfo, &signal_info, &wait_info);

  // The render fence will block until the commands finish excecuting (on the next call)
  KA_VK_ASSERT(vkQueueSubmit2(graphics_queue, 1, &submit, frame.render_fen));

  auto present_info = vkmk_zero<VkPresentInfoKHR>();
  present_info.pSwapchains = &swapchain;
  present_info.swapchainCount = 1;
  present_info.pImageIndices = &frame.swapchain_idx;

  // Wait on the render semaphore (the drawing commands have to finish before presenting)
  present_info.pWaitSemaphores = &frame.render_sem;
  present_info.waitSemaphoreCount = 1;

  ret = vkQueuePresentKHR(present_queue, &present_info);
  vk.framedata.next_frame();
  if (ret == VK_ERROR_OUT_OF_DATE_KHR || ret == VK_SUBOPTIMAL_KHR) {
    return vk.update_surf ? rebuild_swapchain()
                          : VkMsgExpect<void>{unexpect, "Swapchain out of date", ret};
  } else if (ret != VK_SUCCESS) {
    return {unexpect, "Failed to present swapchain image", ret};
  }
  return {};
}

fn vk_submit_immediate_fn(VkContext_Impl& vk, VkImmediateFn func) -> VkExpect<void> {
  const auto device = vk.device.device();
  const auto graphics_queue = vk.device.graphics_queue();
  const auto& imdraw = vk.imdrawdata;
  const auto cmd = imdraw.cmdbuf;
  const auto fence = imdraw.fence;

  KA_VK_ASSERT(vkResetFences(device, 1, &fence));
  KA_VK_ASSERT(vkResetCommandBuffer(cmd, 0));

  const auto cmd_begin_info = vkmk_cmdbuf_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  KA_VK_ASSERT(vkBeginCommandBuffer(cmd, &cmd_begin_info));
  func(cmd);
  KA_VK_ASSERT(vkEndCommandBuffer(cmd));

  const auto cmdinfo = vkmk_command_buffer_submit_info(cmd);
  const auto submit = vkmk_submit_info(cmdinfo, nullptr, nullptr);

  // Submit to queue and execute
  // fence blocks until the gfx commands finish execution
  KA_VK_ASSERT(vkQueueSubmit2(graphics_queue, 1, &submit, fence));
  KA_VK_ASSERT(vkWaitForFences(device, 1, &fence, VK_TRUE, 9999999999));
  return {};
}

} // namespace kappa::render
