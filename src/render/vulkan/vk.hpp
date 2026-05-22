#pragma once

#include "../../util/array.hpp"
#include "../../util/buffer.hpp"
#include "../../util/expected.hpp"
#include "../../util/function.hpp"
#include "../../util/ptr.hpp"

#include <vulkan/vulkan_core.h>

#ifdef KA_VK_ENABLE_IMGUI
#include <imgui_impl_vulkan.h>
#endif

#include <type_traits>

namespace kappa::render {

fn vk_error_string(VkResult res) noexcept -> const char*;

class VkError : public std::exception {
public:
  VkError() noexcept : _err(VK_SUCCESS) {}

  VkError(VkResult err) noexcept : _err(err) {}

public:
  fn what() const noexcept -> const char* override { return vk_error_string(_err); }

  fn code() const noexcept -> VkResult { return _err; }

private:
  VkResult _err;
};

class VkMsgError : public std::exception {
public:
  VkMsgError(const char* msg, VkResult err) noexcept : _msg(msg), _err(err) {}

public:
  fn what() const noexcept -> const char* override { return _msg; }

  fn code() const noexcept -> VkResult { return _err; }

private:
  const char* _msg;
  VkResult _err;
};

template<typename T>
using VkExpect = Expected<T, VkError>;

template<typename T>
using VkMsgExpect = Expected<T, VkMsgError>;

class VkContext_Impl;

enum class VkLifetime {
  manual = 0,
  defer_ctx,
  defer_frame,
};

fn vk_create_shader(VkContext_Impl& vk, VkShaderStageFlags stage, Span<const u8> src)
  -> VkExpect<VkShaderModule>;
fn vk_destroy_shader(VkContext_Impl& vk, VkShaderModule shader) -> void;

class VkDescLayoutBuilder {
public:
  VkDescLayoutBuilder();

public:
  fn build(VkContext_Impl& vk, VkShaderStageFlags stages, void* pNext = nullptr,
           VkDescriptorSetLayoutCreateFlags flags = 0) -> VkExpect<VkDescriptorSetLayout>;
  fn clear() -> void;

public:
  fn add_binding(u32 binding, VkDescriptorType type) -> VkDescLayoutBuilder&;

private:
  Vec<VkDescriptorSetLayoutBinding> _bindings;
};

fn vk_destroy_desc_layout(VkContext_Impl& vk, VkDescriptorSetLayout layout) -> void;

class VkPipelineLayoutBuilder {
public:
  VkPipelineLayoutBuilder();

public:
  fn build(VkContext_Impl& vk, VkPipelineLayoutCreateFlags flags) -> VkExpect<VkPipelineLayout>;
  fn clear() -> void;

public:
  fn add_push_range(VkShaderStageFlags flags, u32 size, u32 offset) -> VkPipelineLayoutBuilder&;
  fn add_layout(VkDescriptorSetLayout layout) -> VkPipelineLayoutBuilder&;

private:
  Vec<VkDescriptorSetLayout> _set_layouts;
  Vec<VkPushConstantRange> _con_range;
};

fn vk_destroy_pipeline_layout(VkContext_Impl& vk, VkPipelineLayout layout) -> void;

class VkGfxPipelineBuilder {
public:
  VkGfxPipelineBuilder();

public:
  fn build(VkContext_Impl& ctx) -> VkExpect<VkPipeline>;
  fn clear() -> void;

public:
  fn set_layout(VkPipelineLayout layout) -> VkGfxPipelineBuilder&;
  fn add_module(VkShaderStageFlags stage, VkShaderModule shader) -> VkGfxPipelineBuilder&;
  fn set_topology(VkPrimitiveTopology topology) -> VkGfxPipelineBuilder&;
  fn set_poly_mode(VkPolygonMode mode, f32 width = 1.f) -> VkGfxPipelineBuilder&;
  fn set_cull_mode(VkCullModeFlags mode, VkFrontFace front_face) -> VkGfxPipelineBuilder&;
  fn set_color_format(VkFormat format) -> VkGfxPipelineBuilder&;
  fn set_depth_format(VkFormat format) -> VkGfxPipelineBuilder&;
  fn disable_multisampling() -> VkGfxPipelineBuilder&;
  fn disable_blending() -> VkGfxPipelineBuilder&;
  fn disable_depth_test() -> VkGfxPipelineBuilder&;

private:
  std::array<VkPipelineShaderStageCreateInfo, 5> _shader_stages;
  VkPipelineInputAssemblyStateCreateInfo _input_assembly;
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  VkPipelineColorBlendAttachmentState _blend_attachment;
  VkPipelineMultisampleStateCreateInfo _multisampling;
  VkPipelineLayout _layout;
  VkPipelineDepthStencilStateCreateInfo _depth_stencil;
  VkPipelineRenderingCreateInfo _rendering_info;
  VkFormat _color_format;
};

fn vk_create_compute_pipeline(VkContext_Impl& vk, VkPipelineLayout layout, VkShaderModule shader)
  -> VkExpect<VkPipeline>;
fn vk_destroy_pipeline(VkContext_Impl& vk, VkPipeline pip) -> void;

struct VkBufferArgs {
  size_t size;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags mem_usage;
};

struct VkAllocBuff_Impl;

class VkAllocBuff {
private:
  struct create_t {};

public:
  using opaque_type = TypeBuffer<VkAllocBuff_Impl, 72, 8>;

public:
  VkAllocBuff(create_t, VkAllocBuff_Impl&& data);

  static fn create(VkContext_Impl& ctx, const VkBufferArgs& args);
  fn destroy(VkContext_Impl& ctx) -> void;

public:
  fn mapped_data() -> void*;
  fn size() -> usize;
  fn addr() -> VkDeviceAddress;
  fn buffer() -> VkBuffer;

public:
  operator VkAllocBuff_Impl&() { return *_data.get(); }

