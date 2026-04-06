#pragma once

#include "../core.hpp"

#include <memory>
#include <type_traits>

namespace kappa {

template<typename... Fs>
struct OverloadFn : Fs... {
  using Fs::operator()...;
};

template<typename... Fs>
OverloadFn(Fs...) -> OverloadFn<Fs...>;

template<typename F>
class DeferFn {
public:
  template<typename Func>
  DeferFn(Func&& func) : _func(std::forward<Func>(func)), _engaged(true) {}

  ~DeferFn() noexcept {
    if (_engaged) {
      invoke();
    }
  }

  DeferFn(DeferFn&&) = delete;
  DeferFn(const DeferFn&) = delete;

  DeferFn& operator=(DeferFn&&) = delete;
  DeferFn& operator=(const DeferFn&) = delete;

public:
  void invoke() noexcept { _func(); }

  void disengage() noexcept { _engaged = false; }

private:
  F _func;
  bool _engaged;
};

namespace impl {

template<typename T, bool IsConst, bool IsNoexcept, typename Ret, typename... Args>
struct ErasedInvoker {
  static constexpr Ret invoke(void* ptr, Args... args) noexcept(IsNoexcept) {
    if constexpr (std::is_void_v<Ret>) {
      (*static_cast<T*>(ptr))(std::forward<Args>(args)...);
    } else {
      return (*static_cast<T*>(ptr))(std::forward<Args>(args)...);
    }
  }
};

template<typename T, bool IsNoexcept, typename Ret, typename... Args>
struct ErasedInvoker<T, true, IsNoexcept, Ret, Args...> {
  static constexpr Ret invoke(const void* ptr, Args... args) noexcept(IsNoexcept) {
    if constexpr (std::is_void_v<Ret>) {
      (*static_cast<const T*>(ptr))(std::forward<Args>(args)...);
    } else {
      return (*static_cast<const T*>(ptr))(std::forward<Args>(args)...);
    }
  }
};

} // namespace impl

template<typename Signature>
class FnRef;

template<bool IsNoexcept, typename Ret, typename... Args>
class FnRef<Ret(Args...) const noexcept(IsNoexcept)> {
public:
  using signature = Ret(Args...) const noexcept(IsNoexcept);
  using return_type = Ret;
  using func_ptr_type = Ret (*)(Args...) noexcept(IsNoexcept);

private:
  using functor_ptr_type = Ret (*)(const void*, Args...) noexcept(IsNoexcept);

public:
  explicit constexpr FnRef(func_ptr_type func) : _data(nullptr), _func_invoke(func) {
    ka_assert(func);
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<FnRef, T>)
  constexpr FnRef(const T& functor) noexcept :
      _data(static_cast<const void*>(std::addressof(functor))),
      _functor_invoke(&impl::ErasedInvoker<T, true, IsNoexcept, Ret, Args...>::invoke) {}

public:
  constexpr ~FnRef() noexcept = default;
  constexpr FnRef(const FnRef&) noexcept = default;
  constexpr FnRef(FnRef&&) noexcept = default;

public:
  constexpr Ret operator()(Args... args) const noexcept(IsNoexcept) {
    if (_data) {
      if constexpr (std::is_void_v<Ret>) {
        _functor_invoke(_data, std::forward<Args>(args)...);
      } else {
        return _functor_invoke(_data, std::forward<Args>(args)...);
      }
    } else {
      if constexpr (std::is_void_v<Ret>) {
        _func_invoke(std::forward<Args>(args)...);
      } else {
        return _func_invoke(std::forward<Args>(args)...);
      }
    }
  }

public:
  constexpr FnRef& operator=(func_ptr_type func) noexcept {
    _data = nullptr;
    _func_invoke = func;
    return *this;
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<FnRef, T>)
  constexpr FnRef& operator=(const T& functor) noexcept {
    _data = static_cast<const void*>(std::addressof(functor));
    _functor_invoke = &impl::ErasedInvoker<T, true, IsNoexcept, Ret, Args...>::invoke;
    return *this;
  }

  constexpr FnRef& operator=(const FnRef&) noexcept = default;
  constexpr FnRef& operator=(FnRef&&) noexcept = default;

private:
  const void* _data;

  union {
    func_ptr_type _func_invoke;
    functor_ptr_type _functor_invoke;
  };
};

template<bool IsNoexcept, typename Ret, typename... Args>
class FnRef<Ret(Args...) noexcept(IsNoexcept)> {
public:
  using signature = Ret(Args...) noexcept(IsNoexcept);
  using return_type = Ret;
  using func_ptr_type = Ret (*)(Args...) noexcept(IsNoexcept);

private:
  using functor_ptr_type = Ret (*)(void*, Args...) noexcept(IsNoexcept);

public:
  explicit constexpr FnRef(func_ptr_type func) : _data(nullptr), _func_invoke(func) {
    ka_assert(func);
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<FnRef, T>)
  constexpr FnRef(T& functor) noexcept :
      _data(static_cast<void*>(std::addressof(functor))),
      _functor_invoke(&impl::ErasedInvoker<T, false, IsNoexcept, Ret, Args...>::invoke) {}

public:
  constexpr ~FnRef() noexcept = default;
  constexpr FnRef(const FnRef&) noexcept = default;
  constexpr FnRef(FnRef&&) noexcept = default;

public:
  constexpr Ret operator()(Args... args) const noexcept(IsNoexcept) {
    if (_data) {
      if constexpr (std::is_void_v<Ret>) {
        _functor_invoke(_data, std::forward<Args>(args)...);
      } else {
        return _functor_invoke(_data, std::forward<Args>(args)...);
      }
    } else {
      if constexpr (std::is_void_v<Ret>) {
        _func_invoke(std::forward<Args>(args)...);
      } else {
        return _func_invoke(std::forward<Args>(args)...);
      }
    }
  }

