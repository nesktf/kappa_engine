#pragma once

#include "./model_data.hpp"

namespace kappa {

struct static_model_data {
  model_mesh_data meshes;
  model_material_data materials;
};

class static_model_mesher {
protected:
  enum attr_idx {
    ATTRIB_POS = 0,
    ATTRIB_NORM,
    ATTRIB_UVS,
    ATTRIB_TANG,
    ATTRIB_BITANG,

    ATTRIB_COUNT,
  };

public:
  using vert_buffs = std::array<shogle::buffer_t, ATTRIB_COUNT>;
  using vert_binds = std::array<shogle::vertex_binding, ATTRIB_COUNT>;

};

class static_model3d {

};

} // namespace kappa
