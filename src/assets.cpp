#include "assets.hpp"

#include <ntfstl/utility.hpp>

auto asset_bundle::put_rmodel(rigged_model3d::data_t&& model_data) -> expect<rmodel_idx> {
  return rigged_model3d::create(std::move(model_data))
  .and_then([this](rigged_model3d&& model) -> expect<rmodel_idx> {
    _models.emplace_back(std::move(model));
    u32 pos = _models.size()-1u;
    return static_cast<rmodel_idx>(pos);
  });
}

rigged_model3d* asset_bundle::find_rmodel(std::string_view name) {
  auto it = _model_map.find({name.data(), name.size()});
  if (it == _model_map.end()) {
    return nullptr;
  }
  NTF_ASSERT(it->second < _models.size());
  return &_models[it->second];
}

const rigged_model3d* asset_bundle::find_rmodel(std::string_view name) const {
  auto it = _model_map.find({name.data(), name.size()});
  if (it == _model_map.end()) {
    return nullptr;
  }
  NTF_ASSERT(it->second < _models.size());
  return &_models[it->second];
}

rigged_model3d& asset_bundle::get_rmodel(rmodel_idx model) {
  u32 idx = static_cast<u32>(model);
  NTF_ASSERT(idx < _models.size());
  return _models[idx];
}

const rigged_model3d& asset_bundle::get_rmodel(rmodel_idx model) const {
  u32 idx = static_cast<u32>(model);
  NTF_ASSERT(idx < _models.size());
  return _models[idx];
}

expect<asset_bundle::rmodel_idx> asset_loader::load_rmodel(asset_bundle& bundle,
                                                           const std::string& path,
                                                           const std::string& name,
                                                           const model_opts& opts)
{
  assimp_parser parser;
  return _parse_rmodel(parser, path, name, opts)
  .and_then([&](rigged_model3d::data_t&& data) -> expect<asset_bundle::rmodel_idx> {
    return bundle.put_rmodel(std::move(data));
  });
}

void asset_loader::handle_requests() {
  while (!_responses.empty()) {
    auto res = std::move(_responses.front());
    _responses.pop();
    NTF_ASSERT(!res.callback.empty());
    NTF_ASSERT(res.bundle);
    std::visit(ntf::overload{
      [&](rigged_model3d::data_t&& model) {
        auto ret = res.bundle->put_rmodel(std::move(model))
        .transform([](auto model) -> u32 { return static_cast<u32>(model); });
        std::invoke(res.callback, ret, *res.bundle);
      },
      [&](std::string_view err) {
        expect<u32> ret{ntf::unexpect, err};
        std::invoke(res.callback, ret, *res.bundle);
      }
    }, std::move(res.data));
  }
}


auto asset_loader::_parse_rmodel(assimp_parser& parser, const std::string& path,
                                std::string name, const model_opts& opt) -> expect<rigged_model3d::data_t>
{
  if (auto ret = parser.load(path, opt.flags); !ret) {
    return ntf::unexpected{std::move(ret.error())};
  }

  model_rig_data rigs;
  parser.parse_rigs(rigs);

  if (rigs.armature_registry.find(opt.armature) == rigs.armature_registry.end()) {
    return ntf::unexpected{std::string_view{"Armature not found"}};
  }

  model_anim_data anims;
  parser.parse_animations(anims);

  model_material_data mats;
  parser.parse_materials(mats);

  model_mesh_data meshes;
  parser.parse_meshes(rigs, meshes);

  return expect<rigged_model3d::data_t> {
    ntf::in_place, std::move(name), opt.armature,
    std::move(rigs), std::move(anims), std::move(mats), std::move(meshes)
  };
}
