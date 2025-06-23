#include "renderer.hpp"
#include "assets.hpp"
#include "model_data.hpp"
#include "model.hpp"
#include <ranges>

using namespace ntf::numdefs;

enum cam_movement {
  CAM_FORWARD,
  CAM_BACKWARD,
  CAM_LEFT,
  CAM_RIGHT,
  CAM_UP,
  CAM_DOWN,
};

class camera {
public:
  camera(vec3 pos_ = {0.f, 0.f, 0.f}, vec3 world_up_ = {0.f, 1.f, 0.f}) :
    pos{pos_}, world_up{world_up_}, move_speed{2.5f},
    _yaw{glm::radians(-90.f)}, _pitch(0.f), _mouse_sens{.0025f} {}

public:
  void process_mouse_move(f32 xoff, f32 yoff, bool clamp_pitch = true) {
    xoff *= _mouse_sens;
    yoff *= _mouse_sens;

    _yaw += xoff;
    _pitch += yoff;

    if (clamp_pitch) {
      constexpr f32 pitch_max = glm::radians(89.f);
      _pitch = glm::clamp(_pitch, -pitch_max, pitch_max);
    }

    _do_cam_vecs();
  }

  void process_keyboard(cam_movement movement, f32 delta) {
    f32 vel = move_speed*delta;
    if (movement == CAM_FORWARD) {
      pos += _front*vel;
    } else if (movement == CAM_BACKWARD) {
      pos -= _front*vel;
    } else if (movement == CAM_LEFT){
      pos -= _right*vel;
    } else if (movement == CAM_RIGHT) {
      pos += _right*vel;
    } else if (movement == CAM_UP) {
      pos += world_up*vel;
    } else if (movement == CAM_DOWN){
      pos -= world_up*vel;
    }
  }

  mat4 view() const {
    return glm::lookAt(pos, pos+_front, _up);
  }

private:
  void _do_cam_vecs() {
    const vec3 front {
      glm::cos(_yaw)*glm::cos(_pitch),
      glm::sin(_pitch),
      glm::sin(_yaw)*glm::cos(_pitch),
    };
    const vec3 right = glm::normalize(glm::cross(front, world_up));

    _front = glm::normalize(front);
    _right = right;
    _up = glm::normalize(glm::cross(right, front));
  }

public:
  vec3 pos;
  vec3 world_up;
  f32 move_speed;

private:
  vec3 _front;
  vec3 _up;
  vec3 _right;
  f32 _yaw;
  f32 _pitch;
  f32 _mouse_sens;
};

