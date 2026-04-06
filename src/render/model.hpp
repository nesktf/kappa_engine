#pragma once

#include "../assets/model.hpp"
#include "../assets/texture.hpp"

#include "./instance.hpp"

namespace kappa::render {

struct model3d_texture {
public:
  static fn from_asset(const assets::image_data& asset, texture_type type)
    -> s_expect<model3d_texture>;
  static fn from_model(const assets::model3d_data& model)
    -> s_expect<unique_array<model3d_texture>>;

public:
  buffer_name name;
  extent2d extent;
  texture_handle texture;
  texture_type type;
};

class model3d_renderable {
public:
  static constexpr size_t MAX_MATERIAL_TEXTURES = 16;

  struct mesh_t {
    buffer_name name;
    size_t vertex_offset, vertex_count;
    size_t index_offset, index_count;
    bits32 attribute_flags;
    u32 material;
  };

  struct material_t {
    std::array<u32, MAX_MATERIAL_TEXTURES> textures;
    u32 texture_count;
    bits32 texture_flags;
    pipeline_handle pipeline;
  };

  struct bone_t {
    buffer_name name;
    i32 parent;
  };

  struct rig_t {
    std::unordered_map<std::string_view, u32> bone_map;
    unique_array<bone_t> bones;
    unique_array<m4f32> bone_locals;
    unique_array<m4f32> bone_inv_models;
  };

  using asset_filter_fn =
    shogle::inplace_trivial_fn<bool(const assets::model3d_data::mesh_data&) const,
                               2 * sizeof(void*)>;

private:
  struct create_t {};

public:
  model3d_renderable(create_t, const buffer_name& name, buffer_handle mesh_buffer,
                     unique_array<mesh_t>&& meshes, unique_array<material_t>&& materials,
                     unique_array<model3d_texture>&& textures, optional<rig_t>&& rig);

  ~model3d_renderable();

  model3d_renderable(const model3d_renderable&) = delete;
  model3d_renderable(model3d_renderable&&) noexcept = default;

  model3d_renderable& operator=(const model3d_renderable&) = delete;
  model3d_renderable& operator=(model3d_renderable&&) noexcept = default;

public:
  static fn from_asset(const assets::model3d_data& model, asset_filter_fn filter)
    -> s_expect<model3d_renderable>;

public:
  fn name() const -> std::string_view;
  fn meshes() const -> span<const mesh_t>;
  fn materials() const -> span<const material_t>;

  fn find_bone_idx(std::string_view name) const -> optional<u32>;
  fn bones() const -> span<const bone_t>;
  fn bone_locals() const -> span<const m4f32>;
  fn bone_inv_models() const -> span<const m4f32>;

  fn mesh_buffer() const -> buffer_handle { return _mesh_buffer; }

  fn has_bones() const -> bool { return _rig.has_value(); }

private:
  buffer_name _name;
  buffer_handle _mesh_buffer;
  unique_array<mesh_t> _meshes;
  unique_array<material_t> _materials;
  unique_array<model3d_texture> _textures;
  optional<rig_t> _rig;
};

struct buffer_binding {
  size_t size;
  size_t offset;
  buffer_handle handle;
};

class model3d_instance_handler {
public:
  struct instance_data {
    m4f32 model;
  };

private:
  struct create_t {};

public:
  model3d_instance_handler(const model3d_renderable& model, unique_array<m4f32>&& bone_transforms,
                           unique_array<m4f32>&& bone_cache, unique_array<instance_data>&& data,
                           size_t buffer_offset, buffer_handle buffer, u32 instances);

  ~model3d_instance_handler();

  model3d_instance_handler(model3d_instance_handler&& other) noexcept;
  model3d_instance_handler(const model3d_instance_handler&) = delete;

  model3d_instance_handler& operator=(model3d_instance_handler&& other) noexcept;
  model3d_instance_handler& operator=(const model3d_instance_handler& other) = delete;

public:
  static fn create(const model3d_renderable& model, u32 instances)
    -> s_expect<model3d_instance_handler>;

public:
  fn update_buffers() -> void;
  fn retrieve_render_data(const buffer_binding& scene_buffer, vec<render_data>& data) const -> u32;

public:
  fn set_transform(u32 instance, const m4f32& transform) -> void;
  fn set_bone_transform(u32 instance, u32 bone, const m4f32& transform) -> void;

public:
  fn model() const -> const model3d_renderable& { return *_model; }

private:
  const model3d_renderable* _model;
  unique_array<m4f32> _bone_transforms, _bone_cache;
  unique_array<instance_data> _instance_data;
  size_t _buffer_offset;
  buffer_handle _buffer;
  u32 _instances;
};

} // namespace kappa::render
