#pragma once

#include "./texture.hpp"

namespace kappa::assets {

struct model3d_data {
public:
  static constexpr size_t MAX_MESH_UVS = 2;
  static constexpr size_t MAX_MESH_COLORS = 2;

  enum mesh_primitive : u32 {
    MESH_PRIMITIVE_POINT = 0,
    MESH_PRIMITIVE_LINE,
    MESH_PRIMITIVE_TRIANGLE,
    MESH_PRIMITIVE_POLYGON,
  };

  enum texture_map_mode : u32 {
    TEXTURE_MAP_MODE_WRAP = 0,
    TEXTURE_MAP_MODE_CLAMP,
    TEXTURE_MAP_MODE_DECAL,
    TEXTURE_MAP_MODE_MIRROR,
  };

  enum material_shading : u32 {
    MATERIAL_SHADING_NONE = 0,
    MATERIAL_SHADING_FLAT,
    MATERIAL_SHADING_PHONG,
    MATERIAL_SHADING_BLINN,
    MATERIAL_SHADING_TOON,
    MATERIAL_SHADING_PBR_BRDF,
  };

#if 0
  enum anim_behaviour : u32 {
    ANIM_BEHAVIOUR_USE_TRANSFORM = 0, // Use default transform
    ANIM_BEHAVIOUR_CLAMP,             // Clamp animation to the first/last keyframe
    ANIM_BEHAVIOUR_EXTRAPOLATE,       // Extrapolate using the first/last keyframe
    ANIM_BEHAVIOUR_REPEAT,            // Repeat the animation
  };

  enum keyframe_setting : u32 {
    KEYFRAME_SETTING_STEP = 0,
    KEYFRAME_SETTING_LINEAR,
    KEYFRAME_SETTING_SPH_LINEAR,
    KEYFRAME_SETTING_CUBIC_SPLINE,
  };
#endif

public:
  struct mesh_data {
  public:
    array_range positions() const { return {positions_start, nverts}; }

    bool has_positions() const { return positions_start != (u32)-1; }

    array_range normals() const { return {normals_start, nverts}; }

    bool has_normals() const { return normals_start != (u32)-1; }

    array_range tangents() const { return {tangents_start, nverts}; }

    bool has_tangents() const { return tangents_start != (u32)-1; }

    array_range uvs(size_t idx) const {
      assert(idx < MAX_MESH_UVS);
      return {uvs_start[idx], nverts};
    }

    bool has_uvs(size_t idx) const { return (idx >= MAX_MESH_UVS || uvs_start[idx] != (u32)-1); }

    array_range colors(size_t idx) const {
      assert(idx < MAX_MESH_COLORS);
      return {colors_start[idx], nverts};
    }

    bool has_colors(size_t idx) const {
      return (idx >= MAX_MESH_COLORS || colors_start[idx] != (u32)-1);
    }

    array_range bones() const { return {bones_start, nverts}; }

    bool has_bones() const { return bones_start != (u32)-1; }

    array_range indices() const { return {index_start, index_count}; }

    bool has_indices() const { return index_count > 0; }

    u32 elem_count() const { return has_indices() ? index_count : nverts; }

    array_range blend_shapes() const { return {blend_start, blend_count}; }

    bool has_blend_shapes() const { return blend_start != (u32)-1; }

  public:
    buffer_name name;
    buffer_name uv_name[MAX_MESH_UVS];
    u32 nverts;
    u32 positions_start;
    u32 normals_start;
    u32 tangents_start;
    u32 bones_start;
    u32 uvs_start[MAX_MESH_UVS];
    u32 colors_start[MAX_MESH_COLORS];
    u32 index_start;
    u32 index_count;
    u32 face_count;
    u32 blend_start;
    u32 blend_count;
    v3f32 bbox_min, bbox_max;
    u32 material_index;
    mesh_primitive primitive;
  };

  struct blend_shape_data {
  public:
    array_range positions() const { return {positions_start, nverts}; }

    bool has_positions() const { return positions_start != (u32)-1; }

    array_range normals() const { return {normals_start, nverts}; }

    bool has_normals() const { return normals_start != (u32)-1; }

    array_range tangents() const { return {tangents_start, nverts}; }

    bool has_tangents() const { return tangents_start != (u32)-1; }

    array_range uvs(size_t idx) const {
      assert(idx < MAX_MESH_UVS);
      return {uvs_start[idx], nverts};
    }

    bool has_uvs(size_t idx) const { return (idx >= MAX_MESH_UVS || uvs_start[idx] != (u32)-1); }

    array_range colors(size_t idx) const {
      assert(idx < MAX_MESH_COLORS);
      return {colors_start[idx], nverts};
    }

    bool has_colors(size_t idx) const {
      return (idx >= MAX_MESH_COLORS || colors_start[idx] != (u32)-1);
    }

  public:
    buffer_name name;
    u32 nverts;
    u32 positions_start;
    u32 normals_start;
    u32 tangents_start;
    u32 uvs_start[MAX_MESH_UVS];
    u32 colors_start[MAX_MESH_COLORS];
    f32 weight;
  };

  struct texture_data {
    buffer_name name;
    buffer_path path;
    void* data;
    extent2d extent;
    image_format format;
    texture_type type;
  };

  struct material_data {
    buffer_name name;
    array_range texture_indices;
    material_shading shading_mode;
  };

  struct bone_data {
    buffer_name name;
    i32 parent;
  };

#if 0
  template<typename T>
  struct keyframe_data {
    T value;
    f64 timestamp;
    keyframe_setting setting;
  };

