#pragma once

#include "./optional.hpp"

#include <utility>

namespace kappa {

template<typename>
class BadExpectedAccess;

template<>
class BadExpectedAccess<void> : public std::exception {
public:
  BadExpectedAccess() = default;

public:
  const char* what() const noexcept override { return "bad_expected_access"; }
};

template<typename E>
class BadExpectedAccess : public BadExpectedAccess<void> {
public:
  explicit BadExpectedAccess(const E& err) : _err(err) {}

  explicit BadExpectedAccess(E&& err) : _err(std::move(err)) {}

public:
  const char* what() const noexcept override {
    if constexpr (requires(const E err) {
                    { err.what() } -> std::same_as<const char*>;
                  }) {
      return _err.what();
    } else {
      return BadExpectedAccess<void>::what();
    }
  }

  E& error() & noexcept { return _err; }

  const E& error() const& noexcept { return _err; }

  E&& error() && noexcept { return std::move(_err); }

  const E&& error() const&& noexcept { return std::move(_err); }

private:
  E _err;
};

template<typename E>
requires(!std::is_void_v<E>)
class unexpected {
public:
  unexpected() = delete;

  constexpr unexpected(E&& err) noexcept(std::is_nothrow_move_constructible_v<E>) :
      _err(std::move(err)) {}

  constexpr unexpected(const E& err) noexcept(std::is_nothrow_copy_constructible_v<E>) :
      _err(err) {}

  template<typename... Args>
  constexpr explicit unexpected(Args&&... args) noexcept(
    std::is_nothrow_constructible_v<E, Args...>) : _err(std::forward<Args>(args)...) {}

  template<typename U, typename... Args>
  constexpr explicit unexpected(std::initializer_list<U> list, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...>) :
      _err(list, std::forward<Args>(args)...) {}

public:
  constexpr E& error() & noexcept { return _err; };

  constexpr const E& error() const& noexcept { return _err; }

  constexpr E&& error() && noexcept { return std::move(_err); };

