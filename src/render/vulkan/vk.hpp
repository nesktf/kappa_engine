#pragma once

#include "../../util/array.hpp"
#include "../../util/buffer.hpp"
#include "../../util/expected.hpp"
#include "../../util/function.hpp"
#include "../../util/ptr.hpp"

#include <vulkan/vulkan_core.h>

#define KA_VK_ENABLE_IMGUI
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

struct VkContext_Impl;

enum class VkLifetime {
  manual = 0,
  defer_ctx,
  defer_frame,
};

fn vk_create_shader(VkContext_Impl& vk, Span<const u8> src) -> VkExpect<VkShaderModule>;
fn vk_destroy_shader(VkContext_Impl& vk, VkShaderModule shader) noexcept -> void;

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

fn vk_destroy_desc_layout(VkContext_Impl& vk, VkDescriptorSetLayout layout) noexcept -> void;

class VkPipelineLayoutBuilder {
public:
  VkPipelineLayoutBuilder();

public:
  fn build(VkContext_Impl& vk, VkPipelineLayoutCreateFlags flags = 0)
    -> VkExpect<VkPipelineLayout>;
  fn clear() -> void;

public:
  fn add_push_range(VkShaderStageFlags flags, u32 size, u32 offset) -> VkPipelineLayoutBuilder&;
  fn add_layout(VkDescriptorSetLayout layout) -> VkPipelineLayoutBuilder&;

private:
  Vec<VkDescriptorSetLayout> _set_layouts;
  Vec<VkPushConstantRange> _con_range;
};

fn vk_destroy_pipeline_layout(VkContext_Impl& vk, VkPipelineLayout layout) noexcept -> void;

class VkGfxPipelineBuilder {
public:
  struct ShaderEntry {
    VkShaderModule shader;
    const char* entrypoint;
  };

  enum ShaderStages {
    STAGE_VERTEX = 0,
    STAGE_FRAGMENT,
    STAGE_GEOMETRY,
    STAGE_TESS_EVAL,
    STAGE_TESS_CTRL,
    STAGE_COUNT,
  };

public:
  VkGfxPipelineBuilder();

public:
  fn build(VkContext_Impl& ctx) -> VkExpect<VkPipeline>;
  fn clear() -> void;

public:
  fn set_layout(VkPipelineLayout layout) -> VkGfxPipelineBuilder&;
  fn add_module(VkShaderStageFlagBits stage, VkShaderModule shader,
                const char* entrypoint = nullptr) -> VkGfxPipelineBuilder&;
  fn set_topology(VkPrimitiveTopology topology) -> VkGfxPipelineBuilder&;
  fn set_poly_mode(VkPolygonMode mode, f32 width = 1.f) -> VkGfxPipelineBuilder&;
  fn set_cull_mode(VkCullModeFlags mode, VkFrontFace front_face) -> VkGfxPipelineBuilder&;
  fn set_color_format(VkFormat format) -> VkGfxPipelineBuilder&;
  fn set_depth_format(VkFormat format) -> VkGfxPipelineBuilder&;
  fn disable_multisampling() -> VkGfxPipelineBuilder&;
  fn disable_blending() -> VkGfxPipelineBuilder&;
  fn disable_depth_test() -> VkGfxPipelineBuilder&;

private:
  std::array<ShaderEntry, STAGE_COUNT> _shader_stages;
  VkPipelineInputAssemblyStateCreateInfo _input_assembly;
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  VkPipelineColorBlendAttachmentState _blend_attachment;
  VkPipelineMultisampleStateCreateInfo _multisampling;
  VkPipelineLayout _layout;
  VkPipelineDepthStencilStateCreateInfo _depth_stencil;
  VkPipelineRenderingCreateInfo _rendering_info;
  VkFormat _color_format;
};

fn vk_create_compute_pipeline(VkContext_Impl& vk, VkPipelineLayout layout, VkShaderModule shader,
                              const char* entrypoint = nullptr) -> VkExpect<VkPipeline>;
fn vk_destroy_pipeline(VkContext_Impl& vk, VkPipeline pip) noexcept -> void;

// Same as VmaMemoryUsage
enum VkMemoryUsage {
  KA_VK_MEM_USAGE_GPU_ONLY = 1,
  KA_VK_MEM_USAGE_CPU_ONLY = 2,
  KA_VK_MEM_USAGE_CPU_TO_GPU = 3,
  KA_VK_MEM_USAGE_GPU_TO_CPU = 4,
  KA_VK_MEM_USAGE_CPU_COPY = 5,
};

struct VkBufferArgs {
  size_t size;
  VkBufferUsageFlags usage;
  VkMemoryUsage mem_usage;
};

struct VkAllocBuff_Impl;

class VkAllocBuff {
private:
  struct create_t {};

public:
  using opaque_type = TypeBuffer<VkAllocBuff_Impl, 72, 8>;

public:
  VkAllocBuff(create_t, VkAllocBuff_Impl&& data);

public:
  static fn allocate(VkContext_Impl& ctx, const VkBufferArgs& args) -> VkExpect<VkAllocBuff>;

public:
  fn mapped_data() -> void*;
  fn size() -> VkDeviceSize;
  fn addr(VkContext_Impl& vk) -> VkDeviceAddress;
  fn buffer() -> VkBuffer;

public:
  operator VkAllocBuff_Impl&() { return *_data.get(); }

