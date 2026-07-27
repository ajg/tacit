// Verifies `import tacit;` end to end — the vocabulary, hidden-friend sections, composition, and the
// type-level `bind` all crossing the module boundary. Built only by the clang `modules` CI job (it
// needs a module build, so it is not part of the CMake/ctest suite). The module's default surface is
// `_` + `bind`; the opt-in combinators would need the interface built with -DTACIT_COMBINATORS.
import tacit;

#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

using tacit::_;

int main() {
  std::vector<int> v{3, 1, 2};
  std::ranges::sort(v, _ < _); // hidden-friend section across the module
  assert(std::ranges::is_sorted(v));
  assert((_.size() >= 2u)(v)); // composition (fn hidden friends) across the module
  static_assert(               // type-level bind re-exported across the module
      std::is_same_v<tacit::bind<std::vector, _::blank<>>::with<int>, std::vector<int>>);
  static_assert( // type-level projection reached through the same `_`
      std::is_same_v<tacit::_::value_type::of<std::vector<int>>, int>);
  return 0;
}