  constexpr const E&& error() const&& noexcept { return std::move(_err); }

private:
  E _err;
};

template<typename E, typename G>
constexpr bool operator==(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() == rhs.error();
}

template<typename E, typename G>
constexpr bool operator!=(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() != rhs.error();
}

template<typename E, typename G>
constexpr bool operator<=(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() <= rhs.error();
}

template<typename E, typename G>
constexpr bool operator>=(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() >= rhs.error();
}

template<typename E, typename G>
constexpr bool operator<(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() < rhs.error();
}

template<typename E, typename G>
constexpr bool operator>(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() > rhs.error();
}

template<typename E>
unexpected(E) -> unexpected<E>;

template<typename E>
unexpected<typename std::decay_t<E>> make_unexpected(E&& e) {
  return unexpected<typename std::decay_t<E>>{std::forward<E>(e)};
}

struct unexpect_t {};

constexpr inline unexpect_t unexpect;

namespace impl {

template<bool valid, typename T, typename E, typename... Args>
constexpr void expect_rebind_error(T& val, E& err, Args&&... args) noexcept(
  std::is_nothrow_constructible_v<T, Args...> || std::is_nothrow_move_constructible_v<T>) {
  static_assert(std::is_nothrow_destructible_v<E>, "E has to be nothrow destructible");
  static_assert(std::is_nothrow_destructible_v<T>, "T has to be nothrow destructible");
  if constexpr (valid) {
    // If is val a constructed object
    if constexpr (std::is_nothrow_constructible_v<E, Args...>) {
      std::destroy_at(std::addressof(val));
      new (std::addressof(err)) E(std::forward<Args>(args)...);
    } else if constexpr (std::is_nothrow_move_constructible_v<E>) {
      E new_err(std::forward<Args>(args)...); // Might throw
      std::destroy_at(std::addressof(val));
      new (std::addressof(err)) E(std::move(new_err));
    } else {
      T old_val(std::move(val)); // Might or might not throw
      std::destroy_at(std::addressof(val));
#ifdef __cpp_exceptions
      try {
#endif
        new (std::addressof(err)) E(std::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        new (std::addressof(val)) T(std::move(old_val));
        throw;
      }
#endif
    }
  } else {
    // If is err a constructed object
    if constexpr (std::is_nothrow_constructible_v<E, Args...>) {
      std::destroy_at(std::addressof(err));
      new (std::addressof(err)) E(std::forward<Args>(args)...);
    } else if constexpr (std::is_nothrow_move_constructible_v<E>) {
      E new_err(std::forward<Args>(args)...); // Might throw
      std::destroy_at(std::addressof(err));
      new (std::addressof(err)) E(std::move(new_err));
    } else {
      E old_err(std::move(err)); // Might throw
      std::destroy_at(std::addressof(err));
#ifdef __cpp_exceptions
      try {
#endif
        new (std::addressof(err)) E(std::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        new (std::addressof(err)) E(std::move(old_err));
        throw;
      }
#endif
    }
  }
}

template<bool valid, typename T, typename E, typename... Args>
constexpr void expect_rebind_value(T& val, E& err, Args&&... args) noexcept(
  std::is_nothrow_constructible_v<T, Args...> || std::is_nothrow_move_constructible_v<T>) {
  static_assert(std::is_nothrow_destructible_v<E>, "E has to be nothrow destructible");
  static_assert(std::is_nothrow_destructible_v<T>, "T has to be nothrow destructible");

  if constexpr (valid) {
    // If is val a constructed object
    if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
      std::destroy_at(std::addressof(val));
      new (std::addressof(val)) T(std::forward<Args>(args)...);
    } else if constexpr (std::is_nothrow_move_constructible_v<T>) {
      T new_val(std::forward<Args>(args)...); // Might throw
      std::destroy_at(std::addressof(val));
      new (std::addressof(val)) T(std::move(new_val));
    } else {
      T old_val(std::move(val)); // Might throw
      std::destroy_at(std::addressof(val));
#ifdef __cpp_exceptions
      try {
#endif
        new (std::addressof(val)) T(std::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        new (std::addressof(val)) T(std::move(old_val));
        throw;
      }
#endif
    }
  } else {
    // If is err a constructed object
    if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
      std::destroy_at(std::addressof(err));
      new (std::addressof(val)) T(std::forward<Args>(args)...);
    } else if constexpr (std::is_nothrow_move_constructible_v<T>) {
      T new_val(std::forward<Args>(args)...); // Might throw
      std::destroy_at(std::addressof(err));
      new (std::addressof(val)) T(std::move(new_val));
    } else {
      E old_err(std::move(err)); // Might or might not throw
      std::destroy_at(std::addressof(err));
#ifdef __cpp_exceptions
      try {
#endif
        new (std::addressof(val)) T(std::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        new (std::addressof(err)) E(std::move(old_err));
        throw;
      }
#endif
    }
  }
}

template<typename T, typename E>
class ExpectedStorage {
private:
  static constexpr bool _triv_destr_val = std::is_trivially_destructible_v<T>;
  static constexpr bool _triv_destr_err = std::is_trivially_destructible_v<E>;

  static constexpr bool _triv_movec =
    std::is_trivially_move_constructible_v<T> && std::is_trivially_move_constructible_v<E>;

  static constexpr bool _triv_copyc =
    std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_constructible_v<E>;

public:
  constexpr ExpectedStorage() noexcept(std::is_nothrow_default_constructible_v<T>)
  requires(std::is_default_constructible_v<T>)
      : _value(), _valid(true) {}

  template<typename... Args>
  constexpr ExpectedStorage(in_place_t,
                            Args&&... arg) noexcept(std::is_nothrow_constructible_v<T, Args...>) :
      _value(std::forward<Args>(arg)...), _valid(true) {}

  template<typename U, typename... Args>
  constexpr ExpectedStorage(in_place_t, std::initializer_list<U> il, Args&&... arg) noexcept(
    std::is_nothrow_constructible_v<T, std::initializer_list<U>, Args...>) :
      _value(il, std::forward<Args>(arg)...), _valid(true) {}

  template<typename... Args>
  constexpr ExpectedStorage(unexpect_t,
                            Args&&... arg) noexcept(std::is_nothrow_constructible_v<E, Args...>) :
      _error(std::forward<Args>(arg)...), _valid(false) {}

  template<typename U, typename... Args>
  constexpr ExpectedStorage(unexpect_t, std::initializer_list<U> il, Args&&... arg) noexcept(
    std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...>) :
      _error(il, std::forward<Args>(arg)...), _valid(false) {}

  constexpr ExpectedStorage(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>) :
      _value(std::move(val)), _valid(true) {}

  constexpr ExpectedStorage(const T& val) noexcept(std::is_nothrow_copy_constructible_v<T>) :
      _value(val), _valid(true) {}

  template<typename U = std::remove_cv_t<T>>
  constexpr explicit(!std::is_convertible_v<U, T>)
    ExpectedStorage(U&& val) noexcept(std::is_nothrow_constructible_v<T, U>) :
      _value(std::forward<U>(val)), _valid(true) {}

  template<typename G>
  constexpr explicit(!std::is_convertible_v<const G&, E>) ExpectedStorage(
    const unexpected<G>& unex) noexcept(std::is_nothrow_constructible_v<E, const G&>) :
      _error(unex.error()), _valid(false) {}

  template<typename G>
  constexpr explicit(!std::is_convertible_v<G, E>)
    ExpectedStorage(unexpected<G>&& unex) noexcept(std::is_nothrow_constructible_v<E, G>) :
      _error(std::move(unex).error()), _valid(false) {}

public:
  constexpr ExpectedStorage(ExpectedStorage&& other) noexcept
  requires(_triv_movec)
  = default;

  constexpr ExpectedStorage(ExpectedStorage&& other) noexcept(
    std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
  requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<E> && !_triv_movec)
      : _valid(std::move(other._valid)) {
    if (other.has_value()) {
      new (std::addressof(_value)) T(std::move(other.get()));
    } else {
      new (std::addressof(_error)) E(std::move(other.get_error()));
    }
  }

public:
  constexpr ExpectedStorage(const ExpectedStorage& other) noexcept
  requires(_triv_copyc)
  = default;

  constexpr ExpectedStorage(const ExpectedStorage& other) noexcept(
    std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<E>)
  requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E> && !_triv_copyc)
      : _valid(other._valid) {
    if (other.has_value()) {
      new (std::addressof(_value)) T(other.get());
    } else {
      new (std::addressof(_error)) E(other.get_error());
    }
  }

public:
  constexpr ExpectedStorage&
  operator=(ExpectedStorage& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                             std::is_nothrow_move_constructible_v<E>)
  requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<E>)
  {
    if (std::addressof(other) == this) {
      return *this;
    }

    if (has_value()) {
      if (other.has_value()) {
        expect_rebind_value<true>(_value, _error, std::move(other.get()));
      } else {
        expect_rebind_error<true>(_value, _error, std::move(other.get_error()));
      }
    } else {
      if (other.has_value()) {
        expect_rebind_value<false>(_value, _error, std::move(other.get()));
      } else {
        expect_rebind_error<false>(_value, _error, std::move(other.get_error()));
      }
    }
    _valid = std::move(other._valid);
    return *this;
  }

public:
  constexpr ExpectedStorage&
  operator=(const ExpectedStorage& other) noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                                   std::is_nothrow_copy_constructible_v<E>)
  requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>)
  {
    if (std::addressof(other) == this) {
      return *this;
    }

    if (has_value()) {
      if (other.has_value()) {
        expect_rebind_value<true>(_value, _error, other.get());
      } else {
        expect_rebind_error<true>(_value, _error, other.get_error());
      }
    } else {
      if (other.has_value()) {
        expect_rebind_value<false>(_value, _error, other.get());
      } else {
        expect_rebind_error<false>(_value, _error, other.get_error());
      }
    }
    _valid = other._valid;
    return *this;
  }

public:
  constexpr ~ExpectedStorage() noexcept
  requires(_triv_destr_val && _triv_destr_err)
  = default;

