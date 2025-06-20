#pragma once

#include "renderable.hpp"
#include <shogle/assets.hpp>

#include <ntfstl/threadpool.hpp>
#include <ntfstl/function.hpp>

#include <variant>

class asset_bundle {
public:
  enum class rmodel_idx : u32 {};

public:
  asset_bundle(ntfr::context_view render_ctx);

public:
  expect<rmodel_idx> put_rmodel(rigged_model3d::data_t&& model_data);

  rigged_model3d* find_rmodel(std::string_view name);
  const rigged_model3d* find_rmodel(std::string_view name) const;

  rigged_model3d& get_rmodel(rmodel_idx model);
  const rigged_model3d& get_rmodel(rmodel_idx model) const;

public:
  ntfr::context_view context() const { return _render_ctx; }

private:
  ntfr::context_view _render_ctx;
  std::vector<rigged_model3d> _models;
  std::unordered_map<std::string, u32> _model_map;
};

class asset_loader {
private:
  struct response_t {
    template<typename F>
    response_t(asset_bundle& bundle_, F&& callback_, std::string_view err_) :
      callback{std::forward<F>(callback_)},
      data{std::in_place_type_t<std::string_view>{}, err_},
      bundle{&bundle_} {}

    template<typename F>
    response_t(asset_bundle& bundle_, F&& callback_, rigged_model3d::data_t&& data_) :
      callback{std::forward<F>(callback_)},
      data{std::in_place_type_t<rigged_model3d::data_t>{}, std::move(data_)},
      bundle{&bundle_} {}

    ntf::inplace_function<void(expect<u32>, asset_bundle&)> callback;
    std::variant<std::string_view, rigged_model3d::data_t> data;
    asset_bundle* bundle;
  };

public:
  struct model_opts {
    u32 flags;
    std::string_view armature;
  };

public:
  asset_loader() = default;

public:
  expect<asset_bundle::rmodel_idx> load_rmodel(asset_bundle& bundle,
                                               const std::string& path, const std::string& name,
                                               const model_opts& opts);

public:
  template<typename F>
  void request_rmodel(asset_bundle& bundle, const std::string& path, const std::string& name,
                      const model_opts& opts, F&& callback)
  {
    _tpool.enqueue([this, &bundle, path, name, opts, cb = std::forward<F>(callback)]() mutable {
      assimp_parser parser;
      auto data = _parse_rmodel(parser, path, std::move(name), opts);

      std::scoped_lock lock{_res_mtx};
      if (!data) {
        _responses.emplace(bundle, std::move(cb), data.error());
        return;
      }
      _responses.emplace(bundle, std::move(cb), std::move(*data));
    });
  }

public:
  void handle_requests();

private:
  static expect<rigged_model3d::data_t> _parse_rmodel(assimp_parser& parser,
                                                      const std::string& path,
                                                      std::string name, const model_opts& opts);

private:
  ntf::thread_pool _tpool;
  std::mutex _res_mtx;
  std::queue<response_t> _responses;
};
