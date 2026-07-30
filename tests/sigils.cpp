// Synthetic sigils, behind `TACIT_COMBINATORIAL_OPERATORS`.
//
//     f >>* g      compose, left to right   x -> g(f(x))
//     f <<* g      compose, right to left   x -> f(g(x))
//     f &&& g      fanout                   x -> {f(x), g(x)}
//     f *** g      product                  (a, b) -> {f(a), g(b)}
//
// None of these is a C++ operator — the overloadable set is closed. Each is a token sequence the
// lexer splits into operators that do exist: `f &&& g` is binary `&&` on `f` and unary `&` on `g`.
// What this file pins is the trade that makes that possible: the old meanings of `&` and `*` on a
// closure are UNCHANGED (the marker derives from `fn`), and the cost is one reading per sigil —
// `f && (&g)`, `f << (*g)`, `f >> (*g)`, `f * (**g)`, closure against marked closure — none of
// which anybody writes.
#define TACIT_COMBINATORIAL_OPERATORS
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <map>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

using tacit::_;

int main() {
  // ---- what must NOT have changed: the unary sections the sigils are carved out of ----
  {
    int c = 7;
    assert((&_)(c) == &c);                 // address-of section, exactly as before
    int v = 42, *p = &v;
    assert((*_)(p) == 42);                 // deref section
    assert(((*_) + 1)(p) == 43);           // ...still composes
    assert((_ && true)(true));             // logical-and section
    assert((_ && _)(true, true));
    assert((_ << 2)(3) == 12);             // shift section
    assert((_ * 3)(4) == 12);              // multiply section
  }

  // ---- fanout: one input, both closures ----
  {
    auto f = _.size() &&& _.front();
    auto p = f(std::string("abc"));
    assert(p.first == 3 && p.second == 'a');
    // a pair at two operands, matching the comma section's own convention
    static_assert(std::is_same_v<decltype(p), std::pair<std::size_t, char>>);
  }

  // ---- compose, both directions ----
  {
    std::vector<std::string> v{"abcd"};
    assert((_.size() <<* _.front())(v) == 4);   // right to left: size(front(v))
    assert((_.front() >>* _.size())(v) == 4);   // left to right: size(front(v))
    // the arrow direction is the only difference, so both spellings say the same thing
    assert((_.size() <<* _.front())(v) == (_.front() >>* _.size())(v));
    // PRECEDENCE is inherited from the binary half. `>>` sits below arithmetic, so the LEFT operand
    // of `>>*` needs no parentheses even when it is an expression...
    assert((_ + 1 >>* (_ * 2))(3) == 8);
    // ...but the RIGHT operand is a different matter for every sigil: the unary half binds tightest
    // of all and grabs only the primary expression after it, so `f >>* _ * 2` is `f >> ((*_) * 2)`
    // and `f &&& _ * 2` is `f && ((&_) * 2)` — neither a composition nor a fanout. Anything past a
    // single postfix expression wants parentheses.
    assert((_ + 1 &&& (_ * 2))(3).first == 4);
    assert((_ + 1 &&& (_ * 2))(3).second == 6);
    assert((_.size() &&& _.front())(std::string("hi")).first == 2);   // postfix: no parens needed
  }

  // ---- product: each side of a pair through its own closure ----
  {
    auto pd = _.size() *** _.front();
    auto q = pd(std::string("ab"), std::string("xyz"));
    assert(q.first == 2 && q.second == 'x');
    auto r = pd(std::pair{std::string("abc"), std::string("zy")});   // or one pair in
    assert(r.first == 3 && r.second == 'z');
  }

  // ---- they are ordinary closures, so they feed algorithms ----
  {
    std::vector<std::string> v{"ccc", "a", "bb"};
    std::ranges::sort(v, {}, _.size() >>* (_ * 2));
    assert(v[0] == "a" && v[2] == "ccc");
    assert(std::ranges::count_if(v, _.size() >>* (_ > 1u)) == 2);
  }

  // ---- `->*` is NOT a sigil: it keeps its natural, ungated meaning under the gate too ----
  {
    struct W { int x; };
    W w{7};
    assert((_ ->* &W::x)(&w) == 7);
  }

  // ---- and the marked closure still IS an fn everywhere else ----
  {
    int v = 5, *p = &v;
    auto d = *_;                       // a marked closure...
    assert(d(p) == 5);                 // ...calls as the deref section
    assert((d + 1)(p) == 6);           // ...and composes as one
    static_assert(tacit::detail::is_fn_v<decltype(d)>);
  }

  return 0;
}