  constexpr ~ExpectedStorage() noexcept
  requires(!_triv_destr_val || !_triv_destr_err)
  {
    static_assert(std::is_nothrow_destructible_v<E>, "E has to be nothrow destructible");
    static_assert(std::is_nothrow_destructible_v<T>, "T has to be nothrow destructible");
    if constexpr (_triv_destr_val && !_triv_destr_err) {
      if (!_valid) {
        std::destroy_at(std::addressof(_error));
      }
    } else if constexpr (!_triv_destr_val && _triv_destr_err) {
      if (_valid) {
        std::destroy_at(std::addressof(_value));
      }
    } else {
      if (_valid) {
        std::destroy_at(std::addressof(_value));
      } else {
        std::destroy_at(std::addressof(_error));
      }
    }
  }

public:
  template<typename... Args>
  constexpr T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...> ||
                                                std::is_nothrow_move_constructible_v<T>) {
    if (has_value()) {
      expect_rebind_value<true>(_value, _error, std::forward<Args>(args)...);
    } else {
      expect_rebind_value<false>(_value, _error, std::forward<Args>(args)...);
      _valid = true;
    }
    return _value;
  }

  template<typename U, typename... Args>
  constexpr T& emplace(std::initializer_list<U> il, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<T, std::initializer_list<U>, Args...> ||
    std::is_nothrow_move_constructible_v<T>) {
    if (has_value()) {
      expect_rebind_value<true>(_value, _error, il, std::forward<Args>(args)...);
    } else {
      expect_rebind_value<false>(_value, _error, il, std::forward<Args>(args)...);
      _valid = true;
    }
    return _value;
  }

  template<typename... Args>
  constexpr E&
  emplace_error(Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...> ||
                                         std::is_nothrow_move_constructible_v<E>) {
    if (has_value()) {
      expect_rebind_error<true>(_value, _error, std::forward<Args>(args)...);
      _valid = false;
    } else {
      expect_rebind_error<false>(_value, _error, std::forward<Args>(args)...);
    }
    return _error;
  }

  template<typename U, typename... Args>
  constexpr E& emplace_error(std::initializer_list<U> il, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...> ||
    std::is_nothrow_move_constructible_v<E>) {
    if (has_value()) {
      expect_rebind_error<true>(_value, _error, il, std::forward<Args>(args)...);
      _valid = false;
    } else {
      expect_rebind_error<false>(_value, _error, il, std::forward<Args>(args)...);
    }
    return _error;
  }

public:
  constexpr bool has_value() const noexcept { return _valid; }

  constexpr bool has_error() const noexcept { return !_valid; }

protected:
  constexpr T& get() & noexcept { return _value; }

  constexpr T&& get() && noexcept { return std::move(_value); }

  constexpr const T& get() const& noexcept { return _value; }

  constexpr const T&& get() const&& noexcept { return std::move(_value); }

  constexpr E& get_error() & noexcept { return _error; }

  constexpr E&& get_error() && noexcept { return std::move(_error); }

  constexpr const E& get_error() const& noexcept { return _error; }

  constexpr const E&& get_error() const&& noexcept { return std::move(_error); }

private:
  union {
    T _value;
    E _error;
  };

  bool _valid;
};

template<typename E>
class ExpectedStorage<void, E> {
private:
  static constexpr bool _triv_destr = std::is_trivially_destructible_v<E>;
  static constexpr bool _triv_movec = std::is_trivially_move_constructible_v<E>;
  static constexpr bool _triv_copyc = std::is_trivially_copy_constructible_v<E>;

public:
  constexpr ExpectedStorage() noexcept : _valid(true) {}

  constexpr ExpectedStorage(in_place_t) noexcept : _valid(true) {}

  template<typename... Args>
  constexpr ExpectedStorage(unexpect_t,
                            Args&&... arg) noexcept(std::is_nothrow_constructible_v<E, Args...>) :
      _error(std::forward<Args>(arg)...), _valid(false) {}

