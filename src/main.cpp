#include "./assets/model.hpp"
#include "./render/instance.hpp"

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
  const auto cirno = cirnoloader().value();

  static constexpr size_t MAX_MESH_TEXTURES = 16;

  struct mesh_data {
    size_t nverts;
    const v3f32* positions;
    const v3f32* normals;
    const v2f32* uvs;
    const v3f32* tangents;
    const v3f32* bitangents;
    std::array<u32, MAX_MESH_TEXTURES> textures;
    u32 texture_count;
    kappa::assets::model3d_data::material_shading shading;
  };

  struct texture_data {
    buffer_path path;
    const void* data;
    extent2d extent;
    kappa::assets::image_format format;
    kappa::assets::texture_type type;
  };

  struct material_data {};

  std::vector<texture_data> tex_map;

  std::vector<mesh_data> meshes;
  for (const auto& mesh : cirno.meshes()) {
    mesh_data data{};
    data.nverts = (size_t)mesh.nverts;
    data.positions = cirno.mesh_positions(mesh.positions()).data();

    assert(mesh.has_normals());
    data.normals = cirno.mesh_normals(mesh.normals()).data();

    assert(mesh.has_uvs(0));
    data.uvs = cirno.mesh_uvs(0, mesh.uvs(0)).data();

    assert(mesh.has_tangents());
    data.tangents = cirno.mesh_tangents(mesh.tangents()).data();
    data.bitangents = cirno.mesh_bitangents(mesh.tangents()).data();

    const auto& mat = cirno.material_at(mesh.material_index);
    const auto texes = cirno.material_textures(mesh.material_index);
    for (const auto& tex : texes) {
      auto it = std::find_if(tex_map.begin(), tex_map.end(), [&](const auto& the_tex) {
        return the_tex.path.as_view() == tex.path.as_view();
      });
      texture_data texdata{};
      texdata.path = tex.path;
      texdata.data = tex.data;
      texdata.extent = tex.extent;
      texdata.format = tex.format;
      texdata.type = tex.type;
    }

    meshes.emplace_back(data);
  }

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
