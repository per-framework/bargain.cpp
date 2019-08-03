#pragma once

#include "bargain_v0/synopsis.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>

namespace bargain_v0 {

class STM_private : STM {
  friend struct STM;

  class atom_base_t;

  static std::atomic<uint64_t> s_clock;

  struct access_base_t;
  template <class Value> struct access_t;

  static access_base_t *insert_base(access_base_t *accesses,
                                    access_base_t *access);

  template <class Value>
  static access_t<Value> *insert(access_base_t *accesses,
                                 access_t<Value> *access);

  template <class Value> static void write(uint64_t t, access_base_t *access);

  struct Local;

  static bool try_commit(uint64_t t, access_base_t *accesses);

  struct ro;
  struct rw;
};

} // namespace bargain_v0

//

template <class Value>
void bargain_v0::STM_private::write(uint64_t t, access_base_t *access_base) {
  access_t<Value> *access = static_cast<access_t<Value> *>(access_base);
  atom_t<Value> *atom = static_cast<atom_t<Value> *>(access->atom);
  atom->value.store(access->value, std::memory_order_relaxed);
  atom->lock.store(t, std::memory_order_release);
}

//

template <class Value>
bargain_v0::STM_private::access_t<Value> *
bargain_v0::STM_private::insert(access_base_t *accesses,
                                access_t<Value> *access) {
  return static_cast<access_t<Value> *>(insert_base(accesses, access));
}

//

struct bargain_v0::STM_private::access_base_t {
  access_base_t *children[2];
  atom_base_t *atom;
  void (*write)(uint64_t t, access_base_t *self);
};

template <class Value>
struct bargain_v0::STM_private::access_t : access_base_t {
  Value value;
};

//

struct bargain_v0::STM_private::ro {
  template <class Op> static result_of_t<Op> run(const Op &op) {
    std::optional<result_of_t<Op>> result;
    do {
      op.ro(STM_private::s_clock.load(),
            [&](auto, auto &&r) { result = std::forward<decltype(r)>(r); });
    } while (!result.has_value());
    return result.value();
  }
};

struct bargain_v0::STM_private::rw {
  template <class Op> static result_of_t<Op> run(const Op &op) {
    std::optional<result_of_t<Op>> result;
    do {
      op.rw(STM_private::s_clock.load(),
            nullptr,
            [&](auto t, auto accesses, auto &&r) {
              if (STM_private::try_commit(t, accesses))
                result = std::forward<decltype(r)>(r);
            });
    } while (!result.has_value());
    return result.value();
  }
};

//

struct bargain_v0::STM::unit_t final {};

//

template <class Value> struct bargain_v0::STM::get_t {
  atom_t<Value> *atom;

  template <class OnSuccess>
  void ro(uint64_t t, const OnSuccess &onSuccess) const {
    auto s = atom->lock.load();
    Value v = atom->value.load();
    if (s <= t)
      onSuccess(t, std::as_const(v));
  }

  template <class OnSuccess>
  void rw(uint64_t t,
          STM_private::access_base_t *before,
          const OnSuccess &onSuccess) const {
    STM_private::access_t<Value> node;
    auto s = atom->lock.load();
    node.value = atom->value.load();
    if (s <= t) {
      node.atom = atom;
      node.write = nullptr;
      STM_private::access_t<Value> *after = STM_private::insert(before, &node);
      onSuccess(t, after, std::as_const(after->value));
    }
  }

  //

  using result_t = Value;
  static constexpr bool is_readonly = true;
};

template <class Value> struct bargain_v0::STM::set_t {
  atom_t<Value> *atom;
  Value value;

  template <class OnSuccess>
  void rw(uint64_t t,
          STM_private::access_base_t *before,
          const OnSuccess &onSuccess) const {
    if (atom->lock.load() <= t) {
      STM_private::access_t<Value> node;
      node.atom = atom;
      node.write = STM_private::write<Value>;
      node.value = value;
      STM_private::access_t<Value> *after = STM_private::insert(before, &node);
      onSuccess(t, after, reinterpret_cast<const unit_t &>(*this));
    }
  }

  //

  using result_t = unit_t;
  static constexpr bool is_readonly = false;
};

