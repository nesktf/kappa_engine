#include "./vk_swapchain.hpp"
#include "./vk_util.hpp"
#include "render/vulkan/vk_private.hpp"

#include <algorithm>

namespace kappa::render {

VulkanSwapchain::VulkanSwapchain(create_t, VkSwapchainKHR swapchain, VkFormat format,
                                 VkExtent2D extent, UniqueArray<VkImage>&& images,
                                 UniqueArray<VkImageView>&& image_views) :
    _swapchain(swapchain), _format(format), _extent(extent), _images(std::move(images)),
    _image_views(std::move(image_views)) {
  ka_assert(_swapchain != VK_NULL_HANDLE);
  ka_assert(!_images.empty());
  ka_assert(!_image_views.empty());
}

fn VulkanSwapchain::create(const VulkanSwapchainArgs& args, VkSwapchainKHR old_swapchain)
  -> VkExpect<VulkanSwapchain> {
  ka_assert(args.device != VK_NULL_HANDLE);
  ka_assert(args.physical_device != VK_NULL_HANDLE);
  ka_assert(args.surface != VK_NULL_HANDLE);
  ka_assert(!args.surface_present_modes.empty());
  ka_assert(!args.surface_formats.empty());

  // How are the images in the swapchain presented?
  const fn find_present_mode = [&]() -> VkPresentModeKHR {
    // VK_PRESENT_MODE_IMMEDIATE_KHR -> Images submited are transferred to the screen immediately
    // VK_PRESENT_MODE_FIFO_KHR -> The swap chain is a FIFO queue of images (double buffering)
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR -> Doesn't wait for next vblank if the queue was empty
    // VK_PRESENT_MODE_MAILBOX_KHR -> Triple buffering

    for (const auto& mode : args.surface_present_modes) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
    }

    return VK_PRESENT_MODE_FIFO_KHR; // Only one guaranteed to be available
  };

  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(args.physical_device, args.surface, &capabilities);

  // What is the format of the swapchain images?
  const auto swapchain_format = [&]() -> VkSurfaceFormatKHR {
    // Try to get a swap surface with B8G8R8A8 pixel format or just use the first one
    // if there is not one available
    for (const auto& format : args.surface_formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
          format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return format;
      }
    }
    return args.surface_formats[0];
  }();
  const auto image_format = swapchain_format.format;

  // Resolution of the swapchain images (in pixels), usually the resolution of the window
  const auto swapchain_extent = [&]() -> VkExtent2D {
    if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
      return capabilities.currentExtent;
    }

    VkExtent2D actual_extent = args.surface_extent;
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

  auto create_info = vkmk_zero<VkSwapchainCreateInfoKHR>();
  create_info.surface = args.surface;
  create_info.imageFormat = image_format;
  create_info.minImageCount = image_count;
  create_info.imageColorSpace = swapchain_format.colorSpace;
  create_info.imageExtent = swapchain_extent;
  create_info.imageArrayLayers = 1; // Layers for each image, 1 if not using stereoscopic 3D

  // Sets the operations for the swapchain, in this case we only render directly
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  // create_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT; // For postprocessing

  const u32 queue_indices[] = {args.graphics_queue, args.present_queue};
  // imageSharingMode only refers to ownership transfer needs
  if (args.graphics_queue != args.present_queue) {
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

  VkSwapchainKHR swapchain;
  if (auto res = vkCreateSwapchainKHR(args.device, &create_info, vkalloc, &swapchain);
      res != VK_SUCCESS) {
    return {unexpect, res};
  }

  vkGetSwapchainImagesKHR(args.device, swapchain, &image_count, nullptr);
  ka_assert(image_count > 0);
  auto images = make_array<VkImage>(uninitialized, image_count);
  auto image_views = make_array<VkImageView>(uninitialized, image_count);

  vkGetSwapchainImagesKHR(args.device, swapchain, &image_count, images.data());
  for (usize i = 0; i < images.size(); ++i) {
    const auto create_info =
      vkmk_imageview_info(image_format, images[i], VK_IMAGE_ASPECT_COLOR_BIT);
    KA_VK_ASSERT(vkCreateImageView(args.device, &create_info, vkalloc, &image_views[i]));
  }
  KA_VK_LOG(debug, "Creating swapchain: {}x{}", swapchain_extent.width, swapchain_extent.height);

  return {in_place,
          create_t(),
          swapchain,
          image_format,
          swapchain_extent,
          std::move(images),
          std::move(image_views)};
}

fn VulkanSwapchain::destroy(VkDevice device) -> void {
  for (usize i = 0; i < _images.size(); ++i) {
    vkDestroyImageView(device, _image_views[i], vkalloc);
  }
  vkDestroySwapchainKHR(device, _swapchain, vkalloc);
  _images.reset();
  _image_views.reset();
}

struct VulkanFrameData::TempFrameData {
  VkCommandPool cmdpool;
  VkCommandBuffer cmdbuf;
  VkFence render_fen;
  VkSemaphore swapchain_sem, render_sem;
  Optional<VulkanDescPool> pool;
};

