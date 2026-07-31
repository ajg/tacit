// λ — the lambda head. What this file pins: the macro emits the HEAD ONLY (`[&](auto&& a, ...)`),
// so the body is ordinary C++ with no comma or parenthesization rules; the trailing-return slot
// stays open for the user; capture is by reference; and the header is completely standalone — this
// file deliberately includes NOTHING from tacit but <tacit/λ.hpp> until the coexistence section.
#include <tacit/λ.hpp>

#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <vector>

int main() {
  // ---- the shapes: zero, one, many parameters ----
  {
    assert(λ() { return 42; }() == 42);
    assert(λ(x) { return x * x; }(3) == 9); // the argument used twice — the thing `_` cannot say
    assert(λ(a, b, c) { return a + b + c; }(1, 2, 3) == 6);
  }

  // ---- the body is plain C++: top-level commas, statements, no escaping ----
  {
    auto f = λ(x) { return std::max(x, 0); }; // a comma in the body is just a comma
    assert(f(-5) == 0);
    int n = 0;
    λ(d) {
      n += d;
      n += d;
    }(5); // statements, multiple uses, immediate invocation
    assert(n == 10);
  }

  // ---- capture is [&]: locals are simply visible ----
  {
    int scale = 3;
    assert(λ(x) { return x * scale; }(4) == 12);
    auto add = λ(x) { return λ(y) { return x + y; }; }; // nested: inner [&] sees x
    assert(add(2)(3) == 5);
  }

  // ---- ASCII spellings: a UCN-spelled identifier IS the identifier, macro replacement included.
  // MSVC has not implemented that equivalence (P2314): the glyph works there, these spellings
  // don't, so this section is clang/GCC-only. ----
#if !defined(_MSC_VER) || defined(__clang__)
  {
    assert(\u03BB(x) { return x * 2; }(21) == 42);   // the classic 4-hex form
#if __cplusplus >= 202302L
    assert(\u{3BB}(a, b) { return a + b; }(40, 2) == 42); // the C++23 delimited form
#endif
  }
#endif // !_MSC_VER

  // ---- the trailing-return slot is open: opt into reference preservation per site ----
  {
    std::vector<std::string> v{"abc"};
    λ(s) -> decltype(auto) { return s.front(); }(v[0]) = 'x';
    assert(v[0] == "xbc");
  }

  // ---- it feeds algorithms like any closure ----
  {
    std::vector<std::string> v{"ccc", "a", "bb"};
    std::ranges::sort(v, λ(a, b) { return a.size() < b.size(); });
    assert(v[0] == "a" && v[2] == "ccc");
    assert(std::ranges::count_if(v, λ(s) { return s.size() * s.size() > 4u; }) == 1);
  }

  return 0;
}

// ---- coexistence: λ and `_` in one TU, each on its own side of the grammar line ----
#include <tacit/_.hpp>
using tacit::_;
static_assert((_ * 2)(21) == 42); // the expression grammar: no λ needed
