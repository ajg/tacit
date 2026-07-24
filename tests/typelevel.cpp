// Type-level tacit: `_` as a type-level hole via the elaborated `struct _`; fixed args are plain
// types.
#include <map>
#include <tacit/_.hpp>
#include <type_traits>
#include <vector>

using tacit::_; // brings the value `_` and (same name) the hole type, so `struct _` works
                // unqualified

static_assert(TACIT_VERSION >= 200, "version macro");
static_assert(std::is_same_v<tacit::bind<std::vector, struct _>::with<int>, std::vector<int>>);
static_assert(std::is_same_v<tacit::bind<std::map, struct _, struct _>::with<char, int>,
                             std::map<char, int>>);
static_assert(
    std::is_same_v<tacit::bind<std::map, int, struct _>::with<double>, std::map<int, double>>);
static_assert(
    std::is_same_v<tacit::bind<std::map, struct _, int>::with<char>, std::map<char, int>>);

int main() { return 0; }