  template<typename U, typename... Args>
  constexpr ExpectedStorage(unexpect_t, std::initializer_list<U> l, Args&&... arg) noexcept(
    std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...>) :
      _error(l, std::forward<Args>(arg)...), _valid(false) {}

  template<typename G>
  constexpr explicit(!std::is_convertible_v<const G&, E>) ExpectedStorage(
    const unexpected<G>& unex) noexcept(std::is_nothrow_constructible_v<E, const G&>) :
      _error(unex.error()), _valid(false) {}

  template<typename G>
  constexpr explicit(!std::is_convertible_v<G, E>)
    ExpectedStorage(unexpected<G>&& unex) noexcept(std::is_nothrow_constructible_v<E, G>) :
      _error(std::move(unex).error()), _valid(false) {}

public:
  constexpr ExpectedStorage(ExpectedStorage&& other) noexcept
  requires(_triv_movec)
  = default;

  constexpr ExpectedStorage(ExpectedStorage&& other) noexcept(
    std::is_nothrow_move_constructible_v<E>)
  requires(std::is_move_constructible_v<E> && !_triv_movec)
      : _valid(std::move(other._valid)) {
    if (other.has_error()) {
      new (std::addressof(_error)) E(std::move(other.get_error()));
    }
  }

public:
  constexpr ExpectedStorage(const ExpectedStorage& other) noexcept
  requires(_triv_copyc)
  = default;

  constexpr ExpectedStorage(const ExpectedStorage& other) noexcept(
    std::is_nothrow_copy_constructible_v<E>)
  requires(std::is_copy_constructible_v<E> && !_triv_copyc)
      : _valid(other._valid) {
    if (other.has_error()) {
      new (std::addressof(_error)) E(other.get_error());
    }
  }

public:
  constexpr ExpectedStorage&
  operator=(ExpectedStorage&& other) noexcept(std::is_nothrow_move_constructible_v<E>)
  requires(std::is_move_constructible_v<E>)
  {
    if (has_value()) {
      if (other.has_error()) {
        rebind_nullable<false>(_error, std::move(other.get_error()));
      }
    } else {
      if (other.has_error()) {
        rebind_nullable<true>(_error, std::move(other.get_error()));
      } else {
        std::destroy_at(std::addressof(_error));
      }
    }
    _valid = std::move(other._valid);
    return *this;
  }

public:
  constexpr ExpectedStorage&
  operator=(const ExpectedStorage& other) noexcept(std::is_nothrow_copy_constructible_v<E>)
  requires(std::is_copy_constructible_v<E>)
  {
    if (has_value()) {
      if (other.has_error()) {
        rebind_nullable<false>(_error, other.get_error());
      }
    } else {
      if (other.has_error()) {
        rebind_nullable<true>(_error, other.get_error());
      } else {
        std::destroy_at(std::addressof(_error));
      }
    }
    _valid = other._valid;
    return *this;
  }

public:
  constexpr ~ExpectedStorage() noexcept
  requires(_triv_destr)
  = default;

  constexpr ~ExpectedStorage() noexcept(std::is_nothrow_destructible_v<E>)
  requires(!_triv_destr)
  {
    if (has_error()) {
      std::destroy_at(std::addressof(_error));
    }
  }

public:
  constexpr void emplace() noexcept {
    if (has_value()) {
      return;
    }
    std::destroy_at(std::addressof(_error));
    _valid = true;
  }

  template<typename... Args>
  constexpr E&
  emplace_error(Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>) {
    if (has_value()) {
      rebind_nullable<false>(_error, std::forward<Args>(args)...);
      _valid = false;
    } else {
      rebind_nullable<true>(_error, std::forward<Args>(args)...);
    }
    return _error;
  }

  template<typename U, typename... Args>
  constexpr E& emplace_error(std::initializer_list<U> il, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...>) {
    if (has_value()) {
      rebind_nullable<false>(_error, il, std::forward<Args>(args)...);
      _valid = false;
    } else {
      rebind_nullable<true>(_error, il, std::forward<Args>(args)...);
    }
    return _error;
  }

  constexpr bool has_value() const noexcept { return _valid; }

  constexpr bool has_error() const noexcept { return !_valid; }

protected:
  void get() const noexcept {}

  constexpr E& get_error() & noexcept { return _error; }

  constexpr E&& get_error() && noexcept { return std::move(_error); }

  constexpr const E& get_error() const& noexcept { return _error; }

  constexpr const E&& get_error() const&& noexcept { return std::move(_error); }

private:
  union {
    char _dummy;
    E _error;
  };

  bool _valid;
};

} // namespace impl

template<typename T, typename E>
class Expected;

