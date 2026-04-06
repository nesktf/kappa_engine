#pragma once

#include "./matrix4x4.hpp"

namespace kappa::math {

template<typename Func, typename T>
concept cam3d_view_func =
  requires(const Func f, const ::kappa::VecNum<3, typename Func::value_type>& pos,
           const ::kappa::VecNum<3, typename Func::value_type>& target,
           const ::kappa::VecNum<3, typename Func::value_type>& up) {
    { f(pos, target, up) } -> std::same_as<::kappa::Mat<4, 4, typename Func::value_type>>;
    requires noexcept(f(pos, target, up));
    std::convertible_to<typename Func::value_type, T>;
  };

template<numeric_type T>
struct Cam3DLookatRHS {
public:
  using value_type = T;

public:
  KA_MATH_DEF Mat<4, 4, T> operator()(const ::kappa::VecNum<3, T>& pos,
                                      const ::kappa::VecNum<3, T>& target,
                                      const ::kappa::VecNum<3, T>& up) const noexcept {
    return ::kappa::math::lookat_rh(pos, target, up);
  }
};

template<numeric_type T>
struct Cam3DLookatLHS {
public:
  using value_type = T;

public:
  KA_MATH_DEF Mat<4, 4, T> operator()(const ::kappa::VecNum<3, T>& pos,
                                      const ::kappa::VecNum<3, T>& target,
                                      const ::kappa::VecNum<3, T>& up) const noexcept {
    return ::kappa::math::lookat_lh(pos, target, up);
  }
};

enum class cam3d_movement {
  forward = 0,
  backward,
  left,
  right,
  up,
  down,
};

template<numeric_type T, cam3d_view_func<T> ViewFunc = Cam3DLookatRHS<T>>
struct Camera3DEuler {
public:
  using value_type = T;
  using func_type = ViewFunc;

public:
  Camera3DEuler() noexcept :
      pos(T(0), T(0), T(0)), world_up(T(0), T(1), T(0)), _front(T(0), T(0), T(1)),
      _up(T(0), T(1), T(0)), _right(T(-1), T(0), T(0)), _yaw(::kappa::math::rad(T(-90))),
      _pitch(T(0)), _mouse_sens(T(0.0025)) {}

public:
  void process_mouse(T xoff, T yoff, bool clamp_pitch = true) {
    yoff *= _mouse_sens;
    xoff *= _mouse_sens;

    _yaw += xoff;
    _pitch += yoff;

    if (clamp_pitch) {
      constexpr T pitch_limit = ::kappa::math::rad(T(89));
      _pitch = ::kappa::math::clamp(_pitch, -pitch_limit, pitch_limit);
    }
    _update_vecs();
  }

  void process_movement(cam3d_movement movement, T speed) {
    switch (movement) {
      case cam3d_movement::forward: {
        pos += _front * speed;
      } break;
      case cam3d_movement::backward: {
        pos -= _front * speed;
      } break;
      case cam3d_movement::left: {
        pos -= _right * speed;
      } break;
      case cam3d_movement::right: {
        pos += _right * speed;
      } break;
      case cam3d_movement::up: {
        pos += world_up * speed;
      } break;
      case cam3d_movement::down: {
        pos -= world_up * speed;
      } break;
    }
  }

public:
  Mat<4, 4, T> matrix() const noexcept { return get_func()(pos, pos + _front, _up); }

public:
  KA_MATH_DEF func_type& get_func() noexcept { return static_cast<ViewFunc&>(*this); }

  KA_MATH_DEF const func_type& get_func() const noexcept {
    return static_cast<const ViewFunc&>(*this);
  }

private:
  KA_MATH_DEF void _update_vecs() noexcept {
    const T cpitch = ::kappa::math::cos(_pitch);
    VecNum<3, T> front;
    front.x = ::kappa::math::cos(_yaw) * cpitch;
    front.y = ::kappa::math::sin(_pitch);
    front.z = ::kappa::math::sin(_yaw) * cpitch;
    _right = ::kappa::math::normalize(::kappa::math::cross(front, world_up));
    _front = ::kappa::math::normalize(front);
    _up = ::kappa::math::normalize(::kappa::math::cross(_right, _front));
  }

public:
  ::kappa::VecNum<3, T> pos;
  ::kappa::VecNum<3, T> world_up;

private:
  ::kappa::VecNum<3, T> _front;
  ::kappa::VecNum<3, T> _up;
  ::kappa::VecNum<3, T> _right;
  T _yaw, _pitch, _mouse_sens;
};

} // namespace kappa::math
