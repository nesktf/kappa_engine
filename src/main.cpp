#include <shogle/boilerplate.hpp>
#include "assets.hpp"
#include "model_data.hpp"
#include "model.hpp"
#include <ranges>

using namespace ntf::numdefs;

int main() {
  ntf::logger::set_level(ntf::log_level::debug);

  u32 win_width = 1280;
  u32 win_height = 720;
  const ntfr::win_x11_params x11 {
    .class_name = "cino_anim",
    .instance_name = "cino_anim",
  };
  const ntfr::win_gl_params win_gl {
    .ver_major = 4,
    .ver_minor = 6,
    .swap_interval = 1,
    .fb_msaa_level = 8,
    .fb_buffer = ntfr::fbo_buffer::depth24u_stencil8u,
    .fb_use_alpha = false,
  };
  auto win = ntfr::window::create({
    .width = win_width,
    .height = win_height,
    .title = "test",
    .attrib = ntfr::win_attrib::decorate | ntfr::win_attrib::resizable,
    .renderer_api = ntfr::context_api::opengl,
    .platform_params = &x11,
    .renderer_params = &win_gl,
  }).value();
  auto ctx = ntfr::make_gl_ctx(win, {.3f, .3f, .3f, .0f}).value();

  auto fbo = ntfr::framebuffer::get_default(ctx);
  bool do_things = false;
  win.set_viewport_callback([&](ntfr::window&, const ntf::extent2d& ext) {
    fbo.viewport({0, 0, ext.x, ext.y});
  });
  win.set_key_press_callback([&](ntfr::window& win, const ntfr::win_key_data& k) {
    if (k.action == ntfr::win_action::press) {
      if (k.key == ntfr::win_key::escape) {
        win.close();
      }
      if (k.key == ntfr::win_key::space) {
        do_things = !do_things;
      }
    }
  });

  asset_bundle bundle{ctx};
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
      m.transform().pos(.5f, -.75f, 0.f).scale(1.f, 1.f, 1.f);
    } else {
      m.transform().pos(-.5f, -.75f, 0.f).scale(1.f, 1.f, 1.f);
    }
  };
  
  loader.request_rmodel(bundle, "./res/chiruno/chiruno.gltf", "chiruno",
                        cirno_opts, model_callback);
  loader.request_rmodel(bundle, "./res/koosh/koosh.gltf", "koishi",
                        cirno_opts, model_callback);
  // loader.request_rmodel(bundle, "./lib/shogle/demos/res/cirno_fumo/cirno_fumo.obj", "fumo",
  //                       cirno_opts, model_callback);

  const auto fb_ratio = (float)win_width/(float)win_height;
  auto proj_mat = glm::perspective(glm::radians(35.f), fb_ratio, .1f, 100.f);
  auto view_mat = glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.f, -3.f});
  
  auto bone_transform = ntf::transform3d<f32>{}
    .scale(1.f, 1.f, 1.f).pos(0.f, 0.f, 0.f);

  float t = 0.f;
  ntfr::render_loop(win, ctx, 60u, ntf::overload{
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
        .ctx = ctx,
        .fbo = fbo,
        .view = view_mat,
        .proj = proj_mat,
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
