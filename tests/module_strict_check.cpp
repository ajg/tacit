// The strict consumer: compiled with -pedantic-errors AGAINST the `$`-bearing interface. This is
// the proof behind folding `$` into the one `tacit` module — an import injects names, not tokens,
// so a strictly-conforming TU that keeps to the conforming spellings never lexes `$` and never
// knows it is there. Built only by the clang `modules` CI job.
import tacit;

#include <cassert>
#include <functional>
#include <set>
#include <vector>

using tacit::_;
using tacit::lift;
using tacit::make;

int main() {
  assert(lift(-42).abs() == 42);
  assert(lift(std::vector{1, 2}).size() == 2);
  auto s = make<std::set, _, std::greater<>>(3, 1, 2);
  assert(*s.begin() == 3);
  assert((_ + 1)(41) == 42);
  return 0;
}
