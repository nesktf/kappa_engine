#pragma once

#include "./model_data.hpp"
#include "../renderer/context.hpp"

namespace kappa {

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
        throw buff.error();
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
        throw ind.error();
      }
      buffs[idx_buff_pos] = *ind;
    }

    // Copy data
    size_t offset = 0u;
    for (size_t mesh_idx = 0; mesh_idx < mesh_data.mesh_count(); ++mesh_idx) {
      for (u32 buff_idx = 0; const auto& [conf_size, conf_name] : MeshDataT::VERT_CONFIG) {
        ntf::logger::debug("Uploading vertex data \"{}\"", conf_name);
        const auto [data_ptr, data_count] = mesh_data.get_data_ptr(buff_idx);
        if (!data_ptr) {
          ntf::logger::warning("Vertex with \"{}\" with no data", conf_name);
          continue;
        }
        NTF_ASSERT(buffs[buff_idx]);
        [[maybe_unused]] auto ret = shogle::buffer_upload(buffs[buff_idx], {
          .data = data_ptr,
          .size = data_count*conf_size,
          .offset = offset*conf_size,
        });
        ++buff_idx;
      }
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
          // .size = data_count*conf_size,
          // .offset = offset*conf_size,
        });
      }
    }
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
    const auto idx_buff = shogle::to_typed<shogle::buffer_type::index>(_binds[_binds.size()-1]);
    return data.emplace_back(binds, idx_buff,
                             offset.index_count, offset.vertex_count, offset.index_count, 0u);
  }

private:
  vert_buffs _buffs;
  vert_binds _binds;
  ntf::unique_array<mesh_offset> _offsets;
};

struct rigged_model_data {
public:
  static constexpr std::array<vertex_config, 7> VERT_CONFIG{{
    {.size = sizeof(vec3), .name = "rigged_position"},
    {.size = sizeof(vec3), .name = "rigged_normal"},
    {.size = sizeof(vec2), .name = "rigged_uv"},
    {.size = sizeof(vec3), .name = "rigged_tangents"},
    {.size = sizeof(vec3), .name = "rigged_bitangents"},
    {.size = sizeof(model_mesh_data::vertex_bones),   .name = "rigged_bones"},
    {.size = sizeof(model_mesh_data::vertex_weights), .name = "rigged_weights"},
  }};
  static constexpr bool USE_INDICES = true;

public:
  model_mesh_data meshes;
  model_material_data materials;
  model_rig_data rigs;
  model_anim_data anims;

public:
  size_t vertex_count() const { return meshes.positions.size(); }
  size_t index_count() const { return meshes.indices.size(); }
  std::pair<const void*, size_t> get_data_ptr(size_t idx) const {
    enum {
      IDX_POS = 0,
      IDX_NORM,
      IDX_UVS,
      IDX_TANG,
      IDX_BITANG,
      IDX_BONES,
      IDX_WEIGHTS,
      IDX_INDICES,
    };
    NTF_ASSERT(idx <= IDX_INDICES);

    return std::make_pair(nullptr, 0u);
  }
};
static_assert(meta::mesh_data_type<rigged_model_data>);

class rigged_model_mesher {
protected:
  enum attr_idx {
    ATTRIB_POS = 0,
    ATTRIB_NORM,
    ATTRIB_UVS,
    ATTRIB_TANG,
    ATTRIB_BITANG,
    ATTRIB_BONES,
    ATTRIB_WEIGHTS,

    ATTRIB_COUNT,
  };

public:
  using vert_buffs = std::array<shogle::buffer_t, ATTRIB_COUNT>;
  using vert_binds = std::array<shogle::vertex_binding, ATTRIB_COUNT>;

private:
  struct mesh_offset {
    u32 index_offset;
    u32 index_count;
    u32 vertex_offset;
  };

public:
  rigged_model_mesher(vert_buffs buffs, shogle::index_buffer&& index_buff,
               ntf::unique_array<mesh_offset>&& mesh_offsets) noexcept;

  ~rigged_model_mesher() noexcept;

  rigged_model_mesher(const rigged_model_mesher&) = delete;
  rigged_model_mesher(rigged_model_mesher&& other) noexcept;

public:
  rigged_model_mesher& operator=(const rigged_model_mesher&) = delete;
  rigged_model_mesher& operator=(rigged_model_mesher&&) noexcept;

protected:
  static expect<rigged_model_mesher> create(const model_mesh_data& meshes);

protected:
  mesh_render_data& retrieve_mesh_data(u32 idx, std::vector<mesh_render_data>& data);
  u32 mesh_count() const { return _mesh_offsets.size(); }

private:
  void _free_buffs() noexcept;

private:
  vert_buffs _buffs;
  vert_binds _binds;
  ntf::unique_array<mesh_offset> _mesh_offsets;
  shogle::index_buffer _index_buff;
  u32 _active_layouts;
};

