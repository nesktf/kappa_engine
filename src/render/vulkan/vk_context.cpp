#include "./vk_context.hpp"

#include "./vk_imgui.hpp"

using namespace kappa;
using namespace kappa::render;

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
  auto& ctx = *static_cast<ka_VulkanContext>(user_data);
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

fn init_imdraw_data(ka_VulkanContext_impl::ImDrawData& data, VkDevice device, u32 graphics_queue)
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

fn init_render_target(ka_VulkanContext_impl::RenderTarget& target, VkDevice device,
                      VmaAllocator vmalloc, VkExtent2D draw_extent) -> VkExpect<void> {
  static constexpr auto target_format = VK_FORMAT_R16G16B16A16_SFLOAT;
  VkExtent3D extent;
  extent.width = draw_extent.width;
  extent.height = draw_extent.height;
  extent.depth = 1;
  target.extent = draw_extent;
  target.layout = VK_IMAGE_LAYOUT_UNDEFINED;
  return vk_alloc_image(target.image, device, vmalloc, extent, target_format);
};

constexpr auto desc_pool_sizes = std::to_array<VulkanDescPoolRatio>({
  {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
});
constexpr VulkanDescPoolArgs desc_pool_args{
  .max_sets = 10,
  .ratios = desc_pool_sizes,
};

} // namespace

#define KA_VK_UNEX_CODE(_func, _msg)               \
  if (auto _vkret = (_func); _vkret.has_error()) { \
    if (errmsg) {                                  \
      (*errmsg) = _msg;                            \
    }                                              \
    return _vkret.error().code();                  \
  }

#define KA_VK_UNEX_CODE_RET(_name, _func, _msg) \
  auto _name = (_func);                         \
  if (_name.has_error()) {                      \
    if (errmsg) {                               \
      (*errmsg) = _msg;                         \
    }                                           \
    return _name.error().code();                \
  }

