#pragma once

#include "./common.hpp"

namespace kappa {

template<typename T, typename U>
concept interpolable = requires(T a, T b, U scalar) {
  { a + b } -> std::convertible_to<T>;
  { a - b } -> std::convertible_to<T>;
  { scalar*a } -> std::convertible_to<T>;
};

template<typename F, typename T, typename U>
concept lerp_func = requires(const F func, const T& a, const T& b, U t) {
  { std::invoke(func, a, b, t) } -> std::convertible_to<T>;
};

template<typename T, typename U = f32>
struct glm_mixer {
  constexpr T operator()(const T& a, const T& b, U t) const { return glm::mix(a, b, t); }
};

template<typename T, std::floating_point U, typename Derived>
requires(interpolable<T, U>)
struct easing_mixer_base {
  constexpr T operator()(const T& a, const T& b, U t) const {
    if (t == U{1}) {
      return b;
    }
    return a + std::invoke(static_cast<const Derived&>(*this), t)*(b-a);
  }
};

template<typename T, typename U = f32>
struct easing_linear : public easing_mixer_base<T, U, easing_linear<T, U>>  {
  using easing_mixer_base<T, U, easing_linear<T, U>>::operator();
  constexpr U operator()(U t) const { return t; }
};

// https://easings.net/
template<typename T, typename U = f32>
struct easing_elastic_in : public easing_mixer_base<T, U, easing_elastic_in<T, U>> {
  using easing_mixer_base<T, U, easing_elastic_in<T, U>>::operator();
  constexpr U operator()(U t) const {
    const U c4 = U{2}*U{M_PI} / U{3};
    if (t <= U{0} || t >= U{1}) {
      return t;
    }
    return -std::pow(U{2}, U{10}*t - U{10})*std::sin((t*U{10} - (U{10.75f}))*c4);
  }
};

template<typename T, typename U = f32>
struct easing_back_inout : public easing_mixer_base<T, U, easing_back_inout<T, U>> {
  using easing_mixer_base<T, U, easing_back_inout<T, U>>::operator();
  constexpr U operator()(U t) const {
    const U c2 = U{1.70158f*1.525f};
    if (t < U{.5f}) {
      return (std::pow(U{2}*t, U{2})*((c2+U{1})*U{2}*t - c2))/U{2};
    }
    return (std::pow(U{2}*t-U{2}, U{2})*((c2+U{1})*(t*U{2}-U{2})+c2) +U{2})/U{2};
  }
};

template<typename T, typename U = f32>
struct easing_pow : public easing_mixer_base<T, U, easing_pow<T, U>> {
  using easing_mixer_base<T, U, easing_pow<T, U>>::operator();
  constexpr easing_pow(u32 p) :
    _p{p} {}

  constexpr U operator()(U t) const {
    U res{1};
    for (u32 i = _p; i--;) {
      res *= t;
    }
    return res;
  }

private:
  u32 _p;
};

template<typename T, std::floating_point U, lerp_func<T, U> F, typename Derived>
requires(interpolable<T, U>)
class lerper_base : private F {
public:
  using interpolator_type = F;

public:
  constexpr lerper_base(const T& first, const T& last) :
    F{},
    _first{first}, _last{last} {}

  constexpr lerper_base(const T& first, const T& last, const F& lerper) :
    F{lerper},
    _first{first}, _last{last} {}

  constexpr lerper_base(const T& first, const T& last, F&& lerper) :
    F{std::move(lerper)},
    _first{first}, _last{last} {}

  template<typename... Args>
  constexpr lerper_base(const T& first, const T& last,
                        std::in_place_t, Args&&... args) :
    F{std::forward<Args>(args)...},
    _first{first}, _last{last} {}

protected:
  constexpr T evaluate(U t) const { return std::invoke(get_interpolator(), _first, _last, t); }

public:
  constexpr T first() const { return _first; }
  constexpr T last() const { return _last; }

  constexpr Derived& first(const T& val) {
    _first = val;
    return static_cast<Derived&>(*this);
  }
  constexpr Derived& last(const T& val) {
    _last = val;
    return static_cast<Derived&>(*this);
  }

