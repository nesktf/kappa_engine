#pragma once

#include "./loader2.hpp"

namespace kappa::assets {

fn rigged_model_data::vertex_count() const -> u32 {
  return meshes.positions.size();
}

fn rigged_model_data::index_count() const -> u32 {
  return meshes.indices.size();
}

fn rigged_model_data::mesh_count() const -> u32 {
  return meshes.meshes.size();
}

fn rigged_model_data::mesh_index_range(u32 mesh_idx) const -> vec_span {
  NTF_ASSERT(mesh_idx < meshes.meshes.size());
  return meshes.meshes[mesh_idx].indices;
}

fn rigged_model_data::index_data() const -> cspan<u32> {
  return {meshes.indices.data(), meshes.indices.size()};
}

fn rigged_model_data::vertex_data(u32 attr_idx, u32 mesh_idx) const -> mesh_attr_data {
  enum {
    ATTR_POS = 0,
    ATTR_NORM,
    ATTR_UVS,
    ATTR_TANG,
    ATTR_BITANG,
    ATTR_BONES,
    ATTR_WEIGHTS,

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
    case ATTR_BONES: {
      const auto span = mesh_meta.bones.to_cspan(meshes.bones.data());
      return std::make_pair(static_cast<const void*>(span.data()), span.size());
    } break;
    case ATTR_WEIGHTS: {
      const auto span = mesh_meta.bones.to_cspan(meshes.weights.data());
      return std::make_pair(static_cast<const void*>(span.data()), span.size());
    } break;
    default:
      break;
  }
  NTF_UNREACHABLE();
}

fn static_model_data::vertex_count() const -> u32 {
  return meshes.positions.size();
}

fn static_model_data::index_count() const -> u32 {
  return meshes.indices.size();
}

fn static_model_data::mesh_count() const -> u32 {
  return meshes.meshes.size();
}

fn static_model_data::mesh_index_range(u32 mesh_idx) const -> vec_span {
  NTF_ASSERT(mesh_idx < meshes.meshes.size());
  return meshes.meshes[mesh_idx].indices;
}

fn static_model_data::index_data() const -> cspan<u32> {
  return {meshes.indices.data(), meshes.indices.size()};
}

fn static_model_data::vertex_data(u32 attr_idx, u32 mesh_idx) const -> mesh_attr_data {
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

} // namespace kappa::assets
