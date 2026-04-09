#include "./vk_context.hpp"
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "./vk_private.hpp"

#include "./vk_util.hpp"

#include <unordered_set>

namespace kappa::render {

namespace {

constexpr auto base_device_extensions = std::to_array<const char*>({
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
});
constexpr auto validation_layers = std::to_array<const char*>({
  "VK_LAYER_KHRONOS_validation",
});

constexpr VkAllocationCallbacks* vkalloc = nullptr;

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

#ifndef NDEBUG
  // Create the debug messenger
  if (auto ret = CreateDebugUtilsMessengerEXT(ctx.vk, &messenger_info, vkalloc, &ctx.messenger);
      ret != VK_SUCCESS) {
    vkDestroyInstance(ctx.vk, nullptr);
    return {unexpect, "Failed to create debug messenger", ret};
  }
#endif

  VK_LOG(debug, "Vulkan instance initialized");

  return {};
}

fn select_device(VulkanContextImpl& ctx) -> VkSvExpect<void> {
  u32 device_count{};
  VkResult res = vkEnumeratePhysicalDevices(ctx.vk, &device_count, nullptr);
  if (device_count == 0) {
    return {unexpect, "Failed to find a GPU with Vulkan support", res};
  }

  Optional<u32> graphics;
  Optional<u32> present;
  Optional<u32> transfer;
  Vec<VkQueueFamilyProperties> queue_families;
  const fn find_queue_indices = [&](VkPhysicalDevice device) -> bool {
    queue_families.clear();

    u32 queue_family_count{};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    queue_families.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (u32 i = 0; const auto& family : queue_families) {
      // Require a device with a queue family that supports graphics commands
      if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        graphics.emplace(i);
      }

      // The same but for transfers
      // if (family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
      //   indices.transfer_family = i;
      // }

      if ((family.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
          !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        transfer.emplace(i);
      }

      // Require a device with a queue family that supports presentation
      // (might not be the same queue as the graphics one, so we store another index)
      VkBool32 present_support{false};
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, ctx.surface, &present_support);
      if (present_support) {
        present.emplace(i);
      }

      if (graphics.has_value() && present.has_value() && transfer.has_value()) {
        return true;
      }

      ++i;
    }
    return false;
  };

  Vec<VkExtensionProperties> avail_ext;
  const fn check_extension_support = [&](VkPhysicalDevice device) -> bool {
    // Check if the physical device has all the extensions in device_extensions
    u32 ext_count{};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, nullptr);

    avail_ext.resize(ext_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, avail_ext.data());

    // auto required_extensions = make_scratch_set<std::string_view>(arena);
    std::unordered_set<std::string_view> required_extensions;
    required_extensions.insert(base_device_extensions.begin(), base_device_extensions.end());
    for (const auto& ext : avail_ext) {
      required_extensions.erase(ext.extensionName);
    }