class rigged_model_texturer {
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
  rigged_model_texturer(ntf::unique_array<texture_t>&& textures,
                 std::unordered_map<std::string_view, u32>&& tex_reg,
                 ntf::unique_array<vec_span>&& mat_spans,
                 ntf::unique_array<u32>&& mat_texes) noexcept;

protected:
  static expect<rigged_model_texturer> create(const model_material_data& materials);

public:
  shogle::texture2d_view find_texture(std::string_view name);

protected:
  ntf::optional<u32> find_texture_idx(std::string_view name) const;
  u32 retrieve_material_textures(u32 mat_idx, std::vector<shogle::texture_binding>& texs) const;

private:
  ntf::unique_array<texture_t> _textures;
  std::unordered_map<std::string_view, u32> _tex_reg;
  ntf::unique_array<vec_span> _mat_spans;
  ntf::unique_array<u32> _mat_texes;
};

class model_rigger {
private:
  struct bone_t {
    std::string name;
    u32 parent;
  };

public:
  struct bone_transform {
    vec3 pos;
    vec3 scale;
    quat rot;
  };

public:
  model_rigger(shogle::shader_storage_buffer&& ssbo,
               ntf::unique_array<bone_t>&& bones,
               std::unordered_map<std::string_view, u32>&& bone_reg,
               ntf::unique_array<mat4>&& bone_locals,
               ntf::unique_array<mat4>&& bone_inv_models,
               ntf::unique_array<mat4>&& bone_transforms,
               ntf::unique_array<mat4>&& transf_cache,
               ntf::unique_array<mat4>&& transf_output) noexcept;

protected:
  static expect<model_rigger> create(const model_rig_data& rigs, std::string_view armature);

public:
  bool set_transform(std::string_view bone, const bone_transform& transf);
  bool set_transform(std::string_view bone, const mat4& transf);
  bool set_transform(std::string_view bone, shogle::transform3d<f32>& transf);
  
  void set_transform(u32 bone, const bone_transform& transf);
  void set_transform(u32 bone, const mat4& transf);
  void set_transform(u32 bone, shogle::transform3d<f32>& transf);

  ntf::optional<u32> find_bone(std::string_view name);

public:
  void apply_animation(const model_anim_data& anims, std::string_view name, u32 tick);

protected:
  void tick_bones(const mat4& root);
  u32 retrieve_buffer_bindings(std::vector<shogle::shader_binding>& binds) const;

private:
  shogle::shader_storage_buffer _ssbo;
  ntf::unique_array<bone_t> _bones;
  std::unordered_map<std::string_view, u32> _bone_reg;
  ntf::unique_array<mat4> _bone_locals;
  ntf::unique_array<mat4> _bone_inv_models;
  ntf::unique_array<mat4> _bone_transforms;
  ntf::unique_array<mat4> _transf_cache;
  ntf::unique_array<mat4> _transf_output;
};

struct tickable {
  virtual ~tickable() = default;
  virtual void tick() = 0;
};

class rigged_model3d : public rigged_model_mesher, public rigged_model_texturer, public model_rigger,
                       public renderable, public tickable {
public:
  struct data_t {
    std::string name;
    std::string_view armature;
    model_rig_data rigs;
    model_anim_data anims;
    model_material_data mats;
    model_mesh_data meshes;
  };

public:
  rigged_model3d(rigged_model_mesher&& meshes,
                 rigged_model_texturer&& texturer,
                 model_rigger&& rigger,
                 std::vector<model_material_data::material_meta>&& mats,
                 std::unordered_map<std::string_view, u32>&& mat_reg,
                 shogle::pipeline pip,
                 std::vector<u32> mesh_mats,
                 std::string&& name) noexcept;

public:
  static expect<rigged_model3d> create(data_t&& data);

public:
  void tick() override;
  u32 retrieve_render_data(const scene_render_data& scene, object_render_data& data) override;

public:
  shogle::transform3d<f32>& transform() { return _transf; }
  std::string_view name() const { return _name; }

private:
  std::vector<model_material_data::material_meta> _mats;
  std::unordered_map<std::string_view, u32> _mat_reg;
  shogle::pipeline _pip;
  std::vector<u32> _mesh_mats;
  std::string _name;
  shogle::transform3d<f32> _transf;
};

} // namespace kappa
