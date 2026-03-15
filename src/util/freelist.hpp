#pragma once

#include "../core.hpp"

namespace kappa {

template<typename T, size_t MaxElems>
class inplace_freelist {
public:
  static constexpr size_t max_element_count = MaxElems;
  using value_type = T;
  using element_slot = u32;

private:
  struct slot_t {

    slot_t() {}

    u32 next;

    union {
      T elem;
    };
  };

  static constexpr element_slot ELEM_TOMB = static_cast<element_slot>(-1);
  static constexpr element_slot ELEM_ACTIVE = ELEM_TOMB - 1;
  static_assert(MaxElems < ELEM_ACTIVE, "Invalid max element count");

public:
  inplace_freelist() noexcept : _empty_head(0), _count(0) {
    std::memset(_slots, 0xFF, sizeof(_slots)); // Sets every slot to ELEM_TOMB
  }

  inplace_freelist(inplace_freelist&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
  requires(!std::is_trivially_move_constructible_v<T>)
      : _count(other._count), _empty_head(other._empty_head) {
    std::memcpy(_slots, other._slots, sizeof(_slots));
    other.for_each([this](value_type& elem, element_slot slot) {
      new (std::addressof(_slots[slot].elem)) T(std::move(elem));
    });
  }

  inplace_freelist(inplace_freelist&& other) noexcept
  requires(std::is_trivially_move_constructible_v<T>)
  = default;

  inplace_freelist(const inplace_freelist& other) noexcept(std::is_nothrow_copy_constructible_v<T>)
  requires(!std::is_trivially_copy_constructible_v<T>)
      : _count(other._count), _empty_head(other._empty_head) {
    std::memcpy(_slots, other._slots, sizeof(_slots));
    other.for_each([this](const value_type& elem, element_slot slot) {
      new (std::addressof(_slots[slot].elem)) T(elem);
    });
  }

  inplace_freelist(const inplace_freelist& other) noexcept
  requires(std::is_trivially_copy_constructible_v<T>)
  = default;

  ~inplace_freelist() noexcept
  requires(std::is_trivially_destructible_v<T>)
  = default;

  ~inplace_freelist() noexcept
  requires(!std::is_trivially_destructible_v<T>)
  {
    static_assert(std::is_nothrow_destructible_v<T>);
    for_each([](T& elem) { elem.~T(); });
  }

public:
  element_slot insert(const value_type& elem) { return _do_insert(elem); }

  element_slot insert(value_type&& elem) { return _do_insert(std::move(elem)); }

  template<typename... Args>
  element_slot emplace(Args&&... args) {
    return _do_insert(std::forward<Args>(args)...);
  }

  void remove(element_slot slot) {
    if (!has_element(slot)) {
      return;
    }
    _do_remove(slot);
  }

private:
  template<typename... Args>
  element_slot _do_insert(Args&&... args) {
    assert(_count < MaxElems);
    element_slot pos = _empty_head;
    assert(pos < MaxElems);

    auto& slot = _slots[pos];
    assert(slot.next != ELEM_ACTIVE);
    if (slot.next == ELEM_TOMB) {
      ++_empty_head;
    } else {
      _empty_head = slot.next;
    }

    new (std::addressof(slot.elem)) T(std::forward<Args>(args)...);
    slot.next = ELEM_ACTIVE;

    ++_count;
    return pos;
  }

  void _do_remove(element_slot pos) {
    auto& slot = _slots[pos];
    assert(slot.next == ELEM_ACTIVE);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      slot.elem.~T();
    }

    slot.next = _empty_head;
    _empty_head = pos;

    --_count;
  }

public:
  template<typename F>
  void for_each(F&& f) {
    for (element_slot pos = 0, i = 0; pos < _count && i < (element_slot)MaxElems; ++i) {
      if (_slots[i].next == ELEM_ACTIVE) {
        if constexpr (std::is_invocable_v<std::remove_cvref_t<F>, decltype(_slots[i].elem),
                                          element_slot>) {
          f(_slots[i].elem, i);
        } else {
          f(_slots[i].elem);
        }
        ++pos;
      }
    }
  }

  void clear() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for_each([](T& elem) { elem.~T(); });
    }
    std::memset(_slots, 0xFF, sizeof(_slots));
    _empty_head = 0;
    _count = 0;
  }

public:
  size_t size() const noexcept { return _count; }

  size_t capacity() const noexcept { return max_element_count; }

  bool empty() const noexcept { return size() == 0; }

  bool has_element(element_slot slot) const noexcept {
    return slot < MaxElems && _slots[slot].next == ELEM_ACTIVE;
  }

  const value_type& operator[](element_slot slot) const {
    assert(has_element(slot));
    return _slots[slot].elem;
  }

  value_type& operator[](element_slot slot) {
    return const_cast<value_type&>(std::as_const(*this)[slot]);
  }

  const value_type& at(element_slot slot) const {
    if (!has_element(slot)) {
      throw std::out_of_range("Slot has no element");
    }
    return _slots[slot].elem;
  }

  value_type& at(element_slot slot) {
    return const_cast<value_type&>(std::as_const(*this).at(slot));
  }

  const value_type* at_opt(element_slot slot) const noexcept {
    return has_element(slot) ? &_slots[slot].elem : nullptr;
  }

  value_type* at_opt(element_slot slot) noexcept {
    return const_cast<value_type*>(std::as_const(*this).at_opt(slot));
  }

private:
  slot_t _slots[MaxElems];
  element_slot _empty_head;
  u32 _count;
};

} // namespace kappa