  constexpr FnRef& operator=(func_ptr_type func) noexcept {
    _data = nullptr;
    _func_invoke = func;
    return *this;
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<FnRef, T>)
  constexpr FnRef& operator=(T& functor) noexcept {
    _data = static_cast<void*>(std::addressof(functor));
    _functor_invoke = &impl::ErasedInvoker<T, false, IsNoexcept, Ret, Args...>::invoke;
    return *this;
  }

  constexpr FnRef& operator=(const FnRef&) noexcept = default;
  constexpr FnRef& operator=(FnRef&&) noexcept = default;

private:
  void* _data;

  union {
    func_ptr_type _func_invoke;
    functor_ptr_type _functor_invoke;
  };
};

template<typename Signature, usize MaxSize, usize MaxAlign = alignof(std::max_align_t)>
class TrivFn;

template<usize MaxSize, usize MaxAlign, bool IsNoexcept, typename Ret, typename... Args>
class TrivFn<Ret(Args...) const noexcept(IsNoexcept), MaxSize, MaxAlign> {
public:
  using func_ptr_type = Ret (*)(Args...) noexcept(IsNoexcept);

private:
  using functor_ptr_type = Ret (*)(const void*, Args...) noexcept(IsNoexcept);

  template<typename T>
  static constexpr bool is_valid_func =
    std::is_invocable_r_v<Ret, std::remove_cvref_t<T>, Args...> &&
    !std::is_same_v<TrivFn, std::remove_cvref_t<T>> &&
    std::is_trivially_copyable_v<std::remove_cvref_t<T>> &&
    std::is_trivially_destructible_v<std::remove_cvref_t<T>>;

public:
  explicit TrivFn(func_ptr_type func) : _func_invoke(func), _is_functor(false) { ka_assert(func); }

  template<typename F>
  requires(is_valid_func<F>)
  TrivFn(F&& functor) noexcept :
      _functor_invoke(
        &impl::ErasedInvoker<std::remove_cvref_t<F>, true, IsNoexcept, Ret, Args...>::invoke),
      _is_functor(true) {
    std::construct_at(_buffer_as<std::remove_cvref_t<F>>(), std::forward<F>(functor));
  }

  template<typename F, typename... Args2>
  requires(is_valid_func<F> && std::constructible_from<F, Args2...>)
  TrivFn(std::in_place_type_t<F>, Args2&&... args) :
      _functor_invoke(&impl::ErasedInvoker<F, true, IsNoexcept, Ret, Args...>::invoke),
      _is_functor(true) {
    std::construct_at(_buffer_as<F>(), std::forward<Args2>(args)...);
  }

public:
  TrivFn(const TrivFn&) noexcept = default;
  TrivFn(TrivFn&&) noexcept = default;
  ~TrivFn() noexcept = default;

public:
  constexpr Ret operator()(Args... args) const noexcept(IsNoexcept) {
    if (_is_functor) {
      if constexpr (std::is_void_v<Ret>) {
        _functor_invoke((const void*)_buffer, std::forward<Args>(args)...);
      } else {
        return _functor_invoke((const void*)_buffer, std::forward<Args>(args)...);
      }
    } else {
      if constexpr (std::is_void_v<Ret>) {
        _func_invoke(std::forward<Args>(args)...);
      } else {
        return _func_invoke(std::forward<Args>(args)...);
      }
    }
  }

public:
  template<typename F>
  requires(is_valid_func<F>)
  TrivFn& emplace(F&& functor) noexcept {
    _functor_invoke =
      &impl::ErasedInvoker<std::remove_cvref_t<F>, true, IsNoexcept, Ret, Args...>::invoke;
    _is_functor = true;
    std::construct_at(_buffer_as<std::remove_cvref_t<F>>(), std::forward<F>(functor));
    return *this;
  }

  template<typename F, typename... Args2>
  requires(is_valid_func<F> && std::constructible_from<F, Args2...>)
  TrivFn& emplace(std::in_place_type_t<F>, Args2&&... args) noexcept {
    _functor_invoke =
      &impl::ErasedInvoker<std::remove_cvref_t<F>, true, IsNoexcept, Ret, Args...>::invoke;
    _is_functor = true;
    std::construct_at(_buffer_as<F>(), std::forward<Args2>(args)...);
    return *this;
  }

