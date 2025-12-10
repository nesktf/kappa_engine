#pragma once

#include "../core.hpp"
#include <functional>
#include <queue>
#include <vector>

namespace kappa::physics {

class particle_entity {
public:
  particle_entity(const vec3& pos, real mass) noexcept;
  particle_entity(const vec3& pos, real mass, const vec3& vel, real damping) noexcept;
  particle_entity(const vec3& pos, real mass, const vec3& vel, real damping,
                  const vec3& acc) noexcept;

public:
  particle_entity& integrate(real dt);

public:
  vec3 pos() const;
  particle_entity& set_pos(const vec3& pos_);

  real mass() const;
  real inv_mass() const;

  particle_entity& set_mass(real mass_);
  particle_entity& set_inv_mass(real inv_mass_);

  vec3 vel() const;
  particle_entity& set_vel(const vec3& vel_);

  real damping() const;
  particle_entity& set_damping(real damping_);

  vec3 acc() const;
  particle_entity& set_acc(vec3 acc_);

  vec3 forces() const;
  particle_entity& add_force(const vec3& force);
  particle_entity& clear_forces();

public:
  bool has_finite_mass() const;

private:
  vec3 _pos;
  real _inv_mass;
  vec3 _vel;
  real _damping;
  vec3 _acc;
  vec3 _forces;
};

template<typename F>
concept particle_force_generator = requires(F generator, particle_entity& particle, real dt) {
  { std::invoke(generator, particle, dt) } -> std::same_as<void>;
};

class particle_force_registry {
private:
  using generator_func = ntf::function_view<void(particle_entity&, real)>;

  struct force_entry {
    u64 particle;
    u32 tag;
    generator_func generator;
  };

public:
  particle_force_registry();

public:
  template<particle_force_generator F>
  u32 add_force(particle_entity& particle, F& generator) {
    generator_func generator_func{generator};
    return _add_force(particle, generator);
  }

  void remove_force(u32 force_idx);

  void clear_forces();

  template<typename F>
  requires(std::is_invocable_r_v<particle_entity&, F, u64, u32>)
  void update_forces(real dt, F&& func) {
    for (auto& elem : _registry) {
      if (!elem.has_value()) {
        continue;
      }
      auto& [particle_handle, tag, generator] = *elem;
      particle_entity& particle = std::invoke(func, particle_handle, tag);
      NTF_ASSERT(!generator.is_empty());
      std::invoke(generator, particle, dt);
    }
  }

private:
  u32 _add_force(ntf::weak_ptr<particle_entity> particle, generator_func generator);

private:
  std::vector<ntf::nullable<force_entry>> _registry;
  std::queue<u32> _free;
};

static constexpr vec3 DEFAULT_GRAVITY{0.f, -9.81f, 0.f};

class particle_gravity {
public:
  particle_gravity(vec3 gravity = DEFAULT_GRAVITY) noexcept;

public:
  void operator()(particle_entity& particle, real dt) noexcept;

private:
  vec3 _gravity;
};

static_assert(particle_force_generator<particle_gravity>);

class particle_drag {
public:
  particle_drag(real k1, real k2) noexcept;

public:
  void operator()(particle_entity& particle, real dt) noexcept;

private:
  // Velocity drag coefficient
  real _k1;
  // Velocity squared drag coefficient
  real _k2;
};

static_assert(particle_force_generator<particle_drag>);

class particle_spring {
public:
  particle_spring(particle_entity& other, real spring_const, real rest_len) noexcept;

public:
  void operator()(particle_entity& particle, real dt);

private:
  ntf::weak_ptr<particle_entity> _other;
  real _spring_const;
  real _rest_len;
};

static_assert(particle_force_generator<particle_spring>);

class particle_spring_anchor {
public:
  particle_spring_anchor(const vec3& anchor, real spring_const, real rest_len) noexcept;

public:
  void operator()(particle_entity& particle, real dt);

  void set_anchor(const vec3& anchor);

private:
  vec3 _anchor;
  real _spring_const;
  real _rest_len;
};

static_assert(particle_force_generator<particle_spring_anchor>);

class particle_bungee {
public:
  particle_bungee(particle_entity& other, real spring_const, real rest_len) noexcept;

public:
  void operator()(particle_entity& particle, real dt);

private:
  ntf::weak_ptr<particle_entity> _other;
  real _spring_const;
  real _rest_len;
};

static_assert(particle_force_generator<particle_bungee>);

class particle_bungee_anchor {
public:
  particle_bungee_anchor(const vec3& anchor, real spring_const, real rest_len) noexcept;

public:
  void operator()(particle_entity& particle, real dt);

  void set_anchor(const vec3& anchor);

private:
  vec3 _anchor;
  real _spring_const;
  real _rest_len;
};

static_assert(particle_force_generator<particle_bungee_anchor>);

} // namespace kappa::physics
