#pragma once

#include "../renderer/context.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <unordered_map>

namespace kappa {

struct model_rig_data {
  struct bone_meta {
    std::string name;
    u32 parent;
  };

  struct armature_meta {
    std::string name;
    vec_span bones;
  };

  std::vector<mat4> bone_locals;
  std::vector<mat4> bone_inv_models;
  std::vector<bone_meta> bones;
  std::vector<armature_meta> armatures;
  std::unordered_map<std::string_view, u32> bone_registry;
  std::unordered_map<std::string_view, u32> armature_registry;
};

struct model_anim_data {
  template<typename T>
  struct key_frame {
    double timestamp;
    T value;
  };

  struct animation_meta {
    std::string name;
    double duration;
    double tps;
    vec_span frames;
  };

  struct key_frame_meta {
    std::string bone_name;
    vec_span pos_keys;
    vec_span rot_Keys;
    vec_span sca_keys;
  };

  std::vector<key_frame_meta> keyframes;
  std::vector<animation_meta> animations;
  std::vector<key_frame<vec3>> positions;
  std::vector<key_frame<vec3>> scales;
  std::vector<key_frame<quat>> rotations;
  std::unordered_map<std::string_view, u32> animation_registry;
};

struct model_material_data {
  struct texture_meta {
    std::string name;
    std::string path;
    ntf::unique_array<uint8> bitmap;
    extent3d extent;
    shogle::image_format format;
    // aiTextureType tex_type;
    // aiTextureMapMode mapping;
    // aiTextureOp combine_op;
    // aiTextureFlags flags;
  };

  struct material_meta {
    std::string name;
    vec_span textures;
    // aiShadingMode shading;
    // aiBlendMode blending;
  };

  std::vector<texture_meta> textures;
  std::vector<material_meta> materials;
  std::vector<u32> material_textures;
  std::unordered_map<std::string_view, u32> texture_registry;
  std::unordered_map<std::string_view, u32> material_registry;
};

struct model_mesh_data {
  static constexpr size_t VERTEX_BONE_COUNT = 4u;
  using vertex_bones = std::array<u32, VERTEX_BONE_COUNT>;
  using vertex_weights = std::array<float, VERTEX_BONE_COUNT>;

  static constexpr vertex_bones EMPTY_BONE_INDEX = []() {
    vertex_bones arr;
    for (auto& bone : arr) {
      bone = vec_span::INDEX_TOMB;
    }
    return arr;
  }();

  static constexpr vertex_weights EMPTY_BONE_WEIGHT = []() {
    vertex_weights arr;
    for (auto& weight : arr) {
      weight = 0.f;
    }
    return arr;
  }();

  struct mesh_meta {
    vec_span positions;
    vec_span normals;
    vec_span uvs;
    vec_span tangents;
    vec_span colors;
    vec_span bones;
    vec_span indices;
    std::string name;
    std::string material_name;
    u32 face_count;
    // aiPrimitiveType prim;
  };