int main() {
  ntf::logger::set_level(ntf::log_level::debug);
  auto rh__ = renderer::construct();

  auto& r = renderer::instance();
  auto fbo = ntfr::framebuffer::get_default(r.ctx());

  bool do_things = true;

  camera cam;
  bool first_mouse = true;
  f32 last_cam_x = 0.f;
  f32 last_cam_y = 0.f;

  r.win().set_viewport_callback([&](ntfr::window&, const ntf::extent2d& ext) {
    fbo.viewport({0, 0, ext.x, ext.y});
  }).set_key_press_callback([&](ntfr::window& win, const ntfr::win_key_data& k) {
    if (k.action == ntfr::win_action::press) {
      if (k.key == ntfr::win_key::escape) {
        win.close();
      }
      if (k.key == ntfr::win_key::enter) {
        do_things = !do_things;
      }
    }
  }).set_cursor_pos_callback([&](ntfr::window&, ntf::dvec2 pos) {
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
  const asset_loader::model_opts cirno_opts {
    .flags = assimp_parser::DEFAULT_ASS_FLAGS,
    .armature = "model",
  };

  std::vector<asset_bundle::rmodel_idx> rmodels;
  auto model_callback = [&rmodels](expect<u32> idx, asset_bundle& bundle) {
    if (!idx) {
      ntf::logger::error("Can't load model, {}", idx.error());
      return;
    }

    auto model = rmodels.emplace_back(static_cast<asset_bundle::rmodel_idx>(*idx));
    auto& m = bundle.get_rmodel(model);
    if (m.name() == "koishi") {
      m.transform().pos(.9f, -.75f, 0.f).scale(1.f, 1.f, 1.f);
    } else if (m.name() == "chiruno") {
      m.transform().pos(-.9f, -.75f, 0.f).scale(1.f, 1.f, 1.f);
    } else {
      m.transform().pos(0.f, -.75f, 0.f).scale(1.f, 1.f, 1.f);
    }
  };
  
  loader.request_rmodel(bundle, "./res/chiruno/chiruno.gltf", "chiruno",
                        cirno_opts, model_callback);
  loader.request_rmodel(bundle, "./res/koosh/koosh.gltf", "koishi",
                        cirno_opts, model_callback);
  loader.request_rmodel(bundle, "./res/mari/mari.gltf", "marisa",
                        cirno_opts, model_callback);
  // loader.request_rmodel(bundle, "./lib/shogle/demos/res/cirno_fumo/cirno_fumo.obj", "fumo",
  //                       cirno_opts, model_callback);

  const auto fb_ratio = (float)1280/(float)720;
  
  auto bone_transform = ntf::transform3d<f32>{}
    .scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);

  mat4 transf_mats[2u] = {
    glm::perspective(glm::radians(90.f), fb_ratio, .1f, 100.f), // proj
    cam.view(),
  };
  const ntfr::buffer_data scene_transf_data {
    .data = transf_mats,
    .size = sizeof(transf_mats),
    .offset = 0u,
  };
  auto scene_transf = ntfr::uniform_buffer::create(r.ctx(),{
    .flags = ntfr::buffer_flag::dynamic_storage,
    .size = sizeof(transf_mats),
    .data = scene_transf_data,
  }).value();

  float t = 0.f;
  r.render_loop(ntf::overload{
    [&](u32 fdt) {
      f32 delta = 1/(f32)fdt;
      loader.handle_requests();

      if (r.win().poll_key(ntfr::win_key::w) == ntfr::win_action::press) {
        cam.process_keyboard(CAM_FORWARD, delta);
      } else if (r.win().poll_key(ntfr::win_key::s) == ntfr::win_action::press) {
        cam.process_keyboard(CAM_BACKWARD, delta);
      }

      if (r.win().poll_key(ntfr::win_key::a) == ntfr::win_action::press){
        cam.process_keyboard(CAM_LEFT, delta);
      } else if (r.win().poll_key(ntfr::win_key::d) == ntfr::win_action::press) {
        cam.process_keyboard(CAM_RIGHT, delta);
      }

      if (r.win().poll_key(ntfr::win_key::space) == ntfr::win_action::press){
        cam.process_keyboard(CAM_UP, delta);
      } else if (r.win().poll_key(ntfr::win_key::lshift) == ntfr::win_action::press) {
        cam.process_keyboard(CAM_DOWN, delta);
      }

      if (!do_things) {
        return;
      }

      t += delta;
      // bone_transform.rot(ntf::vec3{-t*M_PIf*.5f, 0.f, 0.f});

      for (auto idx : rmodels) {
        auto& model = bundle.get_rmodel(idx);
        model.transform().rot(ntf::vec3{t*M_PIf*.5f, 0.f, 0.f});
        model.set_bone_transform("Head", bone_transform);
        model.tick();
      }
    },
    [&](f32 dt, f32 alpha) {
      const game_frame r {
        .fbo = fbo,
        .stransf = scene_transf,
        .dt = dt,
        .alpha = alpha,
      };
      mat4 cam_view = cam.view();
      scene_transf.upload(cam_view, sizeof(mat4));

      for (auto idx : rmodels) {
        bundle.get_rmodel(idx).render(r);
      }
    }
  });

  return EXIT_SUCCESS;
}
