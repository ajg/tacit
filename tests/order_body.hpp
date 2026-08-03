// SPDX-License-Identifier: BSL-1.0
// The shared body of the include-order tests: the same assertions, reached from a different
// permutation of <tacit/_.hpp>, <tacit/$.hpp> and <tacit/λ.hpp> in each including TU. Keeping it in
// one file is the point — the ONLY difference between order.cpp and order_reverse.cpp is the order.
#pragma once

// Included here too, per the include-what-you-use rule the rest of the suite follows. By the time
// this file is read the including TU has already established the order under test, so these three
// are no-ops (`#pragma once`) and cannot influence what is being measured — they only make this
// header stand on its own.
#include <tacit/$.hpp>
#include <tacit/_.hpp>
#include <tacit/λ.hpp>

// ...and with all three included, in whatever order the TU chose, NO generator macro may remain.
// This is the only place `$.hpp`'s copy of the `make` table gets its cleanup checked — strict_using
// includes the core alone and cannot see it.
#include "no_generator_macros.hpp"

#include <cassert>
#include <functional>
#include <set>
#include <string>
#include <vector>

using tacit::$;
using tacit::_;

inline int order_body() {
  // ---- the core: sections, chained comparison, vocabulary, composition ----
  assert((_ + 1)(41) == 42);
  assert((1u <= _.size() < 4u)(std::string("ab")));
  assert((_ < _)(1, 2));
  assert(tacit::compose(_ + 1, _ * 2)(3) == 8);

  // ---- the term wrapper, including the partial-CTAD shapes the duplicated table emits ----
  assert($(-42).abs() == 42);
  assert($(std::vector{1, 2, 3}).size() == 3);
  assert(*($<std::set, _, std::greater<>>(3, 1, 2)).begin() == 3);
  assert(($<std::vector>(1, 2, 3)) == (tacit::make<std::vector>(1, 2, 3)));
  assert(tacit::lift(-1).abs() == $(-1).abs());

  // ---- the type level, reached through the same `_` ----
  static_assert(std::is_same_v<tacit::bind<std::vector, _::blank<>>::with<int>, std::vector<int>>);

  // ---- the lambda head, in the same TU as both of the above ----
  assert(λ(a, b) { return a + b; }(40, 2) == 42);
  assert(λ() { return 42; }() == 42);
  return 0;
}
