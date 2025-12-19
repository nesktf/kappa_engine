#include "assets/loader.hpp"

#include <ntfstl/utility.hpp>
#include <variant>

namespace kappa::assets {

namespace {

struct response_t {
  response_t(ntf::unexpect_t, asset_callback&& callback_, std::string&& error) :
      callback{std::move(callback_)}, data{std::move(error)} {}

  template<typename T, typename... Args>
  response_t(std::in_place_type_t<T> tag, asset_callback&& callback_, Args&&... args) :
      callback{std::move(callback_)}, data{tag, std::forward<Args>(args)...} {}

  asset_callback callback;
  std::variant<std::string, rigged_model::data_t> data;
};

struct loader_t {
  std::unordered_map<std::string, u64> asset_map;
  ntf::freelist<rigged_model> models;

  ntf::thread_pool tpool;
  std::mutex res_mtx;
  std::queue<response_t> responses;
};

ntf::nullable<loader_t> g_loader;

} // namespace

[[nodiscard]] handle_t initialize() {
  NTF_ASSERT(!g_loader.has_value());
  g_loader.emplace();

  return {};
}

void* get_asset(u64 handle, asset_tag tag) {
  NTF_ASSERT(g_loader.has_value());
  const auto h = ntf::freelist_handle::from_u64(handle);
  switch (tag) {
    case asset_tag::rigged_model: {
      return static_cast<void*>(g_loader->models.at_opt(h));
    } break;
    default:
      NTF_UNREACHABLE();
  }
}

ntf::optional<asset_handle> do_find_asset(const std::string& name, asset_tag tag) {
  NTF_ASSERT(g_loader.has_value());
  switch (tag) {
    case asset_tag::rigged_model: {
      auto it = g_loader->asset_map.find(name);
      if (it == g_loader->asset_map.end()) {
        return {ntf::nullopt};
      }
      return {ntf::in_place, it->second, tag};
    } break;
    default:
      NTF_UNREACHABLE();
  }
}

void clear_assets() {
  NTF_ASSERT(g_loader.has_value());
  g_loader->asset_map.clear();
  g_loader->models.clear();
}

void handle_requests() {
  NTF_ASSERT(g_loader.has_value());
  auto& responses = g_loader->responses;
  while (!responses.empty()) {
    auto res = std::move(responses.front());
    responses.pop();
    NTF_ASSERT(!res.callback.empty());

    const auto upload_model = [&res](rigged_model::data_t&& model) {
      std::string name_copy{model.name.data(), model.name.size()};
      const auto put_rig = [&name_copy](rigged_model&& model) -> expect<asset_handle> {
        const auto h = g_loader->models.emplace(std::move(model));
        const auto val = h.as_u64();
        auto [_, empl] = g_loader->asset_map.try_emplace(name_copy, val);
        if (!empl) {
          return {ntf::unexpect, fmt::format("Duplicate rig \"{}\"", name_copy)};
        }
        return {ntf::in_place, val, asset_tag::rigged_model};
      };
      auto ret = rigged_model::create(std::move(model)).and_then(put_rig);
      res.callback(ret);
    };

    const auto handle_error = [&](std::string&& err) {
      expect<asset_handle> ret{ntf::unexpect, std::move(err)};
      res.callback(std::move(ret));
    };

    const ntf::overload visitor{upload_model, handle_error};
    std::visit(visitor, std::move(res.data));
  }
}

namespace {

void emplace_error(asset_callback&& cb, std::string&& err) {
  std::scoped_lock lock{g_loader->res_mtx};
  g_loader->responses.emplace(ntf::unexpect, std::move(cb), std::move(err));
}

template<typename T, typename... Args>
void emplace_data(asset_callback&& cb, Args&&... args) {
  std::scoped_lock lock{g_loader->res_mtx};
  g_loader->responses.emplace(std::in_place_type_t<T>{}, std::move(cb),
                              std::forward<Args>(args)...);
}

void handle_rigged_model(const std::string& name, const std::string& path,
                         asset_callback&& callback, const rigged_model_opts* opts) {
  const u32 flags = opts ? opts->flags : assimp_parser::DEFAULT_ASS_FLAGS;
  std::string armature = opts ? std::string{opts->armature.data(), opts->armature.size()} : "";
  g_loader->tpool.enqueue([name, path, flags, armature, cb = std::move(callback)]() mutable {
    assimp_parser parser;
    if (auto ret = parser.load(path, flags); !ret) {
      emplace_error(std::move(cb), std::move(ret.error()));
      return;
    }

    model_rig_data rigs;
    model_anim_data anims;
    if (!armature.empty()) {
      parser.parse_rigs(rigs);
      if (rigs.armature_registry.find(armature) == rigs.armature_registry.end()) {
        emplace_error(std::move(cb), fmt::format("Armature \"{}\" not found", armature));
        return;
      }
      parser.parse_animations(anims);
    }
    model_material_data mats;
    parser.parse_materials(mats);

    model_mesh_data meshes;
    parser.parse_meshes(rigs, meshes, name);

    emplace_data<rigged_model::data_t>(std::move(cb), name, armature, std::move(meshes),
                                       std::move(mats), std::move(rigs), std::move(anims));
  });
}

} // namespace

void do_request_asset(const std::string& name, const std::string& path, asset_tag tag,
                      asset_callback callback, const void* opts) {
  NTF_ASSERT(g_loader.has_value());
  switch (tag) {
    case asset_tag::rigged_model: {
      const auto* rigged_opt = static_cast<const rigged_model_opts*>(opts);
      handle_rigged_model(name, path, std::move(callback), rigged_opt);
    } break;
    default:
      NTF_UNREACHABLE();
  }
}

} // namespace kappa::assets
