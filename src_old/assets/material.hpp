#pragma once

#include "./texture.hpp"

namespace kappa::assets {

enum material_texture_type {
  MATERIAL_TEX_ALBEDO = 0,
  MATERIAL_TEX_COUNT,
};

struct material {
  std::string name;
  shogle::pipeline pipeline;
  std::array<texture_handle, MATERIAL_TEX_COUNT> textures;
};

DECLARE_ASSET(asset_tag::material, material, material_handle);

} // namespace kappa::assets
