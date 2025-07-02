#pragma once

#include "common.hpp"

struct steplerp_pow {
  constexpr steplerp_pow(u32 p) :
    _p{p} {}

  constexpr f32 operator()(f32 t) const {
    f32 res = 1.f;
    for (u32 i = _p; i--;) {
      res *= t;
    }
    return res;
  }

private:
  u32 _p;
};

struct linear_mix {
  constexpr f32 operator()(f32 t) const {
    return t;
  }
};

template<typename T>
struct glm_mix {
  constexpr T operator()(const T& a, const T& b, f32 t) const {
    return glm::mix(a, b, t);
  }
};

struct easing_elastic_in {
  constexpr f32 operator()(f32 t) const {
    const f32 c4 = 2.f*M_PIf / 3.f;
    if (t <= 0.f || t >= 1.f) {
      return t;
    }
    return -std::pow(2, 10.*t - 10.f)*std::sin((t*10.f - 10.75f)*c4);
  }
};

struct easing_backinout {
  constexpr f32 operator()(f32 t) const {
    const f32 c1 = 1.70158f;
    const f32 c2 = c1*1.525f;
    if (t < .5f) {
      return (std::pow(2.f*t, 2.f)*((c2+1.f)*2*t - c2))/2.f;
    }
    return (std::pow(2.f*t-2, 2.f)*((c2+1.f)*(t*2.f-2.f)+c2) +2.f)/2.f;
  }
};

template<typename F>
concept steplerp_factor_func = requires(const F func, f32 t) {
  { std::invoke(func, t) } -> std::same_as<f32>;
};

template<typename F, typename T>
concept steplerp_range_func = requires(const F func, const T& val, f32 t){
  { std::invoke(func, val, val, t) } -> std::same_as<T>;
};

template<typename F, typename T>
concept steplerp_func =
  (steplerp_factor_func<F> && !steplerp_range_func<F, T>) ||
  (!steplerp_factor_func<F> && steplerp_range_func<F, T>);

template<typename T, typename F, typename Derived>
requires(steplerp_func<F, T>)
struct steplerp_base : private F {
protected:
  constexpr steplerp_base(const T& first, const T& last) :
    F{},
    _first{first}, _last{last} {}

  explicit constexpr steplerp_base(const T& first, const T& last, const F& func) :
    F{func},
    _first{first}, _last{last} {}

  explicit constexpr steplerp_base(const T& first, const T& last, F&& func) :
    F{std::move(func)},
    _first{first}, _last{last} {}

  template<typename... Args>
  explicit constexpr steplerp_base(const T& first, const T& last, std::in_place_t, Args&&... arg) :
    F{std::forward<Args>(arg)...},
    _first{first}, _last{last} {}

public:
  constexpr T value(i32 step) const {
    const auto& self = static_cast<const Derived&>(*this);

    const i32 max = self.steps();
    if (step == max) {
      return _last;
    }
    const f32 t = static_cast<f32>(step)/static_cast<f32>(max);
    if constexpr (steplerp_range_func<F, T>) {
      return std::invoke(self.get_func(), _first, _last, t);
    } else {
      return _first + (_last-_first)*std::invoke(get_func(), t);
    }
  }
  constexpr T operator()(i32 step) const { return value(step); }

  template<typename G>
  constexpr Derived& for_each(G&& func, i32 first_step = 0) {
    auto& self = static_cast<Derived&>(*this);
    for (i32 i = first_step; i <= self.steps(); ++i){
      func(i, self.value(i));
    }
    return self;
  }

  template<typename G>
  constexpr const Derived& for_each(G&& func, i32 first_step = 0) const {
    const auto& self = static_cast<const Derived&>(*this);
    for (i32 i = first_step; i <= self.steps(); ++i){
      func(i, self.value(i));
    }
    return self;
  }

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

  constexpr F& get_func() { return static_cast<F&>(*this); }
  constexpr const F& get_func() const { return static_cast<const F&>(*this); }

private:
  T _first, _last;
};

constexpr u32 dynamic_step = std::numeric_limits<u32>::max();
template<typename T, typename F = linear_mix, u32 StepSize = dynamic_step>
class steplerp : public steplerp_base<T, F, steplerp<T, F, StepSize>> {
private:
  using base_t = steplerp_base<T, F, steplerp<T, F, StepSize>>;

public:
  static constexpr u32 step_size = StepSize;

public:
  using base_t::base_t;

public:
  constexpr u32 steps() const { return step_size; }
};

template<typename T, typename F>
class steplerp<T, F, dynamic_step> :
  public steplerp_base<T, F, steplerp<T, F, dynamic_step>>
{
private:
  using base_t = steplerp_base<T, F, steplerp<T, F, dynamic_step>>;

public:
  static constexpr u32 step_size = dynamic_step;

public:
  constexpr steplerp(const T& first, const T& last, u32 steps) :
    base_t{first, last}, _steps{steps} {}

  constexpr steplerp(const T& first, const T& last, u32 steps, const F& func) :
    base_t{first, last, func}, _steps{steps} {}

  constexpr steplerp(const T& first, const T& last, u32 steps, F&& func) :
    base_t{first, last, std::move(func)}, _steps{steps} {}

  template<typename... Args>
  constexpr steplerp(const T& first, const T& last, u32 steps, std::in_place_t t, Args&&... args) :
    base_t{first, last, t, std::forward<Args>(args)...}, _steps{steps} {}

public:
  constexpr u32 steps() const { return _steps; }
  constexpr steplerp& steps(u32 value) {
    _steps = value;
    return *this;
  }

private:
  u32 _steps;
};

template<typename T, typename F = linear_mix, u32 StepSize = dynamic_step>
class steplerp_aged : public steplerp<T, F, StepSize> {
private:
  using base_t = steplerp<T, F, StepSize>;

public:
  using base_t::steplerp;
  using base_t::operator();

public:
  constexpr T value() const { return base_t::value(_step); }

  constexpr T operator*() const { return value(); }

  constexpr T value_tick(i32 count = 1) {
    const T val = value();
    tick(count);
    return val;
  }

  constexpr T tick_value(i32 count = 1) { return tick(count).value(); }

public:
  constexpr i32 age() const { return _step; }

  constexpr steplerp_aged& tick(i32 count = 1) {
    _step += count;
    return *this;
  }

  constexpr steplerp_aged& age(i32 count) {
    _step = count;
    return *this;
  }

private:
  i32 _step{0};
};
