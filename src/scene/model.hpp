#pragma once

#include "../assets/loader.hpp"
#include "../assets/rigged_model.hpp"
#include "../physics/particle.hpp"
#include "../util/free_list.hpp"

namespace kappa::scene {

enum class entity_type {
  static3d = 0,
  rigged3d,
};

namespace meta {

template<entity_type Ent>
struct entity_enum_mapper {};

template<entity_type Ent>
using entity_enum_mapper_t = entity_enum_mapper<Ent>::type;

template<typename Ent>
concept scene_entity_type = requires() {
  requires std::same_as<std::remove_cv_t<decltype(Ent::ent_type)>, entity_type>;
  requires std::same_as<entity_enum_mapper_t<Ent::ent_type>, Ent>;
};

} // namespace meta

class rigged_model {
public:
  static constexpr entity_type ent_type = entity_type::rigged3d;

  struct bone_transform {
    vec3 pos;
    vec3 scale;
    quat rot;
  };

public:
  rigged_model(u32 model, const vec3& pos, real mass, shogle::shader_storage_buffer&& bone_buffer,
               ntf::unique_array<mat4>&& bone_transforms, shogle::transform3d<f32> transform);

public:
  void update_bones(ntf::span<mat4> transform_cache, ntf::cspan<mat4> bone_locals,
                    ntf::cspan<mat4> bone_invs, ntf::cspan<assets::rigged_model_bone> bones);

public:
  u32 retrieve_buffer_bindings(std::vector<shogle::shader_binding>& binds) const;

  u32 model_idx() const { return _model; }

  physics::particle_entity& particle() { return _particle; }

private:
  shogle::shader_storage_buffer _bone_buffer;
  ntf::unique_array<mat4> _bone_transforms;
  physics::particle_entity _particle;
  shogle::transform3d<f32> _transform;
  u32 _model;
};

namespace meta {

template<>
struct entity_enum_mapper<entity_type::rigged3d> {
  using type = rigged_model;
};

} // namespace meta

static_assert(meta::scene_entity_type<rigged_model>);

using ent_handle = ntf::freelist_handle;

class entity_registry : public render::renderable {
public:
  entity_registry() = default;

public:
  void update();
  u32 retrieve_render_data(const render::scene_render_data& scene,
                           render::object_render_data& data) override;

public:
  template<typename F>
  void request_model(assets::asset_loader& loader, const std::string& path,
                     const std::string& name, const assets::asset_loader::model_opts& opts,
                     F&& cb) {
    auto callback = [cb = std::forward<F>(cb)](expect<u32> idx, auto&) {
      if (!idx) {
        logger::error("Can't load model, {}", idx.error());
        return;
      }
      std::invoke(cb, *idx);
    };
    loader.request_rmodel(_bundle, path, name, opts, std::move(callback));
  }

  ent_handle add_entity(u32 model_idx, const vec3& pos, real mass);

  template<physics::meta::particle_force_generator F>
  u32 add_force(ent_handle entity, F& generator) {
    return _forces.add_force(entity.as_u64(), 0u, generator);
  }

private:
  assets::asset_bundle _bundle;
  ntf::freelist<rigged_model> _rigged_instances;
  std::vector<mat4> _rig_cache;
  physics::particle_force_registry _forces;
};

} // namespace kappa::scene