namespace meta {

template<typename>
struct expected_check : public ::std::false_type {};

template<typename T, typename E>
struct expected_check<Expected<T, E>> : public ::std::true_type {};

template<typename T>
constexpr bool expected_check_v = expected_check<T>::value;

template<typename T>
concept expected_type = expected_check_v<T>;

template<typename Exp, typename T>
struct expected_check_t : public std::false_type {};

template<typename T>
struct expected_check_t<Expected<T, void>, T> : public std::false_type {};

template<typename T, typename E>
struct expected_check_t<Expected<T, E>, T> : public std::true_type {};

template<typename Exp, typename T>
constexpr bool expected_check_t_v = expected_check_t<Exp, T>::value;

template<typename Exp, typename T>
concept expected_with_type = expected_check_t_v<Exp, T>;

template<typename Exp, typename E>
struct expected_check_e : public std::false_type {};

template<typename T>
struct expected_check_e<Expected<T, void>, void> : public std::false_type {};

template<typename T, typename E>
struct expected_check_e<Expected<T, E>, E> : public std::true_type {};

template<typename Exp, typename E>
constexpr bool expected_check_e_v = expected_check_e<Exp, E>::value;

template<typename Exp, typename E>
concept expected_with_error = expected_check_e_v<Exp, E>;

template<bool cond, typename T, typename Err>
struct expected_wrap_if {
  using type = T;
};

template<typename T, typename Err>
struct expected_wrap_if<true, T, Err> {
  using type = Expected<T, Err>;
};

} // namespace meta

namespace impl {

template<typename F, typename T>
struct expect_monadic_chain {
  using type = std::remove_cvref_t<std::invoke_result_t<F, T>>;
};

template<typename F>
struct expect_monadic_chain<F, void> {
  using type = std::remove_cvref_t<std::invoke_result_t<F>>;
};

template<typename F, typename T>
using expect_monadic_chain_t = expect_monadic_chain<F, T>::type;

template<typename F, typename T>
struct expect_monadic_transform {
  using type = std::remove_cv_t<std::invoke_result_t<F, T>>;
};

template<typename F>
struct expect_monadic_transform<F, void> {
  using type = std::remove_cv_t<std::invoke_result_t<F>>;
};

template<typename F, typename T>
using expect_monadic_transform_t = expect_monadic_transform<F, T>::type;

template<typename T, typename E>
class ExpectedMonadicOps : public impl::ExpectedStorage<T, E> {
public:
  using impl::ExpectedStorage<T, E>::ExpectedStorage;

public:
  template<typename F>
  constexpr auto and_then(F&& func) & {
    if constexpr (std::is_void_v<T>) {
      using U = impl::expect_monadic_chain_t<F, void>;
      static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");
      if (this->has_value()) {
        return std::invoke(std::forward<F>(func));
      } else {
        return U{unexpect, this->get_error()};
      }
    } else {
      using U = impl::expect_monadic_chain_t<F, decltype(this->get())>;
      static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");
      if (this->has_value()) {
        return std::invoke(std::forward<F>(func), this->get());
      } else {
        return U{unexpect, this->get_error()};
      }
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) && {
    if constexpr (std::is_void_v<T>) {
      using U = impl::expect_monadic_chain_t<F, void>;
      static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");
      if (this->has_value()) {
        return std::invoke(std::forward<F>(func));
      } else {
        return U{unexpect, std::move(this->get_error())};
      }
    } else {
      using U = impl::expect_monadic_chain_t<F, decltype(std::move(this->get()))>;
      static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");
      if (this->has_value()) {
        return std::invoke(std::forward<F>(func), std::move(this->get()));
      } else {
        return U{unexpect, std::move(this->get_error())};
      }
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) const& {
    if constexpr (std::is_void_v<T>) {
      using U = impl::expect_monadic_chain_t<F, void>;
      static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");
      if (this->has_value()) {
        return std::invoke(std::forward<F>(func));
      } else {
        return U{unexpect, this->get_error()};
      }
    } else {
      using U = impl::expect_monadic_chain_t<F, decltype(this->get())>;
      static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");
      if (this->has_value()) {
        return std::invoke(std::forward<F>(func), this->get());
      } else {
        return U{unexpect, this->get_error()};
      }
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) const&& {
    if constexpr (std::is_void_v<T>) {
      using U = impl::expect_monadic_chain_t<F, void>;
      static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");
      if (this->has_value()) {
        return std::invoke(std::forward<F>(func));
      } else {
        return U{unexpect, std::move(this->get_error())};
      }
    } else {
      using U = impl::expect_monadic_chain_t<F, decltype(std::move(this->get()))>;
      static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");
      if (this->has_value()) {
        return std::invoke(std::forward<F>(func), std::move(this->get()));
      } else {
        return U{unexpect, std::move(this->get_error())};
      }
    }
  }

public:
  template<typename F>
  constexpr auto or_else(F&& func) & {
    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        using G = impl::expect_monadic_chain_t<F, void>;
        static_assert(meta::expected_with_type<G, T>,
                      "F needs to return an expected with value T");
        return G{in_place};
      } else {
        using G = impl::expect_monadic_chain_t<F, decltype(this->get_error())>;
        static_assert(meta::expected_with_type<G, T>,
                      "F needs to return an expected with value T");
        return G{in_place, this->get()};
      }
    } else {
      return std::invoke(std::forward<F>(func), this->get_error());
    }
  }

  template<typename F>
  constexpr auto or_else(F&& func) && {
    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        using G = impl::expect_monadic_chain_t<F, void>;
        static_assert(meta::expected_with_type<G, T>,
                      "F needs to return an expected with value T");
        return G{in_place};
      } else {
        using G = impl::expect_monadic_chain_t<F, decltype(std::move(this->get_error()))>;
        static_assert(meta::expected_with_type<G, T>,
                      "F needs to return an expected with value T");
        return G{in_place, std::move(this->get())};
      }
    } else {
      return std::invoke(std::forward<F>(func), std::move(this->get_error()));
    }
  }

