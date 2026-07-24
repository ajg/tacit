// Composition: `_`'s single-argument closures are composable `fn`s, so
// sections, subscript, and arithmetic chain point-free.
#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <tacit/_.hpp>
#include <vector>

using tacit::_;

int main() {
  // a section composed onto a member/CPO projection
  auto big = _.size() >= 2u; // x -> size(x) >= 2
  assert(big(std::string("abc")) && !big(std::string("a")));

  // reversed operand, and a chained arithmetic pipeline
  auto lo = 10 < _; // x -> 10 < x
  assert(lo(11) && !lo(9));
  auto f = (_ + 1) * 2; // x -> (x + 1) * 2
  assert(f(3) == 8);

  // subscript, and subscript-then-section
  assert(_[0](std::vector<int>{9, 8, 7}) == 9);
  auto first_pos = _[0] > 0; // x -> x[0] > 0
  assert(first_pos(std::vector<int>{5, -1}) &&
         !first_pos(std::vector<int>{-5, 1}));

  // fn < fn -> a projected binary comparator, usable by ranges::sort
  std::vector<std::string> v{"ccc", "a", "bb"};
  std::ranges::sort(v, _.size() < _.size());
  assert(v[0] == "a" && v[2] == "ccc");

  // a composed predicate through a std algorithm
  std::vector<int> n{1, 2, 3, 4, 5};
  assert(std::ranges::count_if(n, _ % 2 == 0) == 2);
  return 0;
}
