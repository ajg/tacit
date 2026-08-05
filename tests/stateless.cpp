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
  // The library's guarantee is that the closure TYPE is empty (asserted above, and it holds on
  // every front end). Whether a container then applies empty-base optimization is the standard
  // library's business: clang-cl does not treat a class whose emptiness comes from
  // [[msvc::no_unique_address]] members as EBO-eligible, so the MSVC STL cannot compress it there
  // — even though it compresses a plain empty comparator fine. MSVC's own front end does.
#if !(defined(_MSC_VER) && defined(__clang__))
  {
    static_assert(sizeof(std::set<int, decltype(_ > _)>) == sizeof(std::set<int, std::greater<>>));
    static_assert(sizeof(std::set<int, decltype(_ > _)>) == sizeof(std::set<int>));
  }
#endif

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

  // ---- a COMPOSED closure reaches the type world too ----
  // This was a KNOWN LIMIT here, and the note that stood in its place prescribed the fix: a closure
  // whose operands are themselves closures — `_.size() < _.size()`, `!(_ < _)` — held nothing that
  // mattered yet could not be default-constructed, because the sections were built as capturing
  // lambdas and *any* capture deletes the default constructor, empty capture or not. Building them
  // from named function objects with `[[no_unique_address]]` members instead is what `_.hpp` now
  // does, so the limit is gone and ordering by a projection no longer has to fall back to the value
  // form (`std::ranges::sort(v, {}, _.size())`).
  {
    static_assert(std::is_default_constructible_v<decltype(_.size() < _.size())>);
    static_assert(std::is_default_constructible_v<decltype(!(_ < _))>);
    static_assert(!std::is_default_constructible_v<decltype(_ > 3)>); // binding a value still forfeits it
    std::vector<std::string> v{"ccc", "a", "bb"};
    std::ranges::sort(v, decltype(_.size() < _.size()){}); // the closure as a TYPE, not a value
    assert(v[0] == "a" && v[2] == "ccc");
  }

  // WHAT DID NOT FOLLOW, and cannot. Only default-constructibility propagates; EMPTINESS does not. A
  // composed closure holds its operand closures as distinct subobjects, and two subobjects of the
  // same type cannot share an address whatever `[[no_unique_address]]` says — `_.size() < _.size()`
  // is two bytes, not one, everywhere. (Clang's `is_empty` answers yes regardless; `sizeof` is the
  // honest witness and the MSVC ABI agrees with it.) That is a fact about object layout rather than
  // a limit waiting to be lifted, so the zero-cost claim above stays scoped to closures built purely
  // from `_`, which really do hold nothing at all. `properties.cpp` carries the same claims across
  // every closure form, and `allocation.cpp` guards the size directly.
  static_assert(sizeof(decltype(_ > _)) == 1);
  static_assert(sizeof(decltype(_.size() < _.size())) > 1);

  return 0;
}
