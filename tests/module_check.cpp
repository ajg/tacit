// Verifies `import tacit;` end to end — the vocabulary, hidden-friend sections, composition, and the
// tuple combinators all crossing the module boundary. Built only by the clang `modules` CI job (it
// needs a module build, so it is not part of the CMake/ctest suite).
import tacit;

#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <tuple>
#include <vector>

using tacit::_;

int main() {
  std::vector<int> v{3, 1, 2};
  std::ranges::sort(v, _ < _);                 // hidden-friend section across the module
  assert(std::ranges::is_sorted(v));
  assert((_.size() >= 2u)(v));                 // composition (fn hidden friends) across the module
  auto t = std::tuple{std::vector<int>{1, 2}, std::string("xyz")};
  assert(tacit::all_of_element(t, _.size() >= 1u));
  return 0;
}
