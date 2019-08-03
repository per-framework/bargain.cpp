#pragma once

#include "polyfill_v1/type_traits.hpp"

namespace bargain_v0 {

template <class Forwardable>
using by_value_t =
    std::conditional_t<std::is_function_v<std::remove_cvref_t<Forwardable>>,
                       std::remove_cvref_t<Forwardable> *,
                       std::remove_cvref_t<Forwardable>>;

struct STM {
  //

  struct unit_t;

  //

  template <class Value> class atom_t;

  //

  template <class Op>
  using result_of_t = typename std::remove_cvref_t<Op>::result_t;

  template <class Op>
  static constexpr bool is_readonly_v = std::remove_cvref_t<Op>::is_readonly;

  //

  template <class Value> struct get_t;
  template <class Value> struct set_t;
  template <class Result> struct pure_t;
  template <class Fn, class Op> struct bind_t;

  //

  template <class Value> static get_t<Value> get(atom_t<Value> &atom);

  template <class Value>
  static set_t<Value> set(atom_t<Value> &atom, const Value &value);

  template <class Result>
  static pure_t<by_value_t<Result>> pure(Result &&result);

  template <class Fn, class Op>
  static bind_t<by_value_t<Fn>, by_value_t<Op>> bind(Fn &&fn, Op &&op);

  //

  template <class Op> static result_of_t<Op> run(const Op &op);

  //
};

//

template <class Fn,
          class Op,
          class = STM::result_of_t<
              std::invoke_result_t<Fn, const STM::result_of_t<Op> &>>>
STM::bind_t<by_value_t<Fn>, by_value_t<Op>> operator>>(Op &&op, Fn &&fn);

} // namespace bargain_v0