    return required_extensions.empty();
  };

  Vec<VkSurfaceFormatKHR> swapchain_formats;
  Vec<VkPresentModeKHR> swapchain_present_modes;
  const fn query_swapchain_support = [&](VkPhysicalDevice device) -> bool {
    // Get supported formats and present modes from a device
    swapchain_formats.clear();
    swapchain_formats.clear();

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, ctx.surface, &caps);

    u32 format_count{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, ctx.surface, &format_count, nullptr);
    if (format_count) {
      swapchain_formats.resize(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, ctx.surface, &format_count,
                                           swapchain_formats.data());
    }

    u32 present_mode_count{};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, ctx.surface, &present_mode_count, nullptr);
    if (present_mode_count) {
      swapchain_present_modes.resize(present_mode_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, ctx.surface, &present_mode_count,
                                                swapchain_present_modes.data());
    }
    bool swapchain_adequate = !swapchain_formats.empty() && !swapchain_present_modes.empty();
    return swapchain_adequate;
  };

  Vec<VkPhysicalDevice> devices;
  devices.resize(device_count);
  vkEnumeratePhysicalDevices(ctx.vk, &device_count, devices.data());

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  for (const auto& dev : devices) {
    // Select the first suitable physical device found
    const bool has_queue_indices = find_queue_indices(dev);
    const bool ext_supported = check_extension_support(dev);
    const bool swapchain_adequate = query_swapchain_support(dev);
    if (has_queue_indices && ext_supported && swapchain_adequate) {
      physical_device = dev;
      break;
    }
  }
  if (physical_device == VK_NULL_HANDLE) {
    return {unexpect, "Failed to find a suitable GPU", VK_ERROR_DEVICE_LOST};
  }

  // Create a device to interface with the physical device
  std::array<VkDeviceQueueCreateInfo, 3> queue_infos;
  u32 queue_idx = 0u;
  const float queue_priority = 1.f; // Priority for the scheduling of command buffer execution
  {
    // To avoid adding the same queue family more than once
    std::unordered_set<u32> unique_queue_families;
    // auto unique_queue_families = make_scratch_set<u32>(arena);
    unique_queue_families.emplace(graphics.value());
    unique_queue_families.emplace(present.value());
    unique_queue_families.emplace(transfer.value());

    // How many queues we want for each queue family?
    for (u32 family : unique_queue_families) {
      VkDeviceQueueCreateInfo info{};
      info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      info.queueFamilyIndex = family;
      info.queueCount = 1;
      info.pQueuePriorities = &queue_priority;
      queue_infos[queue_idx] = info;
      ++queue_idx;
    }
  }
  ka_assert(queue_idx > 0);

  VkPhysicalDeviceVulkan13Features vk13feats{};
  vk13feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vk13feats.dynamicRendering = true;
  vk13feats.synchronization2 = true;

  VkPhysicalDeviceVulkan12Features vk12feats{};
  vk12feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vk12feats.bufferDeviceAddress = true;
  vk12feats.descriptorIndexing = true;
  vk12feats.pNext = &vk13feats;

  // Which physical device features are we going to use?
  VkPhysicalDeviceFeatures features{}; // all VK_FALSE for now

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pQueueCreateInfos = queue_infos.data();
  create_info.queueCreateInfoCount = queue_idx;
  create_info.pEnabledFeatures = &features;
  create_info.pNext = &vk12feats;

  // Specify extensions and validation layers (device specific this time)
  create_info.enabledExtensionCount = static_cast<u32>(base_device_extensions.size());
  create_info.ppEnabledExtensionNames = base_device_extensions.data();

#ifndef NDEBUG
  create_info.enabledLayerCount = static_cast<u32>(validation_layers.size());
  create_info.ppEnabledLayerNames = validation_layers.data();
#else
  create_info.enabledLayerCount = 0;
#endif

  if (auto ret = vkCreateDevice(physical_device, &create_info, vkalloc, &ctx.device.device);
      ret != VK_SUCCESS) {
    return {unexpect, "Failed to initialize vulkan device", ret};
  }

  // Fill device struct
  ctx.device.transfer_queue = transfer.value();
  ctx.device.present_queue = present.value();
  ctx.device.graphics_queue = graphics.value();
  ctx.device.surface_formats = std::move(swapchain_formats);
  ctx.device.surface_present_modes = std::move(swapchain_present_modes);
  ctx.device.physical_device = physical_device;

  VmaAllocatorCreateInfo vma_info{};
  vma_info.instance = ctx.vk;
  vma_info.device = ctx.device.device;
  vma_info.physicalDevice = physical_device;
  vma_info.vulkanApiVersion = KA_VULKAN_VERSION;
  vma_info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT |
                   VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  if (auto ret = vmaCreateAllocator(&vma_info, &ctx.vmalloc); ret != VK_SUCCESS) {
    if (ctx.device.device) {
      vkDestroyDevice(ctx.device.device, vkalloc);
    }
    return {unexpect, "Failed to initialize vulkan allocator", ret};
  }

  return {};
}

