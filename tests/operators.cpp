// The extended operator surface: unary, logical, shift/stream, bitwise, assignment (incl. compound),
// and `->`. The comparison/arithmetic sections live in blanks/compose; this file covers the additions.
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <tuple>
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
  assert((_ | _)(0b1000, 0b0011) == 0b1011);   // (a, b) -> a | b  (two-input)
  assert((_ << 2)(3) == 12);
  assert((_ >> 1)(8) == 4);

  // ---- logical: two-blank is a two-INPUT combiner (like the comparators); the value form is unary ----
  {
    assert((_ && _)(true, true) == true);    // (a, b) -> a && b
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

  // ---- comma: the tupling section, `_, _` == (a, b) -> {a, b} ----
  {
    auto pr = (_, _)(3, 4);
    assert(pr.first == 3 && pr.second == 4);
    assert((_, 9)(5).second == 9);  // _, value  -> x -> {x, 9}
    assert((7, _)(5).first == 7);   // value, _  -> x -> {7, x}

    // projections are operands like any other — without the fn overload the built-in comma would
    // evaluate and discard the left one, silently yielding just the right
    auto sp = (_.size(), _.front())(std::string("abc"), std::string("xyz"));
    assert(sp.first == 3 && sp.second == 'x');
    assert((_.size(), 9)(std::string("abcd")).first == 4);
    assert((9, _.size())(std::string("abcd")).second == 4);
    assert((_.size(), _)(std::string("ab"), 7).second == 7);  // projected + plain blank

    // three or more operands accumulate into a tuple (no pair to be), one fill per blank
    auto t3 = (_, _, _)(1, 2, 3);
    static_assert(std::tuple_size_v<decltype(t3)> == 3);
    assert(std::get<0>(t3) == 1 && std::get<2>(t3) == 3);
    auto tm = (_, 5, _)(1, 3);  // a bound value mid-list takes no fill
    assert(std::get<0>(tm) == 1 && std::get<1>(tm) == 5 && std::get<2>(tm) == 3);
    auto tp = (_.size(), _, _.front())(std::string("ab"), 9, std::string("zy"));
    assert(std::get<0>(tp) == 2 && std::get<1>(tp) == 9 && std::get<2>(tp) == 'z');

    // the list is flat: parenthesising doesn't nest, it concatenates
    static_assert(std::tuple_size_v<decltype((_, (_, _))(1, 2, 3))> == 3);
    auto cc = ((_, _), (_, _))(1, 2, 3, 4);
    static_assert(std::tuple_size_v<decltype(cc)> == 4);
    assert(std::get<3>(cc) == 4);

    // ordinary tuple-likes: structured bindings, apply, constexpr
    auto [a, b, c] = (_, _, _)(1, 2, 3);
    assert(a + b + c == 6);
    assert(std::apply([](int x, int y, int z) { return x + y + z; }, (_, _, _)(1, 2, 3)) == 6);
    constexpr auto k = (_, _, _)(1, 2, 3);
    static_assert(std::get<1>(k) == 2);

    // a comma section composes onward through the value it builds, keeping its arity
    auto eq = (_, _) == std::pair{1, 2};
    assert(eq(1, 2) && !eq(1, 3));
    auto lt = (_, _) < std::pair{2, 0};  // lexicographic, as pair defines it
    assert(lt(1, 9) && !lt(2, 1));
    assert((std::pair{2, 0} > (_, _))(1, 9));  // and from the left
    auto key = (_.size(), _.front()) == std::pair<std::size_t, char>{2, 'a'};
    assert(key(std::string("xy"), std::string("ab")));
    auto t3cmp = (_, _, _) < std::tuple{1, 2, 4};
    assert(t3cmp(1, 2, 3) && !t3cmp(1, 2, 9));
    assert((!((_, _) == std::pair{1, 2}))(9, 9));  // the result is a plain fn, so it keeps going
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
