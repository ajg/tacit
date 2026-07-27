// The type-argument vocabulary (`_.any_cast<int>()`, `_.to<std::vector>()`, …) and the field-style
// verbs (`_.first`, `_.second`).
//
// Two additions in one family: names whose argument is a *type* rather than a value, and names that
// are a *field* rather than a call. Both are reachable only because `<…>` and `.` bind to a member —
// see tests/get.cpp for why the bare `_<…>` form never can.
#include <tacit/_.hpp>

#include <algorithm>
#include <any>
#include <cassert>
#include <chrono>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

using tacit::_;

int main() {
  // ---- type-argument free functions, reached by ADL with no extra include in the header ----
  {
    std::any a = 42;
    assert(_.any_cast<int>()(a) == 42);
    std::variant<int, std::string> v{std::string("hi")};
    assert(_.holds_alternative<std::string>()(v));
    assert(!_.holds_alternative<int>()(v));
    auto ms = std::chrono::milliseconds{1500};
    assert(_.duration_cast<std::chrono::seconds>()(ms).count() == 1);
    auto sp = std::static_pointer_cast<void>(std::make_shared<int>(7));
    assert(*_.static_pointer_cast<int>()(sp) == 7);
  }

  // ---- they compose, and they compose FROM a projection too ----
  {
    auto ms = std::chrono::milliseconds{2500};
    assert(_.duration_cast<std::chrono::seconds>().count()(ms) == 2);
    std::vector<std::chrono::milliseconds> v{std::chrono::milliseconds{1500}};
    assert(_.front().duration_cast<std::chrono::seconds>().count()(v) == 1);
  }

  // ---- `ranges::to`: the pipeline terminator, in both kinds of argument ----
  {
    auto r = std::views::iota(1, 4);
    auto v = _.to<std::vector>()(r);              // template argument, element deduced
    static_assert(std::is_same_v<decltype(v), std::vector<int>>);
    assert(v.size() == 3 && v[2] == 3);
    auto w = _.to<std::vector<long>>()(r);        // type argument, spelled out
    static_assert(std::is_same_v<decltype(w), std::vector<long>>);
    // from a projection
    std::vector<std::vector<int>> vv{{1, 2, 3}};
    assert(_.front().to<std::vector>()(vv).size() == 3);
  }

  // ---- the lift mirrors them: `$(x).f<T>() == _.f<T>()(x)` ----
  {
    std::any a = 42;
    assert(tacit::lift(a).any_cast<int>() == _.any_cast<int>()(a));
    auto r = std::views::iota(1, 4);
    assert(tacit::lift(r).to<std::vector>().size() == 3);
  }

  // ---- field-style verbs: a projection of a data member, no parens ----
  {
    std::vector<std::pair<int, std::string>> v{{3, "c"}, {1, "a"}, {2, "b"}};
    std::ranges::sort(v, {}, _.first);
    assert(v[0].first == 1 && v[2].first == 3);
    assert(std::ranges::count_if(v, _.first > 1) == 2);
    assert(_.first(std::pair{7, 'z'}) == 7);
    assert(_.second(std::pair{7, 'z'}) == 'z');
    assert(_.second.size()(v[0]) == 1);           // composes onward like any other verb
    std::map<int, std::string> m{{1, "x"}, {2, "yy"}};
    assert(std::ranges::count_if(m, _.second.size() == 2u) == 1);
  }

  // ---- field style is a reference, so it can be written through ----
  {
    std::pair<int, int> p{1, 2};
    _.first(p) = 9;
    assert(p.first == 9);
  }

  // ---- and its two documented limits: no chaining INTO it, no lift mirror ----
  {
    // `_.front().first` has no member to find (a static data member of `fn` would need `fn`
    // complete inside its own definition); the same access chains as `.get<0>()`
    std::vector<std::pair<int, char>> v{{5, 'q'}};
    assert(_.front().get<0>()(v) == 5);
    // `$(p).first` cannot mirror `_.first` — a data member cannot compute from the subject, so the
    // closed cell spells it `get<0>`. The `$(x).f() == _.f()(x)` rule is about CALLS.
    assert(tacit::lift(std::pair{5, 'q'}).get<0>() == 5);
  }

  return 0;
}
