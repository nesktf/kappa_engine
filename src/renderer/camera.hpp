#pragma once

#include "../common.hpp"

namespace kappa {

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
    _yaw{glm::radians(-90.f)}, _pitch(0.f), _mouse_sens{.0025f} { _do_cam_vecs(); }

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

} // namespace kappa
