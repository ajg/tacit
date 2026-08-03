// The one ordered SUBSET worth its own TU: <tacit/$.hpp> alone, with no `_.hpp` before it and none
// after. It says something the six permutations cannot — that `$.hpp` is self-sufficient, bringing
// its own core rather than relying on a consumer having included one first.
//
// This case used to be covered incidentally by `dollar.cpp`, until that file started including
// `<tacit/_.hpp>` explicitly under the include-what-you-use rule. That was the right change for
// dollar.cpp and it silently removed the only TU that included `$.hpp` by itself — so the case gets
// a file that exists for it on purpose, where it cannot be lost to an unrelated edit again.
#include <tacit/$.hpp>

// `$.hpp`'s own cleanup, checked with nothing else in the TU to have done it for it.
#include "no_generator_macros.hpp"

#include <cassert>
#include <functional>
#include <set>
#include <vector>

using tacit::$;
using tacit::_; // reachable because `$.hpp` includes the core — the point of this file

int main() {
  assert($(-42).abs() == 42);
  assert((_ + 1)(41) == 42); // the core came along
  assert(*($<std::set, _, std::greater<>>(3, 1, 2)).begin() == 3);
  assert(($<std::vector>(1, 2, 3)) == (tacit::make<std::vector>(1, 2, 3)));
  static_assert(std::is_same_v<tacit::bind<std::vector, _::blank<>>::with<int>, std::vector<int>>);
  return 0;
}
