// The general type-level primitive: `tacit::apply` + `tacit::quote`, ungated, currying BOTH grains
// (fix-template-vary-args AND fix-args-vary-template). `typelevel.cpp` covers plain `bind`; this
// file covers what subsumes it.
#include <tacit/_.hpp>

#include <array>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using tacit::_; // the value `_` and (same name) the blank type, so `_::blank<>` works unqualified
using tacit::apply;
using tacit::bind;
using tacit::quote;

// ---- Tier 2: the unified primitive, both grains ---------------------------------------------------
// fix the template, vary the args:
static_assert(std::is_same_v<apply<quote<std::map>, _::blank<>, _::blank<>>::with<int, char>, std::map<int, char>>);
static_assert(std::is_same_v<apply<quote<std::map>, int, _::blank<>>::with<char>, std::map<int, char>>);
static_assert(std::is_same_v<apply<quote<std::vector>, _::blank<>>::with<int>, std::vector<int>>);
// fix the args, vary the template (the grain plain `bind` cannot spell):
static_assert(std::is_same_v<apply<_::blank<>, int, char>::with<quote<std::map>>, std::map<int, char>>);
static_assert(std::is_same_v<apply<_::blank<>, _::blank<>>::with<quote<std::vector>, int>, std::vector<int>>);
// blank in both grains at once:
static_assert(std::is_same_v<apply<_::blank<>, int, _::blank<>>::with<quote<std::map>, char>, std::map<int, char>>);
// apply subsumes bind: same result, template pinned via quote:
static_assert(std::is_same_v<apply<quote<std::map>, _::blank<>, _::blank<>>::with<char, int>,
                             bind<std::map, _::blank<>, _::blank<>>::with<char, int>>);

// ---- the shapes the natural spelling used to cover, in the portable grain ----------------------
// `std::map<struct _, int>::with<char>` needed a specialization of std::map for the blank type, and
// that is [namespace.std] deviancy — it is gone. Every case it reached is reachable here, at the
// cost of naming the template through `quote`:
static_assert(std::is_same_v<apply<quote<std::pair>, _::blank<>, int>::with<char>, std::pair<char, int>>);
static_assert(std::is_same_v<apply<quote<std::set>, _::blank<>>::with<int>, std::set<int>>);
static_assert(std::is_same_v<bind<std::tuple, _::blank<>, int, char>::with<double>, std::tuple<double, int, char>>);

// Value-parameterized templates (`std::array<class, size_t>`, `std::span`) are the one thing the
// natural spelling did that this cannot: a template-template parameter is `template <class...>`, so
// a mixed <class, size_t> head has no slot here. Spell those with the type directly — see ASKS.

int main() { return 0; }
