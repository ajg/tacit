// Closure combinators: compose, fanout (&&&-style), and first/second on pairs.
// All are opt-in — request them before including the header.
#define TACIT_COMBINATORS
#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <tacit/_.hpp>
#include <tuple>
#include <utility>
#include <vector>

using tacit::_;

int main() {
  // compose: tacit::compose(f, g, ...) == x -> ...(g(f(x)))  (left to right)
  auto dbl = tacit::compose(_.size(), [](std::size_t n) { return n * 2; });
  assert(dbl(std::string("ab")) == 4);
  assert(tacit::compose(_.front(), _ == 'a')(std::string("abc"))); // front, then compare
  assert(tacit::compose(_ + 1, _ * 2)(3) == 8);                    // (3 + 1) * 2, arbitrary closures

  // fanout: x -> tuple{ f(x), g(x), ... }
  auto [n, c] = tacit::fanout(_.size(), _.front())(std::string("abc"));
  assert(n == 3 && c == 'a');

  // first / second: transform one component of a pair
  auto pr = std::pair{std::string("ab"), 5};
  auto [a1, b1] = tacit::first(_.size())(pr);
  assert(a1 == 2 && b1 == 5);
  auto [a2, b2] = tacit::second(_ + 1)(pr);
  assert(a2 == "ab" && b2 == 6);

  // a combinator's result is itself an fn -> a ready-made projection
  std::vector<std::string> v{"bb", "a", "cc"};
  std::ranges::sort(v, {}, tacit::fanout(_.size(), _.front()));
  assert(v[0] == "a");

  // the real ranges pipe still works (a range on the left, not an fn — no hijack)
  auto twos = v | std::views::filter([](auto &s) { return s.size() == 2; });
  assert(std::ranges::distance(twos) == 2);
  return 0;
}
