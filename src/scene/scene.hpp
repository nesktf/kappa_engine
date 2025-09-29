#pragma once

#include "../assets/rigged_model.hpp"
#include "../stash_list.hpp"

#include <list>

namespace kappa {

class scene_data {
private:
  std::list<rigged_model3d_asset> _rmodel_assets;
  stash_list<rigged_model3d> _rmodels;
};

class scene_handler : public ntf::singleton<scene_handler> {
private:
  friend ntf::singleton<scene_handler>;

  struct handle_t {
    ~handle_t() { renderer::destroy(); }
  };

private:
  scene_handler() noexcept : _curr_scene{ntf::nullopt} {}

public:
  static handle_t construct();

private:
  ntf::optional<scene_data> _curr_scene;
};

} // namespace kappa