  std::vector<vec3> positions;
  std::vector<vec3> normals;
  std::vector<vec2> uvs;
  std::vector<vec3> tangents;
  std::vector<vec3> bitangents;
  std::vector<color4> colors;
  std::vector<vertex_bones> bones;
  std::vector<vertex_weights> weights;
  std::vector<uint32> indices;
  std::vector<mesh_meta> meshes;
  std::unordered_map<std::string_view, u32> mesh_registry;
};

class assimp_parser {
private:
  using unex_t = ntf::unexpected<std::string>;
  using bone_inv_map = std::unordered_map<std::string, mat4>;

public:
  // lel
  static constexpr u32 DEFAULT_ASS_FLAGS =
    aiProcess_Triangulate | aiProcess_GenUVCoords | aiProcess_CalcTangentSpace;

public:
  assimp_parser();

public:
  expect<void> load(const std::string& path, uint32 assimp_flags = DEFAULT_ASS_FLAGS);

public:
  expect<void> parse_rigs(model_rig_data& rigs);
  expect<void> parse_animations(model_anim_data& anims);
  expect<void> parse_materials(model_material_data& materials);
  expect<void> parse_meshes(const model_rig_data& rigs, model_mesh_data& meshes,
                            std::string_view model_name);

private:
  void _parse_bone_nodes(const bone_inv_map& bone_invs, u32 parent, u32& bone_count,
                         const aiNode* node, model_rig_data& rigs);

private:
  Assimp::Importer _imp;
  uint32 _flags;
  std::string _dir;
};

struct vertex_config {
  size_t size;
  std::string_view name;
};

namespace meta {

template<typename Arr, typename T>
struct is_std_array_of : public std::false_type {};

template<typename T, size_t N>
struct is_std_array_of<std::array<T, N>, T> : public std::true_type {};

template<typename T>
concept mesh_data_type = requires(const T mesh_data, u32 attr_idx, u32 mesh_idx) {
  requires ntf::meta::std_cont<std::remove_const_t<decltype(T::VERT_CONFIG)>, vertex_config>;
  { mesh_data.vertex_count() } -> std::convertible_to<u32>;
  { mesh_data.index_count() } -> std::convertible_to<u32>;
  { mesh_data.mesh_count() } -> std::convertible_to<u32>;
  { mesh_data.mesh_index_range(mesh_idx) } -> std::same_as<vec_span>;
  { mesh_data.vertex_data(attr_idx, mesh_idx) } -> std::same_as<std::pair<const void*, u32>>;
  { mesh_data.index_data() } -> std::same_as<cspan<u32>>;
};

} // namespace meta

struct mesh_offset {
  u32 index_offset;
  u32 index_count;
  u32 vertex_offset;
};

template<meta::mesh_data_type MeshDataT>
class model3d_mesh_buffers {
public:
  static constexpr size_t VERTEX_ATTRIB_COUNT = MeshDataT::VERT_CONFIG.size();
  using vert_buffs = std::array<shogle::buffer_t, VERTEX_ATTRIB_COUNT>;
  using vert_binds = std::array<shogle::vertex_binding, VERTEX_ATTRIB_COUNT>;

public:
  model3d_mesh_buffers(vert_buffs buffs, u32 vertex_count, shogle::index_buffer&& idx_buff,
                       u32 index_count, ntf::unique_array<mesh_offset>&& offsets) :
      _buffs{buffs},
      _offsets{std::move(offsets)}, _idx_buff{std::move(idx_buff)}, _vertex_count{vertex_count},
      _index_count{index_count} {
    NTF_ASSERT(!_idx_buff.empty());
    NTF_ASSERT(vertex_count > 0);
    for (u32 i = 0; i < _buffs.size(); ++i) {
      NTF_ASSERT(_buffs[i], "Vertex \"{}\" missing", MeshDataT::VERT_CONFIG[i].name);
      _binds[i].buffer = _buffs[i];
      _binds[i].layout = i;
    }
  }

  model3d_mesh_buffers(model3d_mesh_buffers&& other) noexcept :
      _buffs{std::move(other._buffs)}, _binds{std::move(other._binds)},
      _offsets{std::move(other._offsets)}, _idx_buff{std::move(other._idx_buff)},
      _vertex_count{std::move(other._vertex_count)}, _index_count{std::move(other._index_count)} {
    other._reset_buffs();
  }

  ~model3d_mesh_buffers() noexcept { _free_buffs(); }

  model3d_mesh_buffers(const model3d_mesh_buffers&) = delete;

public:
  model3d_mesh_buffers& operator=(model3d_mesh_buffers&& other) noexcept {
    _free_buffs();

    _buffs = std::move(other._buffs);
    _binds = std::move(other._binds);
    _offsets = std::move(other._offsets);
    _idx_buff = std::move(other._idx_buff);
    _vertex_count = std::move(other._vertex_count);
    _index_count = std::move(other._index_count);

    other._reset_buffs();
    return *this;
  }

  model3d_mesh_buffers& operator=(const model3d_mesh_buffers&) = delete;

private:
  void _reset_buffs() noexcept {
    std::memset(_buffs.data(), 0, _buffs.size() * sizeof(shogle::buffer_t));
    std::memset(_binds.data(), 0, _binds.size() * sizeof(shogle::vertex_binding));
    _index_count = 0u;
    _vertex_count = 0u;
  }