  operator const VkAllocBuff_Impl&() const { return *_data.get(); }

private:
  opaque_type _data;
};

fn vk_dealloc_buffer(VkContext_Impl& vk, VkAllocBuff_Impl& buff) noexcept -> void;

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

public:
  static fn allocate(VkContext_Impl& ctx, const VkImageArgs& args) -> VkExpect<VkAllocImage>;

public:
  fn extent() const -> VkExtent3D;
  fn format() const -> VkFormat;
  fn view() const -> VkImageView;
  fn image() const -> VkImage;

public:
  operator VkAllocImage_Impl&() { return *_data.get(); }

  operator const VkAllocImage_Impl&() const { return *_data.get(); }

private:
  opaque_type _data;
};

fn vk_dealloc_image(VkContext_Impl& vk, VkAllocImage_Impl& image) noexcept -> void;

struct VkSurfaceArgs {
public:
  using UpdateSurfExtFn = TrivFn<VkExtent2D(), 2 * sizeof(void*), 8>;
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

fn vk_rebuild_swapchain(VkContext_Impl& vk, VkExtent2D extent) -> VkExpect<void>;

class VkContext {
public:
  struct FrameContext {
    VkCommandBuffer cmd;
    VkImage swapchain_image;
    VkImageView swapchain_view;
    VkExtent2D swapchain_extent;
  };

  struct Deleter {
    void operator()(VkContext_Impl* ctx) noexcept { VkContext::destroy(ctx); }
  };

private:
  struct create_t {};

  using UserCmdFn = FnRef<void(const FrameContext&)>;
  using UserImFn = FnRef<void(VkCommandBuffer)>;

public:
  VkContext(create_t, VkContext_Impl* ctx) noexcept : _vk(ctx) {}

  static fn create(const VkContextArgs& args) -> VkMsgExpect<VkContext>;
  static fn destroy(VkContext_Impl* ctx) noexcept -> void;

public:
  fn rebuild_swapchain(VkExtent2D extent) -> VkExpect<void> {
    return vk_rebuild_swapchain(*_vk.get(), extent);
  }

  fn new_frame() -> VkExpect<void>;
  fn end_frame() -> VkExpect<void>;

  fn device_wait() -> void;

  fn alloc_desc(VkDescriptorSetLayout layout) -> VkExpect<VkDescriptorSet>;
  fn update_sets(Span<const VkWriteDescriptorSet> writes, Span<const VkCopyDescriptorSet> copies)
    -> void;

  template<typename Fn>
  requires(std::invocable<std::remove_cvref_t<Fn>, FrameContext>)
  fn submit_cmd(Fn&& func) -> VkExpect<void> {
    return _submit_cmd(UserCmdFn{func});
  }

  template<typename Fn>
  requires(std::invocable<std::remove_cvref_t<Fn>, VkCommandBuffer>)
  fn submit_immediate(Fn&& func) -> VkExpect<void> {
    return _submit_immediate(UserImFn{func});
  }

private:
  fn _submit_immediate(UserImFn func) -> VkExpect<void>;
  fn _submit_cmd(UserCmdFn func) -> VkExpect<void>;

public:
  VkContext_Impl& get() { return *_vk.get(); }

  operator VkContext_Impl&() { return *_vk.get(); }

private:
  std::unique_ptr<VkContext_Impl, Deleter> _vk;
};

fn vkmk_image_subresource_range(VkImageAspectFlags mask) -> VkImageSubresourceRange;

fn vkmk_attach_info(VkImageView view, VkClearValue* clear, VkImageLayout layout)
  -> VkRenderingAttachmentInfo;

fn vkmk_render_info(VkExtent2D render_extent, const VkRenderingAttachmentInfo* color_attachment,
                    const VkRenderingAttachmentInfo* depth_attachment) -> VkRenderingInfo;

fn vkcmd_transition_image(VkCommandBuffer cmd, VkImage img, VkImageLayout curr_layout,
                          VkImageLayout new_layout) -> VkImageLayout;

fn vkcmd_transfer_image(VkCommandBuffer cmdbuf, VkImage src, VkImage dst, VkExtent2D src_ext,
                        VkExtent2D dst_ext) -> void;

#ifdef KA_VK_ENABLE_IMGUI
struct VkImGuiInfo {
  VkInstance vk;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkFormat swapchain_format;
  VkDescriptorPool descpool;
};

fn vkmk_imgui_info(VkContext_Impl& vk, VkImGuiInfo& info,
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
  if (auto ret = vkmk_imgui_info(vk, imgui_info, pool_info); ret != VK_SUCCESS) {
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

inline fn vk_draw_imgui(VkCommandBuffer cmd, ImDrawData* draw_data,
                        VkPipeline pipeline = VK_NULL_HANDLE) {
  ImGui_ImplVulkan_RenderDrawData(draw_data, cmd, pipeline);
}

inline fn vk_shutdown_imgui() -> void {
  ImGui_ImplVulkan_Shutdown();
}
#endif

} // namespace kappa::render