  operator const VkAllocBuff_Impl&() const { return *_data.get(); }

private:
  opaque_type _data;
};

struct VkImageArgs {
  VkExtent3D extent;
  VkFormat format;
};

struct VkAllocImage_Impl;

class VkAllocImage {
private:
  struct create_t {};

public:
  using opaque_type = TypeBuffer<VkAllocImage_Impl, 40, 8>;

public:
  VkAllocImage(create_t, VkAllocImage_Impl&& data);

  static fn create(VkContext_Impl& ctx, const VkImageArgs& args);
  fn destroy(VkContext_Impl& ctx) -> void;

public:
  fn extent() const -> VkExtent2D;
  fn format() const -> VkFormat;
  fn view() const -> VkImageView;
  fn image() const -> VkImage;

public:
  operator VkAllocImage_Impl&() { return *_data.get(); }

  operator const VkAllocImage_Impl&() const { return *_data.get(); }

private:
  opaque_type _data;
};

struct VkPushConstant {
  const void* data;
  u32 size;
  u32 offset;
  VkShaderStageFlags stage;
};

struct VkIndexBinding {
  Ref<const VkAllocBuff> buffer;
  VkDeviceSize offset;
  VkIndexType index_type;
};

struct VkGfxCmd {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  Span<const VkPushConstant> push_constants;
  Optional<VkIndexBinding> index_binding;
  u32 instance_count;
  u32 first_instance;
  u32 draw_count;
  u32 first_item;
  s32 vertex_offset;
};

struct VkGfxData {
  Optional<VkViewport> viewport;
  Optional<VkRect2D> scissor;
  Optional<VkClearColorValue> clear_color;
  Span<const VkGfxCmd> cmds;
};

struct VkComputeData {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  VkPushConstant push_constant;
  Span<const VkAllocImage> work_images;
  u32 group_count_x, group_count_y, group_count_z;
};

struct VkSurfaceArgs {
public:
  using UpdateSurfExtFn = TrivFn<void(VkExtent2D*), 2 * sizeof(void*), 8>;
  using CreateSurfFn = TrivFn<VkResult(VkInstance, VkSurfaceKHR*, const VkAllocationCallbacks*),
                              2 * sizeof(void*), 8>;

public:
  Span<const char*> extensions;
  CreateSurfFn create;
  UpdateSurfExtFn update_extent;
  VkExtent2D swapchain_extent;
};

struct VkContextArgs {
  Optional<VkSurfaceArgs> surface;
  const char* app_name;
  u32 app_ver;
};

class VkContext {
private:
  struct create_t {};

  using UserGfxFn = FnRef<void(VkCommandBuffer)>;

public:
  struct Deleter {
    void operator()(VkContext_Impl* ctx) noexcept { VkContext::destroy(ctx); }
  };

  static fn destroy(VkContext_Impl* ctx) noexcept -> void;

public:
  VkContext(create_t, VkContext_Impl* ctx) noexcept : _ctx(ctx) {}

  static fn create(const VkContextArgs& args) -> VkMsgExpect<VkContext>;

public:
  fn rebuild_swapchain(VkExtent2D ext) -> VkExpect<void>;

  fn new_frame() -> VkExpect<void>;
  fn end_frame() -> VkExpect<void>;

  fn record_gfx(const VkGfxData& data) -> VkExpect<void>;
  fn record_compute(const VkComputeData& data) -> VkExpect<void>;

  template<typename Fn>
  requires(std::invocable<std::remove_cvref_t<Fn>, VkCommandBuffer>)
  fn record_gfx_immediate(Fn&& func) -> VkExpect<void> {
    return _record_gfx_immediate(UserGfxFn{func});
  }

private:
  fn _record_gfx_immediate(UserGfxFn func) -> VkExpect<void>;

public:
  operator VkContext_Impl&() { return *_ctx.get(); }

private:
  std::unique_ptr<VkContext_Impl, Deleter> _ctx;
};

#ifdef KA_VK_ENABLE_IMGUI
struct VkImGuiInfo {
  VkInstance vk;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkFormat swapchain_format;
  VkDescriptorPool descpool;
};

fn vk_create_imgui_info(VkContext_Impl& vk, VkImGuiInfo& info,
                        const VkDescriptorPoolCreateInfo& pool_info) -> VkResult;

template<typename F>
fn vk_init_imgui(VkContext_Impl& vk, F&& imgui_initer) -> VkExpect<void> {
  static constexpr auto pool_sizes =
    std::to_array<VkDescriptorPoolSize>({{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}});

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (u32)pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();
  VkImGuiInfo imgui_info;
  if (auto ret = vk_create_imgui_info(vk, imgui_info, pool_info); ret != VK_SUCCESS) {
    return {unexpect, ret};
  }

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = imgui_info.vk;
  init_info.PhysicalDevice = imgui_info.physical_device;
  init_info.Device = imgui_info.device;
  init_info.Queue = imgui_info.queue;
  init_info.DescriptorPool = imgui_info.descpool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;

  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
    &imgui_info.swapchain_format;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  imgui_initer();
  ImGui_ImplVulkan_Init(&init_info);

  return {};
}

struct VkImGuiDrawData {
  ImDrawData* draw_data;
  VkPipeline pipeline;
};

inline fn vk_draw_imgui(void* user, VkCommandBuffer cmd) {
  const auto& imgui_data = *static_cast<VkImGuiDrawData*>(user);
  ImGui_ImplVulkan_RenderDrawData(imgui_data.draw_data, cmd, imgui_data.pipeline);
}

inline fn vk_shutown_imgui() -> void {
  ImGui_ImplVulkan_Shutdown();
}
#endif

} // namespace kappa::render
