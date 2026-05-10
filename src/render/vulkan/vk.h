#ifndef KA_VK_H_
#define KA_VK_H_

#include "../../macro.h"

#include <vulkan/vulkan_core.h>

#define KA_VK_ENABLE_IMGUI

#define KA_VK_DEFAULT_TARGET   VK_NULL_HANDLE
#define KA_VK_SWAPCHAIN_TARGET ((ka_VkTarget)0xDEADBEEF)

#ifdef __cplusplus
#define KA_VK_CONSTEXPR constexpr
#else
#define KA_VK_CONSTEXPR static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

KA_DECL_HANDLE(ka_VkContext);
KA_DECL_HANDLE(ka_VkTarget);
KA_DECL_OPAQUE(ka_VkImage, 40, 8);
KA_DECL_OPAQUE(ka_VkBuffer, 72, 8);

typedef VkResult (*PFN_ka_vk_create_surface)(void* user, VkInstance vk, VkSurfaceKHR* surface,
                                             VkAllocationCallbacks const* vkalloc);
typedef void (*PFN_ka_vk_update_surface_extent)(void* user, VkExtent2D* extent);
typedef void (*PFN_ka_vk_user_cmd)(void* user, VkCommandBuffer cmd);

typedef enum ka_VkGfxCmdType {
  KA_VK_GFX_CMD_INDEXED = 0,
  KA_VK_GFX_CMD_VERTICES,
} ka_VkComandType;

typedef enum ka_VkLifetime {
  KA_VK_LIFETIME_MANUAL = 0,
  KA_VK_LIFETIME_AUTO,
  KA_VK_LIFETIME_AUTO_FRAME,
} ka_VkLifetime;

typedef enum ka_VkPipelineModulePos {
  KA_VK_MODULE_VERTEX = 0,
  KA_VK_MODULE_FRAGMENT,
  KA_VK_MODULE_GEOMETRY,
  KA_VK_MODULE_TESS_CTRL,
  KA_VK_MODULE_TESS_EVAL,
  KA_VK_MODULE_COUNT,
} ka_VkPipelineModulePos;

typedef struct ka_VkSurfaceArgs {
  const char** extensions;
  PFN_ka_vk_create_surface create_surface;
  void* create_surface_user;
  PFN_ka_vk_update_surface_extent update_extent;
  void* update_extent_user;
  VkExtent2D swapchain_extent;
  uint32_t extension_count;
} ka_VkSurfaceArgs;

typedef struct ka_VkContextArgs {
  ka_VkSurfaceArgs const* surface;
  char const* app_name;
  uint32_t app_ver;
} ka_VkContextArgs;

typedef struct ka_VkImageArgs {
  VkExtent3D extent;
  VkFormat format;
} ka_VkImageArgs;

typedef struct ka_VkBufferArgs {
  size_t size;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags mem_usage;
} ka_VkBufferArgs;

typedef struct ka_VkShaderArgs {
  uint8_t const* src;
  size_t src_size;
  VkShaderStageFlags stage;
  ka_VkLifetime lifetime;
} ka_VkShaderArgs;

typedef struct ka_VkPipelineGfxArgs {
  VkPipelineLayout layout;
  VkShaderModule shader_modules[KA_VK_MODULE_COUNT];
  VkPrimitiveTopology topology;
  VkPolygonMode poly_mode;
  float poly_width;
  VkCullModeFlags cull_mode;
  VkFrontFace cull_front_face;
  VkFormat color_format;
  VkFormat depth_format;
} ka_VkPipelineGfxArgs;

typedef struct ka_VkPipelineCmpArgs {
  VkPipelineLayout layout;
  VkShaderModule shader;
  ka_VkLifetime lifetime;
} ka_VkPipelineCmpArgs;

typedef struct ka_VkPipelineLayoutArgs {
  VkPipelineLayoutCreateFlags flags;
  VkDescriptorSetLayout const* set_layouts;
  VkPushConstantRange const* push_constant_ranges;
  uint32_t push_constant_range_count;
  uint32_t set_layout_count;
  uint32_t push_constant_count;
  ka_VkLifetime lifetime;
} ka_VkPipelineLayoutArgs;

typedef struct ka_VkDscSetLayoutArgs {
  VkDescriptorSetLayoutBinding const* bindings;
  uint32_t binding_count;
  VkDescriptorSetLayoutCreateFlags flags;
  ka_VkLifetime lifetime;
} ka_VkDscSetLayoutInfo;

typedef struct ka_VkPushConstant {
  void const* data;
  uint32_t size;
  uint32_t offset;
  VkShaderStageFlags stage;
} ka_VkPushConstant;

typedef struct ka_VkIndexBinding {
  ka_VkBuffer const* buffer;
  VkDeviceSize offset;
  VkIndexType index_type;
} ka_VkIndexBinding;

typedef struct ka_VkGfxCmd {
  ka_VkIndexBinding index_binding;
  VkPipeline pipeline;
  VkPipelineLayout layout;
  ka_VkPushConstant const* push_constants;
  ka_VkGfxCmdType type;
  uint32_t push_constant_count;
  uint32_t instance_count;
  uint32_t first_instance;
  uint32_t draw_count;
  uint32_t first_item;
  int32_t vertex_offset;
} ka_VkGfxCmd;

typedef struct ka_VkGfxCmdData {
  VkViewport const* viewport;
  VkRect2D const* scissor;
  VkClearColorValue const* clear_color;
  ka_VkGfxCmd const* cmds;
  uint32_t cmd_count;
} ka_VkGfxCmdData;

