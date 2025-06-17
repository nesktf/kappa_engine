#pragma once

#include "model_data.hpp"

struct mesh_render_data {
  cspan<ntfr::vertex_binding> vertex_buffers;
  ntfr::buffer_t index_buffer;
  u32 vertex_count;
  u32 vertex_offset;
  u32 index_offset;
  u32 sort_offset;
};

struct mesh_provider {
  virtual ~mesh_provider() = default;
  virtual u32 retrieve_render_data(std::vector<mesh_render_data>& data) = 0;
  virtual u32 retrieve_bindings(std::vector<ntfr::attribute_binding>& bindings) = 0;
};


class model_mesh_provider : public mesh_provider {
public:
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

  using mesh_vert_buffs = std::array<ntfr::buffer_t, ATTRIB_COUNT>;
  using mesh_vert_binds = std::array<ntfr::vertex_binding, ATTRIB_COUNT>;

private:
  struct mesh_offset {
    u32 index_offset;
    u32 index_count;
    u32 vertex_offset;
  };

public:
  model_mesh_provider(mesh_vert_buffs buffs,
                      ntfr::buffer_t index_buff, std::vector<mesh_offset>&& mesh_offsets) noexcept;

  ~model_mesh_provider() noexcept;

  model_mesh_provider(const model_mesh_provider&) = delete;
  model_mesh_provider(model_mesh_provider&& other) noexcept;

public:
  model_mesh_provider& operator=(const model_mesh_provider&) = delete;
  model_mesh_provider& operator=(model_mesh_provider&&) noexcept;

public:
  static ntfr::expect<model_mesh_provider> create(ntfr::context_view ctx,
                                                  const model_mesh_data& meshes);

public:
  u32 retrieve_render_data(std::vector<mesh_render_data>& data) override;
  u32 retrieve_bindings(std::vector<ntfr::attribute_binding>& bindings) override;

private:
  void _free_buffs() noexcept;

private:
  mesh_vert_buffs _buffs;
  mesh_vert_binds _binds;
  std::vector<mesh_offset> _mesh_offsets;
  ntfr::buffer_t _index_buff;
  u32 _active_layouts;
};
