#include "./vk_device.hpp"
#include "./vk_util.hpp"

#include <unordered_set>

namespace kappa::render {

namespace {

constexpr auto base_device_extensions = std::to_array<const char*>({
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
});

} // namespace

VulkanDevice::VulkanDevice(create_t, VkDevice device, VkPhysicalDevice physical_device,
                           QueueIndices queues, Vec<VkSurfaceFormatKHR>&& surface_formats,
                           Vec<VkPresentModeKHR>&& surface_present_modes) :
    _device(device), _physical_device(physical_device), _queues(queues),
    _surface_formats(std::move(surface_formats)),
    _surface_present_modes(std::move(surface_present_modes)) {
  ka_assert(_device != VK_NULL_HANDLE);
  ka_assert(_physical_device != VK_NULL_HANDLE);
  ka_assert(!_surface_formats.empty());
  ka_assert(!_surface_present_modes.empty());
}

fn VulkanDevice::create(VkInstance vk, VkSurfaceKHR surface) -> VkExpect<VulkanDevice> {
  ka_assert(vk != VK_NULL_HANDLE);
  ka_assert(surface != VK_NULL_HANDLE);

  u32 device_count{};
  VkResult res = vkEnumeratePhysicalDevices(vk, &device_count, nullptr);
  if (device_count == 0) {
    return {unexpect, res};
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
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &caps);

    u32 format_count{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    if (format_count) {
      swapchain_formats.resize(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                           swapchain_formats.data());
    }

    u32 present_mode_count{};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
    if (present_mode_count) {
      swapchain_present_modes.resize(present_mode_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count,
                                                swapchain_present_modes.data());
    }
    bool swapchain_adequate = !swapchain_formats.empty() && !swapchain_present_modes.empty();
    return swapchain_adequate;
  };

  Vec<VkPhysicalDevice> devices;
  devices.resize(device_count);
  vkEnumeratePhysicalDevices(vk, &device_count, devices.data());

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
    return {unexpect, VK_ERROR_DEVICE_LOST};
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

  VkDevice device;
  if (auto ret = vkCreateDevice(physical_device, &create_info, vkalloc, &device);
      ret != VK_SUCCESS) {
    return {unexpect, ret};
  }

  // Fill device struct
  return {in_place,
          create_t(),
          device,
          physical_device,
          QueueIndices(graphics.value(), present.value(), transfer.value()),
          std::move(swapchain_formats),
          std::move(swapchain_present_modes)};
}

fn VulkanDevice::add_to_delqueue(VulkanDelQueue& queue) -> void {
  queue.enqueue(_device);
}

fn VulkanDevice::wait_idle() -> void {
  vkDeviceWaitIdle(_device);
}

} // namespace kappa::render
