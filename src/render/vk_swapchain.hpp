#pragma once

#include "./vk_device.hpp"

namespace kappa::render {

class WindowSwapchain {
public:
  WindowSwapchain(VkSwapchainKHR swapchain, VkFormat format, VkExtent2D extent,
                  VkRenderPass renderpass, std::vector<VkImage>&& images,
                  std::vector<VkImageView>&& image_views,
                  std::vector<VkFramebuffer>&& framebuffers);

public:
  static fn create(const GraphicsDevice& device, VkView<VkSurfaceKHR> surface, VkExtent2D extent,
                   VkView<VkSwapchainKHR> old_swapchain) -> VkSvExpect<WindowSwapchain>;

  fn rebuild(const GraphicsDevice& device, VkView<VkSurfaceKHR> surface, VkExtent2D extent)
    -> VkSvExpect<void>;
  fn destroy(const GraphicsDevice& device) -> void;

public:
  fn swapchain() const -> VkView<VkSwapchainKHR>;
  fn format() const -> VkFormat;
  fn extent() const -> VkExtent2D;
  fn renderpass() const -> VkView<VkRenderPass>;
  fn images() const -> Span<const VkImage>;
  fn image_views() const -> Span<const VkImageView>;
  fn framebuffers() const -> Span<const VkFramebuffer>;

public:
  operator VkSwapchainKHR() const { return swapchain(); }

  operator VkRenderPass() const { return renderpass(); }

private:
  VkSwapchainKHR _swapchain;
  VkFormat _format;
  VkExtent2D _extent;
  VkRenderPass _renderpass;
  std::vector<VkImage> _images;
  std::vector<VkImageView> _image_views;
  std::vector<VkFramebuffer> _framebuffers;
};

} // namespace kappa::render
