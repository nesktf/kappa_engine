#include "./vk_context.hpp"

#include "./vk_pipeline.hpp"

#include "../../util/filesystem.hpp"
#include "render/vulkan/vk_util.hpp"
#include "vk_private.hpp"
#include <vulkan/vulkan_core.h>

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
  auto& ctx = *static_cast<VulkanContextImpl*>(user_data);
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

fn make_vk_instance(VulkanContextImpl& ctx, const VulkanInfo& app_info,
                    Span<const char*> extensions) -> VkSvExpect<void> {

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
    return {unexpect, "Validation layers not available", VK_ERROR_EXTENSION_NOT_PRESENT};
  }
#endif

  VkApplicationInfo vkapp_info{};
  vkapp_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // struct type
  // app_info.pNext = nullptr; // No extensions, not needed if default constructed
  vkapp_info.pApplicationName = app_info.app_name;
  vkapp_info.applicationVersion = app_info.app_ver;
  vkapp_info.pEngineName = KA_ENGINE_NAME;
  vkapp_info.engineVersion = KA_ENGINE_VER;
  vkapp_info.apiVersion = KA_VULKAN_VERSION;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &vkapp_info;

  u32 ext_count{0};
  vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
  Vec<VkExtensionProperties> exts;
  exts.resize(ext_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());
  VK_LOG(debug, "Vulkan extensions available ({}):", ext_count);
  for (const auto& ext : exts) {
    VK_LOG(debug, "- {}", ext.extensionName);
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
  messenger_info.pUserData = (void*)&ctx;

  // Which global validation layers to enable
  create_info.enabledLayerCount = static_cast<u32>(validation_layers.size());
  create_info.ppEnabledLayerNames = validation_layers.data();

  // Enable a debug messenger for vkCreateInstance and vkDestroyInstance
  create_info.pNext = static_cast<const void*>(&messenger_info);
#else
  create_info.enabledLayerCount = 0;
  create_info.pNext = nullptr;
#endif
  if (auto ret = vkCreateInstance(&create_info, vkalloc, &ctx.vk); ret != VK_SUCCESS) {
    return {unexpect, "Failed to create vulkan instance", ret};
  }
  ctx.delqueue.enqueue(ctx.vk);

#ifndef NDEBUG
  // Create the debug messenger
  if (auto ret = CreateDebugUtilsMessengerEXT(ctx.vk, &messenger_info, vkalloc, &ctx.messenger);
      ret != VK_SUCCESS) {
    return {unexpect, "Failed to create debug messenger", ret};
  }
  ctx.delqueue.enqueue(ctx.messenger, ctx.vk);
#endif

  VK_LOG(debug, "Vulkan instance initialized");

  return {};
}

namespace {

struct command_context {
  std::array<VulkanContextImpl::FrameData, MAX_FRAMES_IN_FLIGHT> frames;
  VulkanContextImpl::ImDrawData imdraw;
};

} // namespace

fn create_command_data(VkDevice device, u32 graphics_queue) -> command_context {
  command_context ctx{};
  // create a command pool for commands submitted to the graphics queue.
  // we also want the pool to allow for resetting of individual command buffers
  const auto cmdpool_info =
    vkmk_cmdpool_info(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphics_queue);
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_ASSERT(vkCreateCommandPool(device, &cmdpool_info, vkalloc, &ctx.frames[i].cmdpool));
    // ctx.delqueue.enqueue(ctx.frames[i]->cmdpool, device);

    // allocate the default command buffer that we will use for rendering
    const auto cmd_alloc_info =
      vkmk_cmdbuf_alloc_info(ctx.frames[i].cmdpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VK_ASSERT(vkAllocateCommandBuffers(device, &cmd_alloc_info, &ctx.frames[i].cmdbuf));
  }

  // Immediate draw command pool
  {
    VK_ASSERT(vkCreateCommandPool(device, &cmdpool_info, vkalloc, &ctx.imdraw.cmdpool));
    const auto cmd_alloc_info =
      vkmk_cmdbuf_alloc_info(ctx.imdraw.cmdpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VK_ASSERT(vkAllocateCommandBuffers(device, &cmd_alloc_info, &ctx.imdraw.cmdbuf));
  }

  // create syncronization structures
  // one fence to control when the gpu has finished rendering the frame,
  // and 2 semaphores to syncronize rendering with swapchain
  // we want the fence to start signalled so we can wait on it on the first frame
  const auto fence_create_info = vkmk_fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
  const auto semaphore_create_info = vkmk_semaphore_info(0);
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_ASSERT(vkCreateFence(device, &fence_create_info, vkalloc, &ctx.frames[i].render_fen));
    // ctx.delqueue.enqueue(frames[i].render_fen, device);

    VK_ASSERT(
      vkCreateSemaphore(device, &semaphore_create_info, vkalloc, &ctx.frames[i].swapchain_sem));
    // ctx.delqueue.enqueue(ctx.frames[i].swapchain_sem, ctx.device.device);

    VK_ASSERT(
      vkCreateSemaphore(device, &semaphore_create_info, vkalloc, &ctx.frames[i].render_sem));
    // ctx.delqueue.enqueue(ctx.frames[i].render_sem, ctx.device.device);
  }

  // Immediate draw sync things
  {
    VK_ASSERT(vkCreateFence(device, &fence_create_info, vkalloc, &ctx.imdraw.fence));
  }

  return ctx;
}

