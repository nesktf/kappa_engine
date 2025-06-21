#pragma once

#include "model.hpp"
#include <shogle/scene.hpp>

struct game_frame {
  ntfr::context_view ctx;
  ntfr::framebuffer_view fbo;
  ntfr::uniform_buffer_view stransf;
  float dt;
  float alpha;
};

struct game_object {
  virtual ~game_object() = default;
  virtual void tick() = 0;
  virtual void render(const game_frame& frame) = 0;
};

class rigged_model3d : public game_object {
public:
  struct data_t {
    std::string name;
    std::string_view armature;
    model_rig_data rigs;
    model_anim_data anims;
    model_material_data mats;
    model_mesh_data meshes;
  };

  struct texture_t {
    std::string name;
    ntfr::texture2d tex;
  };

  struct material_meta {
    std::vector<texture_t> textures;
    std::unordered_map<std::string_view, u32> texture_registry;
    std::vector<model_material_data::material_meta> materials;
    std::unordered_map<std::string_view, u32> material_registry;
    std::vector<u32> material_textures;
  };

  struct armature_meta {
    model_anim_data anims;
    model_rig_data rigs;
    model_rigger rigger;
  };

  struct render_meta {
    ntfr::pipeline pip;
    model_mesh_provider meshes;
    ntfr::uniform_view u_sampler;
    std::vector<mesh_render_data> render_data;
    std::vector<ntfr::shader_binding> binds;
    std::vector<u32> mesh_texs;
  };

public:
  rigged_model3d(std::string&& name,
                 material_meta&& materials,
                 armature_meta&& armature,
                 render_meta&& rendering,
                 ntf::transform3d<f32> transf);

public:
  static expect<rigged_model3d> create(ntfr::context_view ctx, data_t&& data);

public:
  void tick() override;
  void render(const game_frame& frame) override;

public:
  ntf::transform3d<f32>& transform() { return _transf; }
  std::string_view name() const { return _name; }

  void set_bone_transform(std::string_view name, const model_rigger::bone_transform& transf);
  void set_bone_transform(std::string_view name, const mat4& transf);
  void set_bone_transform(std::string_view name, ntf::transform3d<f32>& transf);

private:
  std::string _name;
  material_meta _materials;
  armature_meta _armature;
  render_meta _rendering;
  ntf::transform3d<f32> _transf;
};