  template<typename F>
  constexpr auto or_else(F&& func) const& {
    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        using G = impl::expect_monadic_chain_t<F, void>;
        static_assert(meta::expected_with_type<G, T>,
                      "F needs to return an expected with value T");
        return G{in_place};
      } else {
        using G = impl::expect_monadic_chain_t<F, decltype(this->get_error())>;
        static_assert(meta::expected_with_type<G, T>,
                      "F needs to return an expected with value T");
        return G{in_place, this->get()};
      }
    } else {
      return std::invoke(std::forward<F>(func), this->get_error());
    }
  }

  template<typename F>
  constexpr auto or_else(F&& func) const&& {
    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        using G = impl::expect_monadic_chain_t<F, void>;
        static_assert(meta::expected_with_type<G, T>,
                      "F needs to return an expected with value T");
        return G{in_place};
      } else {
        using G = impl::expect_monadic_chain_t<F, decltype(std::move(this->get_error()))>;
        static_assert(meta::expected_with_type<G, T>,
                      "F needs to return an expected with value T");
        return G{in_place, std::move(this->get())};
      }
    } else {
      return std::invoke(std::forward<F>(func), std::move(this->get_error()));
    }
  }

public:
  template<typename F>
  constexpr auto transform(F&& func) & {
    if constexpr (std::is_void_v<T>) {
      using U = impl::expect_monadic_transform_t<F, void>;
      static_assert(!std::is_reference_v<U>, "F can't return a reference type");

      if (this->has_value()) {
        if constexpr (std::is_void_v<U>) {
          std::invoke(std::forward<F>(func));
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, std::invoke(std::forward<F>(func))};
        }
      } else {
        return Expected<U, E>{unexpect, this->get_error()};
      }
    } else {
      using U = impl::expect_monadic_transform_t<F, decltype(this->get())>;
      static_assert(!std::is_reference_v<U>, "F can't return a reference type");

      if (this->has_value()) {
        if constexpr (std::is_void_v<U>) {
          std::invoke(std::forward<F>(func), this->get());
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, std::invoke(std::forward<F>(func), this->get())};
        }
      } else {
        return Expected<U, E>{unexpect, this->get_error()};
      }
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) && {
    if constexpr (std::is_void_v<T>) {
      using U = impl::expect_monadic_transform_t<F, void>;
      static_assert(!std::is_reference_v<U>, "F can't return a reference type");

      if (this->has_value()) {
        if constexpr (std::is_void_v<U>) {
          std::invoke(std::forward<F>(func));
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, std::invoke(std::forward<F>(func))};
        }
      } else {
        return Expected<U, E>{unexpect, std::move(this->get_error())};
      }
    } else {
      using U = impl::expect_monadic_transform_t<F, decltype(std::move(this->get()))>;
      static_assert(!std::is_reference_v<U>, "F can't return a reference type");

      if (this->has_value()) {
        if constexpr (std::is_void_v<U>) {
          std::invoke(std::forward<F>(func), std::move(this->get()));
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place,
                                std::invoke(std::forward<F>(func), std::move(this->get()))};
        }
      } else {
        return Expected<U, E>{unexpect, std::move(this->get_error())};
      }
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) const& {
    if constexpr (std::is_void_v<T>) {
      using U = impl::expect_monadic_transform_t<F, void>;
      static_assert(!std::is_reference_v<U>, "F can't return a reference type");

      if (this->has_value()) {
        if constexpr (std::is_void_v<U>) {
          std::invoke(std::forward<F>(func));
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, std::invoke(std::forward<F>(func))};
        }
      } else {
        return Expected<U, E>{unexpect, this->get_error()};
      }
    } else {
      using U = impl::expect_monadic_transform_t<F, decltype(this->get())>;
      static_assert(!std::is_reference_v<U>, "F can't return a reference type");

      if (this->has_value()) {
        if constexpr (std::is_void_v<U>) {
          std::invoke(std::forward<F>(func), this->get());
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, std::invoke(std::forward<F>(func), this->get())};
        }
      } else {
        return Expected<U, E>{unexpect, this->get_error()};
      }
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) const&& {
    if constexpr (std::is_void_v<T>) {
      using U = impl::expect_monadic_transform_t<F, void>;
      static_assert(!std::is_reference_v<U>, "F can't return a reference type");

      if (this->has_value()) {
        if constexpr (std::is_void_v<U>) {
          std::invoke(std::forward<F>(func));
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, std::invoke(std::forward<F>(func))};
        }
      } else {
        return Expected<U, E>{unexpect, std::move(this->get_error())};
      }
    } else {
      using U = impl::expect_monadic_transform_t<F, decltype(std::move(this->get()))>;
      static_assert(!std::is_reference_v<U>, "F can't return a reference type");

      if (this->has_value()) {
        if constexpr (std::is_void_v<U>) {
          std::invoke(std::forward<F>(func), std::move(this->get()));
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place,
                                std::invoke(std::forward<F>(func), std::move(this->get()))};
        }
      } else {
        return Expected<U, E>{unexpect, std::move(this->get_error())};
      }
    }
  }