fn swapchain_args_from_ctx(VulkanContextImpl& ctx, VkExtent2D surface_extent)
  -> VulkanSwapchainArgs {
  ka_assert(ctx.device);
  VulkanSwapchainArgs swargs{};
  swargs.device = ctx.device->device();
  swargs.physical_device = ctx.device->physical_device();
  swargs.surface = ctx.surface;
  swargs.surface_extent = surface_extent;
  swargs.surface_formats = ctx.device->surface_formats();
  swargs.surface_present_modes = ctx.device->surface_present_modes();
  const auto [graphics, present, _] = ctx.device->queues();
  swargs.graphics_queue = graphics;
  swargs.present_queue = present;
  return swargs;
}

fn make_vulkan_ctx_destroyer(VulkanContextImpl* ctx) {
  return [=]() {
    if (ctx->device) {
      ctx->device->wait_idle();
      if (ctx->swapchain) {
        // We have to handle the swapchain destruction manually
        ctx->swapchain->destroy(ctx->device->device());
      }
    }
    ctx->delqueue.flush();
    std::destroy_at(ctx);
    std::allocator<VulkanContextImpl>().deallocate(ctx, 1);
  };
}

fn create_compute_pipeline(VkDevice device, VkPipelineLayout layout, const char* path)
  -> VkPipeline {
  const auto real_path = std::string(KA_RES_DIR) + path;
  auto file = load_entire_file(real_path.c_str());
  ka_assert(!file.empty());

  auto shader = vk_create_shader(device, {file.data(), file.size()}).value();

  VkPipelineShaderStageCreateInfo stage_info{};
  stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage_info.pNext = nullptr;

  stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage_info.module = shader;
  stage_info.pName = "main";

  VkComputePipelineCreateInfo compute_info{};
  compute_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  compute_info.pNext = nullptr;

  compute_info.layout = layout;
  compute_info.stage = stage_info;

  VkPipeline pip{};
  VK_ASSERT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_info, vkalloc, &pip));
  vkDestroyShaderModule(device, shader, vkalloc);

  return pip;
}

// temp: test rendering things
fn create_draw_thing(VkDevice device, VkExtent2D draw_extent, VmaAllocator vmalloc)
  -> VulkanContextImpl::DrawThing {
  // Init draw image
  const VulkanImageArgs imgargs{
    .device = device,
    .vma = vmalloc,
    .extent = draw_extent,
    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
  };
  auto image = vk_alloc_image(imgargs).value();

  // Init descriptors
  const auto sizes = std::to_array<VulkanDescPoolRatio>({
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
  });
  auto desc_pool = VulkanDescPool::create(device, 10, sizes).value();
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

  VkWriteDescriptorSet draw_image_write{};
  draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  draw_image_write.pNext = nullptr;

  draw_image_write.dstBinding = 0;
  draw_image_write.dstSet = image_desc;
  draw_image_write.descriptorCount = 1;
  draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  draw_image_write.pImageInfo = &img_info;

  vkUpdateDescriptorSets(device, 1, &draw_image_write, 0, nullptr);

  // Init pipelines
  VkPipelineLayoutCreateInfo compute_layout{};
  compute_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  compute_layout.pNext = nullptr;
  compute_layout.pSetLayouts = &image_desc_layout;
  compute_layout.setLayoutCount = 1;

  VkPushConstantRange push_constant{};
  push_constant.offset = 0;
  push_constant.size = sizeof(ComputeConstants);
  push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  compute_layout.pPushConstantRanges = &push_constant;
  compute_layout.pushConstantRangeCount = 1;

  VkPipelineLayout layout{};
  VK_ASSERT(vkCreatePipelineLayout(device, &compute_layout, vkalloc, &layout));

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

  return VulkanContextImpl::DrawThing{.image = std::move(image),
                                      .extent = draw_extent,
                                      .desc_pool = std::move(desc_pool),
                                      .image_desc = image_desc,
                                      .image_desc_layout = image_desc_layout,
                                      .background_effects = effects,
                                      .effect_idx = 0};
}

} // namespace

