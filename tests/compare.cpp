// Comparison chains: `0 < _ < 10` means `(0 < x) && (x < 10)`, not C++'s `(0 < x) < 10`.
// Covers the three shapes of link (bound value, blank, projection), chains of length > 2, mixed
// operators, and the boundary — where a chain does NOT form, and where single sections are unchanged.
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <vector>

using tacit::_;

// A value that counts the comparisons it takes part in — used to show the chain short-circuits.
struct probe {
  int v;
  static inline int cmps = 0;
  friend bool operator<(int a, probe const &b) {
    ++cmps;
    return a < b.v;
  }
  friend bool operator<(probe const &b, int a) {
    ++cmps;
    return b.v < a;
  }
};

int main() {
  // ---- the motivating case: a two-sided range test, in one expression ----
  {
    auto in_range = 0 < _ < 10;
    assert(!in_range(-1));
    assert(!in_range(0)); // strict on both sides
    assert(in_range(1));
    assert(in_range(9));
    assert(!in_range(10));
    assert(!in_range(11));
    // exactly what the spelled-out two-input form says about the same value
    assert(in_range(5) == ((_ > 0) && (_ < 10))(5, 5));
  }

  // ---- inclusive / mixed operators; either operand order ----
  {
    assert((0 <= _ <= 10)(0) && (0 <= _ <= 10)(10) && !(0 <= _ <= 10)(11));
    assert((0 < _ <= 10)(10) && !(0 < _ <= 10)(0));
    assert((10 > _ > 0)(5) && !(10 > _ > 0)(0)); // descending reads the same way
    assert((0 != _ != 7)(3) && !(0 != _ != 7)(7) && !(0 != _ != 7)(0));
  }

  // ---- chains of any length ----
  {
    auto c = 0 < _ < 10 < 20; // (0 < x) && (x < 10) && (10 < 20)
    assert(c(5) && !c(50));
    auto d = 0 <= _ < 100 <= 100;
    assert(d(7) && !d(-1));
  }

  // ---- the middle term may be a projection: evaluated per link, never the bool ----
  {
    auto small = 1u <= _.size() < 4u;
    assert(!small(std::string{}));
    assert(small(std::string("ab")));
    assert(!small(std::string("abcd")));

    std::vector<std::string> words{"", "a", "bb", "ccc", "dddd", "eeeee"};
    assert(std::ranges::count_if(words, 1u <= _.size() < 4u) == 3);

    auto first_in_range = 0 < _[0] < 10; // subscript projection chains too
    std::vector<int> row{5};
    assert(first_in_range(row));
    row[0] = 50;
    assert(!first_in_range(row));
  }

  // ---- short-circuit: the right link is only reached when the left one holds ----
  {
    auto in_range = 0 < _ < 10;
    probe::cmps = 0;
    assert(!in_range(probe{-1}));
    assert(probe::cmps == 1); // `0 < x` failed; `x < 10` never ran
    probe::cmps = 0;
    assert(in_range(probe{5}));
    assert(probe::cmps == 2);
  }

  // ---- single comparisons are unchanged (result type and value) ----
  {
    assert((_ < 10)(5) && !(_ < 10)(10));
    assert((10 > _)(5));
    assert((_ == 3)(3) && (_ != 3)(4));
    assert((_.size() < 3u)(std::string("ab")));
    assert((_ < _)(1, 2)); // two-blank stays a two-INPUT comparator
    assert(!(_ < _)(2, 1));
    assert((_.size() < _.size())(std::string("a"), std::string("bb")));
    std::vector<int> v{3, 1, 2};
    std::ranges::sort(v, _ < _);
    assert((v == std::vector<int>{1, 2, 3}));
  }

  // ---- the chain ends at any non-comparison operator ----
  {
    // `(0 < _) + 0` is arithmetic on the bool, so the `< 2` that follows compares that bool
    auto broken = (0 < _) + 0 < 2;
    assert(broken(5) && broken(-5)); // 0/1 is always < 2 — a plain fn, no chain state
    // negation likewise drops the chain
    auto n = !(_ < 10);
    assert(n(20) && !n(5));
  }

  // ---- constexpr ----
  {
    constexpr auto in_range = 0 < _ < 10;
    static_assert(in_range(5));
    static_assert(!in_range(10));
    static_assert(!(0 < _ < 10)(-3));
    static_assert((1 <= _ <= 3)(2));
  }

  return 0;
}
