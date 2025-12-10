#pragma once

#include <ntfstl/optional.hpp>
#include <ntfstl/ptr.hpp>
#include <ntfstl/types.hpp>

#include <queue>
#include <vector>

namespace kappa {

using namespace ntf::numdefs;

template<typename T>
class free_list {
private:
  struct storage {
    ntf::nullable<T> obj;
    u32 version;
  };

public:
  enum class element : u64 {};

public:
  free_list() : _elems{}, _free{} {}

public:
  template<typename... Args>
  ntf::optional<element> request_elem(Args&&... args) {
    if (!_free.empty()) {
      const u32 idx = _free.front();
      auto& [elem, version] = _elems[idx];

      NTF_ASSERT(!elem.has_value());
      elem.emplace(std::forward<Args>(args)...);
      auto handle = _compose_handle(idx, version);
      NTF_ASSERT(is_valid(handle));

      _free.pop();
      ++version;
      return handle;
    }

    const u32 idx = _elems.size();
    auto& [elem, version] = _elems.emplace_back(ntf::nullopt, 0);
    elem.emplace(std::forward<Args>(args)...);
    auto handle = _compose_handle(idx, version);
    NTF_ASSERT(is_valid(handle));
    return handle;
  }

  free_list& return_elem(element handle) {
    const auto [idx, _] = _decompose_handle(handle);
    NTF_ASSERT(idx < _elems.size());

    auto& [elem, version] = _elems[idx];
    NTF_ASSERT(elem.has_value());
    elem.reset();

    _free.push(idx);
    ++version;
    return *this;
  }

  void clear() {
    for (auto& [elem, version] : _elems) {
      if (elem.has_value()) {
        elem.reset();
        ++version;
      }
    }
    while (!_free.empty()) {
      _free.pop();
    }
  }

  template<typename F>
  free_list& clear_where(F&& func) {
    for (u32 idx = 0; auto& [elem, version] : _elems) {
      if (!elem.has_value()) {
        ++idx;
        continue;
      }
      const bool should_reset = std::invoke(func, *elem);
      if (!should_reset) {
        ++idx;
        continue;
      }

      NTF_ASSERT(elem.has_value());
      elem.reset();
      NTF_ASSERT(!elem.has_value());
      ++version;
      _free.push(idx);
      ++idx;
    }
    return *this;
  }

  template<typename F>
  free_list& for_each(F&& func) {
    for (auto& [elem, _] : _elems) {
      if (elem.has_value()) {
        std::invoke(func, *elem);
      }
    }
    return *this;
  }

  template<typename F>
  const free_list& for_each(F&& func) const {
    for (auto& [elem, _] : _elems) {
      if (elem.has_value()) {
        std::invoke(func, *elem);
      }
    }
    return *this;
  }

  bool is_valid(element handle) const {
    const auto [idx, ver] = _decompose_handle(handle);
    if (idx >= _elems.size()) {
      return false;
    }
    const auto& [elem, version] = _elems[idx];
    if (!elem.has_value()) {
      return false;
    }
    if (ver != version) {
      return false;
    }

    return true;
  }

  T& elem_at(element handle) {
    NTF_ASSERT(is_valid(handle));
    const auto [idx, _] = _decompose_handle(handle);
    NTF_ASSERT(idx < _elems.size());
    auto& [elem, __] = _elems[idx];
    NTF_ASSERT(elem.has_value());
    return *elem;
  }

  const T& elem_at(element handle) const {
    NTF_ASSERT(is_valid(handle));
    const auto [idx, _] = _decompose_handle(handle);
    NTF_ASSERT(idx < _elems.size());
    auto& [elem, __] = _elems[idx];
    NTF_ASSERT(elem.has_value());
    return *elem;
  }

private:
  static element _compose_handle(u32 idx, u32 ver) {
    return static_cast<element>((static_cast<u64>(ver) << 32) | idx);
  }

  static std::pair<u32, u32> _decompose_handle(element handle) {
    return {static_cast<u64>(handle) & 0xFFFFFFFF, static_cast<u64>(handle) >> 32};
  }

private:
  std::vector<storage> _elems;
  std::queue<u32> _free;
};

} // namespace kappa