fn VulkanContext::Deleter::operator()(VulkanContextImpl* ctx) noexcept -> void {
  make_vulkan_ctx_destroyer(ctx)();
}

fn VulkanContext::create(const VulkanInfo& app_info, VkExtent2D surface_extent,
                         Span<const char*> surface_extensions, SurfaceProviderFn surface_provider)
  -> VkSvExpect<VulkanContext> {

  Vec<const char*> extensions;
  extensions.reserve(surface_extensions.size() + 1);
  if (!surface_extensions.empty()) {
    VK_LOG(debug, "Required Vulkan surface extensions ({}):", surface_extensions.size());
    for (const char* ext : surface_extensions) {
      VK_LOG(debug, "- {}", ext);
      extensions.push_back(ext);
    }
  } else {
    VK_LOG(warn, "No Vulkan extensions provided for surface creation");
  }
#ifndef NDEBUG
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  auto* ctx = std::allocator<VulkanContextImpl>().allocate(1);
  std::memset((void*)ctx, 0x00, sizeof(*ctx));
  ctx = std::construct_at(ctx);
  DeferFn ctxerr = make_vulkan_ctx_destroyer(ctx);

  const fn create_surface = [&]() -> VkSvExpect<VkSurfaceKHR> {
    VkSurfaceKHR surface{};
    const auto res = surface_provider(ctx->vk, &surface, vkalloc);
    if (res != VK_SUCCESS || surface == VK_NULL_HANDLE) {
      return {unexpect, "Failed to create surface",
              res == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : res};
    }
    return {in_place, surface};
  };

  const fn select_device = [&](VkSurfaceKHR&& surface) -> VkSvExpect<VulkanDevice> {
    ctx->surface = surface;
    ctx->delqueue.enqueue(surface, ctx->vk);
    return VulkanDevice::create(ctx->vk, ctx->surface)
      .transform_error([](VkError&& err) -> VkSvError {
        return {"Failed to find a suitable Vulkan device", err.code()};
      });
  };

  const fn create_buffer_alloc = [&](VulkanDevice&& device) -> VkSvExpect<VmaAllocator> {
    ctx->device.emplace(std::move(device));
    ctx->device->add_to_delqueue(ctx->delqueue);
    return vk_create_vma_alloc(ctx->vk, ctx->device->device(), ctx->device->physical_device())
      .transform_error([](VkError&& err) -> VkSvError {
        return {"Failed to create buffer allocator", err.code()};
      });
  };

  const fn create_swapchain = [&](VmaAllocator&& vmalloc) -> VkSvExpect<VulkanSwapchain> {
    ctx->vmalloc = vmalloc;
    ctx->delqueue.enqueue(vmalloc, ctx->device->device());
    const auto swargs = swapchain_args_from_ctx(*ctx, surface_extent);
    return VulkanSwapchain::create(swargs).transform_error(
      [](VkError&& err) -> VkSvError { return {"Failed to create swapchain", err.code()}; });
  };

  const fn initialize_command_data = [&](VulkanSwapchain&& swapchain) -> VulkanContext {
    ctx->swapchain.emplace(std::move(swapchain));
    const auto device = ctx->device->device();
    const auto [graphics, _, __] = ctx->device->queues();
    auto cmds = create_command_data(device, graphics);
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      ctx->frames[i].emplace(std::move(cmds.frames[i]));
      ctx->delqueue.enqueue(ctx->frames[i]->cmdpool, ctx->device->device());
      ctx->delqueue.enqueue(ctx->frames[i]->render_fen, ctx->device->device());
      ctx->delqueue.enqueue(ctx->frames[i]->render_sem, ctx->device->device());
      ctx->delqueue.enqueue(ctx->frames[i]->swapchain_sem, ctx->device->device());
    }
    ctx->imdraw.emplace(std::move(cmds.imdraw));
    ctx->delqueue.enqueue(ctx->imdraw->cmdpool, ctx->device->device());
    ctx->delqueue.enqueue(ctx->imdraw->fence, ctx->device->device());

    // temp: test rendering things
    ctx->draw.emplace(create_draw_thing(device, ctx->swapchain->extent(), ctx->vmalloc));
    ctx->delqueue.enqueue(ctx->draw->image.image, ctx->draw->image.alloc, ctx->vmalloc);
    ctx->delqueue.enqueue(ctx->draw->image.view, device);
    ctx->draw->desc_pool.add_to_delqueue(ctx->delqueue);
    ctx->delqueue.enqueue(ctx->draw->image_desc_layout, device);
    for (auto& effect : ctx->draw->background_effects) {
      ctx->delqueue.enqueue(effect.pipeline, device);
    }
    ctx->delqueue.enqueue(ctx->draw->background_effects[0].layout, device);

    ctxerr.disengage();
    return {*ctx};
  };

  return make_vk_instance(*ctx, app_info, {extensions.data(), extensions.size()})
    .and_then(create_surface)
    .and_then(select_device)
    .and_then(create_buffer_alloc)
    .and_then(create_swapchain)
    .transform(initialize_command_data);
}

