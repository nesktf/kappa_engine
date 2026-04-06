#pragma once

#include "../defs.hpp"

#include <concepts>
#include <exception>
#include <functional>
#include <memory>

namespace kappa {

class BadOptionalAccess : public std::exception {
public:
  BadOptionalAccess() = default;

public:
  const char* what() const noexcept override { return "bad_optional_access"; }
};

struct nullopt_t {};

constexpr inline nullopt_t nullopt;

using std::in_place_t;
constexpr in_place_t in_place;

namespace meta {

template<typename T>
concept valid_optional_type = !std::same_as<T, in_place_t> && !std::same_as<T, nullopt_t> &&
                              !std::is_void_v<T> && !std::is_reference_v<T>;

} // namespace meta

template<meta::valid_optional_type T>
class Optional;

template<typename T>
using Nullable = Optional<T>;

namespace meta {

template<typename T>
struct optional_checker : public std::false_type {};

template<typename T>
struct optional_checker<Optional<T>> : public std::true_type {};

template<typename T>
constexpr bool optional_checker_v = optional_checker<T>::value;

template<typename T>
concept optional_type = optional_checker_v<T>;

} // namespace meta

namespace impl {

template<bool valid, typename T, typename... Args>
constexpr void
rebind_nullable(T& obj, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
  static_assert(std::is_nothrow_destructible_v<T>, "T has to be nothrow destructible");

  if constexpr (valid) {
    // If is obj a constructed object
    if constexpr (std::is_nothrow_constructible_v<T>) {
      std::destroy_at(std::addressof(obj));
      new (std::addressof(obj)) T(std::forward<Args>(args)...);
    } else {
      T old(std::move(obj)); // Might throw
      std::destroy_at(std::addressof(obj));
#ifdef __cpp_exceptions
      try {
#endif
        new (std::addressof(obj)) T(std::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        new (std::addressof(obj)) T(std::move(old));
        throw;
      }
#endif
    }
  } else {
    new (std::addressof(obj)) T(std::forward<Args>(args)...); // Might throw
  }
}

template<typename T>
class OptionalData {
public:
  constexpr OptionalData() noexcept : _dummy{}, _active{false} {}

  constexpr OptionalData(nullopt_t) noexcept : _dummy{}, _active{false} {}

  constexpr OptionalData(T&& obj) noexcept(std::is_nothrow_move_constructible_v<T>) :
      _value(std::move(obj)), _active{true} {}

  constexpr OptionalData(const T& obj) noexcept(std::is_nothrow_copy_constructible_v<T>) :
      _value(obj), _active{true} {}

  template<typename... Args>
  constexpr OptionalData(std::in_place_t,
                         Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) :
      _value(std::forward<Args>(args)...), _active{true} {}

  template<typename U, typename... Args>
  constexpr OptionalData(std::in_place_t, std::initializer_list<U> il, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<T, std::initializer_list<U>, Args...>) :
      _value(il, std::forward<Args>(args)...), _active{true} {}

  constexpr ~OptionalData()
  requires(!std::is_trivially_destructible_v<T>)
  {
    destroy();
  }

  constexpr ~OptionalData() noexcept
  requires(std::is_trivially_destructible_v<T>)
  = default;

  constexpr OptionalData(const OptionalData& other) noexcept(
    std::is_nothrow_copy_constructible_v<T>)
  requires(!std::is_trivially_copy_constructible_v<T>)
      : _dummy(), _active{other.has_value()} {
    if (_active) {
      construct(other.get_value());
    }
  }

  constexpr OptionalData(const OptionalData& other) noexcept
  requires(std::is_trivially_copy_constructible_v<T>)
  = default;

  template<typename U>
  constexpr OptionalData(const OptionalData<U>& other) noexcept(
    std::is_nothrow_constructible_v<T, U>)
  requires(!std::same_as<U, std::remove_cv_t<T>> && std::convertible_to<U, T>)
      : _value(other.get_value()), _active{true} {}

  constexpr OptionalData(OptionalData&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
  requires(!std::is_trivially_move_constructible_v<T>)
      : _dummy(), _active{other.has_value()} {
    if (_active) {
      construct(std::move(other.get_value()));
    }
  }

  constexpr OptionalData(OptionalData&& other) noexcept
  requires(std::is_nothrow_move_constructible_v<T>)
  = default;

  template<typename U>
  constexpr OptionalData(OptionalData<U>&& other) noexcept(std::is_nothrow_constructible_v<T, U&&>)
  requires(!std::same_as<U, std::remove_cv_t<T>> && std::convertible_to<U &&, T>)
      : _value(std::move(other.get_value())), _active{true} {}

public:
  constexpr OptionalData& operator=(const OptionalData&) noexcept
  requires(std::is_trivially_copy_assignable_v<T>)
  = default;

  constexpr OptionalData&
  operator=(const OptionalData& other) noexcept(std::is_nothrow_copy_assignable_v<T>) {
    auto do_thing = [&, this]() {
      _active = other.has_value();
      if (_active) {
        _value = other.get_value();
      }
    };

    destroy();

    if constexpr (std::is_nothrow_constructible_v<T>) {
      try {
        do_thing();
      } catch (...) {
        _active = false;
        throw;
      }
    } else {
      do_thing();
    }

    return *this;
  }

  template<typename U>
  constexpr OptionalData&
  operator=(const OptionalData& other) noexcept(std::is_nothrow_copy_assignable_v<T>)
  requires(!std::same_as<U, std::remove_cv_t<T>> && std::convertible_to<U &&, T>)
  {
    auto do_thing = [&, this]() {
      _active = other.has_value();
      if (_active) {
        _value = other.get_value();
      }
    };

    destroy();

    if constexpr (std::is_nothrow_constructible_v<T>) {
      try {
        do_thing();
      } catch (...) {
        _active = false;
        throw;
      }
    } else {
      do_thing();
    }

    return *this;
  }

  constexpr OptionalData& operator=(OptionalData&&) noexcept
  requires(std::is_trivially_move_assignable_v<T>)
  = default;

  constexpr OptionalData&
  operator=(OptionalData&& other) noexcept(std::is_nothrow_move_assignable_v<T>) {
    auto do_thing = [&, this]() {
      _active = other.has_value();
      if (_active) {
        _value = std::move(other.get_value());
      }
    };

    destroy();

    if constexpr (std::is_nothrow_constructible_v<T>) {
      try {
        do_thing();
      } catch (...) {
        _active = false;
        throw;
      }
    } else {
      do_thing();
    }

    return *this;
  }

  template<typename U>
  constexpr OptionalData&
  operator=(OptionalData&& other) noexcept(std::is_nothrow_move_assignable_v<T>)
  requires(!std::same_as<U, std::remove_cv_t<T>> && std::convertible_to<U &&, T>)
  {
    auto do_thing = [&, this]() {
      _active = other.has_value();
      if (_active) {
        _value = std::move(other.get_value());
      }
    };

    destroy();

    if constexpr (std::is_nothrow_constructible_v<T>) {
      try {
        do_thing();
      } catch (...) {
        _active = false;
        throw;
      }
    } else {
      do_thing();
    }

    return *this;
  }

protected:
  constexpr bool has_value() const noexcept { return _active; }

  constexpr T& get_value() noexcept { return _value; }

  constexpr const T& get_value() const noexcept { return _value; }

protected:
  constexpr void destroy() noexcept {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      if (has_value()) {
        _value.~T();
      }
    }
    _active = false;
  }

  template<typename... Args>
  constexpr void construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    new (std::addressof(_value)) T(std::forward<Args>(args)...);
    _active = true;
  }

  template<typename U, typename... Args>
  constexpr void construct(std::initializer_list<U> il,
                           Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    new (std::addressof(_value)) T(il, std::forward<Args>(args)...);
    _active = true;
  }

private:
  union {
    T _value;
    char _dummy;
  };

  bool _active;
};
} // namespace impl

template<meta::valid_optional_type T>
class Optional : public impl::OptionalData<T> {
private:
  using base_t = impl::OptionalData<T>;

public:
  using impl::OptionalData<T>::OptionalData;

public:
  constexpr T& value() & {
    KA_THROW_IF(!base_t::has_value(), BadOptionalAccess());
    return base_t::get_value();
  }