VkResult ka_vk_create_context(ka_VulkanContext* ctx_, ka_VulkanContextArgs const* args,
                              const char** errmsg) {
  ka_assert(ctx_);
  ka_assert(args);
  Vec<const char*> extensions;
  u32 extension_count = 1;
  if (args->surface_extensions && args->surface_extension_count) {
    extension_count += args->surface_extension_count;
    extensions.reserve(extension_count);
    KA_VK_LOG(debug, "Required Vulkan surface extensions ({}):", args->surface_extension_count);
    for (usize i = 0; i < args->surface_extension_count; ++i) {
      KA_VK_LOG(debug, "- {}", args->surface_extensions[i]);
      extensions.push_back(args->surface_extensions[i]);
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

  auto* ctx = std::allocator<ka_VulkanContext_impl>().allocate(1);
  DeferFn ctx_err = [&]() {
    std::allocator<ka_VulkanContext_impl>().deallocate(ctx, 1);
  };
  VkInstance vk = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
  KA_VK_UNEX_CODE(init_vk_instance(&vk, &messenger, ctx, args->app_name, args->app_ver,
                                   {extensions.data(), extensions.size()}),
                  "Failed to initialize vulkan context");
  delqueue.enqueue(vk);
#ifndef NDEBUG
  delqueue.enqueue(messenger, vk);
#endif

  const fn create_surface = [&](VkInstance vk, VkSurfaceKHR* surf) -> VkExpect<void> {
    ka_assert(args->create_surface);
    const auto res = args->create_surface(args->create_surface_user, vk, surf, vkalloc);
    if (res != VK_SUCCESS || surf == VK_NULL_HANDLE) {
      return {unexpect, res == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : res};
    }
    return {};
  };

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  KA_VK_UNEX_CODE(create_surface(vk, &surface), "Failed to create surface");
  delqueue.enqueue(surface, vk);

  KA_VK_UNEX_CODE_RET(device, VulkanDevice::create(vk, surface), "Failed to create device");
  device->add_to_delqueue(delqueue);

  KA_VK_UNEX_CODE_RET(vmalloc,
                      vk_create_vma_alloc(vk, device->device(), device->physical_device()),
                      "Failed to create buffer allocator");
  delqueue.enqueue(*vmalloc, device->device());
  const u32 graphics_queue = device->queues().graphics;

  const auto swargs = make_swapchain_args(*device, surface, args->initial_extent);
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

  ka_VulkanContext_impl::RenderTarget render_target;
  KA_VK_UNEX_CODE(
    init_render_target(render_target, device->device(), *vmalloc, args->initial_extent),
    "Failed to create render target");
  delqueue.enqueue(render_target.image, *vmalloc);

  ka_VulkanContext_impl::ImDrawData imdraw;
  KA_VK_UNEX_CODE(init_imdraw_data(imdraw, device->device(), graphics_queue),
                  "Failed to initialize immediate draw data");
  delqueue.enqueue(imdraw.cmdpool, device->device());
  delqueue.enqueue(imdraw.fence, device->device());

  if (args->init_imgui) {
    vk_init_imgui(vk, *device, *swapchain, delqueue, args->init_imgui, args->init_imgui_user);
  }

  new (ctx) ka_VulkanContext_impl(vk, messenger, *vmalloc, surface, *std::move(device),
                                  *std::move(swapchain), *std::move(framedata), std::move(imdraw),
                                  std::move(render_target), std::move(delqueue));

  swapchain_err.disengage();
  ctx_err.disengage();
  delqueue_err.disengage();

  (*ctx_) = ctx;
  return VK_SUCCESS;
}

void ka_vk_destroy_context(ka_VulkanContext ctx) {
  if (!ctx) {
    return;
  }
  ctx->swapchain.destroy(ctx->device.device());
  ctx->delqueue.flush();
  std::destroy_at(ctx);
  std::allocator<ka_VulkanContext_impl>().deallocate(ctx, 1);
}

VkResult ka_vk_rebuild_swapchain(ka_VulkanContext vk, VkExtent2D extent) {
  ka_assert(vk);
  const auto swargs = make_swapchain_args(vk->device, vk->surface, extent);
  return VulkanSwapchain::create(swargs, vk->swapchain.swapchain())
    .transform([&](VulkanSwapchain&& swapchain) {
      vk->swapchain.destroy(vk->device.device());
      vk->swapchain = std::move(swapchain);
    })
    .error_or(VkError(VK_SUCCESS))
    .code();
}

void ka_vk_get_target_image_view(ka_VulkanContext vk, VkImageView* view) {
  if (vk && view) {
    (*view) = vk->target.image.view;
  }
}

namespace {

fn immediate_submit(VkDevice device, VkCommandBuffer cmd, VkQueue graphics_queue,
                    VkFence draw_fence, auto&& func) -> void {
  KA_VK_ASSERT(vkResetFences(device, 1, &draw_fence));
  KA_VK_ASSERT(vkResetCommandBuffer(cmd, 0));

  const auto cmd_begin_info = vkmk_cmdbuf_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  KA_VK_ASSERT(vkBeginCommandBuffer(cmd, &cmd_begin_info));
  func(cmd);
  KA_VK_ASSERT(vkEndCommandBuffer(cmd));

  const auto cmdinfo = vkmk_command_buffer_submit_info(cmd);
  const auto submit = vkmk_submit_info(cmdinfo, nullptr, nullptr);

  // Submit to queue and execute
  // fence blocks until the gfx commands finish execution
  KA_VK_ASSERT(vkQueueSubmit2(graphics_queue, 1, &submit, draw_fence));
  KA_VK_ASSERT(vkWaitForFences(device, 1, &draw_fence, VK_TRUE, 9999999999));
}

} // namespace

VkResult ka_vk_immediate_submit(ka_VulkanContext vk, const ka_VulkanImSubmitData* data) {
  ka_assert(vk);
  ka_assert(data);
  ka_assert(data->submit_callback);

  const auto device = vk->device.device();
  const auto graphics_queue = vk->device.graphics_queue();
  const auto& imdraw = vk->imdrawdata;
  const auto cmd = imdraw.cmdbuf;
  const auto fence = imdraw.fence;
  immediate_submit(device, cmd, graphics_queue, fence, [&](VkCommandBuffer cmd) {
    data->submit_callback(data->submit_callback_user, cmd);
  });

  return VK_SUCCESS;
}

VkResult ka_vk_new_frame(ka_VulkanContext vk) {
  ka_assert(vk);
  auto& frame = vk->framedata.next_frame();
  const auto cmd = frame.cmdbuf;
  const auto device = vk->device.device();
  const auto swapchain = vk->swapchain.swapchain();

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
  return ret;
}

VkResult ka_vk_end_frame(ka_VulkanContext vk) {
  ka_assert(vk);
  const auto& frame = vk->framedata.curr_frame();
  const auto cmd = frame.cmdbuf;
  const auto draw_extent = vk->target.extent;
  const auto& draw_image = vk->target.image;
  const auto graphics_queue = vk->device.graphics_queue();
  const auto present_queue = vk->device.present_queue();
  const auto swapchain = vk->swapchain.swapchain();

  const auto swapchain_extent = vk->swapchain.extent();
  const auto swapchain_image = vk->swapchain.images()[frame.swapchain_idx];

  // Prepare the swapchain image for copying
  VkImageLayout swapchain_layout = VK_IMAGE_LAYOUT_UNDEFINED; // dont care about the older layout
  swapchain_layout = vkcmd_transition_image(cmd, swapchain_image, swapchain_layout,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  // Do the copy
  vkcmd_transfer_image(cmd, draw_image.image, swapchain_image, draw_extent, swapchain_extent);

#if 0
  // Post draw
  swapchain_layout = vkcmd_transition_image(cmd, swapchain_image, swapchain_layout,
                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  // draw here
#endif

  // Prepare swapchain image for presenting
  swapchain_layout = vkcmd_transition_image(cmd, swapchain_image, swapchain_layout,
                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
  return ret;
}

VkResult ka_vk_record_cmd(ka_VulkanContext vk, ka_VulkanCommandData const* data) {}

VkResult ka_vk_record_compute(ka_VulkanContext vk, ka_VulkanComputeData const* data) {}

#if 0
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
                     VulkanContext_impl::ImDrawData& imdraw, VkQueue graphics_queue,
                     const MeshData& mesh) -> VulkanContext_impl::DrawThing {
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

  return VulkanContext_impl::DrawThing{
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

fn draw_background(VulkanContext_impl& ctx, VkCommandBuffer cmd) -> void {
  auto& effect = ctx.draw->background_effects[ctx.draw->effect_idx];

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1,
                          &ctx.draw->image_desc, 0, nullptr);
  vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(effect.data),
                     &effect.data);
  vkCmdDispatch(cmd, std::ceil(ctx.draw->extent.width / 16.f),
                std::ceil(ctx.draw->extent.height / 16.f), 1);
}

fn draw_geometry(VulkanContext_impl& ctx, VkCommandBuffer cmd, VkExtent2D draw_extent,
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
  const auto& frame = _impl->framedata->next_frame();
  const auto draw_extent = _impl->draw->extent;
  const auto cmd = frame.cmdbuf;
  const auto& draw_image = _impl->draw->image.image;
  const auto [device, graphics_queue, present_queue] = get_render_queues(*_impl->device);

  VkResult res = VK_SUCCESS;

  // Imgui user rendering things
  vk_imgui_new_frame();
  imgui_fn();

  // Wait for the previous rendering commands to finish
  KA_VK_ASSERT(vkWaitForFences(device, 1, &frame.render_fen, true, 1000000000));
  KA_VK_ASSERT(vkResetFences(device, 1, &frame.render_fen));

  // Acquire swapchain image. Will signal the swapchain semaphore when we acquire an image.
  const auto [swapchain, swapchain_image, swapchain_view, swapchain_extent, swapchain_idx] =
    acquire_swapchain_image(device, frame.swapchain_sem, *_impl->swapchain, &res);

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
    draw_background(*_impl, cmd);

    // 3. Draw triangle using graphics pipeline (we use a color attachment layout)
    image_layout = vkcmd_transition_image(cmd, draw_image, image_layout,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    draw_geometry(*_impl, cmd, draw_extent, mesh_transform);

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