fn VulkanContext::rebuild_swapchain(VkExtent2D surface_extent) -> VkExpect<void> {
  const auto swargs = swapchain_args_from_ctx(*_impl, surface_extent);
  return VulkanSwapchain::create(swargs, _impl->swapchain->swapchain())
    .transform([&](VulkanSwapchain&& swapchain) {
      _impl->swapchain->destroy(_impl->device->device());
      _impl->swapchain.emplace(std::move(swapchain));
    });
}

namespace {

fn draw_background(VulkanContextImpl& ctx, VkCommandBuffer cmdbuf) -> void {
  // make a clear-color from frame number. This will flash with a 120 frame period.
  // f32 flash = std::abs(std::sin(ctx.curr_frame / 120.f));
  // VkClearColorValue clear_value{{0.0f, 0.0f, flash, 1.0f}};
  // const auto clear_range = vkmk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

#if 0
  // clear image
  vkCmdClearColorImage(cmdbuf, ctx.draw.image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1,
                       &clear_range);
#endif

  auto& effect = ctx.draw->background_effects[ctx.draw->effect_idx];

  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
  vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1,
                          &ctx.draw->image_desc, 0, nullptr);
  vkCmdPushConstants(cmdbuf, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(effect.data),
                     &effect.data);
  vkCmdDispatch(cmdbuf, std::ceil(ctx.draw->extent.width / 16.f),
                std::ceil(ctx.draw->extent.height / 16.f), 1);
}

} // namespace

