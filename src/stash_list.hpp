#pragma once

#include "./common.hpp"
#include <ntfstl/optional.hpp>

#include <queue>
#include <vector>

namespace kappa {

template<typename T>
class stash_list {
public:
  enum class item_handle : u32 {};

public:
  stash_list() noexcept = default;

public:
  template<typename... Args>
  requires(std::constructible_from<T, Args...>)
  item_handle pop_item(Args&&... args) {
    if (_free_list.empty()) {
      u32 pos = _items.size();
      _items.emplace_back(ntf::nullopt);
      _emplace_at(pos, std::forward<Args>(args)...);
      return static_cast<item_handle>(pos);
    }
    u32 pos = _free_list.front();
    _free_list.pop();
    _emplace_at(pos, std::forward<Args>(args)...);
    return static_cast<item_handle>(pos);
  }

  void push_item(item_handle handle) {
    u32 pos = static_cast<u32>(handle);
    NTF_ASSERT(pos < _items.size());
    _items[pos].reset();
    _free_list.push(pos);
  }

  void clear() {
    while (!_free_list.empty()) {
      _free_list.pop();
    }
    for (u32 i = 0; auto& item : _items) {
      item.reset();
      _free_list.push(i++);
    }
  }

  void reset() {
    while (!_free_list.empty()) {
      _free_list.pop();
    }
    _items.clear();
  }

  T& get(item_handle handle) {
    u32 pos = static_cast<u32>(handle);
    NTF_ASSERT(pos < _items.size());
    NTF_ASSERT(_items[pos].has_value());
    return *_items[pos];
  }

  const T& get(item_handle handle) const {
    u32 pos = static_cast<u32>(handle);
    NTF_ASSERT(pos < _items.size());
    NTF_ASSERT(_items[pos].has_value());
    return *_items[pos];
  }

private:
  template<typename... Args>
  T& _emplace_at(u32 pos, Args&&... args) {
    NTF_ASSERT(pos < _items.size());
    NTF_ASSERT(!_items[pos].has_value());
    return _items[pos].emplace(std::forward<Args>(args)...);
  }

private:
  std::vector<ntf::optional<T>> _items;
  std::queue<u32> _free_list;
};

} // namespace kappa