  TrivFn& emplace(func_ptr_type func) {
    ka_assert(func);
    _func_invoke = func;
    _is_functor = false;
    return *this;
  }

public:
  TrivFn& operator=(const TrivFn&) noexcept = default;
  TrivFn& operator=(TrivFn&&) noexcept = default;

  template<typename F>
  requires(is_valid_func<F>)
  TrivFn& operator=(F&& functor) {
    return emplace(std::forward<F>(functor));
  }

  TrivFn& operator==(func_ptr_type func) { return emplace(func); }

private:
  template<typename T>
  T* _buffer_as() noexcept {
    return std::launder(reinterpret_cast<T*>(&_buffer[0]));
  }

private:
  alignas(MaxAlign) u8 _buffer[MaxSize];

  union {
    func_ptr_type _func_invoke;
    functor_ptr_type _functor_invoke;
  };

  bool _is_functor;
};

template<usize MaxSize, usize MaxAlign, bool IsNoexcept, typename Ret, typename... Args>
class TrivFn<Ret(Args...) noexcept(IsNoexcept), MaxSize, MaxAlign> {
public:
  using func_ptr_type = Ret (*)(Args...) noexcept(IsNoexcept);

private:
  using functor_ptr_type = Ret (*)(void*, Args...) noexcept(IsNoexcept);

  template<typename T>
  static constexpr bool is_valid_func =
    std::is_invocable_r_v<Ret, std::remove_cvref_t<T>, Args...> &&
    !std::is_same_v<TrivFn, std::remove_cvref_t<T>> &&
    std::is_trivially_copyable_v<std::remove_cvref_t<T>> &&
    std::is_trivially_destructible_v<std::remove_cvref_t<T>>;

public:
  explicit TrivFn(func_ptr_type func) : _func_invoke(func), _is_functor(false) { ka_assert(func); }

  template<typename F>
  requires(is_valid_func<F>)
  TrivFn(F&& functor) noexcept :
      _functor_invoke(
        &impl::ErasedInvoker<std::remove_cvref_t<F>, false, IsNoexcept, Ret, Args...>::invoke),
      _is_functor(true) {
    std::construct_at(_buffer_as<std::remove_cvref_t<F>>(), std::forward<F>(functor));
  }

  template<typename F, typename... Args2>
  requires(is_valid_func<F> && std::constructible_from<F, Args2...>)
  TrivFn(std::in_place_type_t<F>, Args2&&... args) :
      _functor_invoke(&impl::ErasedInvoker<F, false, IsNoexcept, Ret, Args...>::invoke),
      _is_functor(true) {
    std::construct_at(_buffer_as<F>(), std::forward<Args2>(args)...);
  }

public:
  TrivFn(const TrivFn&) noexcept = default;
  TrivFn(TrivFn&&) noexcept = default;
  ~TrivFn() noexcept = default;

public:
  constexpr Ret operator()(Args... args) noexcept(IsNoexcept) {
    if (_is_functor) {
      if constexpr (std::is_void_v<Ret>) {
        _functor_invoke((void*)_buffer, std::forward<Args>(args)...);
      } else {
        return _functor_invoke((void*)_buffer, std::forward<Args>(args)...);
      }
    } else {
      if constexpr (std::is_void_v<Ret>) {
        _func_invoke(std::forward<Args>(args)...);
      } else {
        return _func_invoke(std::forward<Args>(args)...);
      }
    }
  }

public:
  template<typename F>
  requires(is_valid_func<F>)
  TrivFn& emplace(F&& functor) noexcept {
    _functor_invoke =
      &impl::ErasedInvoker<std::remove_cvref_t<F>, false, IsNoexcept, Ret, Args...>::invoke;
    _is_functor = true;
    std::construct_at(_buffer_as<std::remove_cvref_t<F>>(), std::forward<F>(functor));
    return *this;
  }

  template<typename F, typename... Args2>
  requires(is_valid_func<F> && std::constructible_from<F, Args2...>)
  TrivFn& emplace(std::in_place_type_t<F>, Args2&&... args) noexcept {
    _functor_invoke =
      &impl::ErasedInvoker<std::remove_cvref_t<F>, false, IsNoexcept, Ret, Args...>::invoke;
    _is_functor = true;
    std::construct_at(_buffer_as<F>(), std::forward<Args2>(args)...);
    return *this;
  }

  TrivFn& emplace(func_ptr_type func) {
    ka_assert(func);
    _func_invoke = func;
    _is_functor = false;
    return *this;
  }

public:
  TrivFn& operator=(const TrivFn&) noexcept = default;
  TrivFn& operator=(TrivFn&&) noexcept = default;

  template<typename F>
  requires(is_valid_func<F>)
  TrivFn& operator=(F&& functor) {
    return emplace(std::forward<F>(functor));
  }

  TrivFn& operator==(func_ptr_type func) { return emplace(func); }

private:
  template<typename T>
  T* _buffer_as() noexcept {
    return std::launder(reinterpret_cast<T*>(&_buffer[0]));
  }

private:
  alignas(MaxAlign) u8 _buffer[MaxSize];

  union {
    func_ptr_type _func_invoke;
    functor_ptr_type _functor_invoke;
  };

  bool _is_functor;
};

} // namespace kappa