typedef struct ka_VkCmpCmdData {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  ka_VkPushConstant push_constant;
  ka_VkImage const* work_images;
  uint32_t group_count_x;
  uint32_t group_count_y;
  uint32_t group_count_z;
} ka_VkCmpCmdData;

#ifdef KA_VK_ENABLE_IMGUI
typedef struct ka_VkImGuiInfo {
  VkInstance vk;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkFormat swapchain_format;
  VkDescriptorPool descpool;
} ka_VkImGuiInfo;
#endif

KA_C_API const char* ka_vk_error_string(VkResult res);

KA_C_API VkResult ka_vk_create_context(ka_VkContext* ctx, ka_VkContextArgs const* args,
                                       const char** errmsg);
KA_C_API void ka_vk_destroy_context(ka_VkContext ctx);
KA_C_API VkResult ka_vk_rebuild_swapchain(ka_VkContext vk, VkExtent2D extent);

KA_C_API VkResult ka_vk_alloc_image(ka_VkContext vk, ka_VkImage* image,
                                    ka_VkImageArgs const* args);
KA_C_API void ka_vk_dealloc_image(ka_VkContext vk, ka_VkImage* image);

KA_C_API void ka_vk_image_get_extent(ka_VkImage const* image, VkExtent3D* extent);
KA_C_API void ka_vk_image_get_format(ka_VkImage const* image, VkFormat* format);
KA_C_API void ka_vk_image_get_target(ka_VkImage const* image, ka_VkTarget* target);
KA_C_API void ka_vk_image_get_view(ka_VkImage const* image, VkImageView* view);
KA_C_API void ka_vk_image_get_handle(ka_VkImage const* image, VkImage* handle);

KA_C_API VkResult ka_vk_alloc_buffer(ka_VkContext vk, ka_VkBuffer* buffer,
                                     ka_VkBufferArgs const* args);
KA_C_API void ka_vk_dealloc_buffer(ka_VkContext vk, ka_VkBuffer* buffer);

KA_C_API void ka_vk_buffer_get_mapped_data(ka_VkBuffer const* buffer, void** data);
KA_C_API void ka_vk_buffer_get_size(ka_VkBuffer const* buffer, size_t* size);
KA_C_API void ka_vk_buffer_get_addr(ka_VkBuffer const* buffer, ka_VkContext vk,
                                    VkDeviceAddress* addr);
KA_C_API void ka_vk_buffer_get_handle(ka_VkBuffer const* buffer, VkBuffer* handle);
KA_C_API void ka_vk_buffer_transfer(ka_VkContext vk, ka_VkBuffer const* src,
                                    VkDeviceSize src_offset, VkDeviceSize src_size,
                                    ka_VkBuffer* dst, VkDeviceSize dst_offset,
                                    VkDeviceSize dst_size);

KA_C_API VkResult ka_vk_create_shader(ka_VkContext vk, VkShaderModule* shader,
                                      ka_VkShaderArgs const* args);
KA_C_API void ka_vk_destroy_shader(ka_VkContext vk, VkShaderModule shader);

KA_C_API VkResult ka_vk_create_dscset_layout(ka_VkContext vk, VkDescriptorSetLayout* layout,
                                             ka_VkDscSetLayoutArgs const* args);
KA_C_API void ka_vk_destroy_dscset_layout(ka_VkContext vk, VkDescriptorSetLayout layout);

KA_C_API VkResult ka_vk_alloc_dscsets(ka_VkContext vk, VkDescriptorSet* dscsets, uint32_t count,
                                      VkDescriptorSetLayout layout, ka_VkLifetime lifetime);
KA_C_API void ka_vk_dealloc_dscsets(ka_VkContext vk, VkDescriptorSet* dscsets, uint32_t count);

KA_C_API void ka_vk_update_dscset(ka_VkContext vk, VkWriteDescriptorSet const* write_sets,
                                  uint32_t write_sets_count);

KA_C_API VkResult ka_vk_create_pipeline_layout(ka_VkContext vk, VkPipelineLayout* layout,
                                               ka_VkPipelineLayoutArgs const* args);
KA_C_API void ka_vk_destroy_pipeline_layout(ka_VkContext vk, VkPipelineLayout layout);

KA_C_API VkResult ka_vk_create_pipeline_gfx(ka_VkContext vk, VkPipeline* pipeline,
                                            ka_VkPipelineGfxArgs const* args);
KA_C_API VkResult ka_vk_create_pipeline_cmp(ka_VkContext vk, VkPipeline* pipeline,
                                            ka_VkPipelineCmpArgs const* args);
KA_C_API void ka_vk_destroy_pipeline(ka_VkContext vk, VkPipeline pipeline);

KA_C_API VkResult ka_vk_new_frame(ka_VkContext vk);
KA_C_API VkResult ka_vk_end_frame(ka_VkContext vk);
KA_C_API VkResult ka_vk_record_gfx_cmd(ka_VkContext vk, ka_VkTarget target,
                                       ka_VkGfxCmdData const* data);
KA_C_API VkResult ka_vk_record_cmp_cmd(ka_VkContext vk, ka_VkCmpCmdData const* data);
KA_C_API VkResult ka_vk_record_user_gfx_cmd(ka_VkContext vk, ka_VkTarget target,
                                            PFN_ka_vk_user_cmd callback, void* user);

#ifdef KA_VK_ENABLE_IMGUI
KA_C_API VkResult ka_vk_create_imgui_info(ka_VkContext vk, ka_VkImGuiInfo* info,
                                          const VkDescriptorPoolCreateInfo* descpool_info);
#endif

#ifdef __cplusplus
}
#endif

#endif // #ifndef KA_VK_H_
