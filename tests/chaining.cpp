// The hybrid: an `fn` carries the std vocabulary (so projections chain — member chaining) and
// counts as a projected blank (so an `fn` in argument position projects the supplied fill).
#include <cassert>
#include <string>
#include <tacit/_.hpp>
#include <vector>

using tacit::_;

struct Acct {
  int a = 0, b = 0;
  void assign(int x, int y) {
    a = x;
    b = y;
  }
};

int main() {
  std::vector<std::string> v{"abc", "de"};

  // member chaining: a projection's result keeps the vocabulary
  assert(_.front().size()(v) == 3);    // size(front(x))
  assert((_.front().size() >= 2u)(v)); // chain, then a composed section

  // projected blanks: an fn in argument position projects the fill
  std::vector<int> c;
  std::string s = "abcd";
  _.push_back(_.size())(c, s); // c.push_back(s.size())
  assert(c.size() == 1 && c[0] == 4);
  _.push_back(_)(c, 7); // a plain blank still works
  assert(c[1] == 7);

  Acct t;
  std::string k = "ab";
  _.assign(_, _.size())(t, 9, k); // t.assign(9, size(k)) — plain blank + projected blank
  assert(t.a == 9 && t.b == 2);
  return 0;
}