fn create_swapchain(VulkanContextImpl& ctx, VkExtent2D surface_extent) -> VkSvExpect<void> {
  ka_assert(!ctx.device.surface_present_modes.empty());
  ka_assert(!ctx.device.surface_formats.empty());

  const auto old_swapchain = ctx.swapchain.swapchain;

  // How are the images in the swapchain presented?
  const fn find_present_mode = [&]() -> VkPresentModeKHR {
    // VK_PRESENT_MODE_IMMEDIATE_KHR -> Images submited are transferred to the screen immediately
    // VK_PRESENT_MODE_FIFO_KHR -> The swap chain is a FIFO queue of images (double buffering)
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR -> Doesn't wait for next vblank if the queue was empty
    // VK_PRESENT_MODE_MAILBOX_KHR -> Triple buffering

    for (const auto& mode : ctx.device.surface_present_modes) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
    }

    return VK_PRESENT_MODE_FIFO_KHR; // Only one guaranteed to be available
  };

  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.device.physical_device, ctx.surface,
                                            &capabilities);

  // What is the format of the swapchain images?
  const auto swapchain_format = [&]() -> VkSurfaceFormatKHR {
    // Try to get a swap surface with B8G8R8A8 pixel format or just use the first one
    // if there is not one available
    for (const auto& format : ctx.device.surface_formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
          format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return format;
      }
    }
    return ctx.device.surface_formats[0];
  }();
  const auto image_format = swapchain_format.format;

  // Resolution of the swapchain images (in pixels), usually the resolution of the window
  const auto swapchain_extent = [&]() -> VkExtent2D {
    if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
      return capabilities.currentExtent;
    }

    VkExtent2D actual_extent = surface_extent;
    actual_extent.height = std::clamp(actual_extent.width, capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width);
    actual_extent.width = std::clamp(actual_extent.height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    return actual_extent;
  }();

  // Images that will be stored in the swap chain
  // Add one to avoid waiting for the driver to complete operatons
  u32 image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
    // Clamp
    image_count = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = ctx.surface;
  create_info.imageFormat = image_format;
  create_info.minImageCount = image_count;
  create_info.imageColorSpace = swapchain_format.colorSpace;
  create_info.imageExtent = swapchain_extent;
  create_info.imageArrayLayers = 1; // Layers for each image, 1 if not using stereoscopic 3D

  // Sets the operations for the swapchain, in this case we only render directly
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  // create_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT; // For postprocessing

  const u32 queue_indices[] = {ctx.device.graphics_queue, ctx.device.present_queue};
  // imageSharingMode only refers to ownership transfer needs
  if (ctx.device.graphics_queue != ctx.device.present_queue) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // Shared across queue families
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Owned by a single queue family
    // create_info.queueFamilyIndexCount = 0;
    // create_info.pQueueFamilyIndices = nullptr;
  }

  // I hate inverted framebuffers
  create_info.preTransform = capabilities.currentTransform;

  // For blending with other windows
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  create_info.presentMode = find_present_mode();
  create_info.clipped = VK_TRUE;

  // Used when the swap chain has to be reconstructed
  create_info.oldSwapchain = old_swapchain;

  if (auto res =
        vkCreateSwapchainKHR(ctx.device.device, &create_info, vkalloc, &ctx.swapchain.swapchain);
      res != VK_SUCCESS) {
    ctx.swapchain.swapchain = old_swapchain;
    return {unexpect, "Failed to create swapchain", res};
  }

  vkGetSwapchainImagesKHR(ctx.device.device, ctx.swapchain.swapchain, &image_count, nullptr);
  ka_assert(image_count > 0);
  if (old_swapchain == VK_NULL_HANDLE) {
    // First swapchain creation
    ctx.swapchain.images = make_array<VkImage>(uninitialized, image_count);
    ctx.swapchain.image_views = make_array<VkImageView>(uninitialized, image_count);
  } else {
    ka_assert(image_count == ctx.swapchain.images.size());
    // Swapchain recreations
    for (usize i = 0; i < ctx.swapchain.images.size(); ++i) {
      vkDestroyImageView(ctx.device.device, ctx.swapchain.image_views[i], vkalloc);
    }
    vkDestroySwapchainKHR(ctx.device.device, old_swapchain, vkalloc);
  }
  ctx.swapchain.extent = swapchain_extent;

  vkGetSwapchainImagesKHR(ctx.device.device, ctx.swapchain.swapchain, &image_count,
                          ctx.swapchain.images.data());
  for (u32 i = 0; i < ctx.swapchain.images.size(); ++i) {
    const auto create_info =
      vkmk_imageview_info(image_format, ctx.swapchain.images[i], VK_IMAGE_ASPECT_COLOR_BIT);
    VK_ASSERT(
      vkCreateImageView(ctx.device.device, &create_info, vkalloc, &ctx.swapchain.image_views[i]));
  }
  VK_LOG(debug, "Creating swapchain: {}x{}", swapchain_extent.width, swapchain_extent.height);

  return {};
}

