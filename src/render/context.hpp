#pragma once

#include "render/glfw.hpp"
#include "render/vulkan/vk_context.hpp"
#include "render/vulkan/vk_image.hpp"
#include "render/vulkan/vk_pipeline.hpp"
#include "render/vulkan/vk_util.hpp"

#include <ranmath/ran.hpp>

namespace kappa::render {

class IDrawAction;

enum class Image {};

class RenderContext {
private:
  struct create_t {};

public:
  struct DrawTarget {
    VkAllocImage color;
    VkAllocImage depth;
    VkExtent2D extent;
    f32 scale;
  };

  struct FrameData {
    VkDelQueue delqueue;
    VkDynDescAlloc desc_alloc;
  };

  using FrameArray = TypeArrayBuffer<FrameData, MAX_FRAMES_IN_FLIGHT>;

  static constexpr Image DEFAULT_IMAGE = (Image)0;
  static constexpr u32 MAX_IMAGES = 512;

  enum SamplerType {
    SAMPLER_NEAREST = 0,
    SAMPLER_LINEAR,
    SAMPLER_COUNT,
  };

  using SamplerArray = std::array<VkSampler, SAMPLER_COUNT>;

  struct ImageData {
    ImageData(VkAllocImage&& image, SamplerArray&& samplers_) :
        samplers(std::move(samplers_)), images() {
      images.emplace(std::move(image));
    }

    SamplerArray samplers;
    FixedFreelist<VkAllocImage, MAX_IMAGES> images;
  };

public:
  RenderContext(create_t, VkContext&& vk, GLFWImGuiHandler&& glfw_imgui, VkDelQueue&& delqueue,
                VkDynDescAlloc&& desc_alloc, DrawTarget&& target, FrameData* frames,
                VkAllocImage&& default_image, SamplerArray&& samplers);
  ~RenderContext();

public:
  static fn initialize(TypeBufferRef<RenderContext> renderer, GLFWContext& glfw) -> void;

public:
  fn create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags flags, VkImageMipsFlag mips,
                  const void* data = nullptr) -> Image;
  fn destroy_image(Image image) -> void;
  fn submit_image_data(Image image, const void* data) -> void;
  fn get_image(Image image) -> VkAllocImage&;
  fn get_sampler(SamplerType type) -> VkSampler;

public:
  fn draw_things(IDrawAction& on_draw, f64 dt, f64 alpha) -> void;

public:
  fn get_vk() -> VkContext& { return _vk; }

  fn get_delqueue() -> VkDelQueue& { return _delqueue; }

  fn get_desc_alloc() -> VkDynDescAlloc& { return _desc_alloc; }

  fn get_target() -> DrawTarget& { return _target; }

  fn get_frame() -> FrameData& { return _frames[_frame_count % MAX_FRAMES_IN_FLIGHT]; }

private:
  VkContext _vk;
  GLFWImGuiHandler _glfw_imgui;
  VkDelQueue _delqueue;
  VkDynDescAlloc _desc_alloc;
  DrawTarget _target;
  ImageData _images;
  FrameArray _frames;
  u32 _frame_count;
};

} // namespace kappa::render