public:
  template<typename F>
  constexpr auto transform_error(F&& func) & {
    if constexpr (std::is_void_v<T>) {
      using G = impl::expect_monadic_transform_t<F, void>;
      static_assert(!std::is_void_v<G>, "F has to return a non void error value");
      static_assert(!std::is_reference_v<G>, "F can't return a reference type");

      if (this->has_value()) {
        return Expected<void, G>{in_place};
      } else {
        return Expected<T, G>{unexpect, std::invoke(std::forward<F>(func), this->get_error())};
      }
    } else {
      using G = impl::expect_monadic_transform_t<F, decltype(this->get_error())>;
      static_assert(!std::is_void_v<G>, "F has to return a non void error value");
      static_assert(!std::is_reference_v<G>, "F can't return a reference type");

      if (this->has_value()) {
        return Expected<T, G>{in_place, this->get()};
      } else {
        return Expected<T, G>{unexpect, std::invoke(std::forward<F>(func), this->get_error())};
      }
    }
  }

  template<typename F>
  constexpr auto transform_error(F&& func) && {
    if constexpr (std::is_void_v<T>) {
      using G = impl::expect_monadic_transform_t<F, void>;
      static_assert(!std::is_void_v<G>, "F has to return a non void error value");
      static_assert(!std::is_reference_v<G>, "F can't return a reference type");

      if (this->has_value()) {
        return Expected<void, G>{in_place};
      } else {
        return Expected<T, G>{unexpect,
                              std::invoke(std::forward<F>(func), std::move(this->get_error()))};
      }
    } else {
      using G = impl::expect_monadic_transform_t<F, decltype(std::move(this->get_error()))>;
      static_assert(!std::is_void_v<G>, "F has to return a non void error value");
      static_assert(!std::is_reference_v<G>, "F can't return a reference type");

      if (this->has_value()) {
        return Expected<T, G>{in_place, std::move(this->get())};
      } else {
        return Expected<T, G>{unexpect,
                              std::invoke(std::forward<F>(func), std::move(this->get_error()))};
      }
    }
  }

  template<typename F>
  constexpr auto transform_error(F&& func) const& {
    if constexpr (std::is_void_v<T>) {
      using G = impl::expect_monadic_transform_t<F, void>;
      static_assert(!std::is_void_v<G>, "F has to return a non void error value");
      static_assert(!std::is_reference_v<G>, "F can't return a reference type");

      if (this->has_value()) {
        return Expected<void, G>{in_place};
      } else {
        return Expected<T, G>{unexpect, std::invoke(std::forward<F>(func), this->get_error())};
      }
    } else {
      using G = impl::expect_monadic_transform_t<F, decltype(this->get_error())>;
      static_assert(!std::is_void_v<G>, "F has to return a non void error value");
      static_assert(!std::is_reference_v<G>, "F can't return a reference type");

      if (this->has_value()) {
        return Expected<T, G>{in_place, this->get()};
      } else {
        return Expected<T, G>{unexpect, std::invoke(std::forward<F>(func), this->get_error())};
      }
    }
  }

  template<typename F>
  constexpr auto transform_error(F&& func) const&& {
    if constexpr (std::is_void_v<T>) {
      using G = impl::expect_monadic_transform_t<F, void>;
      static_assert(!std::is_void_v<G>, "F has to return a non void error value");
      static_assert(!std::is_reference_v<G>, "F can't return a reference type");

      if (this->has_value()) {
        return Expected<void, G>{in_place};
      } else {
        return Expected<T, G>{unexpect,
                              std::invoke(std::forward<F>(func), std::move(this->get_error()))};
      }
    } else {
      using G = impl::expect_monadic_transform_t<F, decltype(std::move(this->get_error()))>;
      static_assert(!std::is_void_v<G>, "F has to return a non void error value");
      static_assert(!std::is_reference_v<G>, "F can't return a reference type");

      if (this->has_value()) {
        return Expected<T, G>{in_place, std::move(this->get())};
      } else {
        return Expected<T, G>{unexpect,
                              std::invoke(std::forward<F>(func), std::move(this->get_error()))};
      }
    }
  }
};

} // namespace impl

template<typename T, typename E>
class Expected : public impl::ExpectedMonadicOps<T, E> {
private:
  using base_t = impl::ExpectedStorage<T, E>;

public:
  using value_type = T;
  using error_type = E;

  template<typename U>
  using rebind = Expected<U, error_type>;

public:
  using impl::ExpectedMonadicOps<T, E>::ExpectedMonadicOps;

public:
  constexpr explicit operator bool() const noexcept { return this->has_value(); }

  constexpr const T* operator->() const {
    ka_assert(*this, "No object in expected");
    return std::addressof(this->get());
  }

  constexpr T* operator->() {
    ka_assert(*this, "No object in expected");
    return std::addressof(this->get());
  }

  constexpr const T& operator*() const& {
    ka_assert(*this, "No object in expected");
    return this->get();
  }

  constexpr T& operator*() & {
    ka_assert(*this, "No object in expected");
    return this->get();
  }

  constexpr const T&& operator*() const&& {
    ka_assert(*this, "No object in expected");
    return std::move(this->get());
  }

  constexpr T&& operator*() && {
    ka_assert(*this, "No object in expected");
    return std::move(this->get());
  }

public:
  constexpr const T& value() const& {
    KA_THROW_IF(!*this, BadExpectedAccess<E>(std::as_const(this->get_error())));
    return this->get();
  }

