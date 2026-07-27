// `_.get<0>()` and `_.get<int>()` — tuple-like projection, the one std vocabulary name spelled with a
// TEMPLATE argument rather than a call argument.
//
// It is also the only place `<…>` is reachable from `_` at all. A template-argument list after a bare
// name demands that the name BE a template, which `_` can never be (it is a variable, and only a
// class-head name may share a name with one). After a `.`, though, the list binds to a member function
// template — so `_.get<0>()` is legal where `_<0>` is not, and the two kinds of argument, `<0>` and
// `<int>`, coexist because member function templates can be overloaded on parameter kind.
#include <tacit/_.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using tacit::_;

// a type that spells `get` as a MEMBER rather than a free function — the second dispatch route
template <class T>
concept member_get = requires(T t) { t.template get<0>(); };

struct Pt {
  int x, y;
  template <std::size_t I> constexpr int get() const { return I == 0 ? x : y; }
};

int main() {
  // ---- by index and by type, over the tuple-likes ----
  {
    auto t = std::tuple{1, std::string("ab"), 2.5};
    assert(_.get<0>()(t) == 1);
    assert(_.get<1>()(t) == "ab");
    assert(_.get<2>()(t) == 2.5);
    assert(_.get<std::string>()(t) == "ab"); // by type, since it appears exactly once
    std::pair p{7, 'z'};
    assert(_.get<0>()(p) == 7);
    assert(_.get<1>()(p) == 'z');
    std::array a{4, 5, 6};
    assert(_.get<1>()(a) == 5);
    std::variant<int, std::string> v{std::string("hi")};
    assert(_.get<std::string>()(v) == "hi");
    assert(_.get<1>()(v) == "hi");
  }

  // ---- none of those have a member `get`: the free `get<…>(x)` is the real route ----
  {
    static_assert(!member_get<std::tuple<int>>);
    static_assert(member_get<Pt>);
    assert(_.get<0>()(Pt{3, 4}) == 3); // ...but a member `get` is used when that is how it is spelled
    assert(_.get<1>()(Pt{3, 4}) == 4);
  }

  // ---- it returns an `fn`, so it composes like any other verb ----
  {
    auto t = std::tuple{1, std::string("abc")};
    assert(_.get<1>().size()(t) == 3);
    assert((_.get<0>() + 1)(t) == 2);
    assert((_.get<0>() < 5)(t));
  }

  // ---- the payoff: projecting a tuple-like inside an algorithm ----
  {
    std::vector<std::tuple<int, std::string>> v{{3, "c"}, {1, "a"}, {2, "b"}};
    std::ranges::sort(v, {}, _.get<0>());
    assert(std::get<0>(v[0]) == 1 && std::get<0>(v[2]) == 3);
    assert(std::ranges::count_if(v, _.get<0>() > 1) == 2);
    std::ranges::sort(v, _ > _, _.get<1>()); // by the string, descending
    assert(std::get<1>(v[0]) == "c");

    std::map<int, std::string> m{{1, "x"}, {2, "y"}};
    assert(std::ranges::count_if(m, _.get<1>() == std::string("y")) == 1);
  }

  // ---- the plain vocabulary `get` is untouched: no ambiguity between the two ----
  {
    auto sp = std::make_shared<int>(5);
    assert(_.get()(sp) == sp.get());
  }

  // ---- and the same names on the lift, so `$(x).f() == _.f()(x)` holds for this verb too ----
  {
    auto t = std::tuple{1, std::string("ab")};
    assert(tacit::lift(t).get<0>() == 1);
    assert(tacit::lift(t).get<std::string>() == "ab");
    assert(tacit::lift(t).get<0>() == _.get<0>()(t));
    auto sp = std::make_shared<int>(5);
    assert(tacit::lift(sp).get() == sp.get());
  }

  return 0;
}
