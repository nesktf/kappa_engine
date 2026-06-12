#pragma once

#include "./render/vulkan/vk_buffer.hpp"
#include "./render/vulkan/vk_context.hpp"

#include "../util/array.hpp"
#include "../util/freelist.hpp"
#include "../util/ptr.hpp"
#include "../util/string.hpp"

#include <ranmath/ran.hpp>

namespace kappa::render {

class RenderContext;

struct MeshAsset {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  ran::Mat4f32 transform;
  VkAllocBuff vertex_buffer;
  VkAllocBuff index_buffer;
  u32 index_start, index_count;
  BuffStr<256> name;
};

class SceneData {
private:
  struct create_t {};

public:
  static constexpr u32 MAX_MESHES = 128;
  using Mesh = freelist_slot;

  struct ComputeConstants {
    ran::Vec4f32 data1;
    ran::Vec4f32 data2;
    ran::Vec4f32 data3;
    ran::Vec4f32 data4;
  };

  struct ComputeData {
    VkDescriptorSetLayout image_desc_layout;
    VkDescriptorSet image_desc;
    VkPipelineLayout layout;
    VkPipeline pipelines[2];
    ComputeConstants data[2];
    s32 effect_idx;
  };

  struct MeshData {
    Span<const u32> indices;
    Span<const ran::Vec3f32> positions;
    Span<const ran::Vec3f32> normals;
    Span<const ran::Vec2f32> uvs;
    Span<const ran::Vec3f32> tangents;
    Span<const ran::Vec3f32> bitangents;
  };

  struct SceneLayouts {
    VkDescriptorSetLayout main_layout;
    VkDescriptorSetLayout image_layout;
  };

public:
  SceneData(create_t, RenderContext& ctx, ComputeData&& compute, SceneLayouts&& layouts);

public:
  static fn initialize(Uninited<SceneData> scene, RenderContext& ctx) -> VkMsgExpect<void>;

public:
  fn add_mesh(const MeshData& mesh, std::string_view name) -> Mesh;
  fn clear() -> void;

public:
  fn render_geometry(VkImageLayout& target_layout, VkCommandBuffer cmd) -> void;
  fn render_imgui(const VkFrameContext& frame) -> void;

private:
  RenderContext* _ctx;
  ComputeData _compute;
  FixedFreelist<MeshAsset, MAX_MESHES> _meshes;
  SceneLayouts _layouts;
};

} // namespace kappa::render
