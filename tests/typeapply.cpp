// The general type-level primitive: `tacit::apply` + `tacit::quote`, ungated, currying BOTH grains
// (fix-template-vary-args AND fix-args-vary-template); plus the experimental natural-spelling std
// holes under TACIT_STD_HOLES. `typelevel.cpp` covers plain `bind`; this file covers what subsumes it.
#define TACIT_STD_HOLES
#include <tacit/_.hpp>

#include <map>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using tacit::_; // the value `_` and (same name) the hole type, so `struct _` works unqualified
using tacit::apply;
using tacit::bind;
using tacit::quote;

// ---- Tier 2: the unified primitive, both grains ---------------------------------------------------
// fix the template, vary the args:
static_assert(std::is_same_v<apply<quote<std::map>, struct _, struct _>::with<int, char>,
                             std::map<int, char>>);
static_assert(std::is_same_v<apply<quote<std::map>, int, struct _>::with<char>, std::map<int, char>>);
static_assert(std::is_same_v<apply<quote<std::vector>, struct _>::with<int>, std::vector<int>>);
// fix the args, vary the template (the grain plain `bind` cannot spell):
static_assert(std::is_same_v<apply<struct _, int, char>::with<quote<std::map>>, std::map<int, char>>);
static_assert(
    std::is_same_v<apply<struct _, struct _>::with<quote<std::vector>, int>, std::vector<int>>);
// hole in both grains at once:
static_assert(std::is_same_v<apply<struct _, int, struct _>::with<quote<std::map>, char>,
                             std::map<int, char>>);
// apply subsumes bind: same result, template pinned via quote:
static_assert(std::is_same_v<apply<quote<std::map>, struct _, struct _>::with<char, int>,
                             bind<std::map, struct _, struct _>::with<char, int>>);

// ---- Tier 1: natural spelling via std holes (TACIT_STD_HOLES) --------------------------------------
static_assert(std::is_same_v<std::vector<struct _>::with<int>, std::vector<int>>);
static_assert(std::is_same_v<std::map<struct _, int>::with<char>, std::map<char, int>>);
static_assert(std::is_same_v<std::map<char, struct _>::with<int>, std::map<char, int>>);
static_assert(std::is_same_v<std::map<struct _, struct _>::with<char, int>, std::map<char, int>>);
static_assert(std::is_same_v<std::pair<struct _, int>::with<char>, std::pair<char, int>>);
static_assert(std::is_same_v<std::pair<int, struct _>::with<char>, std::pair<int, char>>);
static_assert(std::is_same_v<std::pair<struct _, struct _>::with<char, int>, std::pair<char, int>>);
static_assert(std::is_same_v<std::set<struct _>::with<int>, std::set<int>>);
// tuple: leading hole, any arity:
static_assert(
    std::is_same_v<std::tuple<struct _, int, char>::with<double>, std::tuple<double, int, char>>);

int main() { return 0; }
