#include "./static_model.hpp"

namespace kappa {

std::pair<const void*, u32> static_model_data::vertex_data(u32 attr_idx, u32 mesh_idx) const {
  enum {
    ATTR_POS = 0,
    ATTR_NORM,
    ATTR_UVS,
    ATTR_TANG,
    ATTR_BITANG,

    ATTR_COUNT
  };

  NTF_ASSERT(mesh_idx < meshes.meshes.size());
  if (attr_idx >= ATTR_COUNT) {
    return std::make_pair(nullptr, 0u);
  }

  const auto& mesh_meta = meshes.meshes[mesh_idx];
  switch (attr_idx) {
    case ATTR_POS: {
      const auto span = mesh_meta.positions.to_cspan(meshes.positions.data());
      return std::make_pair(static_cast<const void*>(span.data()), span.size());
    } break;
    case ATTR_NORM: {
      const auto span = mesh_meta.normals.to_cspan(meshes.normals.data());
      return std::make_pair(static_cast<const void*>(span.data()), span.size());
    } break;
    case ATTR_UVS: {
      const auto span = mesh_meta.uvs.to_cspan(meshes.uvs.data());
      return std::make_pair(static_cast<const void*>(span.data()), span.size());
    } break;
    case ATTR_TANG: {
      const auto span = mesh_meta.tangents.to_cspan(meshes.tangents.data());
      return std::make_pair(static_cast<const void*>(span.data()), span.size());
    } break;
    case ATTR_BITANG: {
      const auto span = mesh_meta.tangents.to_cspan(meshes.bitangents.data());
      return std::make_pair(static_cast<const void*>(span.data()), span.size());
    } break;
    default:
      break;
  }
  NTF_UNREACHABLE();
}

} // namespace kappa
