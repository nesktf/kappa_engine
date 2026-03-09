#include <assets/model.hpp>
#include <fmt/printf.h>

#include <variant>

namespace {

template<typename... Fs>
struct overload : Fs... {
  using Fs::operator()...;
};

template<typename... Fs>
overload(Fs...) -> overload<Fs...>;

void run_thing() {
  shogle::logger::set_level(shogle::logger::LEVEL_VERBOSE);
  fmt::print("amoma\n");

  kappa::assets::model3d_loader cirno_loader("./res/cirno_fumo/cirno_fumo.obj", "cirno_fumo");
  const auto cirno = cirno_loader.load().value();
  kappa::assets::model3d_loader mariloader("./res/marisa_miy/marisa_miy.gltf", "marisa");
  const auto mari = mariloader.load().value();

  std::vector<std::variant<kappa::assets::model3d_data::texture_data, kappa::assets::texture_data>>
    texes;

  for (const auto& tex : cirno.textures()) {
    if (tex.data) {
      texes.emplace_back(tex);
    } else {
      kappa::logger::info("{}", tex.path.as_view());
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
               fmt::print("- tex: {}, {}\n", tex.name().as_view(), tex.path().as_view());
             }};
  for (const auto& tex : texes) {
    std::visit(visitor, tex);
  }
}

} // namespace

int main() {
  try {
    run_thing();
  } catch (shogle::bad_expected_access<kappa::buffer_str<256>>& err) {
    kappa::logger::error("{}", err.error().as_view());
  }
}
