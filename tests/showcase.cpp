// Mirrors the README quick-start so the front-page examples can't silently rot:
// if any line here stops compiling, CI (and this test) fail.
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

using tacit::_;

int main() {
  std::vector<int> nums{3, 0, 1, 0, 2};
  std::vector<std::string> words{"aa", "b", "cccc"};
  std::vector<std::size_t> lens;

  std::ranges::sort(nums, _ < _); // ascending (two-blank comparator)
  assert(std::ranges::is_sorted(nums));
  assert(std::ranges::count_if(nums, _ == 0) == 2);                  // count zeros
  std::ranges::transform(words, std::back_inserter(lens), _.size()); // string lengths
  assert((lens == std::vector<std::size_t>{2, 1, 4}));

  int kept = 0;
  for (int x : nums | std::views::filter(_ != 0) | std::views::take(2)) {
    (void)x;
    ++kept;
  }
  assert(kept == 2);
  return 0;
}
