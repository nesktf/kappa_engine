#include "renderer.hpp"
#include "assets.hpp"
#include "model_data.hpp"
#include "model.hpp"
#include "interpolator.hpp"
#include <ranges>
#include "camera.hpp"

#include <ntfstl/logger.hpp>
#include <ntfstl/utility.hpp>

using namespace ntf::numdefs;

int main() {
  ntf::logger::set_level(ntf::log_level::verbose);
  auto rh__ = renderer::construct();

  auto& r = renderer::instance();
  auto fbo = shogle::framebuffer::get_default(r.ctx());

  bool do_things = true;

  camera cam;
  bool first_mouse = true;
  f32 last_cam_x = 0.f;
  f32 last_cam_y = 0.f;

  r.win().set_viewport_callback([&](shogle::window&, const shogle::extent2d& ext) {
    fbo.viewport({0, 0, ext.x, ext.y});
  }).set_key_press_callback([&](shogle::window& win, const shogle::win_key_data& k) {
    if (k.action == shogle::win_action::press) {
      if (k.key == shogle::win_key::escape) {
        win.close();
      }
      if (k.key == shogle::win_key::enter) {
        do_things = !do_things;
      }
    }
  }).set_cursor_pos_callback([&](shogle::window&, shogle::dvec2 pos) {
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
  glfwSetInputMode((GLFWwindow*)r.win().get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  asset_bundle bundle;
  asset_loader loader;
  const asset_loader::model_opts koosh_opts {
    .flags = assimp_parser::DEFAULT_ASS_FLAGS,
    .armature = "Koishi V1.0_arm",
  };
  const asset_loader::model_opts cirno_opts {
    .flags = assimp_parser::DEFAULT_ASS_FLAGS,
    .armature = "model",
  };

  std::vector<std::pair<asset_bundle::rmodel_idx, vec3>> rmodels;
  auto model_callback = [&rmodels](expect<u32> idx, asset_bundle& bundle) {
    if (!idx) {
      ntf::logger::error("Can't load model, {}", idx.error());
      return;
    }

    const auto id = static_cast<asset_bundle::rmodel_idx>(*idx);
    auto& m = bundle.get_rmodel(id);
    vec3 pos;
    if (m.name() == "Koishi V1.0") {
      pos = {.9f, -.75f, 0.f};
    } else if (m.name() == "cirno") {
      pos = {-.9f, -.75f, 0.f};
    } else {
      pos = {0, -.75f, 0.f};
    }
    m.transform().pos(pos).scale(1.f, 1.f, 1.f);
    rmodels.emplace_back(id, pos);
  };
  
  loader.request_rmodel(bundle, "./res/chiruno/chiruno.gltf", "cirno",
                        cirno_opts, model_callback);
  loader.request_rmodel(bundle, "./res/koishi/koishi.gltf", "Koishi V1.0",
                        koosh_opts, model_callback);
  loader.request_rmodel(bundle, "./res/mari/mari.gltf", "marisa",
                        cirno_opts, model_callback);
  // loader.request_rmodel(bundle, "./lib/shogle/demos/res/cirno_fumo/cirno_fumo.obj", "fumo",
  //                       cirno_opts, model_callback);

  const auto fb_ratio = (float)1280/(float)720;
  
  auto bone_transform = shogle::transform3d<f32>{}
    .scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);

  mat4 transf_mats[2u] = {
    glm::perspective(glm::radians(90.f), fb_ratio, .1f, 100.f), // proj
    cam.view(),
  };
  const shogle::buffer_data scene_transf_data {
    .data = transf_mats,
    .size = sizeof(transf_mats),
    .offset = 0u,
  };
  auto scene_transf = shogle::uniform_buffer::create(r.ctx(),{
    .flags = shogle::buffer_flag::dynamic_storage,
    .size = sizeof(transf_mats),
    .data = scene_transf_data,
  }).value();

  const quat q1{1.f, 0.f, 0.f, 0.f};
  const quat q2{0.f, 0.f, 0.f, 1.f};
  steplerp<quat, f32, glm_mixer<quat, f32>, 60> rotlerp{q1, q2};

  const vec3 p1{0.f, 0.f, 0.f};
  const vec3 p2{2.f, 0.f, 0.f};
  steplerp<vec3, f32, easing_back_inout<vec3>, 60> poslerp{p1, p2};

  r.render_loop(ntf::overload{
    [&](u32 fdt) {
      f32 delta = 1/(f32)fdt;
      loader.handle_requests();

      if (r.win().poll_key(shogle::win_key::w) == shogle::win_action::press) {
        cam.process_keyboard(CAM_FORWARD, delta);
      } else if (r.win().poll_key(shogle::win_key::s) == shogle::win_action::press) {
        cam.process_keyboard(CAM_BACKWARD, delta);
      }

      if (r.win().poll_key(shogle::win_key::a) == shogle::win_action::press){
        cam.process_keyboard(CAM_LEFT, delta);
      } else if (r.win().poll_key(shogle::win_key::d) == shogle::win_action::press) {
        cam.process_keyboard(CAM_RIGHT, delta);
      }

      if (r.win().poll_key(shogle::win_key::space) == shogle::win_action::press){
        cam.process_keyboard(CAM_UP, delta);
      } else if (r.win().poll_key(shogle::win_key::lshift) == shogle::win_action::press) {
        cam.process_keyboard(CAM_DOWN, delta);
      }

      if (!do_things) {
        return;
      }
      // rotlerp.tick_loop();
      // bone_transform.rot(rotlerp.value());

      for (auto& [idx, pos] : rmodels) {
        auto& model = bundle.get_rmodel(idx);
        // model.transform().pos(pos+*poslerp).rot(*rotlerp);
        model.set_transform("UpperBody", bone_transform);
        model.tick();
      }
      // poslerp.tick_loop();
      // rotlerp.tick_loop();
    },
    [&](f32 dt, f32 alpha) {
      NTF_UNUSED(dt);
      NTF_UNUSED(alpha);

      const scene_render_data rdata {.transform = scene_transf};
      mat4 cam_view = cam.view();
      scene_transf.upload(cam_view, sizeof(mat4));

      for (auto& [idx,_] : rmodels) {
        r.render(fbo, 0u, rdata, bundle.get_rmodel(idx));
      }
    }
  });

  return EXIT_SUCCESS;
}
