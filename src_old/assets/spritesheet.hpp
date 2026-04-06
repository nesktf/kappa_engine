#pragma once

#include "./texture.hpp"
#include <chimatools/chimatools.hpp>

namespace kappa::assets {

struct spritesheet {
  std::string name;
  texture_handle texture;
  ntf::unique_array<chima_sprite> sprites;
  ntf::unique_array<chima_sprite_anim> anims;
};

DECLARE_ASSET(asset_tag::spritesheet, spritesheet, spritesheet_handle);

} // namespace kappa::assets