  constexpr F& get_interpolator() { return static_cast<F&>(*this); }
  constexpr const F& get_interpolator() const { return static_cast<const F&>(*this); }

private:
  T _first, _last;
};

template<typename T, typename U = f32, typename F = easing_linear<T, U>>
class deltalerp_func : public lerper_base<T, U, F, deltalerp_func<T, U, F>> {
private:
  using base_t = lerper_base<T, U, F, deltalerp_func<T, U, F>>;

public:
  using base_t::base_t;

public:
  [[nodiscard]] constexpr T eval(U t) const { return base_t::evaluate(t); }
  [[nodiscard]] constexpr T eval(U t, U alpha) const { return eval(t+alpha); }

public:
  [[nodiscard]] constexpr T operator()(U t) const { return eval(t); }
  [[nodiscard]] constexpr T operator()(U t, U alpha) const { return eval(t, alpha); }
};

constexpr u32 dynamic_step = std::numeric_limits<u32>::max();
template<typename T, typename U = f32, typename F =easing_linear<T,U>, u32 StepSize = dynamic_step>
class steplerp_func : public lerper_base<T, U, F, steplerp_func<T, U, F, StepSize>> {
private:
  using base_t = lerper_base<T, U, F, steplerp_func<T, U, F, StepSize>>;

public:
  static constexpr u32 step_size = StepSize;

public:
  using base_t::base_t;

private:
  constexpr T _eval(U delta) const { return base_t::evaluate(delta/static_cast<U>(steps())); }

public:
  [[nodiscard]] constexpr T eval(i32 steps) const { return _eval(static_cast<U>(steps)); }
  [[nodiscard]] constexpr T eval(i32 steps, U alpha) const {
    return _eval(static_cast<U>(steps)+alpha);
  }

public:
  [[nodiscard]] constexpr T operator()(i32 steps) const { return eval(steps); }
  [[nodiscard]] constexpr T operator()(i32 steps, U alpha) const { return eval(steps, alpha); }

public:
  constexpr u32 steps() const { return step_size; }
};

template<typename T, typename U, typename F>
class steplerp_func<T, U, F, dynamic_step> :
  public lerper_base<T, U, F, steplerp_func<T, U, F, dynamic_step>>
{
private:
  using base_t = lerper_base<T, U, F, steplerp_func<T, U, F, dynamic_step>>;

public:
  static constexpr u32 step_size = dynamic_step;

public:
  constexpr steplerp_func(const T& first, const T& last, u32 steps) :
    base_t{first, last}, _steps{steps} {}

  constexpr steplerp_func(const T& first, const T& last, u32 steps, const F& lerper) :
    base_t{first, last, lerper}, _steps{steps} {}

  constexpr steplerp_func(const T& first, const T& last, u32 steps, F&& lerper) :
    base_t{first, last, std::move(lerper)}, _steps{steps} {}

  template<typename... Args>
  explicit constexpr steplerp_func(const T& first, const T& last, u32 steps,
                              std::in_place_t t, Args&&... args) :
    base_t{first, last, t, std::forward<Args>(args)...}, _steps{steps} {}

private:
  constexpr T _eval(U delta) const { return base_t::evaluate(delta/static_cast<U>(steps())); }

public:
  [[nodiscard]] constexpr T eval(i32 steps) const { return _eval(static_cast<U>(steps)); }
  [[nodiscard]] constexpr T eval(i32 steps, U alpha) const {
    return _eval(static_cast<U>(steps)+alpha);
  }

public:
  [[nodiscard]] constexpr T operator()(i32 steps) const { return eval(steps); }
  [[nodiscard]] constexpr T operator()(i32 steps, U alpha) const { return eval(steps, alpha); }

public:
  constexpr u32 steps() const { return _steps; }
  constexpr steplerp_func& steps(u32 value) {
    _steps = value;
    return *this;
  }

private:
  u32 _steps;
};

template<typename T, std::floating_point U = f32, lerp_func<T, U> F = easing_linear<T, U>>
class deltalerp : public lerper_base<T, U, F, deltalerp<T, U, F>> {
private:
  using base_t = lerper_base<T, U, F, deltalerp<T, U, F>>;

public:
  using base_t::base_t;

  explicit constexpr deltalerp(const T& first, const T& last, U age) :
    base_t{first, last}, _t{age} {}

  constexpr deltalerp(const T& first, const T& last, U age, const F& lerper) :
    base_t{first, last, lerper},
    _t{age} {}

  constexpr deltalerp(const T& first, const T& last, U age, F&& lerper) :
    base_t{first, last, std::move(lerper)}, _t{age} {}

  template<typename... Args>
  explicit constexpr deltalerp(const T& first, const T& last, U age,
                               std::in_place_t tag, Args&&... args) :
    base_t{first, last, tag, std::forward<Args>(args)...}, _t{age} {}

public:
  [[nodiscard]] constexpr T value() const { return base_t::evaluate(_t); }
  [[nodiscard]] constexpr T value(U alpha) const { return base_t::evaluate(_t+alpha); }

public:
  [[nodiscard]] constexpr T operator*() const { return value(); }

public:
  constexpr U age() const { return _t; }
  constexpr deltalerp& tick(U delta) {
    _t += delta;
    return *this;
  }
  constexpr deltalerp& age(U val) {
    _t = val;
    return *this;
  }

private:
  U _t{};
};

template<typename T, typename U = f32, typename F =easing_linear<T,U>, u32 StepSize = dynamic_step>
class steplerp : public lerper_base<T, U, F, steplerp<T, U, F, StepSize>> {
private:
  using base_t = lerper_base<T, U, F, steplerp<T, U, F, StepSize>>;

public:
  static constexpr u32 step_size = StepSize;

public:
  using base_t::base_t;

