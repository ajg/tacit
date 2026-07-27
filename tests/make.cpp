// `make<F, ...>(args...)` — the closed cell that BUILDS a value, where `lift` adopts one. The rule
// is CTAD plus blanks: `_` in the template-argument list means "deduce this position", anything else
// is fixed. That combination — deduce some arguments, fix others — is what stock C++ has no spelling
// for, and it is the whole reason this exists.
#include <tacit/_.hpp>

#include <array>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

using tacit::_;
using tacit::make;

int main() {
  // ---- nothing fixed: plain CTAD, deduction guides and all ----
  {
    auto v = make<std::vector>(1, 2, 3);
    static_assert(std::is_same_v<decltype(v), std::vector<int>>);
    assert(v.size() == 3 && v[2] == 3);
    auto p = make<std::pair>(1, 'a');
    static_assert(std::is_same_v<decltype(p), std::pair<int, char>>);
    auto t = make<std::tuple>(1, 'a', 2.5);
    static_assert(std::is_same_v<decltype(t), std::tuple<int, char, double>>);
    auto o = make<std::optional>(5);
    static_assert(std::is_same_v<decltype(o), std::optional<int>>);
  }

  // ---- nothing deduced: the arguments as given ----
  {
    auto v = make<std::vector, double>(1.0, 2.0);
    static_assert(std::is_same_v<decltype(v), std::vector<double>>);
    auto m = make<std::map, std::string, int>();
    static_assert(std::is_same_v<decltype(m), std::map<std::string, int>>);
    assert(m.empty());
  }

  // ---- PARTIAL CTAD: deduce some positions, fix others ----
  {
    // fix the comparator, deduce the element — the element is the argument you would most hate to
    // re-type, and the one plain CTAD would have got right
    auto s = make<std::set, _, std::greater<>>(3, 1, 2);
    static_assert(std::is_same_v<decltype(s), std::set<int, std::greater<>>>);
    assert(*s.begin() == 3);   // greater<>, so the largest sorts first

    // deduce both key and mapped type, fix only the comparator
    auto m = make<std::map, _, _, std::greater<>>(std::pair{1, std::string("a")},
                                                  std::pair{2, std::string("b")});
    static_assert(std::is_same_v<decltype(m), std::map<int, std::string, std::greater<>>>);
    assert(m.begin()->first == 2);

    // a blank in the middle: fix the mapped type, deduce the key
    auto m2 = make<std::map, _, long>(std::pair{1, 2L});
    static_assert(std::is_same_v<decltype(m2), std::map<int, long>>);

    // depth 4 — unordered_map's <Key, T, Hash, KeyEqual> deduced ahead of a fixed allocator
    using alloc = std::allocator<std::pair<const int, char>>;
    auto u = make<std::unordered_map, _, _, _, _, alloc>(std::pair{1, 'a'});
    static_assert(std::is_same_v<decltype(u),
                                 std::unordered_map<int, char, std::hash<int>,
                                                    std::equal_to<int>, alloc>>);
    assert(u.at(1) == 'a');
  }

  // ---- blanks with nothing fixed are just CTAD, and trailing parameters re-default ----
  {
    static_assert(std::is_same_v<decltype(make<std::vector, _>(1, 2, 3)), std::vector<int>>);
    // the allocator is not mentioned, so it re-defaults for the DEDUCED element, not carried over
    static_assert(std::is_same_v<decltype(make<std::vector, _>(1, 2, 3)),
                                 std::vector<int, std::allocator<int>>>);
    // NOT reachable: `std::array` is a <class, size_t> shape, and `make`'s parameter is
    // `template <class...> class F`. Nothing is lost — an array has one type argument and a deduced
    // extent, so there is no second argument to fix and partial CTAD has nothing to say. Plain CTAD
    // is the whole story there, and `rebind` still covers the shape at type level.
    auto a = std::array{1, 2, 3};
    static_assert(std::is_same_v<decltype(a), std::array<int, 3>>);
    static_assert(std::is_same_v<_::rebind<double>::of<decltype(a)>, std::array<double, 3>>);
  }

  // ---- the result is the value itself, not a lift of it ----
  {
    auto v = make<std::vector>(1, 2, 3);
    static_assert(std::is_same_v<decltype(v), std::vector<int>>);   // a real vector, hand it anywhere
    v.push_back(4);                                                 // its own members, not the vocabulary
    assert(v.size() == 4);
    assert(std::ranges::count_if(v, _ > 2) == 2);                   // and it feeds the open cell
    assert(tacit::lift(make<std::vector>(1, 2, 3)).size() == 3);    // wrap it if you want the vocabulary
  }

  // ---- it composes with the type-level surface, through decltype ----
  {
    using V = decltype(make<std::vector, _>(1, 2, 3));
    static_assert(std::is_same_v<_::value_type::of<V>, int>);
    static_assert(std::is_same_v<_::rebind<double>::of<V>, std::vector<double>>);
  }

  return 0;
}
