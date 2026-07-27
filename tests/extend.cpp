// Teach the one `_` domain verbs in place — no separate placeholder object.
#define TACIT_VERBS area, perimeter
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
  Box b{4, 4};
  assert(_->area()(&b) == 16.0); // the same verb reaches the arrow proxy (_->area())
  return 0;
}
