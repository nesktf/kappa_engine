#include "./particle.hpp"

namespace kappa::physics {

particle_entity::particle_entity(const vec3& pos, real mass) noexcept :
    _pos{pos}, _inv_mass{1.f / mass}, _vel{}, _damping{1.f}, _acc{}, _forces{} {}

particle_entity::particle_entity(const vec3& pos, real mass, const vec3& vel,
                                 real damping) noexcept :
    _pos{pos}, _inv_mass{1.f / mass}, _vel{vel}, _damping{damping}, _acc{}, _forces{} {}

particle_entity::particle_entity(const vec3& pos, real mass, const vec3& vel, real damping,
                                 const vec3& acc) noexcept :
    _pos{pos}, _inv_mass{1.f / mass}, _vel{vel}, _damping{damping}, _acc{acc}, _forces{} {}

vec3 particle_entity::pos() const {
  return _pos;
}

real particle_entity::mass() const {
  return 1.f / _inv_mass;
}

real particle_entity::inv_mass() const {
  return _inv_mass;
}

vec3 particle_entity::vel() const {
  return _vel;
}

real particle_entity::damping() const {
  return _damping;
}

vec3 particle_entity::acc() const {
  return _acc;
}

vec3 particle_entity::forces() const {
  return _forces;
}

particle_entity& particle_entity::set_pos(const vec3& pos_) {
  _pos = pos_;
  return *this;
}

particle_entity& particle_entity::set_mass(real mass_) {
  _inv_mass = 1.f / mass_;
  return *this;
}

particle_entity& particle_entity::set_inv_mass(real inv_mass_) {
  _inv_mass = inv_mass_;
  return *this;
}

particle_entity& particle_entity::set_vel(const vec3& vel_) {
  _vel = vel_;
  return *this;
}

particle_entity& particle_entity::set_acc(vec3 acc_) {
  _acc = acc_;
  return *this;
}

particle_entity& particle_entity::add_force(const vec3& force) {
  _forces += force;
  return *this;
}

particle_entity& particle_entity::clear_forces() {
  _forces.x = 0.f;
  _forces.y = 0.f;
  _forces.z = 0.f;
  return *this;
}

bool particle_entity::has_finite_mass() const {
  return _inv_mass != 0.f;
}

particle_entity& particle_entity::integrate(real dt) {
  if (_inv_mass <= 0.f) {
    return *this;
  }
  NTF_ASSERT(dt > 0.f);

  _pos += _vel * dt;

  const vec3 acc = _acc + (_inv_mass * _forces);
  _vel += acc * dt;
  _vel *= std::pow(_damping, dt);

  clear_forces();

  return *this;
}

particle_force_registry::particle_force_registry() : _registry{}, _free{} {}

u32 particle_force_registry::_add_force(u64 particle, u32 tag, generator_func generator) {
  NTF_ASSERT(!generator.is_empty());
  if (_free.empty()) {
    const u32 pos = static_cast<u32>(_registry.size());
    auto& elem = _registry.emplace_back(ntf::in_place, particle, tag, generator);
    NTF_ASSERT(elem.has_value());
    return pos;
  }

  const u32 idx = _free.front();
  _free.pop();
  NTF_ASSERT(idx < _registry.size());
  auto& elem = _registry[idx];
  NTF_ASSERT(!elem.has_value());
  elem.emplace(particle, tag, generator);
  return idx;
}

void particle_force_registry::remove_force(u32 force_idx) {
  NTF_ASSERT(force_idx < _registry.size());
  auto& elem = _registry[force_idx];
  NTF_ASSERT(elem.has_value());
  elem.reset();
  _free.push(force_idx);
}

void particle_force_registry::clear_forces() {
  for (u32 i = 0; auto& elem : _registry) {
    if (!elem.has_value()) {
      continue;
    }
    elem.reset();
    _free.push(i);
  }
}

particle_gravity::particle_gravity(vec3 gravity) noexcept : _gravity{gravity} {}

void particle_gravity::operator()(particle_entity& particle, real dt) noexcept {
  NTF_UNUSED(dt);

  if (!particle.has_finite_mass()) {
    return;
  }

  const real mass = particle.mass();
  particle.add_force(_gravity * mass);
}

particle_drag::particle_drag(real k1, real k2) noexcept : _k1{k1}, _k2{k2} {}

void particle_drag::operator()(particle_entity& particle, real dt) noexcept {
  NTF_UNUSED(dt);
  const vec3 vel = particle.vel();
  const real vel_mag = glm::length(vel);
  if (shogle::equal(vel_mag, 0.f)) {
    return;
  }
  const real drag_coeff = _k1 * vel_mag + _k2 * vel_mag * vel_mag;
  const vec3 force = -drag_coeff * vel / vel_mag; // Normalize before applying
  particle.add_force(force);
}

particle_spring::particle_spring(particle_entity& other, real spring_const, real rest_len) noexcept
    : _other{other}, _spring_const{spring_const}, _rest_len{rest_len} {}

void particle_spring::operator()(particle_entity& particle, real dt) {
  NTF_UNUSED(dt);
  NTF_ASSERT(!_other.empty());
  const vec3 spring_vec = particle.pos() - _other->pos();
  const real spring_vec_len = glm::length(spring_vec);
  if (shogle::equal(spring_vec_len, 0.f)) {
    return;
  }
  const real spring_mag = std::abs(spring_vec_len - _rest_len) * _spring_const;
  const vec3 force = -spring_mag * spring_vec / spring_vec_len; // Normalize before applying
  particle.add_force(force);
}

particle_spring_anchor::particle_spring_anchor(const vec3& anchor, real spring_const,
                                               real rest_len) noexcept :
    _anchor{anchor}, _spring_const{spring_const}, _rest_len{rest_len} {}

void particle_spring_anchor::operator()(particle_entity& particle, real dt) {
  NTF_UNUSED(dt);
  const vec3 spring_vec = particle.pos() - _anchor;
  const real spring_vec_len = glm::length(spring_vec);
  if (shogle::equal(spring_vec_len, 0.f)) {
    return;
  }
  const real spring_mag = (_rest_len - spring_vec_len) * _spring_const;
  const vec3 force = spring_mag * spring_vec / spring_vec_len; // Normalize before applying
  particle.add_force(force);
}

void particle_spring_anchor::set_anchor(const vec3& anchor) {
  _anchor = anchor;
}

particle_bungee::particle_bungee(particle_entity& other, real spring_const, real rest_len) noexcept
    : _other{other}, _spring_const{spring_const}, _rest_len{rest_len} {}

void particle_bungee::operator()(particle_entity& particle, real dt) {
  NTF_UNUSED(dt);
  NTF_ASSERT(!_other.empty());
  const vec3 spring_vec = particle.pos() - _other->pos();
  const real spring_vec_len = glm::length(spring_vec);
  if (spring_vec_len <= _rest_len) {
    return;
  }
  const real spring_mag = (_rest_len - spring_vec_len) * _spring_const;
  const vec3 force = spring_mag * spring_vec / spring_vec_len; // Normalize before applying
  particle.add_force(force);
}

particle_bungee_anchor::particle_bungee_anchor(const vec3& anchor, real spring_const,
                                               real rest_len) noexcept :
    _anchor{anchor}, _spring_const{spring_const}, _rest_len{rest_len} {}

void particle_bungee_anchor::operator()(particle_entity& particle, real dt) {
  NTF_UNUSED(dt);
  const vec3 spring_vec = particle.pos() - _anchor;
  const real spring_vec_len = glm::length(spring_vec);
  if (spring_vec_len <= _rest_len) {
    return;
  }
  const real spring_mag = (_rest_len - spring_vec_len) * _spring_const;
  const vec3 force = spring_mag * spring_vec / spring_vec_len; // Normalize before applying
  particle.add_force(force);
}

void particle_bungee_anchor::set_anchor(const vec3& anchor) {
  _anchor = anchor;
}

} // namespace kappa::physics
