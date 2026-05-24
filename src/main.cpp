#include "./assets/model.hpp"
#include "./render/context.hpp"
#include "./render/glfw.hpp"

#include "./util/logger.hpp"

namespace {

using namespace kappa;

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

fn extract_model_data(const assets::Model3DData& model) -> render::RenderContext::MeshData {
  const auto& mesh = model.mesh_at(0);
  // FIXME: model.mesh_*(ArrayRange) doesn't work properly, doing this manually
  const auto [idx_start, idx_count] = mesh.indices();
  const auto [pos_start, pos_count] = mesh.positions();
  const auto [norm_start, norm_count] = mesh.normals();
  const auto [uv_start, uv_count] = mesh.uvs(0);
  const auto [tang_start, tang_count] = mesh.tangents();

  // Make sure we have the same amount of vertices
  ka_assert(pos_count == norm_count);
  ka_assert(pos_count == uv_count);
  ka_assert(tang_count == pos_count);

  const auto indices = model.mesh_indices();
  const auto positions = model.mesh_positions();
  const auto normals = model.mesh_normals();
  const auto uvs = model.mesh_uvs(0);
  const auto tangents = model.mesh_tangents();
  const auto bitangents = model.mesh_bitangents();
  return {
    .indices = {indices.data() + idx_start, idx_count},
    .positions = {positions.data() + pos_start, pos_count},
    .normals = {normals.data() + norm_start, norm_count},
    .uvs = {uvs.data() + uv_start, uv_count},
    .tangents = {tangents.data() + tang_start, tang_count},
    .bitangents = {bitangents.data() + tang_start, tang_count},
  };
}

fn run_engine() -> void {
  auto glfw = render::GLFWContext::create(WINDOW_WIDTH, WINDOW_HEIGHT);
  const DeferFn glfw_defer = [&]() {
    glfw.destroy();
  };
  auto vk = render::RenderContext::create(glfw).value();
  const DeferFn rctx_defer = [&]() {
    vk.destroy();
  };

  auto suzanne = assets::Model3DLoader(KA_RES_DIR "/models/suzanne.glb", "suzanne")().value();
  const DeferFn suzanne_kill = [&]() {
    suzanne.destroy();
  };
  const auto model = extract_model_data(suzanne);
  vk.add_mesh(model, suzanne.name().as_view());

  render::render_loop<60>(glfw, vk);
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
