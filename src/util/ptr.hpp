#pragma once

#include "../defs.hpp"

namespace kappa {

template<typename T>
requires(!std::is_void_v<T> && !std::is_reference_v<T>)
class PtrView {
public:
  using element_type = T;
  using value_type = std::remove_cvref_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

public:
  constexpr PtrView() noexcept = default;

  constexpr PtrView(std::nullptr_t) noexcept : _ptr{nullptr} {}

  constexpr PtrView(pointer ptr) noexcept : _ptr{ptr} {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr PtrView(U& obj) noexcept : _ptr{std::addressof(obj)} {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr PtrView(U* ptr) noexcept : _ptr(ptr) {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr PtrView(const PtrView<U>& other) noexcept : _ptr{other.data()} {}

  constexpr PtrView(const PtrView&) noexcept = default;
  constexpr PtrView(PtrView&&) noexcept = default;

  constexpr ~PtrView() noexcept = default;

public:
  constexpr reference get() const {
    ka_assert(_ptr, "Invalid pointer");
    return *_ptr;
  }

  constexpr pointer ptr() const noexcept { return _ptr; }

  constexpr pointer data() const noexcept { return _ptr; }

public:
  [[nodiscard]] constexpr bool empty() const { return _ptr == nullptr; }

  constexpr explicit operator bool() const { return !empty(); }

  constexpr pointer operator->() const {
    ka_assert(_ptr, "Invalid pointer");
    return _ptr;
  }

  constexpr reference operator*() const {
    ka_assert(_ptr, "Invalid pointer");
    return *_ptr;
  }

  constexpr PtrView& operator=(std::nullptr_t) noexcept {
    _ptr = nullptr;
    return *this;
  }

  constexpr PtrView& operator=(pointer ptr) noexcept {
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr PtrView& operator=(U* ptr) {
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr PtrView& operator=(U& obj) noexcept {
    _ptr = std::addressof(obj);
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr PtrView& operator=(const PtrView<U>& other) noexcept {
    _ptr = other.data();
    return *this;
  }

  constexpr PtrView& operator=(const PtrView&) noexcept = default;
  constexpr PtrView& operator=(PtrView&&) noexcept = default;

public:
  operator reference() const { return get(); }

private:
  T* _ptr;
};

template<typename T>
requires(!std::is_void_v<T> && !std::is_reference_v<T>)
class RefView {
public:
  using element_type = T;
  using value_type = std::remove_cvref_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

public:
  constexpr RefView(reference obj) noexcept : _ptr(std::addressof(obj)) {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr RefView(U& obj) noexcept : _ptr(std::addressof(obj)) {}

  constexpr explicit RefView(pointer ptr) : _ptr(ptr) {
    KA_THROW_IF(!_ptr, std::runtime_error("Assigning nullptr to RefView"));
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr RefView(U* ptr) : _ptr(ptr) {
    KA_THROW_IF(!_ptr, std::runtime_error("Assigning nullptr to RefView"));
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr RefView(const RefView<U>& other) noexcept : _ptr{other.data()} {}

  constexpr RefView(const RefView&) noexcept = default;
  constexpr RefView(RefView&&) noexcept = default;

  constexpr ~RefView() noexcept = default;

public:
  constexpr reference get() const noexcept { return *_ptr; }

  constexpr pointer ptr() const noexcept { return _ptr; }

  constexpr pointer data() const noexcept { return _ptr; }

public:
  constexpr pointer operator->() const noexcept { return _ptr; }

  constexpr reference operator*() const noexcept { return *_ptr; }

  constexpr RefView& operator=(pointer ptr) {
    KA_THROW_IF(!ptr, std::runtime_error("Assigning nullptr to RefView"));
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr RefView& operator=(U* ptr) {
    KA_THROW_IF(!ptr, std::runtime_error("Assigning nullptr to RefView"));
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr RefView& operator=(U& obj) noexcept {
    _ptr = std::addressof(obj);
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr RefView& operator=(const RefView<U>& other) noexcept {
    _ptr = other.ptr();
    return *this;
  }

  constexpr RefView& operator=(const RefView&) noexcept = default;
  constexpr RefView& operator=(RefView&&) noexcept = default;

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