fn make_vulkan_ctx_destroyer(VulkanContextImpl* ctx) {
  return [=]() {
    if (ctx->device.device != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(ctx->device.device);
      if (ctx->vmalloc != VK_NULL_HANDLE) {
        if (ctx->draw_image.image != VK_NULL_HANDLE) {
          vkDestroyImageView(ctx->device.device, ctx->draw_image.view, vkalloc);
          vmaDestroyImage(ctx->vmalloc, ctx->draw_image.image, ctx->draw_image.alloc);
        }
        vmaDestroyAllocator(ctx->vmalloc);
      }

      for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (ctx->frames[i].swapchain_sem) {
          vkDestroyFence(ctx->device.device, ctx->frames[i].render_fen, vkalloc);
          vkDestroySemaphore(ctx->device.device, ctx->frames[i].render_sem, vkalloc);
          vkDestroySemaphore(ctx->device.device, ctx->frames[i].swapchain_sem, vkalloc);
        }
        if (ctx->frames[i].cmdpool) {
          ctx->frames[i].delqueue.flush();
          vkDestroyCommandPool(ctx->device.device, ctx->frames[i].cmdpool, vkalloc);
        }
      }
      if (ctx->swapchain.swapchain != VK_NULL_HANDLE) {
        for (usize i = 0; i < ctx->swapchain.images.size(); ++i) {
          vkDestroyImageView(ctx->device.device, ctx->swapchain.image_views[i], vkalloc);
          // vkDestroyImage(ctx->device.device, ctx->swapchain.images[i], vkalloc);
        }
        vkDestroySwapchainKHR(ctx->device.device, ctx->swapchain.swapchain, vkalloc);
      }
      vkDestroyDevice(ctx->device.device, vkalloc);
    }
    if (ctx->surface != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(ctx->vk, ctx->surface, vkalloc);
    }
    if (ctx->messenger != VK_NULL_HANDLE) {
      DestroyDebugUtilsMessengerEXT(ctx->vk, ctx->messenger, vkalloc);
    }
    if (ctx->vk != VK_NULL_HANDLE) {
      vkDestroyInstance(ctx->vk, vkalloc);
    }
    std::destroy_at(ctx);
    std::allocator<VulkanContextImpl>().deallocate(ctx, 1);
  };
};

fn init_commands(VulkanContextImpl& ctx) {
  // create a command pool for commands submitted to the graphics queue.
  // we also want the pool to allow for resetting of individual command buffers
  const auto cmdpool_info =
    vkmk_cmdpool_info(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, ctx.device.graphics_queue);
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_ASSERT(
      vkCreateCommandPool(ctx.device.device, &cmdpool_info, nullptr, &ctx.frames[i].cmdpool));
    // allocate the default command buffer that we will use for rendering
    const auto cmd_alloc_info =
      vkmk_cmdbuf_alloc_info(ctx.frames[i].cmdpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VK_ASSERT(vkAllocateCommandBuffers(ctx.device.device, &cmd_alloc_info, &ctx.frames[i].cmdbuf));
  }
}

fn init_sync(VulkanContextImpl& ctx) -> void {
  // create syncronization structures
  // one fence to control when the gpu has finished rendering the frame,
  // and 2 semaphores to syncronize rendering with swapchain
  // we want the fence to start signalled so we can wait on it on the first frame
  const auto fence_create_info = vkmk_fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
  const auto semaphore_create_info = vkmk_semaphore_info(0);
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_ASSERT(
      vkCreateFence(ctx.device.device, &fence_create_info, vkalloc, &ctx.frames[i].render_fen));
    VK_ASSERT(vkCreateSemaphore(ctx.device.device, &semaphore_create_info, vkalloc,
                                &ctx.frames[i].swapchain_sem));
    VK_ASSERT(vkCreateSemaphore(ctx.device.device, &semaphore_create_info, vkalloc,
                                &ctx.frames[i].render_sem));
  }
}

fn init_draw_image(VulkanContextImpl& ctx) -> void {
  const VkExtent3D draw_extent{
    .width = ctx.swapchain.extent.width, .height = ctx.swapchain.extent.height, .depth = 1};
  // 32bit float
  ctx.draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  ctx.draw_image.extent = draw_extent;

  VkImageUsageFlags draw_usage{};
  draw_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  draw_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  draw_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  draw_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  const auto rimg_info = vkmk_image_info(ctx.draw_image.format, draw_usage, draw_extent);

  // Allocate the image data
  VmaAllocationCreateInfo rimg_allocinfo{};
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_ASSERT(vmaCreateImage(ctx.vmalloc, &rimg_info, &rimg_allocinfo, &ctx.draw_image.image,
                           &ctx.draw_image.alloc, nullptr));

  // Build image view
  const auto rview_info =
    vkmk_imageview_info(ctx.draw_image.format, ctx.draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_ASSERT(vkCreateImageView(ctx.device.device, &rview_info, vkalloc, &ctx.draw_image.view));
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
    VK_LOG(debug, "Surface Vulkan extensions:");
    for (const char* ext : surface_extensions) {
      VK_LOG(debug, "{}", ext);
      extensions.push_back(ext);
    }
  } else {
    VK_LOG(warn, "No Vulkan extensions provided for surface creation");
  }
#ifndef NDEBUG
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  auto* ctx = std::allocator<VulkanContextImpl>().allocate(1);
  std::memset(ctx, 0x00, sizeof(*ctx));
  ctx = std::construct_at(ctx);
  DeferFn ctxerr = make_vulkan_ctx_destroyer(ctx);

  return make_vk_instance(*ctx, app_info, {extensions.data(), extensions.size()})
    .and_then([&]() -> VkSvExpect<void> {
      const auto res = surface_provider(ctx->vk, &ctx->surface, vkalloc);
      if (res != VK_SUCCESS || ctx->surface == VK_NULL_HANDLE) {
        return {unexpect, "Failed to create surface",
                res == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : res};
      }
      return {};
    })
    .and_then([&]() { return select_device(*ctx); })
    .and_then([&]() { return create_swapchain(*ctx, surface_extent); })
    .transform([&]() -> VulkanContext {
      init_commands(*ctx);
      init_sync(*ctx);
      init_draw_image(*ctx);
      ctxerr.disengage();
      return {*ctx};
    });
}

