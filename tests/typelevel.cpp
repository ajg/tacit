// Type-level tacit: `_` as a type-level hole via the elaborated `_::hole<>`; fixed args are plain
// types.
#include <map>
#include <tacit/_.hpp>
#include <type_traits>
#include <vector>

using tacit::_; // brings the value `_` and (same name) the hole type, so `_::hole<>` works
                // unqualified

static_assert(TACIT_VERSION >= 200, "version macro");
static_assert(std::is_same_v<tacit::bind<std::vector, _::hole<>>::with<int>, std::vector<int>>);
static_assert(std::is_same_v<tacit::bind<std::map, _::hole<>, _::hole<>>::with<char, int>,
                             std::map<char, int>>);
static_assert(
    std::is_same_v<tacit::bind<std::map, int, _::hole<>>::with<double>, std::map<int, double>>);
static_assert(
    std::is_same_v<tacit::bind<std::map, _::hole<>, int>::with<char>, std::map<char, int>>);

int main() { return 0; }
