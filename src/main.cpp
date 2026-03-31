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

  kappa::assets::model3d_loader marisa_loader("./res/marisa_miy/marisa_miy.gltf", "marisa");
  auto marisa = marisa_loader.load().value();
  const fn filter_thing = [&](const kappa::assets::model3d_data::mesh_data& mesh) {
    auto pos = mesh.name.as_view().find("Marisa Miy-");
    return pos != std::string::npos;
  };
  auto model = kappa::render::model3d_renderable::from_asset(marisa, filter_thing).value();
  std::vector<render::render_data> render_data;

  const auto on_fixed_update = [&](u32 ups) {
    (void)ups;
  };
  const auto on_render = [&](f64 dt, f64 alpha) {
    (void)dt;
    (void)alpha;
    render_data.clear();
    kappa::render::start_frame();

    // ImGui::ShowDemoWindow();

    const auto materials = model.materials();
    size_t pos = 0;
    for (const auto& mesh : model.meshes()) {
      assert(mesh.material < materials.size());
      const auto& material = materials[pos++];
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
