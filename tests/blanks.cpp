// Blanks (partial application): arity = number of `_`, filled left to right.
#include <cassert>
#include <concepts>
#include <string>
#include <tacit/_.hpp>
#include <vector>

using tacit::_;

int main() {
  std::vector<int> v{1, 2};

  auto pb5 = _.push_back(5); // no blank -> unary, and SFINAE-guarded
  static_assert(std::invocable<decltype(pb5), std::vector<int> &>);
  pb5(v);
  assert(v.back() == 5);

  auto pb = _.push_back(_); // one blank -> binary (container, value)
  pb(v, 9);
  assert(v.back() == 9);
  static_assert(!std::invocable<decltype(pb), std::vector<int> &>);
  static_assert(std::invocable<decltype(pb), std::vector<int> &, int>);

  // ordering via string::append(count, ch)
  std::string s1 = "[", s2 = "[", s3 = "[";
  _.append(_, 'a')(s1, 3);
  _.append(3, _)(s2, 'z');
  _.append(_, _)(s3, 2, 'q');
  assert(s1 == "[aaa");
  assert(s2 == "[zzz");
  assert(s3 == "[qq");

  assert(_.substr(_)(std::string("abcdef"), 2) == "cdef");
  return 0;
}