  struct anim_data {
    buffer_name name;
    array_range bone_keys;
    f64 ticks, tps;
  };

  struct bone_keyframes {
    u32 bone_index;
    anim_behaviour pre_state, post_state;
    array_range pos_keys;
    array_range rot_keys;
    array_range scale_keys;
  };
#endif

  struct model_internal;

public:
  model3d_data(model_internal& data) noexcept;

  void destroy() noexcept;

public:
  buffer_name& name() const;
  buffer_path& path() const;

  mesh_data& mesh_at(size_t idx) const;
  span<mesh_data> meshes() const;
  span<v3f32> mesh_positions() const;
  span<v3f32> mesh_positions(array_range range) const;
  span<v3f32> mesh_normals() const;
  span<v3f32> mesh_normals(array_range range) const;
  span<v2f32> mesh_uvs(size_t idx) const;
  span<v2f32> mesh_uvs(size_t idx, array_range range) const;
  span<v4f32> mesh_colors(size_t idx) const;
  span<v4f32> mesh_colors(size_t idx, array_range range) const;
  span<v3f32> mesh_tangents() const;
  span<v3f32> mesh_tangents(array_range range) const;
  span<v3f32> mesh_bitangents() const;
  span<v3f32> mesh_bitangents(array_range range) const;
  span<v4i32> mesh_bone_indices() const;
  span<v4i32> mesh_bone_indices(array_range range) const;
  span<v4f32> mesh_bone_weights() const;
  span<v4f32> mesh_bone_weights(array_range range) const;
  span<u32> mesh_indices() const;
  span<u32> mesh_indices(array_range range) const;
  size_t mesh_count() const;

  optional<size_t> find_mesh_idx(std::string_view mesh_name) const;

  mesh_data* find_mesh(std::string_view mesh_name) const {
    return find_mesh_idx(mesh_name)
      .transform([this](size_t idx) -> mesh_data* { return &mesh_at(idx); })
      .value_or(nullptr);
  }

  blend_shape_data& blend_shape_at(size_t idx) const;
  span<blend_shape_data> blend_shapes() const;
  span<v3f32> blend_positions() const;
  span<v3f32> blend_positions(array_range range) const;
  span<v3f32> blend_normals() const;
  span<v3f32> blend_normals(array_range range) const;
  span<v2f32> blend_uvs(size_t idx) const;
  span<v2f32> blend_uvs(size_t idx, array_range range) const;
  span<v4f32> blend_colors(size_t idx) const;
  span<v4f32> blend_colors(size_t idx, array_range range) const;
  span<v3f32> blend_tangents() const;
  span<v3f32> blend_tangents(array_range range) const;
  span<v3f32> blend_bitangents() const;
  span<v3f32> blend_bitangents(array_range range) const;
  size_t blend_shape_count() const;

  bool has_blend_shapes() const { return (blend_shape_count() > 0); }

  bone_data& bone_at(size_t idx) const;
  span<bone_data> bones() const;
  span<bone_data> bones(array_range range) const;
  span<m4f32> bone_locals() const;
  span<m4f32> bone_locals(array_range range) const;
  span<m4f32> bone_inverse_models() const;
  span<m4f32> bone_inverse_models(array_range range) const;
  size_t bone_count() const;

  bool has_bones() const { return (bone_count() > 0); }

  optional<size_t> find_bone_idx(std::string_view bone_name) const;

  bone_data* find_bone(std::string_view bone_name) const {
    return find_bone_idx(bone_name)
      .transform([this](size_t idx) -> bone_data* { return &bone_at(idx); })
      .value_or(nullptr);
  }

  material_data& material_at(size_t idx) const;
  span<material_data> materials() const;
  size_t material_count() const;

  bool has_materials() const { return (material_count() > 0); }

  bool has_textures() const { return (texture_count() > 0); }

  texture_data& texture_at(size_t idx) const;
  span<texture_data> textures() const;
  size_t texture_count() const;

  span<texture_data> material_textures(size_t idx) const;

  optional<size_t> find_material_idx(std::string_view material_name) const;

  material_data* find_material(std::string_view material_name) const {
    return find_material_idx(material_name)
      .transform([this](size_t idx) -> material_data* { return &material_at(idx); })
      .value_or(nullptr);
  }

  optional<size_t> find_texture_idx(std::string_view texture_name) const;

  texture_data* find_texture(std::string_view texture_name) const {
    return find_texture_idx(texture_name)
      .transform([this](size_t idx) -> texture_data* { return &texture_at(idx); })
      .value_or(nullptr);
  }

private:
  model_internal* _data;
};

class model3d_loader {
private:
  struct loader_internal;

public:
  enum load_flags : bits32 {
    FLAGS_NONE = 0x0000,
    FLAG_TRIANGULATE = 0x0001,
    FLAG_GEN_TANGENTS = 0x0002,
    FLAG_GEN_UVS = 0x0004,
    FLAG_GEN_NORMALS = 0x0008,
  };

  static constexpr bits32 FLAGS_DEFAULT = FLAG_TRIANGULATE | FLAG_GEN_TANGENTS | FLAG_GEN_UVS;

  struct load_opts {
    std::string_view texture_dir;
    bits32 flags;
  };

public:
  model3d_loader(std::string_view model_path, std::string_view model_name,
                 ptr_view<const load_opts> opts = nullptr);

public:
  // Should be only called ONCE, preferably in a threadpool
  // The internal data is destroyed at the end of the function
  bs_expect<model3d_data, 256> load();

public:
  bs_expect<model3d_data, 256> operator()() { return load(); }

private:
  loader_internal* _impl;
};

} // namespace kappa::assets
