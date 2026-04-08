#include "./particle.hpp"

namespace kappa::physics {

ParticleEntity::ParticleEntity(const Vec3f32& pos, real mass) noexcept :
    _pos{pos}, _inv_mass{1.f / mass}, _vel{}, _damping{1.f}, _acc{}, _forces{} {}

ParticleEntity::ParticleEntity(const Vec3f32& pos, real mass, const Vec3f32& vel,
                               real damping) noexcept :
    _pos{pos}, _inv_mass{1.f / mass}, _vel{vel}, _damping{damping}, _acc{}, _forces{} {}

ParticleEntity::ParticleEntity(const Vec3f32& pos, real mass, const Vec3f32& vel, real damping,
                               const Vec3f32& acc) noexcept :
    _pos{pos}, _inv_mass{1.f / mass}, _vel{vel}, _damping{damping}, _acc{acc}, _forces{} {}

Vec3f32 ParticleEntity::pos() const {
  return _pos;
}

real ParticleEntity::mass() const {
  return 1.f / _inv_mass;
}

real ParticleEntity::inv_mass() const {
  return _inv_mass;
}

Vec3f32 ParticleEntity::vel() const {
  return _vel;
}

real ParticleEntity::damping() const {
  return _damping;
}

Vec3f32 ParticleEntity::acc() const {
  return _acc;
}

Vec3f32 ParticleEntity::forces() const {
  return _forces;
}

ParticleEntity& ParticleEntity::set_pos(const Vec3f32& pos_) {
  _pos = pos_;
  return *this;
}

ParticleEntity& ParticleEntity::set_mass(real mass_) {
  _inv_mass = 1.f / mass_;
  return *this;
}

ParticleEntity& ParticleEntity::set_inv_mass(real inv_mass_) {
  _inv_mass = inv_mass_;
  return *this;
}

ParticleEntity& ParticleEntity::set_vel(const Vec3f32& vel_) {
  _vel = vel_;
  return *this;
}

ParticleEntity& ParticleEntity::set_acc(Vec3f32 acc_) {
  _acc = acc_;
  return *this;
}

ParticleEntity& ParticleEntity::add_force(const Vec3f32& force) {
  _forces += force;
  return *this;
}

ParticleEntity& ParticleEntity::clear_forces() {
  _forces.x = 0.f;
  _forces.y = 0.f;
  _forces.z = 0.f;
  return *this;
}

bool ParticleEntity::has_finite_mass() const {
  return _inv_mass != 0.f;
}

ParticleEntity& ParticleEntity::integrate(real dt) {
  if (_inv_mass <= 0.f) {
    return *this;
  }
  ka_assert(dt > 0.f);

  _pos += _vel * dt;

  const Vec3f32 acc = _acc + (_inv_mass * _forces);
  _vel += acc * dt;
  _vel *= std::pow(_damping, dt);

  clear_forces();

  return *this;
}

ParticelForceRegistry::ParticelForceRegistry() : _registry{}, _free{} {}

u32 ParticelForceRegistry::_add_force(u64 particle, u32 tag, GeneratorFunc generator) {
  if (_free.empty()) {
    const u32 pos = static_cast<u32>(_registry.size());
    auto& elem = _registry.emplace_back(in_place, particle, tag, generator);
    ka_assert(elem.has_value());
    return pos;
  }

  const u32 idx = _free.front();
  _free.pop();
  ka_assert(idx < _registry.size());
  auto& elem = _registry[idx];
  ka_assert(!elem.has_value());
  elem.emplace(particle, tag, generator);
  return idx;
}

void ParticelForceRegistry::remove_force(u32 force_idx) {
  ka_assert(force_idx < _registry.size());
  auto& elem = _registry[force_idx];
  ka_assert(elem.has_value());
  elem.reset();
  _free.push(force_idx);
}

void ParticelForceRegistry::clear_forces() {
  for (u32 i = 0; auto& elem : _registry) {
    if (!elem.has_value()) {
      continue;
    }
    elem.reset();
    _free.push(i);
  }
}

particle_gravity::particle_gravity(Vec3f32 gravity) noexcept : _gravity{gravity} {}

void particle_gravity::operator()(ParticleEntity& particle, real dt) noexcept {
  KA_UNUSED(dt);

  if (!particle.has_finite_mass()) {
    return;
  }

  const real mass = particle.mass();
  particle.add_force(_gravity * mass);
}

ParticleDrag::ParticleDrag(real k1, real k2) noexcept : _k1{k1}, _k2{k2} {}

void ParticleDrag::operator()(ParticleEntity& particle, real dt) noexcept {
  KA_UNUSED(dt);
  const Vec3f32 vel = particle.vel();
  const real vel_mag = math::length(vel);
  if (math::fequal(vel_mag, 0.f)) {
    return;
  }
  const real drag_coeff = _k1 * vel_mag + _k2 * vel_mag * vel_mag;
  const Vec3f32 force = -drag_coeff * vel / vel_mag; // Normalize before applying
  particle.add_force(force);
}

ParticleSpring::ParticleSpring(ParticleEntity& other, real spring_const, real rest_len) noexcept :
    _other{other}, _spring_const{spring_const}, _rest_len{rest_len} {}

void ParticleSpring::operator()(ParticleEntity& particle, real dt) {
  KA_UNUSED(dt);
  const Vec3f32 spring_vec = particle.pos() - _other->pos();
  const real spring_vec_len = math::length(spring_vec);
  if (math::fequal(spring_vec_len, 0.f)) {
    return;
  }
  const real spring_mag = std::abs(spring_vec_len - _rest_len) * _spring_const;
  const Vec3f32 force = -spring_mag * spring_vec / spring_vec_len; // Normalize before applying
  particle.add_force(force);
}

ParticleSringAnchor::ParticleSringAnchor(const Vec3f32& anchor, real spring_const,
                                         real rest_len) noexcept :
    _anchor{anchor}, _spring_const{spring_const}, _rest_len{rest_len} {}

void ParticleSringAnchor::operator()(ParticleEntity& particle, real dt) {
  KA_UNUSED(dt);
  const Vec3f32 spring_vec = particle.pos() - _anchor;
  const real spring_vec_len = math::length(spring_vec);
  if (math::fequal(spring_vec_len, 0.f)) {
    return;
  }
  const real spring_mag = (_rest_len - spring_vec_len) * _spring_const;
  const Vec3f32 force = spring_mag * spring_vec / spring_vec_len; // Normalize before applying
  particle.add_force(force);
}

void ParticleSringAnchor::set_anchor(const Vec3f32& anchor) {
  _anchor = anchor;
}

ParticleBungee::ParticleBungee(ParticleEntity& other, real spring_const, real rest_len) noexcept :
    _other{other}, _spring_const{spring_const}, _rest_len{rest_len} {}

void ParticleBungee::operator()(ParticleEntity& particle, real dt) {
  KA_UNUSED(dt);
  const Vec3f32 spring_vec = particle.pos() - _other->pos();
  const real spring_vec_len = math::length(spring_vec);
  if (spring_vec_len <= _rest_len) {
    return;
  }
  const real spring_mag = (_rest_len - spring_vec_len) * _spring_const;
  const Vec3f32 force = spring_mag * spring_vec / spring_vec_len; // Normalize before applying
  particle.add_force(force);
}

ParticleBungeeAnchor::ParticleBungeeAnchor(const Vec3f32& anchor, real spring_const,
                                           real rest_len) noexcept :
    _anchor{anchor}, _spring_const{spring_const}, _rest_len{rest_len} {}

void ParticleBungeeAnchor::operator()(ParticleEntity& particle, real dt) {
  KA_UNUSED(dt);
  const Vec3f32 spring_vec = particle.pos() - _anchor;
  const real spring_vec_len = math::length(spring_vec);
  if (spring_vec_len <= _rest_len) {
    return;
  }
  const real spring_mag = (_rest_len - spring_vec_len) * _spring_const;
  const Vec3f32 force = spring_mag * spring_vec / spring_vec_len; // Normalize before applying
  particle.add_force(force);
}

void ParticleBungeeAnchor::set_anchor(const Vec3f32& anchor) {
  _anchor = anchor;
}

} // namespace kappa::physics
