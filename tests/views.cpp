// Range-adaptor verbs (opt-in, TACIT_VIEWS): a pipeline written point-free through `_`.
//   _.filter(_ != 0).take(2)(nums)  ==  nums | views::filter(_ != 0) | views::take(2)
// The verb binds its callable argument as a value and routes through std::views, so the result stays a
// unary composable closure (not a projected-blank section) and chains.
#define TACIT_VIEWS
#include <tacit/_.hpp>

#include <cassert>
#include <ranges>
#include <vector>

using tacit::_;

template <class R> static std::vector<int> dump(R &&r) {
  std::vector<int> v;
  for (int x : r)
    v.push_back(x);
  return v;
}

int main() {
  std::vector<int> nums{0, 3, 0, 5, 7, 0, 9};

  // the headline: a real tacit predicate bound as a value, chained, then applied
  auto pipe = _.filter(_ != 0).take(2);
  assert((dump(pipe(nums)) == std::vector<int>{3, 5}));

  // reusable — the closure defers the range (unlike `nums | …`, which binds it eagerly)
  std::vector<int> more{1, 0, 2, 0, 3};
  assert((dump(pipe(more)) == std::vector<int>{1, 2}));

  // longer chain
  assert((dump(_.drop(1).filter(_ > 0).take(2)(nums)) == std::vector<int>{3, 5}));
  assert((dump(_.take_while(_ < 8)(nums)) == std::vector<int>{0, 3, 0, 5, 7, 0}));
  assert((dump(_.reverse().take(2)(nums)) == std::vector<int>{9, 0}));

  // genuinely lazy: take short-circuits an unbounded iota source
  assert((dump(_.filter(_ % 2 == 0).take(3)(std::views::iota(0))) == std::vector<int>{0, 2, 4}));

  // and the closures still drop into an ordinary std::views pipeline (a pipeline result is just a range)
  assert((dump(nums | std::views::filter(_ != 0) | std::views::take(2)) == std::vector<int>{3, 5}));

  return 0;
}
