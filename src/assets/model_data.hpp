#pragma once

#include "../common.hpp"
#include "../renderer/context.hpp"

#include <ntfstl/unique_array.hpp>
#include <ntfstl/optional.hpp>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

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

  static constexpr vertex_bones EMPTY_BONE_INDEX = [](){
    vertex_bones arr;
    for (auto& bone : arr) {
      bone = vec_span::INDEX_TOMB;
    }
    return arr;
  }();

  static constexpr vertex_weights EMPTY_BONE_WEIGHT = [](){
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
  using unex_t = ntf::unexpected<std::string_view>;
  using bone_inv_map = std::unordered_map<std::string, mat4>;

public:
  // lel
  static constexpr u32 DEFAULT_ASS_FLAGS = 
    aiProcess_Triangulate |
    aiProcess_GenUVCoords |
    aiProcess_CalcTangentSpace;

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
  string_view name;
};

namespace meta {

template<typename Arr, typename T>
struct is_std_array_of : public std::false_type{};

template<typename T, size_t N>
struct is_std_array_of<std::array<T, N>, T> : public std::true_type{};

template<typename T>
concept mesh_data_type = requires(const T mesh_data, size_t idx) {
  requires std::convertible_to<decltype(T::USE_INDICES), bool>;
  requires ntf::meta::std_cont<std::remove_const_t<decltype(T::VERT_CONFIG)>, vertex_config>;
  { mesh_data.vertex_count() } -> std::convertible_to<size_t>;
  { mesh_data.index_count() } -> std::convertible_to<size_t>;
  { mesh_data.get_data_ptr(idx) } -> std::same_as<std::pair<const void*, size_t>>;
};

} // namespace meta


template<meta::mesh_data_type MeshDataT>
class mesh_buffers {
private:
  static constexpr size_t ATTRIB_COUNT =
    MeshDataT::VERT_CONFIG.size() + static_cast<size_t>(MeshDataT::USE_INDICES);
  static constexpr size_t INDICES_BUFFER_IDX = MeshDataT::VERT_CONFIG.size()+1u;

  struct mesh_offset {
    u32 index_offset;
    u32 index_count;
    u32 vertex_offset;
  };

public:
  using vert_buffs = std::array<shogle::buffer_t, ATTRIB_COUNT>;
  using vert_binds = std::array<shogle::vertex_binding, ATTRIB_COUNT>;

public:
  mesh_buffers(vert_buffs buffs, ntf::unique_array<mesh_offset>&& offsets) :
    _buffs{buffs}, _offsets{std::move(offsets)}
  {
    for (u32 i = 0; i < _buffs.size(); ++i) {
      NTF_ASSERT(_buffs[i], "Vertex \"{}\" missing", MeshDataT::VERT_CONF[i].name);
      _binds[i].buffer = _buffs[i];
      _binds[i].layout = i;
    }
  }

  mesh_buffers(mesh_buffers&& other) noexcept :
    _buffs{std::move(other._buffs)}, _binds{std::move(other._binds)},
    _offsets{std::move(other._offsets)}
  {
    other._reset_buffs();
  }

  ~mesh_buffers() noexcept { _free_buffs(); }
  mesh_buffers(const mesh_buffers&) = delete;

public:
  mesh_buffers& operator=(mesh_buffers&& other) noexcept {
    _free_buffs();
    
    _buffs = std::move(other._buffs);
    _binds = std::move(other._binds);
    _offsets = std::move(other._offsets);

    other._reset_buffs();
    return *this;
  }

