#include "assets/loader.hpp"
#include "interpolator.hpp"
#include "physics/particle.hpp"
#include "renderer/camera.hpp"
#include "renderer/context.hpp"

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

  asset_bundle bundle;
  asset_loader loader;
  const asset_loader::model_opts koosh_opts{
    .flags = assimp_parser::DEFAULT_ASS_FLAGS,
    .armature = "Koishi V1.0_arm",
  };
  const asset_loader::model_opts cirno_opts{
    .flags = assimp_parser::DEFAULT_ASS_FLAGS,
    .armature = "model",
  };

  std::vector<std::pair<asset_bundle::rmodel_idx, vec3>> rmodels;

  physics::particle_force_registry force_registry;
  physics::particle_gravity gravity;

  physics::particle_entity chiruno_particle{vec3{-.9f, -.75f, -1.f}, 1.f};
  physics::particle_bungee_anchor spring{chiruno_particle.pos(), 5.f, 1.f};

  auto model_callback = [&](expect<u32> idx, asset_bundle& bundle) {
    if (!idx) {
      ntf::logger::error("Can't load model, {}", idx.error());
      return;
    }

    const auto id = static_cast<asset_bundle::rmodel_idx>(*idx);
    auto& m = bundle.get_rmodel(id);
    vec3 pos;
    if (m.name() == "Koishi V1.0") {
      pos = {.9f, -.75f, -1.f};
    } else if (m.name() == "cirno") {
      pos = {-.9f, -.75f, -1.f};
      force_registry.add_force(chiruno_particle, gravity);
      force_registry.add_force(chiruno_particle, spring);
    } else {
      pos = {0, -.75f, -1.f};
    }
    m.transform().pos(pos).scale(1.f, 1.f, 1.f);
    rmodels.emplace_back(id, pos);
  };

  loader.request_rmodel(bundle, "./res/chiruno/chiruno.gltf", "cirno", cirno_opts, model_callback);
  loader.request_rmodel(bundle, "./res/koishi/koishi.gltf", "Koishi V1.0", koosh_opts,
                        model_callback);
  loader.request_rmodel(bundle, "./res/mari/mari.gltf", "marisa", cirno_opts, model_callback);
  // loader.request_rmodel(bundle, "./lib/shogle/demos/res/cirno_fumo/cirno_fumo.obj", "fumo",
  //                       cirno_opts, model_callback);

  auto rarm_transform = shogle::transform3d<f32>{}.scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);
  auto larm_transform = shogle::transform3d<f32>{}.scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);

  auto scene_transf = [&]() {
    mat4 transf_mats[2u] = {
      proj_mat,
      cam.view(),
    };
    return render::create_ubo(sizeof(transf_mats), transf_mats).value();
  }();

  // const quat q1{1.f, 0.f, 0.f, 0.f};
  const auto q1 = shogle::axisquat(glm::radians(45.f), vec3{1.f, 0.f, 0.f});
  const quat q2 = shogle::axisquat(glm::radians(-45.f), vec3{1.f, 0.f, 0.f});
  steplerp<quat, f32, glm_mixer<quat, f32>, 120> rarm_rotlerp{q1, q2};
  steplerp<quat, f32, glm_mixer<quat, f32>, 120> larm_rotlerp{q2, q1};

  const vec3 p1{0.f, 0.f, 0.f};
  const vec3 p2{2.f, 0.f, 0.f};
  steplerp<vec3, f32, easing_back_inout<vec3>, 60> poslerp{p1, p2};

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
      rarm_transform.rot(rarm_rotlerp.value());
      larm_transform.rot(larm_rotlerp.value());
      force_registry.update_forces(delta);

      for (auto& [idx, pos] : rmodels) {
        auto& model = bundle.get_rmodel(idx);
        // model.transform().pos(pos+*poslerp).rot(*rotlerp);
        model.set_transform("Arm_R", rarm_transform);
        model.set_transform("Arm_L", larm_transform);
        if (model.name() == "cirno") {
          chiruno_particle.integrate(delta);
          model.transform().pos(chiruno_particle.pos());
        }
        model.tick();
      }
      // poslerp.tick_loop();
      rarm_rotlerp.tick_loop();
      larm_rotlerp.tick_loop();
    },
    [&](f32 dt, f32 alpha) {
      NTF_UNUSED(dt);
      NTF_UNUSED(alpha);

      const render::scene_render_data rdata{.transform = scene_transf};
      scene_transf.upload(proj_mat, 0u);
      scene_transf.upload(cam.view(), sizeof(mat4));

      for (auto& [idx, _] : rmodels) {
        render::render_thing(fbo, 0u, rdata, bundle.get_rmodel(idx));
      }
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
