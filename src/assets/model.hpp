#pragma once

#include "./ass_common.hpp"

#include "../math/matrix4x4.hpp"
#include "../math/vector2.hpp"
#include "../math/vector3.hpp"
#include "../math/vector4.hpp"

#include "../util/optional.hpp"
#include "../util/ptr.hpp"

namespace kappa::assets {

struct Model3DData {
public:
  static constexpr usize MAX_MESH_UVS = 2;
  static constexpr usize MAX_MESH_COLORS = 2;

  enum MeshPrimitive : u32 {
    MESH_PRIMITIVE_POINT = 0,
    MESH_PRIMITIVE_LINE,
    MESH_PRIMITIVE_TRIANGLE,
    MESH_PRIMITIVE_POLYGON,
  };

  enum TextureMapMode : u32 {
    TEXTURE_MAP_MODE_WRAP = 0,
    TEXTURE_MAP_MODE_CLAMP,
    TEXTURE_MAP_MODE_DECAL,
    TEXTURE_MAP_MODE_MIRROR,
  };

  enum MaterialShading : u32 {
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
  struct MeshData {
  public:
    ArrayRange positions() const { return {positions_start, nverts}; }

    bool has_positions() const { return positions_start != (u32)-1; }

    ArrayRange normals() const { return {normals_start, nverts}; }

    bool has_normals() const { return normals_start != (u32)-1; }

    ArrayRange tangents() const { return {tangents_start, nverts}; }

    bool has_tangents() const { return tangents_start != (u32)-1; }

    ArrayRange uvs(usize idx) const {
      assert(idx < MAX_MESH_UVS);
      return {uvs_start[idx], nverts};
    }

    bool has_uvs(usize idx) const { return (idx >= MAX_MESH_UVS || uvs_start[idx] != (u32)-1); }

    ArrayRange colors(usize idx) const {
      assert(idx < MAX_MESH_COLORS);
      return {colors_start[idx], nverts};
    }

    bool has_colors(usize idx) const {
      return (idx >= MAX_MESH_COLORS || colors_start[idx] != (u32)-1);
    }

    ArrayRange bones() const { return {bones_start, nverts}; }

    bool has_bones() const { return bones_start != (u32)-1; }

    ArrayRange indices() const { return {index_start, index_count}; }

    bool has_indices() const { return index_count > 0; }

    u32 elem_count() const { return has_indices() ? index_count : nverts; }

    ArrayRange blend_shapes() const { return {blend_start, blend_count}; }

    bool has_blend_shapes() const { return blend_start != (u32)-1; }

  public:
    BufferName name;
    BufferName uv_name[MAX_MESH_UVS];
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
    Vec3f32 bbox_min, bbox_max;
    u32 material_index;
    MeshPrimitive primitive;
  };

  struct BlendShapeData {
  public:
    ArrayRange positions() const { return {positions_start, nverts}; }

    bool has_positions() const { return positions_start != (u32)-1; }

    ArrayRange normals() const { return {normals_start, nverts}; }

    bool has_normals() const { return normals_start != (u32)-1; }

    ArrayRange tangents() const { return {tangents_start, nverts}; }

    bool has_tangents() const { return tangents_start != (u32)-1; }

    ArrayRange uvs(usize idx) const {
      assert(idx < MAX_MESH_UVS);
      return {uvs_start[idx], nverts};
    }

    bool has_uvs(usize idx) const { return (idx >= MAX_MESH_UVS || uvs_start[idx] != (u32)-1); }

    ArrayRange colors(usize idx) const {
      assert(idx < MAX_MESH_COLORS);
      return {colors_start[idx], nverts};
    }

    bool has_colors(usize idx) const {
      return (idx >= MAX_MESH_COLORS || colors_start[idx] != (u32)-1);
    }

  public:
    BufferName name;
    u32 nverts;
    u32 positions_start;
    u32 normals_start;
    u32 tangents_start;
    u32 uvs_start[MAX_MESH_UVS];
    u32 colors_start[MAX_MESH_COLORS];
    f32 weight;
  };

  struct TextureData {
    BufferName name;
    BufferPath path;
    void* data;
    Extent2D extent;
    ImageFormat format;
    TextureType type;
  };

  struct MaterialData {
    BufferName name;
    ArrayRange texture_indices;
    MaterialShading shading_mode;
  };

  struct BoneData {
    BufferName name;
    s32 parent;
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
    ArrayRange bone_keys;
    f64 ticks, tps;
  };

  struct bone_keyframes {
    u32 bone_index;
    anim_behaviour pre_state, post_state;
    ArrayRange pos_keys;
    ArrayRange rot_keys;
    ArrayRange scale_keys;
  };
#endif

  struct ModelInternal;

public:
  Model3DData(ModelInternal& data) noexcept;

  void destroy() noexcept;

public:
  BufferName& name() const;
  BufferPath& path() const;

