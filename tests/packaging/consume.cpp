// Proves a downstream project can consume tacit through add_subdirectory / FetchContent / CPM and use
// the value-level surface. Kept deliberately tiny — this checks packaging, not the library.
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

using tacit::_;

int main() {
  std::vector<int> v{3, 1, 2};
  std::ranges::sort(v, _ < _);
  assert(std::ranges::is_sorted(v));
  assert(std::ranges::count_if(v, _ > 1) == 2);
  return 0;
}
