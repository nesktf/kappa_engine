#define KAPPA_MODEL_DATA_INL
#include "./model_data.hpp"
#undef KAPPA_MODEL_DATA_INL

namespace kappa::assets {

template<meta::mesh_data_type MeshDataT>
model_meshes<MeshDataT>::model_meshes(vert_buffs buffs, u32 vertex_count,
                                      shogle::index_buffer&& idx_buff, u32 index_count,
                                      ntf::unique_array<mesh_offset>&& offsets) :
    _buffs{buffs}, _offsets{std::move(offsets)}, _idx_buff{std::move(idx_buff)},
    _vertex_count{vertex_count}, _index_count{index_count} {
  NTF_ASSERT(!_idx_buff.empty());
  NTF_ASSERT(vertex_count > 0);
  for (u32 i = 0; i < _buffs.size(); ++i) {
    NTF_ASSERT(_buffs[i], "Vertex \"{}\" missing", MeshDataT::VERT_CONFIG[i].name);
    _binds[i].buffer = _buffs[i];
    _binds[i].layout = i;
  }
}

template<meta::mesh_data_type MeshDataT>
fn model_meshes<MeshDataT>::create(const MeshDataT& mesh_data) -> expect<model_meshes> {
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

template<meta::mesh_data_type MeshDataT>
fn model_meshes<MeshDataT>::retrieve_mesh_data(u32 mesh_idx,
                                               std::vector<render::mesh_render_data>& data) const
  -> render::mesh_render_data& {
  NTF_ASSERT(mesh_idx < mesh_count());
  cspan<shogle::vertex_binding> binds{_binds.data(), _binds.size()};
  const auto& offset = _offsets[mesh_idx];
  return data.emplace_back(binds, index_buffer(), offset.index_count, offset.vertex_offset,
                           offset.index_offset, 0u);
}

template<meta::mesh_data_type MeshDataT>
model_meshes<MeshDataT>::model_meshes(model_meshes&& other) noexcept :
    _buffs{std::move(other._buffs)}, _binds{std::move(other._binds)},
    _offsets{std::move(other._offsets)}, _idx_buff{std::move(other._idx_buff)},
    _vertex_count{std::move(other._vertex_count)}, _index_count{std::move(other._index_count)} {
  other._reset_buffs();
}

template<meta::mesh_data_type MeshDataT>
fn model_meshes<MeshDataT>::operator=(model_meshes&& other) noexcept -> model_meshes& {
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

template<meta::mesh_data_type MeshDataT>
void model_meshes<MeshDataT>::_reset_buffs() noexcept {
  std::memset(_buffs.data(), 0, _buffs.size() * sizeof(shogle::buffer_t));
  std::memset(_binds.data(), 0, _binds.size() * sizeof(shogle::vertex_binding));
  _index_count = 0u;
  _vertex_count = 0u;
}

template<meta::mesh_data_type MeshDataT>
void model_meshes<MeshDataT>::_free_buffs() noexcept {
  if (!_buffs[0]) {
    return;
  }
  for (shogle::buffer_t buff : _buffs) {
    if (buff) {
      shogle::destroy_buffer(buff);
    }
  }
}

} // namespace kappa::assets