fn VulkanContext::rebuild_swapchain(VkExtent2D surface_extent) -> VkSvExpect<void> {
  return create_swapchain(*_impl, surface_extent);
}

namespace {

fn draw_background(VulkanContextImpl& ctx, VkCommandBuffer cmdbuf) -> void {
  // make a clear-color from frame number. This will flash with a 120 frame period.
  f32 flash = std::abs(std::sin(ctx.curr_frame / 120.f));
  VkClearColorValue clear_value{{0.0f, 0.0f, flash, 1.0f}};
  const auto clear_range = vkmk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

  // clear image
  vkCmdClearColorImage(cmdbuf, ctx.draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1,
                       &clear_range);
}

} // namespace

fn VulkanContext::draw() -> void {
  auto& frame = _impl->frames[_impl->curr_frame % MAX_FRAMES_IN_FLIGHT];

  VkQueue graphics_queue{};
  vkGetDeviceQueue(_impl->device.device, _impl->device.graphics_queue, 0, &graphics_queue);
  ka_assert(graphics_queue != VK_NULL_HANDLE);
  VkQueue present_queue{};
  vkGetDeviceQueue(_impl->device.device, _impl->device.present_queue, 0, &present_queue);
  ka_assert(present_queue != VK_NULL_HANDLE);

  // Wait for fences
  VK_ASSERT(vkWaitForFences(_impl->device.device, 1, &frame.render_fen, true, 1000000000));
  VK_ASSERT(vkResetFences(_impl->device.device, 1, &frame.render_fen));
  frame.delqueue.flush();

  // Acquire swapchain image
  u32 swapchain_image_idx;
  VkResult res =
    vkAcquireNextImageKHR(_impl->device.device, _impl->swapchain.swapchain, 1000000000,
                          frame.swapchain_sem, nullptr, &swapchain_image_idx);
  ka_assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);

  // Initialize command buffer
  VK_ASSERT(vkResetCommandBuffer(frame.cmdbuf, 0));
  const auto cmd_begin_info = vkmk_cmdbuf_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_ASSERT(vkBeginCommandBuffer(frame.cmdbuf, &cmd_begin_info));

  // transition our main draw image into general layout so we can write into it
  // we will overwrite it all so we dont care about what was the older layout
  vk_transition_image(frame.cmdbuf, _impl->draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_GENERAL);

  // clear the thing
  draw_background(*_impl, frame.cmdbuf);

  // transition the draw image and the swapchain image into their correct transfer layouts
  vk_transition_image(frame.cmdbuf, _impl->draw_image.image, VK_IMAGE_LAYOUT_GENERAL,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vk_transition_image(frame.cmdbuf, _impl->swapchain.images[swapchain_image_idx],
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkExtent2D draw_extent;
  draw_extent.width = _impl->draw_image.extent.width;
  draw_extent.height = _impl->draw_image.extent.width;
  // execute a copy from the draw image into the swapchain
  vk_transfer_image(frame.cmdbuf, _impl->draw_image.image,
                    _impl->swapchain.images[swapchain_image_idx], draw_extent,
                    _impl->swapchain.extent);

  // make the swapchain image into presentable mode
  vk_transition_image(frame.cmdbuf, _impl->swapchain.images[swapchain_image_idx],
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
  presentInfo.pSwapchains = &_impl->swapchain.swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &frame.render_sem;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchain_image_idx;

  res = vkQueuePresentKHR(present_queue, &presentInfo);
  ka_assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);

  ++_impl->curr_frame;
}

} // namespace kappa::render
