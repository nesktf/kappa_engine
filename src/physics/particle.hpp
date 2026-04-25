#pragma once

#include "../core.hpp"

#include <ranmath/ran.hpp>

#include "../util/function.hpp"
#include "../util/optional.hpp"
#include "../util/ptr.hpp"

#include <queue>
#include <vector>

namespace kappa::physics {

using real = f32;

class ParticleEntity {
public:
  ParticleEntity(const ran::Vec3f32& pos, real mass) noexcept;
  ParticleEntity(const ran::Vec3f32& pos, real mass, const ran::Vec3f32& vel,
                 real damping) noexcept;
  ParticleEntity(const ran::Vec3f32& pos, real mass, const ran::Vec3f32& vel, real damping,
                 const ran::Vec3f32& acc) noexcept;

public:
  ParticleEntity& integrate(real dt);

public:
  ran::Vec3f32 pos() const;
  ParticleEntity& set_pos(const ran::Vec3f32& pos_);

  real mass() const;
  real inv_mass() const;

  ParticleEntity& set_mass(real mass_);
  ParticleEntity& set_inv_mass(real inv_mass_);

  ran::Vec3f32 vel() const;
  ParticleEntity& set_vel(const ran::Vec3f32& vel_);

  real damping() const;
  ParticleEntity& set_damping(real damping_);

  ran::Vec3f32 acc() const;
  ParticleEntity& set_acc(ran::Vec3f32 acc_);

  ran::Vec3f32 forces() const;
  ParticleEntity& add_force(const ran::Vec3f32& force);
  ParticleEntity& clear_forces();

public:
  bool has_finite_mass() const;

private:
  ran::Vec3f32 _pos;
  real _inv_mass;
  ran::Vec3f32 _vel;
  real _damping;
  ran::Vec3f32 _acc;
  ran::Vec3f32 _forces;
};

namespace meta {

template<typename F>
concept particle_force_generator = requires(F generator, ParticleEntity& particle, real dt) {
  { generator(particle, dt) } -> std::same_as<void>;
};

} // namespace meta

class ParticelForceRegistry {
private:
  using GeneratorFunc = FnRef<void(ParticleEntity&, real)>;

  struct force_entry {
    u64 particle;
    u32 tag;
    GeneratorFunc generator;
  };

public:
  ParticelForceRegistry();

public:
  template<meta::particle_force_generator F>
  u32 add_force(u64 particle, u32 tag, F& generator) {
    GeneratorFunc generator_func{generator};
    return _add_force(particle, tag, generator);
  }

  void remove_force(u32 force_idx);

  void clear_forces();

  template<typename F>
  requires(std::is_invocable_r_v<ParticleEntity&, F, u64, u32>)
  void update_forces(real dt, F&& func) {
    for (auto& elem : _registry) {
      if (!elem.has_value()) {
        continue;
      }
      auto& [particle_handle, tag, generator] = *elem;
      ParticleEntity& particle = std::invoke(func, particle_handle, tag);
      std::invoke(generator, particle, dt);
    }
  }

private:
  u32 _add_force(u64 particle, u32 tag, GeneratorFunc generator);

private:
  std::vector<Nullable<force_entry>> _registry;
  std::queue<u32> _free;
};

static constexpr ran::Vec3f32 DEFAULT_GRAVITY{0.f, -9.81f, 0.f};

class particle_gravity {
public:
  particle_gravity(ran::Vec3f32 gravity = DEFAULT_GRAVITY) noexcept;

public:
  void operator()(ParticleEntity& particle, real dt) noexcept;

private:
  ran::Vec3f32 _gravity;
};

static_assert(meta::particle_force_generator<particle_gravity>);

class ParticleDrag {
public:
  ParticleDrag(real k1, real k2) noexcept;

public:
  void operator()(ParticleEntity& particle, real dt) noexcept;

private:
  // Velocity drag coefficient
  real _k1;
  // Velocity squared drag coefficient
  real _k2;
};

static_assert(meta::particle_force_generator<ParticleDrag>);

class ParticleSpring {
public:
  ParticleSpring(ParticleEntity& other, real spring_const, real rest_len) noexcept;

public:
  void operator()(ParticleEntity& particle, real dt);

private:
  Ref<ParticleEntity> _other;
  real _spring_const;
  real _rest_len;
};

static_assert(meta::particle_force_generator<ParticleSpring>);

class ParticleSringAnchor {
public:
  ParticleSringAnchor(const ran::Vec3f32& anchor, real spring_const, real rest_len) noexcept;

public:
  void operator()(ParticleEntity& particle, real dt);

  void set_anchor(const ran::Vec3f32& anchor);

private:
  ran::Vec3f32 _anchor;
  real _spring_const;
  real _rest_len;
};

static_assert(meta::particle_force_generator<ParticleSringAnchor>);

class ParticleBungee {
public:
  ParticleBungee(ParticleEntity& other, real spring_const, real rest_len) noexcept;

public:
  void operator()(ParticleEntity& particle, real dt);

private:
  Ref<ParticleEntity> _other;
  real _spring_const;
  real _rest_len;
};

static_assert(meta::particle_force_generator<ParticleBungee>);

class ParticleBungeeAnchor {
public:
  ParticleBungeeAnchor(const ran::Vec3f32& anchor, real spring_const, real rest_len) noexcept;

public:
  void operator()(ParticleEntity& particle, real dt);

  void set_anchor(const ran::Vec3f32& anchor);

private:
  ran::Vec3f32 _anchor;
  real _spring_const;
  real _rest_len;
};

static_assert(meta::particle_force_generator<ParticleBungeeAnchor>);

} // namespace kappa::physics
