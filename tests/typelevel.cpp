// Type-level tacit: `_` as a type-level blank via the elaborated `_::blank<>`; fixed args are plain
// types.
#include <map>
#include <tacit/_.hpp>
#include <type_traits>
#include <vector>

using tacit::_; // brings the value `_` and (same name) the blank type, so `_::blank<>` works
                // unqualified

static_assert(TACIT_VERSION >= 200, "version macro");
static_assert(std::is_same_v<tacit::bind<std::vector, _::blank<>>::with<int>, std::vector<int>>);
static_assert(std::is_same_v<tacit::bind<std::map, _::blank<>, _::blank<>>::with<char, int>, std::map<char, int>>);
static_assert(std::is_same_v<tacit::bind<std::map, int, _::blank<>>::with<double>, std::map<int, double>>);
static_assert(std::is_same_v<tacit::bind<std::map, _::blank<>, int>::with<char>, std::map<char, int>>);

int main() { return 0; }