  constexpr T&& value() && {
    KA_THROW_IF(!base_t::has_value(), BadOptionalAccess());
    return std::move(base_t::get_value());
  }

  constexpr const T& value() const& {
    KA_THROW_IF(!base_t::has_value(), BadOptionalAccess());
    return base_t::get_value();
  }

  constexpr const T&& value() const&& {
    KA_THROW_IF(!base_t::has_value(), BadOptionalAccess());
    return std::move(base_t::get_value());
  }

  template<typename U = std::remove_cv_t<T>>
  constexpr T value_or(U&& def_value) const&
  requires(std::is_copy_constructible_v<T> && std::is_convertible_v<U &&, T>)
  {
    return has_value() ? base_t::get_value() : static_cast<T>(std::forward<U>(def_value));
  }

  template<typename U = std::remove_cv_t<T>>
    constexpr T value_or(U&& def_value) &&
    requires(std::is_move_constructible_v<T>&& std::is_convertible_v<U&&, T>) {
      return has_value() ? std::move(base_t::get_value())
                         : static_cast<T>(std::forward<U>(def_value));
    }

    template<typename... Args>
    constexpr T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    base_t::destroy();
    base_t::construct(std::forward<Args>(args)...);
    return base_t::get_value();
  }

  template<typename U, typename... Args>
  constexpr T& emplace(std::initializer_list<U> il,
                       Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    base_t::destroy();
    base_t::construct(il, std::forward<Args>(args)...);
    return base_t::get_value();
  }

  constexpr void reset() noexcept {
    if (base_t::has_value()) {
      base_t::destroy();
    }
  }

  constexpr bool has_value() const noexcept { return base_t::has_value(); }

  constexpr bool empty() const noexcept { return !has_value(); }

