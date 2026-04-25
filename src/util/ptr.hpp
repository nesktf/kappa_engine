#pragma once

#include "../core.hpp"

#include <limits>
#include <memory>
#include <type_traits>

#include <fmt/format.h>

namespace kappa {

template<typename T>
requires(!std::is_void_v<T> && !std::is_reference_v<T>)
class Ref {
public:
  using element_type = T;
  using value_type = std::remove_cvref_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

public:
  constexpr Ref(reference obj) noexcept : _ptr(std::addressof(obj)) {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr Ref(U& obj) noexcept : _ptr(std::addressof(obj)) {}

  constexpr explicit Ref(pointer ptr) : _ptr(ptr) {
    KA_THROW_IF(!_ptr, std::runtime_error("Assigning nullptr to Ref"));
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr Ref(U* ptr) : _ptr(ptr) {
    KA_THROW_IF(!_ptr, std::runtime_error("Assigning nullptr to Ref"));
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr Ref(const Ref<U>& other) noexcept : _ptr{other.data()} {}

  constexpr Ref(const Ref&) noexcept = default;
  constexpr Ref(Ref&&) noexcept = default;

  constexpr ~Ref() noexcept = default;

public:
  constexpr reference get() const noexcept { return *_ptr; }

  constexpr pointer ptr() const noexcept { return _ptr; }

  constexpr pointer data() const noexcept { return _ptr; }

public:
  constexpr pointer operator->() const noexcept { return _ptr; }

  constexpr reference operator*() const noexcept { return *_ptr; }

  constexpr Ref& operator=(pointer ptr) {
    KA_THROW_IF(!ptr, std::runtime_error("Assigning nullptr to Ref"));
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr Ref& operator=(U* ptr) {
    KA_THROW_IF(!ptr, std::runtime_error("Assigning nullptr to Ref"));
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr Ref& operator=(U& obj) noexcept {
    _ptr = std::addressof(obj);
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr Ref& operator=(const Ref<U>& other) noexcept {
    _ptr = other.ptr();
    return *this;
  }

  constexpr Ref& operator=(const Ref&) noexcept = default;
  constexpr Ref& operator=(Ref&&) noexcept = default;

public:
  operator reference() const noexcept { return get(); }

private:
  T* _ptr;
};

constexpr usize dynamic_extent = std::numeric_limits<usize>::max();

namespace impl {

template<usize Extent>
struct SpanExtent {
protected:
  constexpr SpanExtent(usize ext) { ka_assert(ext == Extent); }

  constexpr usize get_extent() const noexcept { return Extent; }

  constexpr void assign_extent(usize) noexcept {}
};

template<>
struct SpanExtent<dynamic_extent> {
protected:
  constexpr SpanExtent() noexcept = default;

  constexpr SpanExtent(usize ext) noexcept : _ext{ext} {}

  constexpr usize get_extent() const noexcept { return _ext; }

  constexpr void assign_extent(usize ext) noexcept { _ext = ext; }

private:
  usize _ext;
};

} // namespace impl

template<typename T, usize Extent = dynamic_extent>
class Span : public impl::SpanExtent<Extent> {
public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

  using iterator = pointer;
  using const_iterator = const_pointer;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  using size_type = usize;
  using difference_type = ptrdiff_t;

  static constexpr size_type extent = dynamic_extent;

public:
  constexpr Span() noexcept
  requires(extent == dynamic_extent)
  = default;

  constexpr explicit Span(reference obj) noexcept
  requires(extent == 1u || extent == dynamic_extent)
      : impl::SpanExtent<Extent>{1u}, _data{std::addressof(obj)} {}

  template<typename It>
  requires(std::contiguous_iterator<It>)
  explicit(extent != dynamic_extent) constexpr Span(It first, size_type count)
  requires(std::is_convertible_v<std::remove_reference_t<std::iter_reference_t<It>> (*)[],
                                 element_type (*)[]>)
      : impl::SpanExtent<Extent>{count}, _data{std::to_address(first)} {}

  template<typename It, typename End>
  requires(std::contiguous_iterator<It> && std::sized_sentinel_for<End, It>)
  explicit(extent != dynamic_extent) constexpr Span(It first, End last)
  requires(std::is_convertible_v<std::remove_reference_t<std::iter_reference_t<It>> (*)[],
                                 element_type (*)[]>)
      : impl::SpanExtent<Extent>{last - first}, _data{std::to_address(first)} {}

  template<usize N>
  constexpr Span(std::type_identity_t<element_type> (&arr)[N]) noexcept
  requires(std::is_convertible_v<std::remove_pointer_t<decltype(std::data(arr))> (*)[],
                                 element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::SpanExtent<Extent>{N}, _data{std::data(arr)} {}

  template<typename U, usize N>
  constexpr Span(std::array<U, N>& arr) noexcept
  requires(std::is_convertible_v<std::remove_pointer_t<decltype(std::data(arr))> (*)[],
                                 element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::SpanExtent<Extent>{N}, _data{std::data(arr)} {}

  template<typename U, usize N>
  constexpr Span(const std::array<U, N>& arr) noexcept
  requires(std::is_convertible_v<std::remove_pointer_t<decltype(std::data(arr))> (*)[],
                                 element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::SpanExtent<Extent>{N}, _data{std::data(arr)} {}

  explicit(extent != dynamic_extent) constexpr Span(std::initializer_list<value_type> il) :
      impl::SpanExtent<Extent>{il.size()}, _data{il.begin()} {}

  template<typename U, usize N>
  explicit(extent != dynamic_extent &&
           N == dynamic_extent) constexpr Span(const Span<U, N>& src) noexcept
  requires(N != extent && std::is_convertible_v<U (*)[], element_type (*)[]> &&
           (extent == dynamic_extent || N == dynamic_extent || extent == N))
      : impl::SpanExtent<Extent>{N}, _data{src.data()} {}

  template<typename U>
  constexpr Span(const Span<U, dynamic_extent>& src) noexcept
  requires(std::is_convertible_v<U (*)[], element_type (*)[]> && extent == dynamic_extent)
      : impl::SpanExtent<dynamic_extent>{src.size()}, _data{src.data()} {}

  constexpr Span(const Span&) noexcept = default;
  constexpr Span(Span&&) noexcept = default;

  constexpr ~Span() noexcept = default;

public:
  template<usize N>
  constexpr Span<element_type, N> first() const
  requires(N <= extent || extent == dynamic_extent)
  {
    if constexpr (extent == dynamic_extent) {
      ka_assert(N < size());
    }
    return {data(), N};
  }

  constexpr Span<element_type, dynamic_extent> first(size_type count) const {
    ka_assert(count < size());
    return {data(), count};
  }

  template<usize N>
  constexpr Span<element_type, N> last() const
  requires(N <= extent || extent == dynamic_extent)
  {
    if constexpr (extent == dynamic_extent) {
      ka_assert(N < size());
    }
    return {data() + (size() - N), N};
  }

  constexpr Span<element_type, dynamic_extent> last(size_type count) const {
    ka_assert(count < size());
    return {data() + (size() - count), count};
  }

public:
  constexpr iterator begin() const noexcept { return _data; }

  constexpr const_iterator cbegin() const noexcept { return _data; }

  constexpr iterator end() const noexcept { return _data + size(); }

  constexpr const_iterator cend() const noexcept { return _data + size(); }

  constexpr reverse_iterator rbegin() const noexcept { return {_data + size() - 1}; }

  constexpr const_reverse_iterator crbegin() const noexcept { return {_data + size() - 1}; }

  constexpr reverse_iterator rend() const noexcept { return {_data - 1}; }

  constexpr const_reverse_iterator crend() const noexcept { return {_data - 1}; }

public:
  constexpr pointer data() const noexcept { return _data; }

  constexpr size_type size() const noexcept { return impl::SpanExtent<Extent>::get_extent(); }

  constexpr size_type size_bytes() const { return size() * sizeof(T); }

  constexpr reference front() const { return *begin(); }

  constexpr reference back() const { return *(end() - 1); }

  constexpr bool empty() const noexcept { return size() == 0u; }

  constexpr reference at(size_type idx) const {
    KA_THROW_IF(idx >= size(), std::out_of_range(fmt::format("Index {} out of range", idx)));
    return _data[idx];
  }

  constexpr pointer at_opt(size_type idx) noexcept {
    return idx >= size() ? nullptr : data() + idx;
  }

public:
  constexpr explicit operator bool() const noexcept { return !empty(); }

  constexpr reference operator[](size_type idx) const {
    ka_assert(idx < size());
    return _data[idx];
  }

  constexpr Span& operator=(const Span& other) noexcept = default;
  constexpr Span& operator=(Span&& other) noexcept = default;

  template<typename U, usize N>
  requires(std::is_convertible_v<U (*)[], element_type (*)[]> &&
           (extent == dynamic_extent || N == dynamic_extent || extent == N))
  constexpr Span& operator=(const Span<U, N>& other) noexcept {
    impl::SpanExtent<Extent>::assign_extent(other.size());
    _data = other.data();
    return *this;
  }

private:
  pointer _data;
};

} // namespace kappa