  explicit constexpr steplerp(const T& first, const T& last, i32 age) :
    base_t{first, last}, _age{age} {}

  constexpr steplerp(const T& first, const T& last, i32 age, const F& lerper) :
    base_t{first, last, lerper}, _age{age} {}

  constexpr steplerp(const T& first, const T& last, i32 age, F&& lerper) :
    base_t{first, last, std::move(lerper)}, _age{age} {}

  template<typename... Args>
  explicit constexpr steplerp(const T& first, const T& last, i32 age,
                              std::in_place_t t, Args&&... args) :
    base_t{first, last, t, std::forward<Args>(args)...}, _age{age} {}

private:
  constexpr T _eval(U delta) const { return base_t::evaluate(delta/static_cast<U>(steps())); }

public:
  [[nodiscard]] constexpr T value() const { return _eval(static_cast<U>(_age)); }
  [[nodiscard]] constexpr T value(U alpha) const { return _eval(static_cast<U>(_age)+alpha); }

public:
  [[nodiscard]] constexpr T operator*() const { return value(); }

public:
  constexpr u32 steps() const { return step_size; }
  constexpr i32 age() const { return _age; }
  constexpr steplerp& tick(i32 count = 1) {
    _age += count;
    return *this;
  }
  constexpr steplerp& tick_loop(i32 count = 1) {
    _age = (_age+count) % static_cast<i32>(steps());
    return *this;
  }
  constexpr steplerp& age(i32 value) {
    _age = value;
    return *this;
  }

private:
  i32 _age{};
};

template<typename T, typename U, typename F>
class steplerp<T, U, F, dynamic_step> :
  public lerper_base<T, U, F, steplerp<T, U, F, dynamic_step>>
{
private:
  using base_t = lerper_base<T, U, F, steplerp<T, U, F, dynamic_step>>;

public:
  static constexpr u32 step_size = dynamic_step;

public:
  constexpr steplerp(const T& first, const T& last, u32 steps) :
    base_t{first, last}, _steps{steps}, _age{} {}

  constexpr steplerp(const T& first, const T& last, u32 steps, const F& lerper) :
    base_t{first, last, lerper}, _steps{steps}, _age{} {}

  constexpr steplerp(const T& first, const T& last, u32 steps, F&& lerper) :
    base_t{first, last, std::move(lerper)}, _steps{steps}, _age{} {}

  template<typename... Args>
  explicit constexpr steplerp(const T& first, const T& last, u32 steps,
                              std::in_place_t tag, Args&&... args) :
    base_t{first, last, tag, std::forward<Args>(args)...}, _steps{steps}, _age{} {}

  explicit constexpr steplerp(const T& first, const T& last, u32 steps, i32 age) :
    base_t{first, last}, _steps{steps}, _age{age} {}

  constexpr steplerp(const T& first, const T& last, u32 steps, i32 age, const F& lerper) :
    base_t{first, last, lerper}, _steps{steps}, _age{age} {}

  constexpr steplerp(const T& first, const T& last, u32 steps, i32 age, F&& lerper) :
    base_t{first, last, std::move(lerper)}, _steps{steps}, _age{age} {}

  template<typename... Args>
  explicit constexpr steplerp(const T& first, const T& last, u32 steps, i32 age,
                              std::in_place_t tag, Args&&... args) :
    base_t{first, last, tag, std::forward<Args>(args)...}, _steps{steps}, _age{age} {}

private:
  constexpr T _eval(U delta) const { return base_t::evaluate(delta/static_cast<U>(steps())); }

public:
  [[nodiscard]] constexpr T value() const { return _eval(static_cast<U>(_age)); }
  [[nodiscard]] constexpr T value(U alpha) const { return _eval(static_cast<U>(_age)+alpha); }

public:
  [[nodiscard]] constexpr T operator*() const { return value(); }

public:
  constexpr u32 steps() const { return _steps; }
  constexpr steplerp& steps(u32 value) {
    _steps = value;
    return *this;
  }
  constexpr i32 age() const { return _age; }
  constexpr steplerp& tick(i32 count = 1) {
    _age += count;
    return *this;
  }
  constexpr steplerp& tick_loop(i32 count = 1) {
    _age = (_age+count) % static_cast<i32>(steps());
    return *this;
  }
  constexpr steplerp& age(i32 value) {
    _age = value;
    return *this;
  }

private:
  u32 _steps;
  i32 _age;
};

} // namespace kappa
