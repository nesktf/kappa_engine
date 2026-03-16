#include "./render/model.hpp"
#include "./util/threadpool.hpp"

namespace {

using namespace kappa;
using namespace std::literals;

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

void run_engine() {
  kappa::thread_pool pool;
  kappa::render::initialize(WINDOW_WIDTH, WINDOW_HEIGHT);
  const shogle::scope_end render_defer = []() {
    kappa::render::destroy();
  };

  std::vector<std::future<kappa::ass_expect<kappa::assets::model3d_data>>> model_futures;
  model_futures.push_back(pool.enqueue_future(std::in_place_type<kappa::assets::model3d_loader>,
                                              "./res/marisa_miy/marisa_miy.gltf", "marisa"));
  std::vector<kappa::render::model3d_renderable> models;
  const auto on_fixed_update = [&](u32 ups) {
    for (auto& fut : model_futures) {
      if (fut.wait_for(0s) == std::future_status::ready) {
        if (auto data = fut.get()) {
          auto model = kappa::render::model3d_renderable::from_asset(*data).value();
          models.push_back(std::move(model));
        } else {
          logger::error("Failed to load model \"{}\" at path \"{}\": {}", data.error().name(),
                        data.error().path(), data.error().msg());
        }
      }
    }
    std::erase_if(model_futures, [](const auto& fut) { return !fut.valid(); });
  };
  const auto on_render = [&](f64 dt, f64 alpha) {
    kappa::render::start_frame();
    kappa::render::end_frame();
  };

  shogle::render_loop<60>(kappa::render::window(), kappa::overload{on_fixed_update, on_render});
}

} // namespace

int kappa::g_argc;
char** kappa::g_argv;

int main(int argc, char* argv[]) {
  kappa::g_argc = argc;
  kappa::g_argv = argv;
  shogle::logger::set_level(shogle::logger::LEVEL_VERBOSE);
  try {
    run_engine();
  } catch (shogle::bad_expected_access<kappa::buffer_str<256>>& err) {
    kappa::logger::error("{}", err.error().as_view());
  }
}