  mesh_buffers& operator=(const mesh_buffers&) = delete;

private:
  void _reset_buffs() noexcept {
    std::memset(_buffs.data(), 0, _buffs.size()*sizeof(shogle::buffer_t));
    std::memset(_binds.data(), 0, _binds.data()*sizeof(shogle::vertex_binding));
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
  static expect<mesh_buffers> create(const MeshDataT& mesh_data) {
    auto ctx = renderer::instance().ctx();
    vert_buffs buffs{}; // init as nullptr

    auto free_buffs = [&]() {
      for (shogle::buffer_t buff : buffs) {
        if (buff) {
          shogle::destroy_buffer(buff);
        }
      }
    };

    for (u32 i = 0; const auto& [conf_size, conf_name] : MeshDataT::VERT_CONFIG) {
      ntf::logger::debug("Creating vertex \"{}\"", conf_name);
      auto buff =  shogle::create_buffer(ctx, {
        .type = shogle::buffer_type::vertex,
        .flags = shogle::buffer_flag::dynamic_storage,
        .size = mesh_data.vertex_count()*conf_size,
        .data = nullptr,
      });
      if (!buff) {
        free_buffs();
        return {ntf::unexpect, buff.error()};
      }
      buffs[i] = *buff;
      ++i;
    }
    constexpr size_t idx_buff_pos = ATTRIB_COUNT-1;
    if constexpr (MeshDataT::USE_INDICES) {
      ntf::logger::debug("Creating index buffer");
      const size_t idx_count = mesh_data.index_count();
      auto ind = shogle::create_buffer(ctx, {
        .type = shogle::buffer_type::vertex,
        .flags = shogle::buffer_flag::dynamic_storage,
        .size = idx_count*sizeof(u32),
        .data = nullptr,
      });
      if (!ind) {
        free_buffs();
        return {ntf::unexpect, ind.error()};
      }
      buffs[idx_buff_pos] = *ind;
    }


    // Copy data
    size_t offset = 0u;
    const size_t mesh_count = mesh_data.mesh_count();
    ntf::unique_array<mesh_offset> mesh_offsets(mesh_count);
    for (size_t mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {

      if constexpr (MeshDataT::USE_INDICES) {
        const auto idx_vspan = mesh_data.idx_vspan_at(mesh_idx);
        mesh_offsets[mesh_idx].index_offset = idx_vspan.idx;
        mesh_offsets[mesh_idx].index_count = idx_vspan.count;
      } else {
        mesh_offsets[mesh_idx].index_offset = 0u;
        mesh_offsets[mesh_idx].index_count = 0u;
      }

      mesh_offsets[mesh_idx].vertex_offset = offset;

      size_t advance = 0u;
      for (u32 buff_idx = 0; const auto& [conf_size, conf_name] : MeshDataT::VERT_CONFIG) {
        ntf::logger::debug("Uploading vertex data \"{}\"", conf_name);
        const auto [data_ptr, data_count] = mesh_data.get_data_ptr(buff_idx);
        if (!data_ptr) {
          ntf::logger::warning("Vertex with \"{}\" with no data", conf_name);
          continue;
        }
        advance = std::max(advance, data_count);
        NTF_ASSERT(buffs[buff_idx]);
        [[maybe_unused]] auto ret = shogle::buffer_upload(buffs[buff_idx], {
          .data = data_ptr,
          .size = data_count*conf_size,
          .offset = offset*conf_size,
        });
        ++buff_idx;
      }
      offset += advance;
    }
    if constexpr (MeshDataT::USE_INDICES) {
      ntf::logger::debug("Uploading index data");
      const auto [data_ptr, data_count] = mesh_data.get_index_data();
      if (!data_ptr) {
        ntf::logger::warning("No index data");
      } else {
        NTF_ASSERT(buffs[idx_buff_pos]);
        [[maybe_unused]] auto ret = shogle::buffer_upload(buffs[idx_buff_pos], {
          .data = data_ptr,
          .size = data_count*sizeof(u32),
          .offset = offset*sizeof(u32),
        });
      }
    }
    return {ntf::in_place, buffs, std::move(mesh_offsets)};
  }

public:
  mesh_render_data& retrieve_meshes(u32 idx, std::vector<mesh_render_data>& data) {
    if constexpr (MeshDataT::USE_INDICES) {
      NTF_ASSERT(idx < _offsets.size()-1);
    } else {
      NTF_ASSERT(idx < _offsets.size());
    }
    cspan<shogle::vertex_binding> binds{_binds.data(), _binds.size()};
    const auto& offset = _offsets[idx];
    shogle::buffer_view idx_buff{_binds[_binds.size()-1]};
    return data.emplace_back(binds, shogle::to_typed<shogle::buffer_type::index>(idx_buff),
                             offset.index_count, offset.vertex_count, offset.index_count, 0u);
  }

private:
  vert_buffs _buffs;
  vert_binds _binds;
  ntf::unique_array<mesh_offset> _offsets;
};

} // namespace kappa
