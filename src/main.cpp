#include "renderer.hpp"
#include "assets.hpp"
#include "model_data.hpp"
#include "model.hpp"
#include <ranges>

using namespace ntf::numdefs;

int main() {
  ntf::logger::set_level(ntf::log_level::debug);
  auto rh__ = renderer::construct();

  auto& r = renderer::instance();
  auto fbo = ntfr::framebuffer::get_default(r.ctx());

  bool do_things = false;
  r.win().set_viewport_callback([&](ntfr::window&, const ntf::extent2d& ext) {
    fbo.viewport({0, 0, ext.x, ext.y});
  }).set_key_press_callback([&](ntfr::window& win, const ntfr::win_key_data& k) {
    if (k.action == ntfr::win_action::press) {
      if (k.key == ntfr::win_key::escape) {
        win.close();
      }
      if (k.key == ntfr::win_key::space) {
        do_things = !do_things;
      }
    }
  });

  asset_bundle bundle{r.ctx()};
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
    glm::perspective(glm::radians(35.f), fb_ratio, .1f, 100.f), // proj
    glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.f, -3.f}), // view
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
      loader.handle_requests();

      if (!do_things) {
        return;
      }

      t += 1/(float)fdt;
      bone_transform.rot(ntf::vec3{-t*M_PIf*.5f, 0.f, 0.f});

      for (auto idx : rmodels) {
        auto& model = bundle.get_rmodel(idx);
        model.transform().rot(ntf::vec3{t*M_PIf*.5f, 0.f, 0.f});
        model.set_bone_transform("Head", bone_transform);
        model.tick();
      }
    },
    [&](f32 dt, f32 alpha) {
      const game_frame r {
        .ctx = renderer::instance().ctx(),
        .fbo = fbo,
        .stransf = scene_transf,
        .dt = dt,
        .alpha = alpha,
      };
      for (auto idx : rmodels) {
        bundle.get_rmodel(idx).render(r);
      }
    }
  });

  return EXIT_SUCCESS;
}
