// The extended operator surface: unary, logical, shift/stream, bitwise, assignment (incl. compound),
// and `->`. The comparison/arithmetic sections live in blanks/compose; this file covers the additions.
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

using tacit::_;

int main() {
  // ---- unary ----
  assert((-_)(5) == -5);
  assert((+_)(5) == 5);
  assert((!_)(false) == true);
  assert((~_)(0) == ~0);
  {
    auto p = std::make_shared<int>(42);
    assert((*_)(p) == 42);        // deref
    assert(((*_) + 1)(p) == 43);  // deref composes onward
  }
  {
    int a = 5;
    (++_)(a);
    assert(a == 6);  // prefix ++, mutating
    int b = 5;
    (_++)(b);
    assert(b == 6);  // postfix ++
    int c = 7;
    assert((&_)(c) == &c);  // address-of
  }

  // ---- bitwise & | / shift (| is an ordinary section now, symmetric with &) ----
  assert((_ & 0b0110)(0b1100) == 0b0100);
  assert((_ | 0b0110)(0b1000) == 0b1110);      // x -> x | 0b0110
  assert((_ | _)(0b1000, 0b0011) == 0b1011);   // (a,b) -> a | b  (two-input)
  assert((_ << 2)(3) == 12);
  assert((_ >> 1)(8) == 4);

  // ---- logical: two-blank is a two-INPUT combiner (like the comparators); the value form is unary ----
  {
    assert((_ && _)(true, true) == true);    // (a,b) -> a && b
    assert((_ && _)(true, false) == false);
    assert((_ || _)(false, false) == false);
    assert((_ && true)(true) == true);       // x -> x && true  (unary section)
    assert((_ && true)(false) == false);
  }

  // ---- streaming (non-copyable left operand binds by reference) ----
  {
    std::ostringstream os;
    std::vector<int> v{7, 8, 9};
    std::ranges::for_each(v, os << _);
    assert(os.str() == "789");
  }

  // ---- assignment + compound (mutating, by reference) ----
  {
    int a = 5;
    (_ = 9)(a);
    assert(a == 9);
    std::vector<int> v{1, 2, 3};
    std::ranges::for_each(v, _ += 10);
    assert((v == std::vector<int>{11, 12, 13}));
    std::ranges::for_each(v, _ *= 2);
    assert((v == std::vector<int>{22, 24, 26}));
    int m = 1;
    (_ |= 4)(m);  // compound of the bitwise | section
    assert(m == 5);
  }

  // ---- operator-> : member of the pointee via the real arrow ----
  {
    auto p = std::make_shared<std::vector<int>>(std::vector<int>{1, 2, 3});
    assert(_->size()(p) == 3);
    assert(_->at(1)(p) == 2);
    std::vector<std::shared_ptr<std::string>> v{std::make_shared<std::string>("bb"),
                                                std::make_shared<std::string>("a")};
    std::ranges::sort(v, {}, _->size());  // order by pointee length
    assert(*v[0] == "a");
    assert(_.get()(p) == p.get());  // the smart pointer's own member still via dot
  }

  return 0;
}
