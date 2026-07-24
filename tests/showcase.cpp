// Mirrors the README quick-start so the front-page examples can't silently rot:
// if any line here stops compiling, CI (and this test) fail.
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

using tacit::_;
using namespace std::ranges;
using std::vector, std::string;

int main() {
  vector<int> nums{3, 0, 1, 0, 2};
  vector<string> words{"aa", "b", "cccc"};
  vector<std::size_t> lens;

  sort(nums, _ < _); // ascending (two-blank comparator)
  assert(is_sorted(nums));
  assert(count_if(nums, _ == 0) == 2);                  // count zeros
  transform(words, std::back_inserter(lens), _.size()); // string lengths
  assert((lens == vector<std::size_t>{2, 1, 4}));

  int kept = 0;
  for (int x : nums | views::filter(_ != 0) | views::take(2)) {
    (void)x;
    ++kept;
  }
  assert(kept == 2);

  // application: _(args) applies the subject to args; _() invokes it
  assert(_(3)(std::negate{}) == -3);
  int calls = 0;
  vector<std::function<void()>> thunks{[&] { ++calls; }, [&] { ++calls; }};
  for_each(thunks, _());
  assert(calls == 2);
  return 0;
}
