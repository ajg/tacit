// Closures of more than one fill: that they compose like any other closure, that they stay bound
// values in argument position rather than becoming projected blanks, and — the other half of the
// same rule — that a blank written where nothing could ever fill it is rejected where it is written.
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <functional>
#include <list>
#include <string>
#include <vector>

using tacit::_;

// Rejection checks want a dependent context: outside one, clang reports the failed member
// constraint as a hard error rather than folding it into the requires-expression.
template <class U = struct tacit::_> constexpr bool chain_blank = requires(U u) { u.front().substr(_); };
template <class U = struct tacit::_> constexpr bool arrow_blank = requires(U u) { u->substr(_); };
template <class U = struct tacit::_> constexpr bool apply_blank = requires(U u) { u.front()._(_); };
template <class U = struct tacit::_> constexpr bool chain_bound = requires(U u) { u.front().substr(1); };
template <class U = struct tacit::_> constexpr bool sect_blank = requires(U u) { u.substr(_); };
template <class U = struct tacit::_> constexpr bool sect_blanks = requires(U u) { u.replace(_, _); };
template <class U = struct tacit::_> constexpr bool two_input_vocab = requires(U u) { (u < u).size(); };

int main() {
  // ---- a two-input section composes, exactly as a one-input one does ----
  {
    assert(((_ + _) + 1)(1, 2) == 4);  // (a, b) -> (a + b) + 1
    assert(((_ + _) * 2)(1, 2) == 6);
    assert((-(_ + _))(1, 2) == -3);          // unary onto a two-input form
    assert(((_ < _) == true)(1, 2));         // comparison onto one
    assert(((_.size() < _.size()) == true)(std::string("a"), std::string("bb")));
    // and it keeps the vocabulary: `.size()` applies to what the comparison produced
    static_assert(two_input_vocab<>);
  }

  // ---- a projection against a blank is the two-input form, like `_ op _` ----
  {
    auto a = _.size() < _;  // (v, n) -> size(v) < n
    assert(a(std::string("ab"), 3u) && !a(std::string("abcd"), 3u));
    auto b = _ < _.size();  // (n, v) -> n < size(v)
    assert(b(1u, std::string("ab")) && !b(9u, std::string("ab")));
    assert((_.size() + _)(std::string("ab"), 1u) == 3u);
    assert((_.size() < 3u)(std::string("ab")));  // the one-sided form is unchanged
  }

  // ---- subscript, compound assignment and application take a blank the same way ----
  {
    std::vector<int> v{10, 20, 30};
    assert((_[_])(v, 1) == 20);  // (x, i) -> x[i]
    assert((_[0])(v) == 10);     // unchanged
    int n = 5;
    (_ += _)(n, 3);
    assert(n == 8);  // (a, b) -> a += b
    int m = 5;
    (_ += 1)(m);
    assert(m == 6);                                 // unchanged
    assert((_(_))(std::negate<int>{}, 4) == -4);    // (f, x) -> f(x)
    assert((_(2, _))(std::plus<int>{}, 5) == 7);    // (f, x) -> f(2, x)
    assert((_(3))(std::negate<int>{}) == -3);       // unchanged
  }

  // ---- `._()` — the application form on a projection: invoke what it produced ----
  {
    std::vector<std::function<int()>> fs{[] { return 42; }};
    assert((_.front()._())(fs) == 42);
    std::vector<std::function<int(int)>> gs{[](int x) { return x + 1; }};
    assert((_.front()._(41))(gs) == 42);
    assert(((_.front()._(41)) + 1)(gs) == 43);  // composes onward
  }

  // ---- a two-input closure in argument position is a bound VALUE, not a projected blank ----
  {
    std::list<int> l{3, 1, 2};
    _.sort(_ < _)(l);  // passes a comparator; a one-fill closure there would project instead
    assert(l.front() == 1);
    std::vector<int> v{3, 1, 2};
    std::ranges::sort(v, _ < _);
    assert((v == std::vector<int>{1, 2, 3}));
  }

  // ---- no dead closures: a blank with nothing to fill it is rejected at the expression ----
  {
    // a chained call binds its arguments, so a blank there could never be filled
    static_assert(!chain_blank<>);
    static_assert(!arrow_blank<>);
    static_assert(!apply_blank<>);
    static_assert(chain_bound<>);  // the bound form is fine
    // whereas a blank the section CAN fill is accepted
    static_assert(sect_blank<>);
    static_assert(sect_blanks<>);
  }

  return 0;
}