  MeshData& mesh_at(usize idx) const;
  Span<MeshData> meshes() const;
  Span<Vec3f32> mesh_positions() const;
  Span<Vec3f32> mesh_positions(ArrayRange range) const;
  Span<Vec3f32> mesh_normals() const;
  Span<Vec3f32> mesh_normals(ArrayRange range) const;
  Span<Vec2f32> mesh_uvs(usize idx) const;
  Span<Vec2f32> mesh_uvs(usize idx, ArrayRange range) const;
  Span<Vec4f32> mesh_colors(usize idx) const;
  Span<Vec4f32> mesh_colors(usize idx, ArrayRange range) const;
  Span<Vec3f32> mesh_tangents() const;
  Span<Vec3f32> mesh_tangents(ArrayRange range) const;
  Span<Vec3f32> mesh_bitangents() const;
  Span<Vec3f32> mesh_bitangents(ArrayRange range) const;
  Span<Vec4s32> mesh_bone_indices() const;
  Span<Vec4s32> mesh_bone_indices(ArrayRange range) const;
  Span<Vec4f32> mesh_bone_weights() const;
  Span<Vec4f32> mesh_bone_weights(ArrayRange range) const;
  Span<u32> mesh_indices() const;
  Span<u32> mesh_indices(ArrayRange range) const;
  usize mesh_count() const;

  Nullable<usize> find_mesh_idx(std::string_view mesh_name) const;

  MeshData* find_mesh(std::string_view mesh_name) const {
    return find_mesh_idx(mesh_name)
      .transform([this](usize idx) -> MeshData* { return &mesh_at(idx); })
      .value_or(nullptr);
  }

  BlendShapeData& blend_shape_at(usize idx) const;
  Span<BlendShapeData> blend_shapes() const;
  Span<Vec3f32> blend_positions() const;
  Span<Vec3f32> blend_positions(ArrayRange range) const;
  Span<Vec3f32> blend_normals() const;
  Span<Vec3f32> blend_normals(ArrayRange range) const;
  Span<Vec2f32> blend_uvs(usize idx) const;
  Span<Vec2f32> blend_uvs(usize idx, ArrayRange range) const;
  Span<Vec4f32> blend_colors(usize idx) const;
  Span<Vec4f32> blend_colors(usize idx, ArrayRange range) const;
  Span<Vec3f32> blend_tangents() const;
  Span<Vec3f32> blend_tangents(ArrayRange range) const;
  Span<Vec3f32> blend_bitangents() const;
  Span<Vec3f32> blend_bitangents(ArrayRange range) const;
  usize blend_shape_count() const;

  bool has_blend_shapes() const { return (blend_shape_count() > 0); }

  BoneData& bone_at(usize idx) const;
  Span<BoneData> bones() const;
  Span<BoneData> bones(ArrayRange range) const;
  Span<Mat4f32> bone_locals() const;
  Span<Mat4f32> bone_locals(ArrayRange range) const;
  Span<Mat4f32> bone_inverse_models() const;
  Span<Mat4f32> bone_inverse_models(ArrayRange range) const;
  usize bone_count() const;

  bool has_bones() const { return (bone_count() > 0); }

  Optional<usize> find_bone_idx(std::string_view bone_name) const;

  BoneData* find_bone(std::string_view bone_name) const {
    return find_bone_idx(bone_name)
      .transform([this](usize idx) -> BoneData* { return &bone_at(idx); })
      .value_or(nullptr);
  }

  MaterialData& material_at(usize idx) const;
  Span<MaterialData> materials() const;
  usize material_count() const;

  bool has_materials() const { return (material_count() > 0); }

  bool has_textures() const { return (texture_count() > 0); }

  TextureData& texture_at(usize idx) const;
  Span<TextureData> textures() const;
  usize texture_count() const;

  Span<u32> material_textures(usize idx) const;

  Optional<usize> find_material_idx(std::string_view material_name) const;

  MaterialData* find_material(std::string_view material_name) const {
    return find_material_idx(material_name)
      .transform([this](usize idx) -> MaterialData* { return &material_at(idx); })
      .value_or(nullptr);
  }

  Optional<usize> find_texture_idx(std::string_view texture_name) const;

  TextureData* find_texture(std::string_view texture_name) const {
    return find_texture_idx(texture_name)
      .transform([this](usize idx) -> TextureData* { return &texture_at(idx); })
      .value_or(nullptr);
  }

private:
  ModelInternal* _data;
};

class Model3DLoader {
private:
  struct LoaderInternal;

public:
  enum LoadFlags : bits32 {
    FLAGS_NONE = 0x0000,
    FLAG_TRIANGULATE = 0x0001,
    FLAG_GEN_TANGENTS = 0x0002,
    FLAG_GEN_UVS = 0x0004,
    FLAG_GEN_NORMALS = 0x0008,
  };

  static constexpr bits32 FLAGS_DEFAULT = FLAG_TRIANGULATE | FLAG_GEN_TANGENTS | FLAG_GEN_UVS;

  struct LoadOpts {
    std::string_view texture_dir;
    bits32 flags;
  };

public:
  Model3DLoader(std::string_view model_path, std::string_view model_name,
                PtrView<const LoadOpts> opts = nullptr);

public:
  // Should be only called ONCE, preferably in a threadpool
  // The internal data is destroyed at the end of the function
  AssExpect<Model3DData> load();

public:
  AssExpect<Model3DData> operator()() { return load(); }

private:
  LoaderInternal* _impl;
};

} // namespace kappa::assets
