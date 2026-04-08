#include "./render/instance.hpp"

#include "./assets/model.hpp"

#include "./util/function.hpp"
#include "./util/logger.hpp"
#include "./util/threadpool.hpp"

namespace {

using namespace kappa;

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

fn run_engine() -> void {
  ThreadPool pool;
  render::initialize(WINDOW_WIDTH, WINDOW_HEIGHT);
  const DeferFn render_defer = []() {
    render::destroy();
  };

#if 0
  assets::Model3DLoader marisa_loader("./res/marisa_miy/marisa_miy.gltf", "marisa");
  auto marisa = marisa_loader.load().value();

  const fn filter_thing = [&](const assets::Model3DData::MeshData& mesh) {
    auto pos = mesh.name.as_view().find("Marisa Miy-");
    return pos != std::string::npos;
  };
#endif

  const fn on_fixed_update = [&](u32 ups) {
    (void)ups;
  };
  const fn on_render = [&](f64 dt, f64 alpha) {
    (void)dt;
    (void)alpha;
    render::start_frame();

    render::end_frame();
  };

  render::render_loop<60>(OverloadFn{on_fixed_update, on_render});

#if 0
  render::buffer_binding scene_buffer{
    .offset = 0,
    .size = 2 * sizeof(m4f32),
    .handle = render::create_buffer(2 * sizeof(m4f32)).value(),
  };
  const shogle::scope_end buffer_defer = [&]() {
    render::destroy_buffer(scene_buffer.handle);
  };

  const fn update_scene = [&](f32 w, f32 h) {
    const m4f32 proj = shogle::math::perspective(shogle::math::rad(90.f), w / h, 0.1f, 100.f);
    const m4f32 view(1.f);
    render::update_buffer(scene_buffer.handle, &proj, sizeof(proj), 0);
    render::update_buffer(scene_buffer.handle, &view, sizeof(view), sizeof(proj));
  };
  auto model = render::model3d_renderable::from_asset(marisa, filter_thing).value();
  vec<render::render_data> render_data;

  auto mari_instancer = render::model3d_instance_handler::create(model, 1).value();

#endif
}

} // namespace

int kappa::g_argc;
char** kappa::g_argv;

int main(int argc, char* argv[]) {
  g_argc = argc;
  g_argv = argv;
  try {
    run_engine();
  } catch (const std::exception& ex) {
    log_error(" {}", ex.what());
  }
}
