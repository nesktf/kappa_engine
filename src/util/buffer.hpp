#pragma once

#include "../core.hpp"

#include <utility>

namespace kappa {

template<usize Size, usize Align>
struct AlignedBuffer {
  alignas(Align) u8 _data[Size];

  template<typename T>
  fn as() -> T* {
    return reinterpret_cast<T*>(_data);
  }

  template<typename T>
  fn as() const -> const T* {
    return reinterpret_cast<T*>(_data);
  }
};

template<typename T>
using AlignedTypeBuffer = AlignedBuffer<sizeof(T), alignof(T)>;

// Shitty trivial nullable
template<typename T>
class NullTrivial {
public:
  NullTrivial() = default;

  ~NullTrivial() { reset(); }

public:
  template<typename... Args>
  T& emplace(Args&&... args) {
    reset();
    auto* ptr = new (reinterpret_cast<T*>(_buffer)) T(std::forward<Args>(args)...);
    _buffer[sizeof(T)] = 1;
    return *ptr;
  }

  void reset() {
    if (*this) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        reinterpret_cast<T*>(_buffer)->~T();
      }
      _buffer[sizeof(T)] = 0;
    }
  }

  const T& get() const {
    ka_assert(_buffer[sizeof(T)] == 1, "Not initialized");
    return *reinterpret_cast<const T*>(_buffer);
  }

  T& get() { return const_cast<T&>(std::as_const(*this).get()); }

public:
  T& operator*() { return get(); }

  const T& operator*() const { return get(); }

  T* operator->() { return &get(); }

  const T* operator->() const { return &get(); }

  explicit operator bool() const { return _buffer[sizeof(T)] == 1; }

private:
  alignas(T) u8 _buffer[sizeof(T) + 1];
};

} // namespace kappa
