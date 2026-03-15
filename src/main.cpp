#include "./render/model.hpp"

namespace {

using namespace kappa;

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

void run_engine() {
  kappa::render::initialize(WINDOW_WIDTH, WINDOW_HEIGHT);
  const shogle::scope_end render_defer = []() {
    kappa::render::destroy();
  };

  kappa::assets::model3d_loader cirnoloader("./res/cirno_fumo/cirno_fumo.obj", "cirno");
  const auto cirno_data = cirnoloader.load().value();
  auto cirno_model = kappa::render::model3d_renderable::from_asset(cirno_data).value();

  const auto on_fixed_update = [&](u32 ups) {
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
