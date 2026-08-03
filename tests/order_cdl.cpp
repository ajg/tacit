// Include order 1 of 6: _ → $ → λ. Also the file that re-includes all three, pinning idempotency
// once rather than six times.
//
// THE SET. A TU has exactly one include order, so order-independence can only be shown by having
// one TU per permutation: order_{cdl,cld,dcl,dlc,lcd,ldc}.cpp, where c = <tacit/_.hpp> (core),
// d = <tacit/$.hpp> (dollar), l = <tacit/λ.hpp> (lambda). All six share `order_body.hpp`, so the
// ONLY variable across the set is the order itself. Six is the whole space, which is why it is
// covered exhaustively rather than sampled — there is no judgment call left about which orders
// matter. (Ordered SUBSETS are not enumerated: the six triples already put every header in first,
// middle and last position, and `order_d_only.cpp` covers the one subset that says something more.)
//
// WHAT IS AT STAKE. `_.hpp` and `$.hpp` each define, expand, and #undef their own copy of the same
// generator macro family (see the note above the table in either header), so a missing #undef or a
// stale definition surfaces differently depending on which came first — and `order_body.hpp`
// re-asserts the macro-hygiene policy after all three, which is the only place `$.hpp`'s copy gets
// checked. `λ.hpp` includes nothing and defines only `λ`, so it should be inert wherever it lands;
// putting it in all three positions is what makes that a tested claim rather than an assumption.
//
// WHY `clang-format off`. clang-format sorts includes alphabetically and `$` < `_` < `λ`, so left
// alone it would collapse every file in this set to the same order and quietly delete the test. The
// order below is the fixture, not a style slip.
// clang-format off
#include <tacit/_.hpp>
#include <tacit/$.hpp>
#include <tacit/λ.hpp>
#include <tacit/_.hpp> // re-include: no-op
#include <tacit/$.hpp> // re-include: no-op
#include <tacit/λ.hpp> // re-include: no-op
// clang-format on

#include "order_body.hpp"

int main() { return order_body(); }