// NOTE: Be prepared for an unholy mess of manual construction and destruction
VulkanFrameData::VulkanFrameData(create_t, TempFrameData* frames, VkDevice device) :
    _device(device), _curr_frame(0) {
  ka_assert(frames);
  auto* self_frames = _frames.as<FrameData>();
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    ka_assert(frames[i].cmdpool != VK_NULL_HANDLE);
    ka_assert(frames[i].cmdbuf != VK_NULL_HANDLE);
    ka_assert(frames[i].render_fen != VK_NULL_HANDLE);
    ka_assert(frames[i].swapchain_sem != VK_NULL_HANDLE);
    ka_assert(frames[i].render_sem != VK_NULL_HANDLE);
    ka_assert(frames[i].pool.has_value());
    new (self_frames + i)
      FrameData(frames[i].cmdpool, frames[i].cmdbuf, frames[i].render_fen, frames[i].swapchain_sem,
                frames[i].render_sem, *std::move(frames[i].pool), VulkanDelQueue());
  }
  static_assert(std::is_trivially_destructible_v<TempFrameData>);
}

VulkanFrameData::VulkanFrameData(VulkanFrameData&& other) noexcept :
    _device(other._device), _curr_frame(other._curr_frame) {
  auto* self_frames = _frames.as<FrameData>();
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    new (self_frames + i) FrameData(std::move(other._frames.as<FrameData>()[i]));
  }
}

VulkanFrameData& VulkanFrameData::operator=(VulkanFrameData&& other) noexcept {
  _device = other._device;
  _curr_frame = other._curr_frame;
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    _frames.as<FrameData>()[i] = std::move(other._frames.as<FrameData>()[i]);
  }
  return *this;
}

VulkanFrameData::~VulkanFrameData() {
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    std::destroy_at(_frames.as<FrameData>() + i);
  }
}

fn VulkanFrameData::create(VkDevice device, u32 graphics_queue,
                           const VulkanDescPoolArgs& descpool_args) -> VkExpect<VulkanFrameData> {

  TempFrameData vkdata[MAX_FRAMES_IN_FLIGHT]{};
  s32 curr_frame = 0;

  DeferFn on_error = [&]() {
    for (s32 frame = curr_frame; frame >= 0; --frame) {
      auto& data = vkdata[frame];
      if (data.cmdpool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, data.cmdpool, vkalloc);
      }
      if (data.render_fen != VK_NULL_HANDLE) {
        vkDestroyFence(device, data.render_fen, vkalloc);
      }
      if (data.render_sem != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, data.render_sem, vkalloc);
      }
      if (data.swapchain_sem != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, data.swapchain_sem, vkalloc);
      }
      if (data.pool) {
        vkDestroyDescriptorPool(device, data.pool->pool_handle(), vkalloc);
      }
    }
  };

  // create a command pool for commands submitted to the graphics queue.
  // we also want the pool to allow for resetting of individual command buffers
  const auto cmdpool_info =
    vkmk_cmdpool_info(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphics_queue);

  // one fence to control when the gpu has finished rendering the frame,
  // and 2 semaphores to syncronize rendering with swapchain
  // we want the fence to start signalled so we can wait on it on the first frame
  const auto fence_create_info = vkmk_fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
  const auto semaphore_create_info = vkmk_semaphore_info(0);

  for (; curr_frame < (s32)MAX_FRAMES_IN_FLIGHT; ++curr_frame) {
    auto& data = vkdata[curr_frame];
    KA_VK_UNEX(vkCreateCommandPool(device, &cmdpool_info, vkalloc, &data.cmdpool));

    const auto cmd_alloc_info =
      vkmk_cmdbuf_alloc_info(data.cmdpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    KA_VK_UNEX(vkAllocateCommandBuffers(device, &cmd_alloc_info, &data.cmdbuf));

    KA_VK_UNEX(vkCreateFence(device, &fence_create_info, vkalloc, &data.render_fen));
    KA_VK_UNEX(vkCreateSemaphore(device, &semaphore_create_info, vkalloc, &data.swapchain_sem));
    KA_VK_UNEX(vkCreateSemaphore(device, &semaphore_create_info, vkalloc, &data.render_sem));

    auto pool = VulkanDescPool::create(device, descpool_args);
    if (!pool) {
      return {unexpect, pool.error()};
    }
    data.pool.emplace(std::move(*pool));
  }

  on_error.disengage();
  return {in_place, create_t(), vkdata, device};
}

fn VulkanFrameData::add_to_delqueue(VulkanDelQueue& queue) -> void {
  FrameData* frames = _frames.as<FrameData>();
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    auto& frame = frames[i];
    queue.enqueue(frame.cmdpool, _device);
    queue.enqueue(frame.render_fen, _device);
    queue.enqueue(frame.swapchain_sem, _device);
    queue.enqueue(frame.render_sem, _device);
    frame.pool.add_to_delqueue(queue);
  }
}

fn VulkanFrameData::next_frame() -> FrameData& {
  return _frames.as<FrameData>()[_curr_frame++ % MAX_FRAMES_IN_FLIGHT];
}

fn VulkanFrameData::curr_frame() -> FrameData& {
  return _frames.as<FrameData>()[_curr_frame % MAX_FRAMES_IN_FLIGHT];
}

} // namespace kappa::render
