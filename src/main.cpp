#include "./render/model.hpp"
#include "./util/threadpool.hpp"
#include "shogle/extern/imgui.h"

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
  std::vector<render::render_data> render_data;

  const auto on_fixed_update = [&](u32 ups) {
    (void)ups;
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
    (void)dt;
    (void)alpha;
    render_data.clear();
    kappa::render::start_frame();

    ImGui::ShowDemoWindow();

    for (const auto& model : models) {
      const auto materials = model.materials();
      for (const auto& mesh : model.meshes()) {
        assert(mesh.material < materials.size());
        const auto& material = materials[mesh.material];
        kappa::render::render_data rdata{};
        rdata.mesh_buffer = model.mesh_buffer();
        rdata.draw_count = mesh.index_count ? mesh.index_count : mesh.vertex_count;
        rdata.pipeline = material.pipeline;

        rdata.instances = 1;
        rdata.shader_binds = {};
        rdata.uniforms = {};
        rdata.textures = {};
        render_data.push_back(rdata);
      }
    }
    span<const render::render_data> batch(render_data.data(), render_data.size());
    kappa::render::submit_render_batch(batch);

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
