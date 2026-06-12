#pragma once

#include "../core.hpp"

#include <memory>
#include <utility>

enum KA_PNEW_T {
  KA_PNEW_TAG,
};

constexpr inline void* operator new(size_t, void* ptr, KA_PNEW_T) {
  return ptr;
}

#define KA_PNEW(_ptr, ...) ::new (_ptr, KA_PNEW_TAG)

namespace kappa {

template<typename T>
using Uninited = T*;

template<typename T, usize Size, usize Align>
struct TypeBuffer {
public:
  alignas(Align) u8 _data[Size];

  template<typename U = T>
  static constexpr fn check_params() noexcept -> bool {
    return (alignof(U) <= Align) && (sizeof(U) == Size);
  }

public:
  fn get_dirty() -> T* { return reinterpret_cast<T*>(_data); }

  fn get_dirty() const -> const T* { return reinterpret_cast<const T*>(_data); }

  fn get() -> T* { return std::launder(reinterpret_cast<T*>(_data)); }

  fn get() const -> const T* { return std::launder(reinterpret_cast<const T*>(_data)); }

  fn operator*()->T& { return *get(); }

  fn operator*() const->const T& { return *get(); }

  fn operator->()->T* { return get(); }

  fn operator->() const->const T* { return get(); }

  fn operator[](usize i)->T& { return *(get() + i); }

  fn operator[](usize i) const->const T& { return *(get() + i); }

public:
  template<typename... Args>
  fn construct(Args&&... args) -> T& {
    return *(KA_PNEW(reinterpret_cast<T*>(_data)) T(std::forward<Args>(args)...));
  }

  template<typename... Args>
  fn construct_offset(usize offset, Args&&... args) -> T& {
    return *(KA_PNEW(reinterpret_cast<T*>(_data) + offset) T(std::forward<Args>(args)...));
  }

  fn destroy() -> void { get()->~T(); }

  fn reset() -> void { get()->~T(); }

  fn destroy_offset(usize offset) -> void { get()[offset].~T(); }

  fn reset_offset(usize offset) -> void { get()[offset].~T(); }
};

template<typename T>
using TypeBufferFor = TypeBuffer<T, sizeof(T), alignof(T)>;

template<typename T, usize Count>
using TypeBufferForArray = TypeBuffer<T, Count * sizeof(T), alignof(T)>;

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
