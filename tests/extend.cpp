// Extend the built-in `_` with domain names, without deriving a new placeholder.
#define TACIT_EXTRA_MEMBERS(X) X(area) X(perimeter)
#include <algorithm>
#include <cassert>
#include <ranges>
#include <tacit/_.hpp>
#include <vector>
using tacit::_;

struct Box {
  double w, h;
  double area() const { return w * h; }
  double perimeter() const { return 2 * (w + h); }
};

int main() {
  std::vector<Box> v{{2, 3}, {1, 1}, {4, 4}};
  std::ranges::sort(v, {}, _.area()); // _.area() is now first-class on the default _
  assert(v[0].area() == 1);
  assert(_.perimeter()(Box{1, 1}) == 4.0);
  assert(_.size()(v) == 3); // std vocabulary still intact
  return 0;
}
