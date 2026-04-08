#pragma once

#include "../core.hpp"

#include <fmt/format.h>

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace kappa {

struct uninitialized_t {};

constexpr inline uninitialized_t uninitialized;

template<typename T>
using Vec = std::vector<T>;

namespace impl {

template<typename T, typename Deleter>
struct UniqueArrDel : private Deleter {
  UniqueArrDel() noexcept(std::is_nothrow_default_constructible_v<Deleter>) : Deleter() {}

  UniqueArrDel(const Deleter& del) noexcept(std::is_nothrow_copy_constructible_v<Deleter>) :
      Deleter(del) {}

  void delete_array(T* arr,
                    typename Deleter::size_type n) noexcept(std::is_nothrow_destructible_v<T>) {
    Deleter::operator()(arr, n);
  }

  Deleter& get_deleter() noexcept { return static_cast<Deleter&>(*this); }

  const Deleter& get_deleter() const noexcept { return static_cast<const Deleter&>(*this); }
};

} // namespace impl

template<typename T>
class DefaultArrayDel {
public:
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  template<typename U>
  using rebind = DefaultArrayDel<U>;

public:
  constexpr DefaultArrayDel() noexcept = default;
  constexpr DefaultArrayDel(const DefaultArrayDel&) noexcept = default;

  template<typename U>
  requires(!std::same_as<T, U>)
  constexpr DefaultArrayDel(const rebind<U>&) noexcept {}

public:
  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  constexpr void operator()(U* ptr) noexcept {
    static_assert(!std::is_void_v<T>, "Can't delete incomplete type");
    static_assert(sizeof(T), "Can't delete incomplete type");
    static_assert(std::is_nothrow_destructible_v<T>, "Can't delete throwing type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_at(ptr);
    }
    std::allocator<T>().deallocate(ptr, 1);
  }

  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  constexpr void operator()(U* ptr, size_type n) noexcept {
    static_assert(!std::is_void_v<T>, "Can't delete incomplete type");
    static_assert(sizeof(T), "Can't delete incomplete type");
    static_assert(std::is_nothrow_destructible_v<T>, "Can't delete throwing type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_n(ptr, n);
    }
    std::allocator<T>().deallocate(ptr, n);
  }

public:
  template<typename U>
  constexpr bool operator==(const rebind<U>&) noexcept {
    return true;
  }

  template<typename U>
  constexpr bool operator!=(const rebind<U>&) noexcept {
    return false;
  }
};

template<typename T, typename Deleter = ::kappa::DefaultArrayDel<T>>
requires(!std::is_reference_v<T>)
class UniqueArray : private impl::UniqueArrDel<T, Deleter> {
public:
  using value_type = T;
  using deleter_type = Deleter;
  using size_type = size_t;

  using pointer = T*;
  using const_pointer = const T*;

  using iterator = pointer;
  using const_iterator = const_pointer;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
  using del_base = impl::UniqueArrDel<T, Deleter>;

public:
  UniqueArray() noexcept(std::is_nothrow_default_constructible_v<Deleter>) :
      del_base(), _arr{nullptr}, _sz{0u} {}

  UniqueArray(std::nullptr_t) noexcept(std::is_nothrow_default_constructible_v<Deleter>) :
      del_base(), _arr{nullptr}, _sz{0u} {}

  explicit UniqueArray(const Deleter& del) noexcept(
    std::is_nothrow_copy_constructible_v<Deleter>) : del_base(del), _arr{nullptr}, _sz{0u} {}

  UniqueArray(pointer arr,
              size_type n) noexcept(std::is_nothrow_default_constructible_v<Deleter>) :
      del_base(), _arr{arr}, _sz{n} {}

  UniqueArray(pointer arr, size_type n,
              const Deleter& del) noexcept(std::is_nothrow_copy_constructible_v<Deleter>) :
      del_base(del), _arr{arr}, _sz{n} {}

  UniqueArray(UniqueArray&& other) noexcept(std::is_nothrow_move_constructible_v<Deleter>) :
      del_base(other.get_deleter()), _arr{other._arr}, _sz{other._sz} {
    other._arr = 0u;
  }

  UniqueArray(const UniqueArray&) = delete;

  ~UniqueArray() noexcept(std::is_nothrow_destructible_v<T>) {
    if (!empty()) {
      del_base::delete_array(_arr, _sz);
    }
  }

public:
  UniqueArray& assign(pointer arr, size_type size) noexcept {
    if (!empty()) {
      del_base::delete_array(_arr, _sz);
    }

    _arr = arr;
    _sz = size;

    return *this;
  }

  void reset() noexcept { assign(nullptr, 0u); }

  [[nodiscard]] std::pair<pointer, size_type> release() noexcept {
    pointer ptr = get();
    size_type sz = size();
    _sz = 0;
    _arr = nullptr;
    return std::make_pair(ptr, sz);
  }

public:
  UniqueArray& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  UniqueArray&
  operator=(UniqueArray&& other) noexcept(std::is_nothrow_move_assignable_v<Deleter>) {
    if (!empty()) {
      del_base::delete_array(_arr, _sz);
    }

    del_base::operator=(static_cast<del_base&&>(other));
    _arr = std::move(other._arr);
    _sz = std::move(other._sz);

    other._arr = nullptr;

    return *this;
  }

  UniqueArray& operator=(const UniqueArray&) = delete;

  value_type& operator[](size_type idx) {
    ka_assert(idx < size());
    return get()[idx];
  }

  const value_type& operator[](size_type idx) const {
    ka_assert(idx < size());
    return get()[idx];
  }

  value_type& at(size_type idx) {
    KA_THROW_IF(idx >= size(), std::out_of_range(fmt::format("Index {} out of range", idx)));
    return _arr[idx];
  }

  const value_type& at(size_type idx) const {
    KA_THROW_IF(idx >= size(), std::out_of_range(fmt::format("Index {} out of range", idx)));
    return _arr[idx];
  }

public:
  size_type size() const noexcept { return _sz; }

  pointer get() noexcept { return _arr; }

  const_pointer get() const noexcept { return _arr; }

  pointer data() noexcept { return _arr; }

  const_pointer data() const noexcept { return _arr; }

  bool empty() const noexcept { return _arr == nullptr; }

  Deleter& get_deleter() noexcept { return del_base::get_deleter(); }

  const Deleter& get_deleter() const noexcept { del_base::get_deleter(); }

public:
  explicit operator bool() const noexcept { return !empty(); }

public:
  iterator begin() noexcept { return get(); }

  const_iterator begin() const noexcept { return get(); }

  const_iterator cbegin() const noexcept { return get(); }

  iterator end() noexcept { return get() + size(); }

  const_iterator end() const noexcept { return get() + size(); }

  const_iterator cend() const noexcept { return get() + size(); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

private:
  pointer _arr;
  size_t _sz;
};

template<typename T>
requires(std::is_default_constructible_v<T>)
UniqueArray<T> make_array(size_t n) {
  T* ptr = std::allocator<T>().allocate(n);
  for (size_t i = 0; i < n; ++i) {
    new (ptr + i) T();
  }
  return UniqueArray<T>(ptr, n);
}

template<typename T>
requires(std::copy_constructible<T>)
UniqueArray<T> make_array(size_t n, const T& copy) {
  T* ptr = std::allocator<T>().allocate(n);
  for (size_t i = 0; i < n; ++i) {
    new (ptr + i) T(copy);
  }
  return UniqueArray<T>(ptr, n);
}

template<typename T>
requires(std::is_trivially_constructible_v<T>)
UniqueArray<T> make_array(uninitialized_t, size_t n) {
  return UniqueArray<T>(std::allocator<T>().allocate(n), n);
}

template<typename T>
fn make_zero_array(size_t count) -> UniqueArray<T> {
  static_assert(std::is_trivial_v<T>);
  auto ret = make_array<T>(uninitialized, count);
  std::memset(ret.data(), 0x00, sizeof(ret[0]) * count);
  return ret;
}

} // namespace kappa
