#include <assets/model.hpp>
#include <fmt/printf.h>

#include <variant>

template<typename... Fs>
struct overload : Fs... {
  using Fs::operator()...;
};

template<typename... Fs>
overload(Fs...) -> overload<Fs...>;

int main() {
  fmt::print("amoma\n");

  kappa::assets::model3d_loader cirno_loader("./res/cirno_fumo/cirno_fumo.obj", "cirno_fumo");
  const auto cirno = cirno_loader.load().value();

  std::vector<std::variant<kappa::assets::model3d_data::texture_data, kappa::assets::texture_data>>
    texes;

  for (size_t i = 0; i < cirno.texture_count; ++i) {
    const auto& tex = cirno.textures[i];
    if (tex.data) {
      texes.emplace_back(tex);
    } else {
      kappa::assets::texture_loader tex_loader(tex.path.as_view(), tex.name.as_view());
      auto loadedtex = tex_loader.load().value();
      texes.emplace_back(std::move(loadedtex));
    }
  }
  const auto visitor =
    overload{[](const kappa::assets::model3d_data::texture_data& tex) {
               fmt::print("- tex: {}, {}\n", tex.name.as_view(), tex.path.as_view());
             },
             [](const kappa::assets::texture_data& tex) {
               fmt::print("- tex: {}, {}\n", tex.name, tex.path);
             }};
  for (const auto& tex : texes) {
    std::visit(visitor, tex);
  }

  return 0;
}
