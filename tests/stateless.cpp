// Closures as TYPES, not just values: `decltype(_ > _)` where you would write `std::greater<>`.
//
// A closure built purely from `_` holds nothing, so it is default-constructible and empty — which is
// exactly what a container's comparator/hasher template parameter demands. That makes `_` reach one
// step into the type world without any type-level machinery at all: the closure is still an ordinary
// value, and `decltype` is the only thing spelling the crossing.
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

using tacit::_;

int main() {
  // ---- a leaf closure is stateless: default-constructible AND empty ----
  {
    static_assert(std::is_default_constructible_v<decltype(_ > _)>);
    static_assert(std::is_default_constructible_v<decltype(_ < _)>);
    static_assert(std::is_default_constructible_v<decltype(_ == _)>);
    static_assert(std::is_default_constructible_v<decltype(_.size())>);
    static_assert(std::is_empty_v<decltype(_ > _)>);
    // a closure that binds a VALUE is correctly not stateless — it has to keep the 3
    static_assert(!std::is_default_constructible_v<decltype(_ > 3)>);
  }

  // ---- so it is a drop-in for std::greater<> in a template-argument position ----
  {
    std::set<int, decltype(_ > _)> s{3, 1, 2};
    assert(*s.begin() == 3);
    std::map<int, char, decltype(_ > _)> m{{1, 'a'}, {2, 'b'}};
    assert(m.begin()->first == 2);
  }

  // ---- and it costs exactly what std::greater<> costs: nothing ----
  {
    static_assert(sizeof(std::set<int, decltype(_ > _)>) == sizeof(std::set<int, std::greater<>>));
    static_assert(sizeof(std::set<int, decltype(_ > _)>) == sizeof(std::set<int>));
  }

  // ---- through `make`, where the deduced element meets the closure comparator ----
  {
    auto s = tacit::make<std::set, _, decltype(_ > _)>(3, 1, 2);
    static_assert(std::is_same_v<decltype(s), std::set<int, decltype(_ > _)>>);
    assert(*s.begin() == 3);
  }

  // ---- stateless also means usable as a NON-TYPE template argument: `fn` is a structural type ----
  {
    []<auto Cmp>() { assert(Cmp(2, 1)); }.template operator()<decltype(_ > _){}>();
  }

  // ---- the same closure, still an ordinary value ----
  {
    std::vector<int> v{3, 1, 2};
    std::ranges::sort(v, _ > _);
    assert(v[0] == 3);
    assert((_ > _)(2, 1));
  }

  // KNOWN LIMIT. A COMPOSED closure — one whose operands are themselves closures, as in
  // `_.size() < _.size()` or `!(_ < _)` — is not stateless today, even though it holds nothing that
  // matters. The section machinery builds closures as capturing lambdas, and *any* capture deletes
  // the default constructor, whether or not the captured object is empty. Nothing about the design
  // requires that: building those closures from named function objects with
  // `[[no_unique_address]]` members instead of lambdas would let statelessness propagate, and would
  // leave `_ > 3` correctly stateful. Until then, ordering by a projection wants the value form
  // (`std::ranges::sort(v, {}, _.size())`), which needs no type at all.

  return 0;
}
