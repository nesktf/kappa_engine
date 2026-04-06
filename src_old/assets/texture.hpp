#pragma once

#include "./common.hpp"

namespace kappa::assets {

struct texture {
  std::string name;
  shogle::texture2d texture;
};

DECLARE_ASSET(asset_tag::texture, texture, texture_handle);

} // namespace kappa::assets