fn VulkanContext::draw() -> void {
  auto& frame = *_impl->frames[_impl->curr_frame % MAX_FRAMES_IN_FLIGHT];

  const auto device = _impl->device->device();
  const auto [graphics_idx, present_idx, _] = _impl->device->queues();

  VkQueue graphics_queue{};
  vkGetDeviceQueue(device, graphics_idx, 0, &graphics_queue);
  ka_assert(graphics_queue != VK_NULL_HANDLE);
  VkQueue present_queue{};
  vkGetDeviceQueue(device, present_idx, 0, &present_queue);
  ka_assert(present_queue != VK_NULL_HANDLE);

  // Wait for fences
  VK_ASSERT(vkWaitForFences(device, 1, &frame.render_fen, true, 1000000000));
  VK_ASSERT(vkResetFences(device, 1, &frame.render_fen));
  // frame.delqueue.flush();

  // Acquire swapchain image
  u32 swapchain_image_idx;
  const auto swapchain = _impl->swapchain->swapchain();
  VkResult res = vkAcquireNextImageKHR(device, swapchain, 1000000000, frame.swapchain_sem, nullptr,
                                       &swapchain_image_idx);
  ka_assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);

  // Initialize command buffer
  VK_ASSERT(vkResetCommandBuffer(frame.cmdbuf, 0));
  const auto cmd_begin_info = vkmk_cmdbuf_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_ASSERT(vkBeginCommandBuffer(frame.cmdbuf, &cmd_begin_info));

  // transition our main draw image into general layout so we can write into it
  // we will overwrite it all so we dont care about what was the older layout
  vkcmd_transition_image(frame.cmdbuf, _impl->draw->image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_GENERAL);

  // clear the thing
  draw_background(*_impl, frame.cmdbuf);

  // transition the draw image and the swapchain image into their correct transfer layouts
  const auto swapchain_images = _impl->swapchain->images();
  const auto swapchain_image_views = _impl->swapchain->image_views();
  const auto swapchain_extent = _impl->swapchain->extent();
  vkcmd_transition_image(frame.cmdbuf, _impl->draw->image.image, VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkcmd_transition_image(frame.cmdbuf, swapchain_images[swapchain_image_idx],
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkExtent2D draw_extent;
  draw_extent.width = _impl->draw->image.extent.width;
  draw_extent.height = _impl->draw->image.extent.height;
  // execute a copy from the draw image into the swapchain
  vkcmd_transfer_image(frame.cmdbuf, _impl->draw->image.image,
                       swapchain_images[swapchain_image_idx], draw_extent, swapchain_extent);

  // make the swapchain image into presentable mode
  vkcmd_transition_image(frame.cmdbuf, swapchain_images[swapchain_image_idx],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // draw imgui
  {
    vk_draw_imgui(frame.cmdbuf, swapchain_image_views[swapchain_image_idx], swapchain_extent);
    vkcmd_transition_image(frame.cmdbuf, swapchain_images[swapchain_image_idx],
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  }

  // finalize the command buffer (we can no longer add commands, but it can now be executed)
  VK_ASSERT(vkEndCommandBuffer(frame.cmdbuf));

  // prepare the submission to the queue.
  // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is
  // ready we will signal the _renderSemaphore, to signal that rendering has finished
  const auto cmdinfo = vkmk_command_buffer_submit_info(frame.cmdbuf);
  const auto wait_info = vkmk_semaphore_submit_info(
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame.swapchain_sem);
  const auto signal_info =
    vkmk_semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.render_sem);
  const auto submit = vkmk_submit_info(cmdinfo, &signal_info, &wait_info);

  // submit command buffer to the queue and execute it.
  //  _renderFence will now block until the graphic commands finish execution
  VK_ASSERT(vkQueueSubmit2(graphics_queue, 1, &submit, frame.render_fen));

  // prepare present
  //  this will put the image we just rendered to into the visible window.
  //  we want to wait on the _renderSemaphore for that,
  //  as its necessary that drawing commands have finished before the image is displayed to the
  //  user
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &frame.render_sem;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchain_image_idx;

  res = vkQueuePresentKHR(present_queue, &presentInfo);
  ka_assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);

  ++_impl->curr_frame;
}

fn vk_get_graphics_queue(VulkanContextImpl& ctx) -> VkQueue {
  const auto [graphics_idx, _, __] = ctx.device->queues();
  VkQueue graphics_queue{};
  vkGetDeviceQueue(ctx.device->device(), graphics_idx, 0, &graphics_queue);
  ka_assert(graphics_queue != VK_NULL_HANDLE);
  return graphics_queue;
}

fn VulkanContext::immediate_submit(ImSubmitFn func) -> void {
  const auto device = _impl->device->device();
  auto& imdraw = *_impl->imdraw;
  const auto cmd = imdraw.cmdbuf;
  const auto graphics_queue = vk_get_graphics_queue(*_impl);

  VK_ASSERT(vkResetFences(device, 1, &imdraw.fence));
  VK_ASSERT(vkResetCommandBuffer(cmd, 0));

  const auto cmd_begin_info = vkmk_cmdbuf_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_ASSERT(vkBeginCommandBuffer(cmd, &cmd_begin_info));
  func(cmd);
  VK_ASSERT(vkEndCommandBuffer(cmd));

  const auto cmdinfo = vkmk_command_buffer_submit_info(cmd);
  const auto submit = vkmk_submit_info(cmdinfo, nullptr, nullptr);

  // Submit to queue and execute
  // fence blocks until the gfx commands finish execution
  VK_ASSERT(vkQueueSubmit2(graphics_queue, 1, &submit, imdraw.fence));
  VK_ASSERT(vkWaitForFences(device, 1, &imdraw.fence, VK_TRUE, 9999999999));
}

fn VulkanContext::get_effect() -> ComputeEffect& {
  return _impl->draw->background_effects[_impl->draw->effect_idx];
}

fn VulkanContext::get_effect_idx() -> s32& {
  return _impl->draw->effect_idx;
}

} // namespace kappa::render
