// The general type-level primitive: `tacit::apply` + `tacit::quote`, ungated, currying BOTH grains
// (fix-template-vary-args AND fix-args-vary-template); plus the experimental natural-spelling std
// blanks under TACIT_STD_BLANKS. `typelevel.cpp` covers plain `bind`; this file covers what subsumes it.
#define TACIT_STD_BLANKS
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

// ---- Tier 1: natural spelling via std blanks (TACIT_STD_BLANKS) --------------------------------------
static_assert(std::is_same_v<std::vector<_::blank<>>::with<int>, std::vector<int>>);
static_assert(std::is_same_v<std::map<_::blank<>, int>::with<char>, std::map<char, int>>);
static_assert(std::is_same_v<std::map<char, _::blank<>>::with<int>, std::map<char, int>>);
static_assert(std::is_same_v<std::map<_::blank<>, _::blank<>>::with<char, int>, std::map<char, int>>);
static_assert(std::is_same_v<std::pair<_::blank<>, int>::with<char>, std::pair<char, int>>);
static_assert(std::is_same_v<std::pair<int, _::blank<>>::with<char>, std::pair<int, char>>);
static_assert(std::is_same_v<std::pair<_::blank<>, _::blank<>>::with<char, int>, std::pair<char, int>>);
static_assert(std::is_same_v<std::set<_::blank<>>::with<int>, std::set<int>>);
// tuple: leading blank, any arity:
static_assert(std::is_same_v<std::tuple<_::blank<>, int, char>::with<double>, std::tuple<double, int, char>>);
// value-parameterized: blank the element type, the extent (an NTTP) rides along as a literal:
static_assert(std::is_same_v<std::array<_::blank<>, 5>::with<int>, std::array<int, 5>>);
static_assert(std::is_same_v<std::span<_::blank<>, 4>::with<int>, std::span<int, 4>>);

int main() { return 0; }
