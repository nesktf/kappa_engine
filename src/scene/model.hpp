#pragma once

#include "../assets/rigged_model.hpp"
#include "../physics/particle.hpp"
#include "../util/free_list.hpp"

namespace kappa::scene {

enum class entity_type {
  static3d = 0,
  rigged3d,
};

template<entity_type Ent>
struct entity_enum_mapper {};

template<entity_type Ent>
using entity_enum_mapper_t = entity_enum_mapper<Ent>::type;

template<typename Ent>
concept scene_entity_type = requires() {
  requires std::same_as<std::remove_cv_t<decltype(Ent::ent_type)>, entity_type>;
  requires std::same_as<entity_enum_mapper_t<Ent::ent_type>, Ent>;
};

class rigged_model3d {
public:
  static constexpr entity_type ent_type = entity_type::rigged3d;

  struct bone_transform {
    vec3 pos;
    vec3 scale;
    quat rot;
  };

public:
  rigged_model3d(u32 model, shogle::shader_storage_buffer&& bone_buffer,
                 ntf::unique_array<mat4>&& bone_transforms, shogle::transform3d<f32> transform);

public:
  void apply_animation(const model_anim_data& anims, std::string_view name, u32 tick);
  void update_bones(ntf::span<mat4> transform_cache, const mat4& root);

  bool set_transform(ntf::cspan<assets::rigged_model3d> models, std::string_view bone,
                     const bone_transform& transf);
  bool set_transform(ntf::cspan<assets::rigged_model3d> models, std::string_view bone,
                     const mat4& transf);
  bool set_transform(ntf::cspan<assets::rigged_model3d> models, std::string_view bone,
                     shogle::transform3d<f32>& transf);

  void set_transform(ntf::cspan<assets::rigged_model3d> models, u32 bone,
                     const bone_transform& transf);
  void set_transform(ntf::cspan<assets::rigged_model3d> models, u32 bone, const mat4& transf);
  void set_transform(ntf::cspan<assets::rigged_model3d> models, u32 bone,
                     shogle::transform3d<f32>& transf);

public:
  u32 retrieve_buffer_bindings(std::vector<shogle::shader_binding>& binds) const;

private:
  shogle::shader_storage_buffer _bone_buffer;
  ntf::unique_array<mat4> _bone_transforms;
  physics::particle_entity _particle;
  shogle::transform3d<f32> _transform;
  u32 _model;
};

template<>
struct entity_enum_mapper<entity_type::rigged3d> {
  using type = rigged_model3d;
};

static_assert(scene_entity_type<rigged_model3d>);

class entity_registry {
private:
  template<typename T>
  using ent_handle = kappa::free_list<T>::element;

public:
  entity_registry() = default;

public:
  template<scene_entity_type T, typename... Args>
  fn add_entity(Args&&... args) -> ent_handle<T> {
    if constexpr (std::same_as<T, rigged_model3d>) {
      return _rigged_instances.request_elem(std::forward<Args>(args)...);
    }
  }

  template<scene_entity_type T>
  fn remove_entity(ent_handle<T> handle) -> void {
    if constexpr (std::same_as<T, rigged_model3d>) {
      _rigged_instances.return_elem(handle);
    }
  }

public:
  void update();
  u32 retrieve_render_data(const render::scene_render_data& scene,
                           render::object_render_data& data);

private:
  std::vector<assets::rigged_model3d> _rmodels;
  kappa::free_list<rigged_model3d> _rigged_instances;
  std::vector<mat4> _rig_cache;
};

} // namespace kappa::scene