//

class bargain_v0::STM_private::atom_base_t {
  friend struct STM;
  friend class STM_private;

  std::atomic<uint64_t> lock;

  atom_base_t() : lock(0) {}
};

template <class Value>
class bargain_v0::STM::atom_t : STM_private::atom_base_t {
  friend struct STM;
  friend class STM_private;

  std::atomic<Value> value;

public:
  atom_t(const atom_t &) = delete;
  atom_t &operator=(const atom_t &) = delete;

  atom_t() {}

  template <class Forwardable>
  atom_t(Forwardable &&value) : value(std::forward<Forwardable>(value)) {}

  get_t<Value> operator*() { return {this}; }

  template <class Forwardable> set_t<Value> operator=(Forwardable &&value) {
    return {this, std::forward<Forwardable>(value)};
  }
};

//

template <class Result> struct bargain_v0::STM::pure_t {
  Result result;

  template <class OnSuccess>
  void ro(uint64_t t, const OnSuccess &onSuccess) const {
    onSuccess(t, std::as_const(result));
  }

  template <class OnSuccess>
  void rw(uint64_t t,
          STM_private::access_base_t *accesses,
          const OnSuccess &onSuccess) const {
    onSuccess(t, accesses, std::as_const(result));
  }

  //

  using result_t = Result;
  static constexpr bool is_readonly = true;
};

template <class Fn, class Op1> struct bargain_v0::STM::bind_t {
  using Op2 = std::invoke_result_t<Fn, const result_of_t<Op1> &>;

  Fn fn;
  Op1 op1;

  template <class OnSuccess>
  void ro(uint64_t t, const OnSuccess &onSuccess) const {
    op1.ro(t, [&, this](auto t, const auto &r1) { fn(r1).ro(t, onSuccess); });
  }

  template <class OnSuccess>
  void rw(uint64_t t,
          STM_private::access_base_t *accesses,
          const OnSuccess &onSuccess) const {
    op1.rw(t, accesses, [&, this](auto t, auto accesses, const auto &r1) {
      fn(r1).rw(t, accesses, onSuccess);
    });
  }

  //

  using result_t = result_of_t<Op2>;
  static constexpr bool is_readonly = Op1::is_readonly && Op2::is_readonly;
};

//

template <class Value>
bargain_v0::STM::get_t<Value> bargain_v0::STM::get(atom_t<Value> &atom) {
  return {&atom};
}

template <class Value>
bargain_v0::STM::set_t<Value> bargain_v0::STM::set(atom_t<Value> &atom,
                                                   const Value &value) {
  return {&atom, value};
}

template <class Result>
bargain_v0::STM::pure_t<bargain_v0::by_value_t<Result>>
bargain_v0::STM::pure(Result &&result) {
  return {std::forward<Result>(result)};
}

template <class Fn, class Op>
bargain_v0::STM::bind_t<bargain_v0::by_value_t<Fn>, bargain_v0::by_value_t<Op>>
bargain_v0::STM::bind(Fn &&fn, Op &&op) {
  return {std::forward<Fn>(fn), std::forward<Op>(op)};
}

//

template <class Op>
bargain_v0::STM::result_of_t<Op> bargain_v0::STM::run(const Op &op) {
  return std::conditional_t<is_readonly_v<Op>,
                            STM_private::ro,
                            STM_private::rw>::run(op);
}

//

template <class Fn, class Op, class>
bargain_v0::STM::bind_t<bargain_v0::by_value_t<Fn>, bargain_v0::by_value_t<Op>>
bargain_v0::operator>>(Op &&op, Fn &&fn) {
  return STM::bind(std::forward<Fn>(fn), std::forward<Op>(op));
}

//
