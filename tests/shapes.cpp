// `make` and `$` must accept the SAME sixteen partial-CTAD shapes and yield the SAME types.
//
// This is the guard on a deliberate duplication: the overload-set generator is a macro table that
// <tacit/_.hpp> expands as `make` and <tacit/$.hpp> expands, from a verbatim copy of the block, as
// `$`. C++ offers no way to give an existing function-template overload set a second name, so the
// copy is forced; this file is what makes it safe. Edit one table and not the other and a shape
// stops compiling here, or starts returning a different type — either way it fails, loudly, rather
// than waiting for a user to be the first to write shape eleven.
//
// The assertions are deliberately written as `decltype(make<...>) == decltype($<...>)` rather than
// against spelled-out expected types: the property under test is that the two agree, and stating it
// that way keeps this file correct even if the overlay rules themselves are revised. Concrete types
// are pinned in make.cpp; a handful of spot-checks below keep this file honest anyway.
#include <tacit/$.hpp>
#include <tacit/_.hpp>

#include <cassert>
#include <type_traits>

using tacit::_;

// A four-parameter aggregate: enough slots to reach the generator's depth limit, and CTAD-able from
// its own members, which is what the deducing half of every shape below exercises.
template <class A, class B, class C, class D> struct quad {
  A a;
  B b;
  C c;
  D d;
};

// Every shape is (prefix of length 1..4, each position either a fixed CLASS or a `_` SLOT), always
// ending in a slot — a trailing FIXED type needs no special overload, it rides the variadic tail.
// 1 + 2 + 4 + 8 = 15, plus the no-prefix base case = the sixteen the table spells.
#define SAME_SHAPE(...)                                                                                                \
  static_assert(std::is_same_v<decltype(tacit::make<__VA_ARGS__>(1, 2, 3, 4)),                                    \
                               decltype(tacit::$<__VA_ARGS__>(1, 2, 3, 4))>)

SAME_SHAPE(quad); // base: nothing fixed, everything deduced

SAME_SHAPE(quad, _); // depth 1
SAME_SHAPE(quad, long, _); // depth 2
SAME_SHAPE(quad, _, _);
SAME_SHAPE(quad, long, long, _); // depth 3
SAME_SHAPE(quad, _, long, _);
SAME_SHAPE(quad, long, _, _);
SAME_SHAPE(quad, _, _, _);
SAME_SHAPE(quad, long, long, long, _); // depth 4
SAME_SHAPE(quad, _, long, long, _);
SAME_SHAPE(quad, long, _, long, _);
SAME_SHAPE(quad, _, _, long, _);
SAME_SHAPE(quad, long, long, _, _);
SAME_SHAPE(quad, _, long, _, _);
SAME_SHAPE(quad, long, _, _, _);
SAME_SHAPE(quad, _, _, _, _);

#undef SAME_SHAPE

int main() {
  // ---- spot-checks: the shapes really do what the parity assertions above take for granted ----
  static_assert(std::is_same_v<decltype(tacit::make<quad>(1, 2, 3, 4)), quad<int, int, int, int>>);
  static_assert(std::is_same_v<decltype(tacit::make<quad, long, _>(1, 2, 3, 4)), //
                               quad<long, int, int, int>>);
  static_assert(std::is_same_v<decltype(tacit::make<quad, _, long, _, long>(1, 2, 3, 4)),
                               quad<int, long, int, long>>);

  // ---- and the VALUES agree, not merely the types ----
  auto m = tacit::make<quad, long, _>(1, 2, 3, 4);
  auto d = tacit::$<quad, long, _>(1, 2, 3, 4);
  assert(m.a == d.a && m.b == d.b && m.c == d.c && m.d == d.d);
  assert(m.a == 1L && m.d == 4);
  return 0;
}
