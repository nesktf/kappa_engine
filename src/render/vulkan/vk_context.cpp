#include "./vk_context.hpp"

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

fn make_swapchain_args(VulkanDevice& dev, VkSurfaceKHR surface, VkExtent2D surface_extent)
  -> VulkanSwapchainArgs {
  VulkanSwapchainArgs swargs{};
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

constexpr auto desc_pool_sizes = std::to_array<VulkanDescPoolRatio>({
  {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
});
constexpr VulkanDescPoolArgs desc_pool_args{
  .max_sets = 10,
  .ratios = desc_pool_sizes,
};

} // namespace

VkContext_Impl::VkContext_Impl(VkInstance vk_, VkDebugUtilsMessengerEXT messenger_,
                               VmaAllocator vmalloc_, VkSurfaceKHR surface_,
                               kappa::render::VulkanDevice&& device_,
                               kappa::render::VulkanSwapchain&& swapchain_,
                               kappa::render::VulkanFrameData&& framedata_,
                               ImDrawData&& imdrawdata_, kappa::render::VulkanDescPool&& descpool_,
                               kappa::render::VulkanDelQueue&& delqueue_) :
    vk(vk_), messenger(messenger_), vmalloc(vmalloc_), surface(surface_),
    imdrawdata(std::move(imdrawdata_)), device(std::move(device_)),
    swapchain(std::move(swapchain_)), framedata(std::move(framedata_)),
    descpool(std::move(descpool_)), delqueue(std::move(delqueue_)) {}

VkContext_Impl::~VkContext_Impl() {
  device.wait_idle();
  for (auto& frame : framedata.frames()) {
    frame.delqueue.flush();
  }
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

  VulkanDelQueue delqueue;
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

  KA_VK_UNEX_CODE_RET(device, VulkanDevice::create(vk, surface), "Failed to create device");
  device->add_to_delqueue(delqueue);

  KA_VK_UNEX_CODE_RET(vmalloc,
                      vk_create_vma_alloc(vk, device->device(), device->physical_device()),
                      "Failed to create buffer allocator");
  delqueue.enqueue(*vmalloc, device->device());
  const u32 graphics_queue = device->queues().graphics;

  const auto swargs = make_swapchain_args(*device, surface, swapchain_extent);
  KA_VK_UNEX_CODE_RET(swapchain, VulkanSwapchain::create(swargs), "Failed to create swapchain");
  DeferFn swapchain_err = [&]() {
    swapchain->destroy(device->device());
  };

  KA_VK_UNEX_CODE_RET(framedata,
                      VulkanFrameData::create(device->device(), graphics_queue, desc_pool_args),
                      "Failed to create frame data");
  framedata->add_to_delqueue(delqueue);

  KA_VK_UNEX_CODE_RET(descpool, VulkanDescPool::create(device->device(), desc_pool_args),
                      "Failed to create descriptor pool");
  descpool->add_to_delqueue(delqueue);

  VkContext_Impl::ImDrawData imdraw{};
  KA_VK_UNEX_CODE(init_imdraw_data(imdraw, device->device(), graphics_queue),
                  "Failed to initialize immediate draw data");
  delqueue.enqueue(imdraw.cmdpool, device->device());
  delqueue.enqueue(imdraw.fence, device->device());

  new (ctx) VkContext_Impl(vk, messenger, *vmalloc, surface, *std::move(device),
                           *std::move(swapchain), *std::move(framedata), std::move(imdraw),
                           *std::move(descpool), std::move(delqueue));

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

fn vk_rebuild_swapchain(VkContext_Impl& vk, VkExtent2D extent) -> VkExpect<void> {
  const auto swargs = make_swapchain_args(vk.device, vk.surface, extent);
  return VulkanSwapchain::create(swargs, vk.swapchain.swapchain())
    .transform([&](VulkanSwapchain&& swapchain) {
      vk.swapchain.destroy(vk.device.device());
      vk.swapchain = std::move(swapchain);
    });
}

fn VkContext::alloc_desc(VkDescriptorSetLayout layout) -> VkExpect<VkDescriptorSet> {
  VkDescriptorSet set{};
  return _vk->descpool.alloc_sets(layout, &set, 1).transform([&]() -> VkDescriptorSet {
    return set;
  });
}

fn VkContext::update_sets(Span<const VkWriteDescriptorSet> writes,
                          Span<const VkCopyDescriptorSet> copies) -> void {
  vkUpdateDescriptorSets(_vk->device.device(), (u32)writes.size(),
                         writes.empty() ? nullptr : writes.data(), (u32)copies.size(),
                         copies.empty() ? nullptr : copies.data());
}

fn VkContext::device_wait() -> void {
  _vk->device.wait_idle();
}

fn VkContext::new_frame() -> VkExpect<void> {
  auto& frame = _vk->framedata.curr_frame();
  const auto cmd = frame.cmdbuf;
  const auto device = _vk->device.device();
  const auto swapchain = _vk->swapchain.swapchain();

  // Clean temporary objects
  frame.delqueue.flush();

  // Wait for the previous rendering commands to finish
  KA_VK_ASSERT(vkWaitForFences(device, 1, &frame.render_fen, true, 1000000000));
  KA_VK_ASSERT(vkResetFences(device, 1, &frame.render_fen));

  // Acquire swapchain image. Will signal the swapchain semaphore when we acquire an image.
  VkResult ret = VK_SUCCESS;
  ret = vkAcquireNextImageKHR(device, swapchain, 1000000000, frame.swapchain_sem, nullptr,
                              &frame.swapchain_idx);
  ka_assert(ret == VK_SUCCESS || ret == VK_SUBOPTIMAL_KHR);

  //  Initialize command buffer
  KA_VK_ASSERT(vkResetCommandBuffer(cmd, 0));
  const auto cmd_begin_info = vkmk_cmdbuf_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  KA_VK_ASSERT(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  return ret == VK_SUCCESS ? VkExpect<void>{} : VkExpect<void>{unexpect, ret};
}

fn VkContext::end_frame() -> VkExpect<void> {
  const auto& frame = _vk->framedata.curr_frame();
  const auto cmd = frame.cmdbuf;
  const auto graphics_queue = _vk->device.graphics_queue();
  const auto present_queue = _vk->device.present_queue();
  const auto swapchain = _vk->swapchain.swapchain();

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

  VkResult ret = VK_SUCCESS;
  ret = vkQueuePresentKHR(present_queue, &present_info);
  ka_assert(ret == VK_SUCCESS || ret == VK_SUBOPTIMAL_KHR);
  _vk->framedata.next_frame();
  return ret == VK_SUCCESS ? VkExpect<void>{} : VkExpect<void>{unexpect, ret};
}

fn VkContext::_submit_immediate(UserImFn func) -> VkExpect<void> {
  const auto device = _vk->device.device();
  const auto graphics_queue = _vk->device.graphics_queue();
  const auto& imdraw = _vk->imdrawdata;
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

fn VkContext::_submit_cmd(UserCmdFn func) -> VkExpect<void> {
  auto& frame = _vk->framedata.curr_frame();
  const auto swapchain_extent = _vk->swapchain.extent();
  const auto swapchain_image = _vk->swapchain.images()[frame.swapchain_idx];
  const auto swapchain_view = _vk->swapchain.image_views()[frame.swapchain_idx];
  const auto cmd = frame.cmdbuf;

  const VkContext::FrameContext framedata{
    .cmd = cmd,
    .swapchain_image = swapchain_image,
    .swapchain_view = swapchain_view,
    .swapchain_extent = swapchain_extent,
  };
  func(framedata);

  return {};
}

#ifdef KA_VK_ENABLE_IMGUI
VkResult vkmk_imgui_info(VkContext_Impl& vk, VkImGuiInfo& info,
                         const VkDescriptorPoolCreateInfo& descpool_info) {
  VkResult ret =
    vkCreateDescriptorPool(vk.device.device(), &descpool_info, vkalloc, &info.descpool);
  if (ret) {
    return ret;
  }
  vk.delqueue.enqueue(info.descpool, vk.device.device());
  info.vk = vk.vk;
  info.device = vk.device.device();
  info.queue = vk.device.graphics_queue();
  info.swapchain_format = vk.swapchain.format();
  info.physical_device = vk.device.physical_device();
  return ret;
}
#endif

} // namespace kappa::render

#if 0
VkResult ka_vk_record_gfx_cmd(ka_VkContext vk, ka_VkTarget target_, ka_VkGfxCmdData const* data) {
  VkResult ret = VK_SUCCESS;
  if (!vk || !data || !data->cmds || !data->cmd_count) {
    ret = VK_ERROR_INITIALIZATION_FAILED;
    return ret;
  }

  auto& frame = vk->framedata.curr_frame();
  auto& target = vk->target;
  const auto draw_extent = target.extent;
  const auto draw_view = target.image.view;
  const auto draw_image = target.image.image;
  const auto cmd = frame.cmdbuf;

  // Prepare the target image, if not already prepared
  static constexpr auto draw_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  if (target.layout != draw_layout) {
    target.layout = vkcmd_transition_image(cmd, draw_image, target.layout, draw_layout);
  }

  // Clear the target image if requested
  if (data->clear_color) {
    const auto clear_range = vkmk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(cmd, draw_image, VK_IMAGE_LAYOUT_GENERAL, data->clear_color, 1,
                         &clear_range);
  }

  // Prepare viewport
  VkViewport viewport;
  if (data->viewport) {
    viewport = *data->viewport;
  } else {
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = draw_extent.width;
    viewport.height = draw_extent.height;
  }
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  // Prepare scissor
  VkRect2D scissor;
  if (data->scissor) {
    scissor = *data->scissor;
  } else {
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = draw_extent;
  }
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Start rendering
  const auto color_attachment = vkmk_attach_info(draw_view, nullptr, draw_layout);
  const auto render_info = vkmk_render_info(draw_extent, &color_attachment, nullptr);
  vkCmdBeginRendering(cmd, &render_info);

  for (usize i = 0; i < data->cmd_count; ++i) {
    const auto& cmd_data = data->cmds[i];

    // Bind pipeline
    ka_assert(cmd_data.pipeline);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd_data.pipeline);

#if 0
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cmd_data.layout, 0, 1,
                            &ctx.draw->image_desc, 0, nullptr);
#endif

    // Upload push constants
    for (usize j = 0; j < cmd_data.push_constant_count; ++j) {
      const auto& push_data = cmd_data.push_constants[j];
      if (!push_data.data || !push_data.size) {
        continue;
      }
      vkCmdPushConstants(cmd, cmd_data.layout, push_data.stage, push_data.offset, push_data.size,
                         push_data.data);
    }

    // Draw
    const u32 instance_count = cmd_data.instance_count;
    const u32 first_instance = cmd_data.first_instance;
    if (cmd_data.type == KA_VK_GFX_CMD_INDEXED) {
      // Bind index buffer
      const auto& index_binding = cmd_data.index_binding;
      ka_assert(index_binding.buffer);
      vkCmdBindIndexBuffer(cmd, index_binding.buffer->buffer, index_binding.offset,
                           index_binding.index_type);

      const u32 index_count = cmd_data.draw_count;
      const u32 first_index = cmd_data.first_item;
      const s32 vertex_offset = cmd_data.vertex_offset;
      vkCmdDrawIndexed(cmd, index_count, instance_count, first_index, vertex_offset,
                       first_instance);
    } else {
      const u32 vertex_count = cmd_data.draw_count;
      const u32 first_vertex = cmd_data.first_item;
      vkCmdDraw(cmd, vertex_count, instance_count, first_vertex, first_instance);
    }
  }

  // End
  vkCmdEndRendering(cmd);

  return ret;
}

    // temp: test rendering things
    ctx->draw.emplace(create_draw_thing(device, ctx->swapchain->extent(), ctx->vmalloc,
                                        *ctx->imdrawdata, graphics_queue, mesh));
    ctx->delqueue.enqueue(ctx->draw->image, ctx->vmalloc);
    ctx->delqueue.enqueue(ctx->draw->image.view, device);
    ctx->draw->desc_pool.add_to_delqueue(ctx->delqueue);
    ctx->delqueue.enqueue(ctx->draw->image_desc_layout, device);
    for (auto& effect : ctx->draw->background_effects) {
      ctx->delqueue.enqueue(effect.pipeline, device);
    }
    ctx->delqueue.enqueue(ctx->draw->background_effects[0].layout, device);
    ctx->delqueue.enqueue(ctx->draw->triangle_pipeline, device);
    ctx->delqueue.enqueue(ctx->draw->triangle_layout, device);
    ctx->delqueue.enqueue(ctx->draw->mesh_buffers.vertex_buffer, ctx->vmalloc);
    ctx->delqueue.enqueue(ctx->draw->mesh_buffers.index_buffer, ctx->vmalloc);
    ctx->delqueue.enqueue(ctx->draw->mesh_pipeline, device);
    ctx->delqueue.enqueue(ctx->draw->mesh_layout, device);

// temp: test rendering things

struct GPUDrawPushConstants {
  ran::Mat4f32 world_mat;
  VkDeviceAddress vertex_buffer;
};

fn create_draw_thing(VkDevice device, VkExtent2D draw_extent, VmaAllocator vmalloc,
                     VulkanContext_Impl::ImDrawData& imdraw, VkQueue graphics_queue,
                     const MeshData& mesh) -> VulkanContext_Impl::DrawThing {
  // Init draw image
  const VulkanImageArgs imgargs{
    .device = device,
    .vma = vmalloc,
    .extent = draw_extent,
    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
  };
  auto image = vk_alloc_image(imgargs).value();

  // Init descriptors
  auto desc_pool = VulkanDescPool::create(device, desc_pool_args).value();
  VkDescriptorSetLayout image_desc_layout{};
  {
    VulkanDescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    image_desc_layout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  const auto image_desc = desc_pool.alloc_set(image_desc_layout).value();

  VkDescriptorImageInfo img_info{};
  img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  img_info.imageView = image.view;

  auto draw_image_write = vkmk_zero<VkWriteDescriptorSet>();
  draw_image_write.dstBinding = 0;
  draw_image_write.dstSet = image_desc;
  draw_image_write.descriptorCount = 1;
  draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  draw_image_write.pImageInfo = &img_info;

  vkUpdateDescriptorSets(device, 1, &draw_image_write, 0, nullptr);

  // Init pipelines
  auto compute_layout = vkmk_zero<VkPipelineLayoutCreateInfo>();
  compute_layout.pSetLayouts = &image_desc_layout;
  compute_layout.setLayoutCount = 1;

  VkPushConstantRange push_constant{};
  push_constant.offset = 0;
  push_constant.size = sizeof(ComputeConstants);
  push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  compute_layout.pPushConstantRanges = &push_constant;
  compute_layout.pushConstantRangeCount = 1;

  VkPipelineLayout layout{};
  KA_VK_ASSERT(vkCreatePipelineLayout(device, &compute_layout, vkalloc, &layout));

  std::array<ComputeEffect, EFFECT_COUNT> effects{};
  effects[0].name = "gradient_color";
  effects[0].layout = layout;
  effects[0].pipeline =
    create_compute_pipeline(device, layout, "/shaders/gradient_color.comp.spv");
  effects[0].data.data1 = ran::Vec4f32(1, 0, 0, 1);
  effects[0].data.data2 = ran::Vec4f32(0, 0, 1, 1);

  effects[1].name = "sky";
  effects[1].layout = layout;
  effects[1].pipeline = create_compute_pipeline(device, layout, "/shaders/sky.comp.spv");
  effects[1].data.data1 = ran::Vec4f32(.1f, .2f, .4f, .97f);

  VkShaderModule triangle_frag = VK_NULL_HANDLE, triangle_vert = VK_NULL_HANDLE;
  VkPipelineLayout triangle_layout = VK_NULL_HANDLE;
  const DeferFn module_defer = [&]() {
    if (triangle_vert != VK_NULL_HANDLE)
      vkDestroyShaderModule(device, triangle_vert, vkalloc);
    if (triangle_frag != VK_NULL_HANDLE)
      vkDestroyShaderModule(device, triangle_frag, vkalloc);
  };
  {
    const auto vert_path = format_file_path("/shaders/colored_triangle.vert.spv");
    auto vert_file = load_entire_file(vert_path.c_str());
    triangle_vert = vk_create_shader(device, {vert_file.data(), vert_file.size()}).value();

    const auto frag_path = format_file_path("/shaders/colored_triangle.frag.spv");
    auto frag_file = load_entire_file(frag_path.c_str());
    triangle_frag = vk_create_shader(device, {frag_file.data(), frag_file.size()}).value();

    auto triangle_layout_info = vkmk_pipeline_layout_info();
    KA_VK_ASSERT(vkCreatePipelineLayout(device, &triangle_layout_info, vkalloc, &triangle_layout));
  }

  VulkanPipelineBuilder builder;
  auto triangle_pipeline = builder.set_layout(triangle_layout)
                             .set_shaders(triangle_vert, triangle_frag)
                             .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                             .set_poly_mode(VK_POLYGON_MODE_FILL)
                             .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                             .set_color_format(image.format)
                             .set_depth_format(VK_FORMAT_UNDEFINED)
                             .disable_multisampling()
                             .disable_depth_test()
                             .disable_blending()
                             .build(device)
                             .value();

  GPUMeshBuffers mesh_buffers;
  {
    // Vertex buffer
    const auto vb_size = mesh.vertices.size_bytes();
    const VulkanBufferArgs vb_args{
      .vma = vmalloc,
      .size = vb_size,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .mem_usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    mesh_buffers.vertex_buffer = vk_alloc_buffer(vb_args).value();
    auto address_info = vkmk_zero<VkBufferDeviceAddressInfo>();
    address_info.buffer = mesh_buffers.vertex_buffer.buffer;
    mesh_buffers.vertex_buffer_addr = vkGetBufferDeviceAddress(device, &address_info);

    // Index buffer
    mesh_buffers.index_count = (u32)mesh.indices.size();
    const auto ib_size = mesh.indices.size_bytes();
    const VulkanBufferArgs ib_args{
      .vma = vmalloc,
      .size = ib_size,
      .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .mem_usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    mesh_buffers.index_buffer = vk_alloc_buffer(ib_args).value();

    // Staging
    const VulkanBufferArgs staging_args{
      .vma = vmalloc,
      .size = vb_size + ib_size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .mem_usage = VMA_MEMORY_USAGE_CPU_ONLY,
    };
    auto staging = vk_alloc_buffer(staging_args).value();

    void* data = staging.info.pMappedData;
    std::memcpy(data, mesh.vertices.data(), vb_size);
    std::memcpy(((u8*)data) + vb_size, mesh.indices.data(), ib_size);
    fn copy_buffer = [&](VkCommandBuffer cmd) {
      VkBufferCopy vertex_copy{};
      vertex_copy.dstOffset = 0;
      vertex_copy.srcOffset = 0;
      vertex_copy.size = vb_size;
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.vertex_buffer.buffer, 1, &vertex_copy);

      VkBufferCopy index_copy;
      index_copy.dstOffset = 0;
      index_copy.srcOffset = vb_size;
      index_copy.size = ib_size;
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.index_buffer.buffer, 1, &index_copy);
    };
    do_immediate_submit(device, imdraw.cmdbuf, graphics_queue, imdraw.fence, copy_buffer);

    vk_dealloc_buffer(vmalloc, staging.buffer, staging.alloc);
  }

  VkShaderModule mesh_vert = VK_NULL_HANDLE;
  VkPipelineLayout mesh_layout = VK_NULL_HANDLE;
  const DeferFn mesh_module_defer = [&]() {
    if (mesh_vert != VK_NULL_HANDLE)
      vkDestroyShaderModule(device, mesh_vert, vkalloc);
  };
  {
    const auto vert_path = format_file_path("/shaders/colored_mesh.vert.spv");
    auto vert_file = load_entire_file(vert_path.c_str());
    mesh_vert = vk_create_shader(device, {vert_file.data(), vert_file.size()}).value();

    VkPushConstantRange buffer_range{};
    buffer_range.offset = 0;
    buffer_range.size = sizeof(GPUDrawPushConstants);
    buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    auto mesh_layout_info = vkmk_pipeline_layout_info();
    mesh_layout_info.pPushConstantRanges = &buffer_range;
    mesh_layout_info.pushConstantRangeCount = 1;
    KA_VK_ASSERT(vkCreatePipelineLayout(device, &mesh_layout_info, vkalloc, &mesh_layout));
  }
  builder.clear();
  auto mesh_pipeline = builder.set_layout(mesh_layout)
                         .set_shaders(mesh_vert, triangle_frag)
                         .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                         .set_poly_mode(VK_POLYGON_MODE_FILL)
                         .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                         .set_color_format(image.format)
                         .set_depth_format(VK_FORMAT_UNDEFINED)
                         .disable_multisampling()
                         .disable_blending()
                         .disable_blending()
                         .build(device)
                         .value();

  return VulkanContext_Impl::DrawThing{
    .image = std::move(image),
    .extent = draw_extent,
    .desc_pool = std::move(desc_pool),
    .image_desc = image_desc,
    .image_desc_layout = image_desc_layout,
    .background_effects = effects,
    .effect_idx = 0,
    .triangle_pipeline = triangle_pipeline,
    .triangle_layout = triangle_layout,
    .mesh_buffers = std::move(mesh_buffers),
    .mesh_pipeline = mesh_pipeline,
    .mesh_layout = mesh_layout,
  };
}


namespace {

fn clear_background() -> void {

  // make a clear-color from frame number. This will flash with a 120 frame period.
  f32 flash = std::abs(std::sin(ctx.curr_frame / 120.f));
  VkClearColorValue clear_value{{0.0f, 0.0f, flash, 1.0f}};
  const auto clear_range = vkmk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
  // clear image
  vkCmdClearColorImage(cmdbuf, ctx.draw.image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1,
                       &clear_range);
}

fn draw_background(VulkanContext_Impl& ctx, VkCommandBuffer cmd) -> void {
  auto& effect = ctx.draw->background_effects[ctx.draw->effect_idx];

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1,
                          &ctx.draw->image_desc, 0, nullptr);
  vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(effect.data),
                     &effect.data);
  vkCmdDispatch(cmd, std::ceil(ctx.draw->extent.width / 16.f),
                std::ceil(ctx.draw->extent.height / 16.f), 1);
}

fn draw_geometry(VulkanContext_Impl& ctx, VkCommandBuffer cmd, VkExtent2D draw_extent,
                 const ran::Mat4f32& mesh_transform) -> void {
  const auto color_attachment =
    vkmk_attach_info(ctx.draw->image.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  const auto render_info = vkmk_render_info(draw_extent, &color_attachment, nullptr);

  // Dynamic state
  VkViewport viewport{};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = draw_extent.width;
  viewport.height = draw_extent.height;
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = draw_extent;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Draw triangle
  {
    vkCmdBeginRendering(cmd, &render_info);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.draw->triangle_pipeline);

    vkCmdDraw(cmd, 3, 1, 0, 0);
  }

  // Draw mesh
  {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.draw->mesh_pipeline);

    GPUDrawPushConstants push_constants;
    push_constants.world_mat = mesh_transform;
    push_constants.vertex_buffer = ctx.draw->mesh_buffers.vertex_buffer_addr;
    vkCmdPushConstants(cmd, ctx.draw->mesh_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(push_constants), &push_constants);
    vkCmdBindIndexBuffer(cmd, ctx.draw->mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, ctx.draw->mesh_buffers.index_count, 1, 0, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

} // namespace

fn VulkanContext::draw(ImGuiDrawFn imgui_fn, const ran::Mat4f32& mesh_transform) -> VkResult {
  const auto& frame = _Impl->framedata->next_frame();
  const auto draw_extent = _Impl->draw->extent;
  const auto cmd = frame.cmdbuf;
  const auto& draw_image = _Impl->draw->image.image;
  const auto [device, graphics_queue, present_queue] = get_render_queues(*_Impl->device);

  VkResult res = VK_SUCCESS;

  // Imgui user rendering things
  vk_imgui_new_frame();
  imgui_fn();

  // Wait for the previous rendering commands to finish
  KA_VK_ASSERT(vkWaitForFences(device, 1, &frame.render_fen, true, 1000000000));
  KA_VK_ASSERT(vkResetFences(device, 1, &frame.render_fen));

  // acquire swapchain image. Will signal the swapchain semaphore when we acquire an image.
  const auto [swapchain, swapchain_image, swapchain_view, swapchain_extent, swapchain_idx] =
    acquire_swapchain_image(device, frame.swapchain_sem, *_Impl->swapchain, &res);

  // Rendering routine
  {
    // 1. Initialize command buffer
    KA_VK_ASSERT(vkResetCommandBuffer(cmd, 0));
    const auto cmd_begin_info =
      vkmk_cmdbuf_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    KA_VK_ASSERT(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    // 2. Draw background using compute pipeline (we use a general layout)
    VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED; // dont care about the older layout
    image_layout = vkcmd_transition_image(cmd, draw_image, image_layout, VK_IMAGE_LAYOUT_GENERAL);
    draw_background(*_Impl, cmd);

    // 3. Draw triangle using graphics pipeline (we use a color attachment layout)
    image_layout = vkcmd_transition_image(cmd, draw_image, image_layout,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    draw_geometry(*_Impl, cmd, draw_extent, mesh_transform);

    // 4. Prepare draw_image for copying
    image_layout =
      vkcmd_transition_image(cmd, draw_image, image_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // 5. Prepare the swapchain image for copying
    VkImageLayout swapchain_layout = VK_IMAGE_LAYOUT_UNDEFINED; // dont care about the older layout
    swapchain_layout = vkcmd_transition_image(cmd, swapchain_image, swapchain_layout,
                                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 6. Do the copy
    vkcmd_transfer_image(cmd, draw_image, swapchain_image, draw_extent, swapchain_extent);

    // 7. Draw ImGui ontop of the copied pixels
    swapchain_layout = vkcmd_transition_image(cmd, swapchain_image, swapchain_layout,
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk_draw_imgui(frame.cmdbuf, swapchain_view, swapchain_extent);

    // 8. Prepare swapchain image for presenting
    swapchain_layout = vkcmd_transition_image(cmd, swapchain_image, swapchain_layout,
                                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // 9. Finalize command buffer
    KA_VK_ASSERT(vkEndCommandBuffer(cmd));
  }

  // Queue submission routine
  {
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
  }

  // Swapchain presentation routine
  {
    auto present_info = vkmk_zero<VkPresentInfoKHR>();
    present_info.pSwapchains = &swapchain;
    present_info.swapchainCount = 1;
    present_info.pImageIndices = &swapchain_idx;

    // Wait on the render semaphore (the drawing commands have to finish before presenting)
    present_info.pWaitSemaphores = &frame.render_sem;
    present_info.waitSemaphoreCount = 1;

    res = vkQueuePresentKHR(present_queue, &present_info);
    ka_assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
  }

  return res;
}
#endif
