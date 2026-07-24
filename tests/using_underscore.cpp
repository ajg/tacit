// Opt-in global `_`: this TU writes no `using` — the macro brings it in.
#define TACIT_USING_UNDERSCORE
#include <tacit/_.hpp>

#include <cassert>
#include <vector>

int main() {
  std::vector<int> v{1, 2, 3};
  assert(_.size()(v) == 3); // bare `_`, injected by TACIT_USING_UNDERSCORE
  assert((_ > 1)(2) == true);
  return 0;
}