  void _free_buffs() noexcept {
    if (!_buffs[0]) {
      return;
    }
    for (shogle::buffer_t buff : _buffs) {
      if (buff) {
        shogle::destroy_buffer(buff);
      }
    }
  }

public:
  static expect<model3d_mesh_buffers> create(const MeshDataT& mesh_data) {
    vert_buffs buffs{}; // init as nullptr

    auto free_buffs = [&]() {
      for (shogle::buffer_t buff : buffs) {
        if (buff) {
          shogle::destroy_buffer(buff);
        }
      }
    };

    // Create vertex buffers
    const size_t vertex_count = mesh_data.vertex_count();
    for (u32 i = 0; const auto& [conf_size, conf_name] : MeshDataT::VERT_CONFIG) {
      ntf::logger::debug("Creating vertex buffer \"{}\"", conf_name);
      auto buff = render::create_vbo(vertex_count * conf_size, nullptr);
      if (!buff) {
        free_buffs();
        return {ntf::unexpect, std::move(buff.error())};
      }
      buffs[i] = buff->release();
      ++i;
    }

    // Copy vertex data
    size_t offset = 0u;
    const size_t mesh_count = mesh_data.mesh_count();
    ntf::unique_array<mesh_offset> mesh_offsets(mesh_count);
    for (size_t mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {
      const vec_span idx_range = mesh_data.mesh_index_range(mesh_idx);
      if (idx_range.empty()) {
        ntf::logger::debug("Mesh {} with no indices!!!", mesh_idx);
        continue;
      }
      mesh_offsets[mesh_idx].index_offset = idx_range.idx;
      mesh_offsets[mesh_idx].index_count = idx_range.count;
      mesh_offsets[mesh_idx].vertex_offset = offset;

      u32 vertex_elems = 0u;
      for (u32 attr_idx = 0; const auto& [conf_size, conf_name] : MeshDataT::VERT_CONFIG) {
        ntf::logger::debug("Uploading vertex data \"{}\" in mesh {}", conf_name, mesh_idx);
        const auto [data_ptr, data_count] = mesh_data.vertex_data(attr_idx, mesh_idx);
        if (!data_ptr) {
          ntf::logger::warning("Attribute \"{}\" with no vertex data at mesh {}", conf_name,
                               mesh_idx);
          continue;
        }
        vertex_elems = std::max(vertex_elems, data_count);
        NTF_ASSERT(buffs[attr_idx]);
        ntf::logger::debug(" - {} => {}", vertex_count, data_count);
        // NTF_ASSERT(data_count == vertex_count);
        [[maybe_unused]] auto ret =
          shogle::buffer_upload(buffs[attr_idx], {
                                                   .data = data_ptr,
                                                   .size = data_count * conf_size,
                                                   .offset = offset * conf_size,
                                                 });
        ++attr_idx;
      }
      offset += vertex_elems;
    }

    // Create index buffer & upload index data
    ntf::logger::debug("Creating index buffer");
    const cspan<u32> idx_span = mesh_data.index_data();
    NTF_ASSERT(!idx_span.empty());
    auto idx_buff = render::create_ebo(idx_span.size_bytes(), idx_span.data());
    if (!idx_buff) {
      free_buffs();
      return {ntf::unexpect, std::move(idx_buff.error())};
    }

    return {ntf::in_place,
            buffs,
            static_cast<u32>(vertex_count),
            std::move(*idx_buff),
            static_cast<u32>(idx_span.size()),
            std::move(mesh_offsets)};
  }

public:
  render::mesh_render_data& retrieve_mesh_data(u32 mesh_idx,
                                               std::vector<render::mesh_render_data>& data) const {
    NTF_ASSERT(mesh_idx < mesh_count());
    cspan<shogle::vertex_binding> binds{_binds.data(), _binds.size()};
    const auto& offset = _offsets[mesh_idx];
    return data.emplace_back(binds, index_buffer(), offset.index_count, offset.vertex_offset,
                             offset.index_offset, 0u);
  }

  bool has_indices() const { return !_idx_buff.empty(); }

  shogle::vertex_buffer_view vertex_buffer(u32 idx) const {
    NTF_ASSERT(idx < _buffs.size());
    return shogle::to_typed<shogle::buffer_type::vertex>(shogle::buffer_view{_buffs[idx]});
  }

  shogle::index_buffer_view index_buffer() const { return _idx_buff; }

  cspan<mesh_offset> offsets() const { return {_offsets.data(), _offsets.size()}; }

  u32 mesh_count() const { return _offsets.size(); }

  u32 vertex_count() const { return _vertex_count; }

  u32 index_count() const { return _index_count; }

private:
  vert_buffs _buffs;
  vert_binds _binds;
  ntf::unique_array<mesh_offset> _offsets;
  shogle::index_buffer _idx_buff;
  u32 _vertex_count, _index_count;
};

class model3d_mesh_textures {
private:
  struct texture_t {
    std::string name;
    shogle::texture2d tex;
    u32 sampler;
  };

  struct material_t {
    std::string name;
    vec_span textures;
  };

public:
  model3d_mesh_textures(ntf::unique_array<texture_t>&& textures,
                        std::unordered_map<std::string_view, u32>&& tex_reg,
                        ntf::unique_array<vec_span>&& mat_spans,
                        ntf::unique_array<u32>&& mat_texes) noexcept;

protected:
  static expect<model3d_mesh_textures> create(const model_material_data& materials);

public:
  shogle::texture2d_view find_texture(std::string_view name);

  ntf::optional<u32> find_texture_idx(std::string_view name) const;
  u32 retrieve_material_textures(u32 mat_idx, std::vector<shogle::texture_binding>& texs) const;

private:
  ntf::unique_array<texture_t> _textures;
  std::unordered_map<std::string_view, u32> _tex_reg;
  ntf::unique_array<vec_span> _mat_spans;
  ntf::unique_array<u32> _mat_texes;
};

} // namespace kappa