  constexpr explicit operator bool() const noexcept { return has_value(); }

public:
  using impl::OptionalData<T>::operator=;

  constexpr T& operator*() & {
    ka_assert(base_t::has_value());
    return base_t::get_value();
  }

  constexpr T&& operator*() && {
    ka_assert(base_t::has_value());
    return std::move(base_t::get_value());
  }

  constexpr const T& operator*() const& {
    ka_assert(base_t::has_value());
    return base_t::get_value();
  }

  constexpr const T&& operator*() const&& {
    ka_assert(base_t::has_value());
    return std::move(base_t::get_value());
  }

  constexpr T* operator->() {
    ka_assert(base_t::has_value());
    return std::addressof(base_t::get_value());
  }

  constexpr const T* operator->() const {
    ka_assert(base_t::has_value());
    return std::addressof(base_t::get_value());
  }

public:
  template<typename F>
  constexpr auto and_then(F&& func) & {
    using U = std::remove_cvref_t<std::invoke_result_t<F, decltype(base_t::get_value())>>;
    static_assert(meta::optional_type<U>, "F needs to return an optional");
    static_assert(!std::is_void_v<U>, "F can't return void");
    if (has_value()) {
      return std::invoke(std::forward<F>(func), base_t::get_value());
    } else {
      return U{nullopt};
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) && {
    using U =
      std::remove_cvref_t<std::invoke_result_t<F, decltype(std::move(base_t::get_value()))>>;
    static_assert(meta::optional_type<U>, "F needs to return an optional");
    static_assert(!std::is_void_v<U>, "F can't return void");
    if (has_value()) {
      return std::invoke(std::forward<F>(func), std::move(base_t::get_value()));
    } else {
      return U{nullopt};
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) const& {
    using U = std::remove_cvref_t<std::invoke_result_t<F, decltype(base_t::get_value())>>;
    static_assert(meta::optional_type<U>, "F needs to return an optional");
    static_assert(!std::is_void_v<U>, "F can't return void");
    if (has_value()) {
      return std::invoke(std::forward<F>(func), base_t::get_value());
    } else {
      return U{nullopt};
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) const&& {
    using U =
      std::remove_cvref_t<std::invoke_result_t<F, decltype(std::move(base_t::get_value()))>>;
    static_assert(meta::optional_type<U>, "F needs to return an optional");
    static_assert(!std::is_void_v<U>, "F can't return void");
    if (has_value()) {
      return std::invoke(std::forward<F>(func), std::move(base_t::get_value()));
    } else {
      return U{nullopt};
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) & {
    using U = std::remove_cv_t<std::invoke_result_t<F, decltype(base_t::get_value())>>;
    static_assert(!std::is_reference_v<U>, "F can't return a reference type");
    static_assert(!std::is_void_v<U>, "F can't return void");
    if (has_value()) {
      return Optional<U>{in_place, std::invoke(std::forward<F>(func), base_t::get_value())};
    } else {
      return Optional<U>{nullopt};
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) && {
    using U = std::remove_cv_t<std::invoke_result_t<F, decltype(std::move(base_t::get_value()))>>;
    static_assert(!std::is_reference_v<U>, "F can't return a reference type");
    static_assert(!std::is_void_v<U>, "F can't return void");
    if (has_value()) {
      return Optional<U>{in_place,
                         std::invoke(std::forward<F>(func), std::move(base_t::get_value()))};
    } else {
      return Optional<U>{nullopt};
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) const& {
    using U = std::remove_cv_t<std::invoke_result_t<F, decltype(base_t::get_value())>>;
    static_assert(!std::is_reference_v<U>, "F can't return a reference type");
    static_assert(!std::is_void_v<U>, "F can't return void");
    if (has_value()) {
      return Optional<U>{in_place, std::invoke(std::forward<F>(func), base_t::get_value())};
    } else {
      return Optional<U>{nullopt};
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) const&& {
    using U = std::remove_cv_t<std::invoke_result_t<F, decltype(std::move(base_t::get_value()))>>;
    static_assert(!std::is_reference_v<U>, "F can't return a reference type");
    static_assert(!std::is_void_v<U>, "F can't return void");
    if (has_value()) {
      return Optional<U>{in_place,
                         std::invoke(std::forward<F>(func), std::move(base_t::get_value()))};
    } else {
      return Optional<U>{nullopt};
    }
  }

  template<typename F>
  constexpr Optional or_else(F&& func) const& {
    using U = std::remove_cvref_t<std::invoke_result_t<F>>;
    static_assert(std::same_as<U, Optional>, "F needs to return the same optional type");
    return has_value() ? *this : std::invoke(std::forward<F>(func));
  }

  template<typename F>
  constexpr Optional or_else(F&& func) && {
    using U = std::remove_cvref_t<std::invoke_result_t<F>>;
    static_assert(std::same_as<U, Optional>, "F needs to return the same optional type");
    return has_value() ? std::move(*this) : std::invoke(std::forward<F>(func));
  }
};

} // namespace kappa
