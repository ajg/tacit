// Verifies `import tacit.dollar;` — the module mirror of <tacit/$.hpp> — alongside `import tacit;`:
// `$` crossing its own module boundary, agreeing with the `lift`/`make` that cross the other one,
// and the two modules' global-module entities merging in one TU. Built only by the clang `modules`
// CI job, like module_check.cpp.
import tacit;
import tacit.dollar;

#include <cassert>
#include <functional>
#include <set>
#include <string>
#include <vector>

using tacit::_;
using tacit::$;

int main() {
  // the lift half, across the module
  assert($(-42).abs() == 42);
  assert($(std::string("abc")).length() == 3u);
  // the make half, partial CTAD included
  assert(($<std::vector>(1, 2, 3) == tacit::make<std::vector>(1, 2, 3)));
  auto s = $<std::set, _, std::greater<>>(3, 1, 2);
  assert(*s.begin() == 3);
  // entities from both imports meet in one expression
  assert($(std::vector<int>{1, 2}).size() == _.size()(std::vector<int>{1, 2}));
  return 0;
}