  constexpr T& value() & {
    KA_THROW_IF(!*this, BadExpectedAccess<E>(std::as_const(this->get_error())));
    return this->get();
  }

  constexpr T&& value() && {
    KA_THROW_IF(!*this, BadExpectedAccess<E>(std::move(this->get_error())));
    return std::move(this->get());
  }

  constexpr const T&& value() const&& {
    KA_THROW_IF(!*this, BadExpectedAccess<E>(std::move(this->get_error())));
    return std::move(this->get());
  }

public:
  constexpr const E& error() const& {
    ka_assert(!*this, "No error in expected");
    return this->get_error();
  }

  constexpr E& error() & {
    ka_assert(!*this, "No error in expected");
    return this->get_error();
  }

  constexpr const E&& error() const&& {
    ka_assert(!*this, "No error in expected");
    return std::move(this->get_error());
  }

  constexpr E&& error() && {
    ka_assert(!*this, "No error in expected");
    return std::move(this->get_error());
  }

public:
  template<typename U = std::remove_cv_t<T>>
  requires(std::convertible_to<T, U> && std::is_copy_constructible_v<T>)
  constexpr T value_or(U&& val) const& {
    return *this ? this->get() : static_cast<T>(std::forward<U>(val));
  }

  template<typename U = std::remove_cv_t<T>>
  requires(std::convertible_to<T, U> && std::is_move_constructible_v<T>)
  constexpr T value_or(U&& val) && {
    return *this ? std::move(this->get()) : static_cast<T>(std::forward<U>(val));
  }

  template<typename G = E>
  requires(std::convertible_to<E, G> && std::is_copy_constructible_v<E>)
  constexpr E error_or(G&& err) const& {
    return !*this ? this->get_error() : static_cast<E>(std::forward<G>(err));
  }

  template<typename G = E>
  requires(std::convertible_to<E, G> && std::is_move_constructible_v<E>)
  constexpr E error_or(G&& err) && {
    return !*this ? std::move(this->get_error()) : static_cast<E>(std::forward<G>(err));
  }
};

template<typename E>
class Expected<void, E> : public impl::ExpectedMonadicOps<void, E> {
private:
  using base_t = impl::ExpectedStorage<void, E>;

public:
  using value_type = void;
  using error_type = E;

  template<typename U>
  using rebind = Expected<U, error_type>;

public:
  using impl::ExpectedMonadicOps<void, E>::ExpectedMonadicOps;
  using impl::ExpectedMonadicOps<void, E>::operator=;

public:
  constexpr explicit operator bool() const noexcept { return this->has_value(); }

  constexpr void operator*() const& noexcept {}

  constexpr void operator*() && noexcept {}

public:
  constexpr void value() const& {
    KA_THROW_IF(!*this, BadExpectedAccess<E>(std::as_const(this->get_error())));
  }

  constexpr void value() && {
    KA_THROW_IF(!*this, BadExpectedAccess<E>(std::move(this->get_error())));
  }

public:
  constexpr const E& error() const& {
    ka_assert(!*this, "No error in expected");
    return this->get_error();
  }

  constexpr E& error() & {
    ka_assert(!*this, "No error in expected");
    return this->get_error();
  }

  constexpr const E&& error() const&& {
    ka_assert(!*this, "No error in expected");
    return std::move(this->get_error());
  }

  constexpr E&& error() && {
    ka_assert(!*this, "No error in expected");
    return std::move(this->get_error());
  }

public:
  template<typename G = E>
  requires(std::convertible_to<E, G> && std::is_copy_constructible_v<E>)
  constexpr E error_or(G&& err) const& {
    return !*this ? this->get_error() : static_cast<E>(std::forward<G>(err));
  }

  template<typename G = E>
  requires(std::convertible_to<E, G> && std::is_move_constructible_v<E>)
  constexpr E error_or(G&& err) && {
    return !*this ? std::move(this->get_error()) : static_cast<E>(std::forward<G>(err));
  }
};

template<typename T, typename E, typename U, typename G>
constexpr bool operator==(const Expected<T, E>& lhs, const Expected<U, G>& rhs)
requires(!std::is_void_v<T> && !std::is_void_v<U>)
{
  return lhs.has_value() != rhs.has_value()
         ? false
         : (lhs.has_value() ? *lhs == *rhs : lhs.error() == rhs.error());
}

template<typename T, typename E, typename G>
constexpr bool operator==(const Expected<T, E>& lhs, const unexpected<G>& unex)
requires(!std::is_void_v<T>)
{
  return !lhs.has_value() && static_cast<bool>(lhs.value() == unex.value());
}

template<typename T, typename E, typename U>
constexpr bool operator==(const Expected<T, E>& lhs, const U& val) noexcept
requires(!std::is_void_v<T>)
{
  return lhs.has_value() && static_cast<bool>(*lhs == val);
}

template<typename E, typename G>
constexpr bool operator==(const Expected<void, E>& lhs, const Expected<void, G>& rhs) noexcept {
  return lhs.has_value() != rhs.has_value()
         ? false
         : lhs.has_value() || static_cast<bool>(lhs.error() == rhs.error());
}

template<typename E, typename G>
constexpr bool operator==(const Expected<void, E>& lhs, const unexpected<G>& unex) noexcept {
  return !lhs.has_value() && static_cast<bool>(lhs.error() == unex.value());
}

} // namespace kappa
