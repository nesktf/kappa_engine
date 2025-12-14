#include "assets/loader.hpp"
#include "physics/particle.hpp"
#include "renderer/camera.hpp"
#include "renderer/context.hpp"
#include "scene/model.hpp"
#include "util/interpolator.hpp"

#include <ntfstl/logger.hpp>
#include <ntfstl/utility.hpp>

namespace {

using namespace kappa;

static void run_engine() {
  auto _rh = render::initialize();
  auto fbo = render::default_fb();
  auto& win = render::window();

  bool do_things = true;

  camera cam;
  bool first_mouse = true;
  f32 last_cam_x = 0.f;
  f32 last_cam_y = 0.f;

  f32 fb_ratio = (f32)1280 / (f32)720;
  mat4 proj_mat = glm::perspective(glm::radians(90.f), fb_ratio, .1f, 100.f);

  win
    .set_viewport_callback([&](shogle::window&, const shogle::extent2d& ext) {
      fb_ratio = (f32)ext.x / (f32)ext.y;
      proj_mat = glm::perspective(glm::radians(90.f), fb_ratio, .1f, 100.f);
      fbo.viewport({0, 0, ext.x, ext.y});
    })
    .set_key_press_callback([&](shogle::window& win, const shogle::win_key_data& k) {
      if (k.action == shogle::win_action::press) {
        if (k.key == shogle::win_key::escape) {
          win.close();
        }
        if (k.key == shogle::win_key::enter) {
          do_things = !do_things;
        }
      }
    })
    .set_cursor_pos_callback([&](shogle::window&, shogle::dvec2 pos) {
      f32 xpos = (float)pos.x;
      f32 ypos = (float)pos.y;
      if (first_mouse) {
        last_cam_x = xpos;
        last_cam_y = ypos;
        first_mouse = false;
      }
      f32 xoff = xpos - last_cam_x;
      f32 yoff = last_cam_y - ypos; // inverted y
      last_cam_x = xpos;
      last_cam_y = ypos;
      cam.process_mouse_move(xoff, yoff);
    });

  // hack
  glfwSetInputMode((GLFWwindow*)win.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  assets::asset_loader loader;
  scene::entity_registry registry;
  const assets::asset_loader::model_opts cirno_opts{
    .flags = assets::assimp_parser::DEFAULT_ASS_FLAGS,
    .armature = "model",
  };
  const assets::asset_loader::model_opts koosh_opts{
    .flags = assets::assimp_parser::DEFAULT_ASS_FLAGS,
    .armature = "Koishi V1.0_arm",
  };
  const assets::asset_loader::model_opts mari_opts = cirno_opts;

  ntf::nullable<scene::ent_handle> cirno;
  decltype(cirno) cirno2;
  physics::particle_gravity gravity;
  const vec3 cirno_pos{-.9f, -.75f, -1.f};
  physics::particle_bungee_anchor spring{cirno_pos, 5.f, 1.f};
  const auto cirno_cb = [&](u32 model_idx) {
    cirno.emplace(registry.add_entity(model_idx, cirno_pos, 1.f));
    cirno2.emplace(registry.add_entity(model_idx, cirno_pos + vec3{-1.f, 0.f, 0.f}, 1.f));
    registry.add_force(*cirno, gravity);
    registry.add_force(*cirno, spring);
  };
  registry.request_model(loader, "./res/chiruno/chiruno.gltf", "cirno", cirno_opts, cirno_cb);

  decltype(cirno) koosh;
  const auto koosh_cb = [&](u32 model_idx) {
    const vec3 koosh_pos{.9f, -.75f, -1.f};
    koosh.emplace(registry.add_entity(model_idx, koosh_pos, 1.f));
  };
  registry.request_model(loader, "./res/koishi/koishi.gltf", "Koishi V1.0", koosh_opts, koosh_cb);

  decltype(cirno) mari;
  const auto mari_cb = [&](u32 model_idx) {
    const vec3 mari_pos{0, -.75f, -1.f};
    mari.emplace(registry.add_entity(model_idx, mari_pos, 1.f));
  };
  registry.request_model(loader, "./res/mari/mari.gltf", "marisa", mari_opts, mari_cb);

  auto scene_transf = [&]() {
    mat4 transf_mats[2u] = {
      proj_mat,
      cam.view(),
    };
    return render::create_ubo(sizeof(transf_mats), transf_mats).value();
  }();

  // auto rarm_transform = shogle::transform3d<f32>{}.scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);
  // auto larm_transform = shogle::transform3d<f32>{}.scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);
  // const quat q1{1.f, 0.f, 0.f, 0.f};
  // const auto q1 = shogle::axisquat(glm::radians(45.f), vec3{1.f, 0.f, 0.f});
  // const quat q2 = shogle::axisquat(glm::radians(-45.f), vec3{1.f, 0.f, 0.f});
  // steplerp<quat, f32, glm_mixer<quat, f32>, 120> rarm_rotlerp{q1, q2};
  // steplerp<quat, f32, glm_mixer<quat, f32>, 120> larm_rotlerp{q2, q1};
  //
  // const vec3 p1{0.f, 0.f, 0.f};
  // const vec3 p2{2.f, 0.f, 0.f};
  // steplerp<vec3, f32, easing_back_inout<vec3>, 60> poslerp{p1, p2};

  f32 t = 0.f;
  auto loop = ntf::overload{
    [&](u32 fdt) {
      f32 delta = 1 / (f32)fdt;
      loader.handle_requests();
      t += delta;

      if (win.poll_key(shogle::win_key::w) == shogle::win_action::press) {
        cam.process_keyboard(CAM_FORWARD, delta);
      } else if (win.poll_key(shogle::win_key::s) == shogle::win_action::press) {
        cam.process_keyboard(CAM_BACKWARD, delta);
      }

      if (win.poll_key(shogle::win_key::a) == shogle::win_action::press) {
        cam.process_keyboard(CAM_LEFT, delta);
      } else if (win.poll_key(shogle::win_key::d) == shogle::win_action::press) {
        cam.process_keyboard(CAM_RIGHT, delta);
      }

      if (win.poll_key(shogle::win_key::space) == shogle::win_action::press) {
        cam.process_keyboard(CAM_UP, delta);
      } else if (win.poll_key(shogle::win_key::lshift) == shogle::win_action::press) {
        cam.process_keyboard(CAM_DOWN, delta);
      }

      if (!do_things) {
        return;
      }
      // rotlerp.tick_loop();
      // rarm_transform.rot(rarm_rotlerp.value());
      // larm_transform.rot(larm_rotlerp.value());
      registry.update();

      // force_registry.update_forces(delta);
      //
      // for (auto& [idx, pos] : rmodels) {
      //   auto& model = bundle.get_rmodel(idx);
      //   // model.transform().pos(pos+*poslerp).rot(*rotlerp);
      //   model.set_transform("Arm_R", rarm_transform.local());
      //   model.set_transform("Arm_L", larm_transform.local());
      //   if (model.name() == "cirno") {
      //     chiruno_particle.integrate(delta);
      //     model.transform().pos(chiruno_particle.pos());
      //   }
      //   model.tick();
      // }
      // poslerp.tick_loop();
      // rarm_rotlerp.tick_loop();
      // larm_rotlerp.tick_loop();
    },
    [&](f32 dt, f32 alpha) {
      NTF_UNUSED(dt);
      NTF_UNUSED(alpha);
      const render::scene_render_data rdata{.transform = scene_transf};
      scene_transf.upload(proj_mat, 0u);
      scene_transf.upload(cam.view(), sizeof(mat4));
      render::render_thing(fbo, 0u, rdata, registry);
    }};
  render::render_loop(GAME_UPS, loop);
}

} // namespace

int main(int argc, char* argv[]) {
  NTF_UNUSED(argc);
  NTF_UNUSED(argv);
  ntf::logger::set_level(ntf::log_level::verbose);
  try {
    run_engine();
  } catch (const std::exception& ex) {
    ntf::logger::error("{}", ex.what());
    return EXIT_FAILURE;
  } catch (...) {
    ntf::logger::error("Caught (...)");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
