#include "assets/model.hpp"
#include "render/context.hpp"
#include "render/glfw.hpp"
#include "render/scene.hpp"

namespace {

using namespace kappa;

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

fn extract_model_data(const assets::Model3DData& model) -> render::SceneData::MeshData {
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

class KappaContext {
public:
  KappaContext();
  ~KappaContext();

public:
  fn start() -> void;
  fn on_render(f64 dt, f64 alpha) -> void;
  fn on_fixed_update(u32 ups) -> void;

private:
  TypeBuffer<render::GLFWContext> _glfw;
  TypeBuffer<render::RenderContext> _renderer;
  TypeBuffer<render::SceneData> _scene;
};

KappaContext::KappaContext() {
  render::GLFWContext::initialize(_glfw, WINDOW_WIDTH, WINDOW_HEIGHT);
  render::RenderContext::initialize(_renderer, *_glfw);
  render::SceneData::initialize(_scene, *_renderer);
}

KappaContext::~KappaContext() {
  _scene.destroy();
  _renderer.destroy();
  _glfw.destroy();
}

fn KappaContext::start() -> void {
  auto suzanne = assets::Model3DLoader(KA_RES_DIR "/models/suzanne.glb", "suzanne")().value();
  const DeferFn suzanne_kill = [&]() {
    suzanne.destroy();
  };
  const auto model = extract_model_data(suzanne);
  _scene->add_mesh(model, suzanne.name().as_view());
  render::render_loop<60>(*_glfw, *this);
}

fn KappaContext::on_render(f64 dt, f64 alpha) -> void {
  _renderer->draw_things(*_scene, dt, alpha);
}

fn KappaContext::on_fixed_update(u32 ups) -> void {
  KA_UNUSED(ups);
}

} // namespace

int kappa::g_argc;
char** kappa::g_argv;

int main(int argc, char* argv[]) {
  g_argc = argc;
  g_argv = argv;
  try {
    KappaContext ka;
    ka.start();
  } catch (const std::exception& ex) {
    log_error(" {}", ex.what());
  } catch (...) {
    log_error(" Caught unknown exception");
  }
}
