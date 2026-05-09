#ifndef KA_VK_H_
#define KA_VK_H_

#include "../../macro.h"

#include <vulkan/vulkan_core.h>

#ifdef __cplusplus
extern "C" {
#endif

KA_DECL_HANDLE(ka_VulkanContext);
KA_DECL_HANDLE(ka_VulkanPipelineBuilder);
KA_DECL_OPAQUE(ka_VulkanImage, 48, 8);
KA_DECL_OPAQUE(ka_VulkanBuffer, 80, 8);
KA_DECL_OPAQUE(ka_VulkanShaderModule, 24, 8);
KA_DECL_OPAQUE(ka_VulkanCompute, 24, 8);
KA_DECL_OPAQUE(ka_VulkanPipeline, 24, 8);

typedef VkResult (*PFN_ka_vk_create_surface)(void* user, VkInstance vk, VkSurfaceKHR* surface,
                                             VkAllocationCallbacks const* vkalloc);

typedef void (*PFN_ka_vk_init_imgui)(void* user);

typedef void (*PFN_ka_vk_immediate_submit_fn)(void* user, VkCommandBuffer cmd);

typedef enum ka_VulkanCommandType {
  KA_VK_COMMAND_INDEXED = 0,
  KA_VK_COMMAND_VERTICES,
} ka_VulkanComandType;

typedef struct ka_VulkanContextArgs {
  VkExtent2D initial_extent;
  const char** surface_extensions;
  size_t surface_extension_count;

  PFN_ka_vk_create_surface create_surface;
  void* create_surface_user;

  PFN_ka_vk_init_imgui init_imgui;
  void* init_imgui_user;

  char const* app_name;
  uint32_t app_ver;
} ka_VulkanContextArgs;

typedef struct ka_VulkanImageArgs {
  VkExtent3D extent;
  VkFormat format;
} ka_VulkanImageArgs;

typedef struct ka_VulkanBufferArgs {
  size_t size;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags mem_usage;
} ka_VulkanBufferArgs;

typedef struct ka_VulkanImSubmitData {
  PFN_ka_vk_immediate_submit_fn submit_callback;
  void* submit_callback_user;
} ka_VulkanImSubmitData;

typedef struct ka_VulkanPushConstant {
  void const* data;
  uint32_t size;
  uint32_t offset;
} ka_VulkanPushConstant;

typedef struct ka_VulkanIndexBinding {
  ka_VulkanBuffer const* buffer;
  VkDeviceSize offset;
  VkIndexType index_type;
} ka_VulkanIndexBinding;

typedef struct ka_VulkanGraphicsCommand {
  ka_VulkanPipeline const* pipeline;
  ka_VulkanCommandType type;
  ka_VulkanIndexBinding index_binding;
  ka_VulkanPushConstant push_constant;
  uint32_t instance_count;
  uint32_t first_instance;
  uint32_t draw_count;
  uint32_t first_item;
  int32_t vertex_offset;
} ka_VulkanGraphicsCommand;

typedef struct ka_VulkanCommandData {
  VkViewport viewport;
  VkRect2D scissor;
  VkClearColorValue const* clear_color;
  ka_VulkanGraphicsCommand const* cmds;
  uint32_t cmd_count;
} ka_VulkanCommandData;

typedef struct ka_VulkanComputeData {
  ka_VulkanPipeline const* pipeline;
  ka_VulkanPushConstant push_constant;
  uint32_t group_count_x;
  uint32_t group_count_y;
  uint32_t group_count_z;
} ka_VulkanComputeData;

KA_C_API const char* ka_vk_error_string(VkResult res);

KA_C_API VkResult ka_vk_allocate_image(ka_VulkanContext vk, ka_VulkanImage* image,
                                       ka_VulkanImageArgs const* args);
KA_C_API void ka_vk_deallocate_image(ka_VulkanImage* image);

KA_C_API void ka_vk_image_extent(ka_VulkanImage const* image, VkExtent3D* extent);
KA_C_API void ka_vk_image_format(ka_VulkanImage const* image, VkFormat* format);
KA_C_API void ka_vk_image_handle(ka_VulkanImage const* image, VkImage* handle);

KA_C_API VkResult ka_vk_allocate_buffer(ka_VulkanContext vk, ka_VulkanBuffer* buffer,
                                        ka_VulkanBufferArgs const* args);
KA_C_API void ka_vk_deallocate_buffer(ka_VulkanBuffer* buffer);

KA_C_API void ka_vk_buffer_mapped_data(ka_VulkanBuffer const* buffer, void** data);
KA_C_API void ka_vk_buffer_size(ka_VulkanBuffer const* buffer, size_t* size);
KA_C_API void ka_vk_buffer_addr(ka_VulkanBuffer const* buffer, VkDeviceAddress* addr);
KA_C_API void ka_vk_buffer_handle(ka_VulkanBuffer const* buffer, VkBuffer* handle);

KA_C_API VkResult ka_vk_create_shader(ka_VulkanContext vk, ka_VulkanShaderModule* shader,
                                      uint8_t const* src, size_t src_size,
                                      VkShaderStageFlags stage);
KA_C_API VkResult ka_vk_destroy_shader(ka_VulkanShaderModule* shader);

KA_C_API VkResult ka_vk_create_pipeline_builder(ka_VulkanPipelineBuilder* builder);
KA_C_API void ka_vk_destroy_pipeline_builder(ka_VulkanPipelineBuilder builder);

KA_C_API void ka_vk_pipb_clear(ka_VulkanPipelineBuilder pipb);
KA_C_API void ka_vk_pipb_set_layout(ka_VulkanPipelineBuilder pipb, VkPipelineLayout layout);
KA_C_API void ka_vk_pipb_add_shader(ka_VulkanPipelineBuilder pipb,
                                    ka_VulkanShaderModule const* shader_module);
KA_C_API void ka_vk_pipb_set_topology(ka_VulkanPipelineBuilder pipb, VkPrimitiveTopology topology);
KA_C_API void ka_vk_pipb_set_poly_mode(ka_VulkanPipelineBuilder pipb, VkPolygonMode mode,
                                       float width);
KA_C_API void ka_vk_pipb_set_cull_mode(ka_VulkanPipelineBuilder pipb, VkCullModeFlags mode,
                                       VkFrontFace front_face);
KA_C_API void ka_vk_pipb_set_color_format(ka_VulkanPipelineBuilder pipb, VkFormat format);
KA_C_API void ka_vk_pipb_set_depth_format(ka_VulkanPipelineBuilder pipb, VkFormat format);
KA_C_API void ka_vk_pipb_disable_ms(ka_VulkanPipelineBuilder pipb);
KA_C_API void ka_vk_pipb_disable_blending(ka_VulkanPipelineBuilder pipb);
KA_C_API void ka_vk_pipb_disable_depth_test(ka_VulkanPipelineBuilder pipb);

KA_C_API VkResult ka_vk_pipb_build(ka_VulkanPipelineBuilder pipb, ka_VulkanContext vk,
                                   ka_VulkanPipeline* pip);

KA_C_API VkResult ka_vk_create_context(ka_VulkanContext* ctx, ka_VulkanContextArgs const* args,
                                       const char** errmsg);
KA_C_API void ka_vk_destroy_context(ka_VulkanContext ctx);

KA_C_API VkResult ka_vk_rebuild_swapchain(ka_VulkanContext vk, VkExtent2D extent);
KA_C_API VkResult ka_vk_immediate_submit(ka_VulkanContext vk, const ka_VulkanImSubmitData* data);

KA_C_API void ka_vk_get_target_image_view(ka_VulkanContext vk, VkImageView* view);

KA_C_API VkResult ka_vk_new_frame(ka_VulkanContext vk);
KA_C_API VkResult ka_vk_record_cmd(ka_VulkanContext vk, ka_VulkanCommandData const* data);
KA_C_API VkResult ka_vk_record_compute(ka_VulkanContext vk, ka_VulkanComputeData const* data);
KA_C_API VkResult ka_vk_end_frame(ka_VulkanContext vk);

#ifdef __cplusplus
}
#endif

#endif // #ifndef KA_VK_H_
