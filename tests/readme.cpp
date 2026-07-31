// Compile-and-run check of the code examples in README.md, section by section.
// Keep in sync with the README: every code block that claims to be valid C++ is exercised here.
#define TACIT_COMBINATORS
#define TACIT_VERBS make_deposit, balance, is_frozen
#include <tacit/$.hpp>
#include <tacit/_.hpp>
#include <tacit/λ.hpp>

#include <any>
#include <cassert>
#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

using tacit::_;
using tacit::lift;
using tacit::make;
using tacit::$;

struct Account {
  int bal = 0;
  bool frozen = false;
  void make_deposit(int a) { bal += a; }
  int balance() const { return bal; }
  bool is_frozen() const { return frozen; }
};

int main() {
  // --- Intro ---
  std::vector<int> nums{3, 1, 0, 2};
  std::ranges::sort(nums, _ < _);
  assert(std::ranges::count_if(nums, _ == 0) == 1);
  std::vector<std::string> words{"abc", "de"};
  std::vector<std::size_t> sizes(words.size());
  std::ranges::transform(words, sizes.begin(), _.size());
  assert(sizes[0] == 3);

  // --- Blanks ---
  std::vector<int> c;
  _.push_back(7)(c);
  _.push_back(_)(c, 8);
  assert(c.size() == 2 && c[1] == 8);
  assert((_ + _)(2, 3) == 5);
  assert(_[_](std::vector<int>{7, 8}, 1) == 8);
  assert(_(_)([](int x) { return x + 1; }, 4) == 5);
  _.push_back(_.size())(c, words);            // projected blank
  assert(c.back() == 2);
  assert((_.size() < _)(words, std::size_t{3}));
  assert(_.substr(_)(std::string("abcd"), std::size_t{2}) == "cd");

  // --- Vocabulary: get ---
  std::vector<std::tuple<int, std::string>> v{{2, "bb"}, {1, "a"}};
  std::ranges::sort(v, {}, _.get<0>());
  assert(std::get<0>(v[0]) == 1);
  assert(std::ranges::count_if(v, _.get<0>() > 1) == 1);
  assert(_.get<std::string>()(std::tuple<int, std::string>{1, "x"}) == "x");
  assert(_.get<1>().size()(std::tuple<int, std::string>{1, "xy"}) == 2);
  assert(_.get()(std::make_shared<int>(5)) != nullptr);   // plain get untouched

  // --- Vocabulary: type-argument family ---
#if TACIT_HAS_RANGES_TO
  assert((_.to<std::vector>()(std::views::iota(0, 3))) == (std::vector{0, 1, 2}));
  assert((_.to<std::vector<long>>()(std::views::iota(0, 3))) == (std::vector<long>{0, 1, 2}));
#endif
  std::any a = 5;
  assert(_.any_cast<int>()(a) == 5);
  std::variant<int, std::string> var = std::string("x");
  assert(_.holds_alternative<std::string>()(var));
  assert(_.duration_cast<std::chrono::seconds>()(std::chrono::milliseconds(2000)).count() == 2);
  struct Base { virtual ~Base() = default; };
  struct Derived : Base {};
  std::shared_ptr<Base> bp = std::make_shared<Derived>();
  assert(_.static_pointer_cast<Derived>()(bp) != nullptr);

  // --- Vocabulary: field-style verbs ---
  std::vector<std::pair<int, std::string>> pv{{2, "bb"}, {1, "a"}};
  std::ranges::sort(pv, {}, _.first);
  assert(pv[0].first == 1);
  assert(std::ranges::count_if(pv, _.second.size() == 2u) == 1);
  std::pair<int, int> p{1, 2};
  _.first(p) = 9;
  assert(p.first == 9);

  // --- Operator sections ---
  assert((-_)(3) == -3);
  assert((!_)(false));
  assert((~_)(0) == -1);
  assert((*_)(std::make_shared<int>(6)) == 6);
  auto sp = std::make_shared<std::vector<int>>(3);
  assert(_->size()(sp) == 3);
  std::ostringstream os;
  std::ranges::for_each(std::vector{1, 2}, os << _);
  assert(os.str() == "12");
  struct Widget { int x; };
  Widget widget{5};
  assert((_ ->* &Widget::x)(&widget) == 5);                        // member-pointer projection
  assert((_ ->* &Widget::x)(std::make_shared<Widget>(widget)) == 5); // through a smart pointer too

  assert((0 < _ < 10)(5));
  assert(!(0 < _ < 10)(10));
  assert((1u <= _.size() < 4u)(std::string("ab")));
  assert((0 <= _ <= 10 < 20)(10));
  assert(!((_ < 10) == false)(5));            // documented gotcha: chains, not negation
  assert((_ < _)(1, 2));                      // two blanks: comparator, not a link

  std::vector<int> mv{1, 2, 3};
  std::ranges::for_each(mv, _ += 1);
  assert(mv[0] == 2);
  std::ranges::for_each(mv, _ = 0);
  assert(mv[2] == 0);
  assert((_ | 4)(1) == 5);

  // --- Comma sections ---
  assert((_, _)(1, 2) == (std::pair{1, 2}));
  assert((_, 9)(1) == (std::pair{1, 9}));
  assert((_, _, _)(1, 2, 3) == (std::tuple{1, 2, 3}));
  assert((_.size(), _.front())(std::string("abc"), std::string("xy")) == (std::pair{std::size_t{3}, 'x'}));
  assert((_, 5, _)(1, 2) == (std::tuple{1, 5, 2}));
  assert(((_, _) == std::pair{1, 2})(1, 2));
  assert(((_, _) < std::pair{2, 0})(1, 99));
  assert(tacit::fanout(_.size(), _.front())(std::string("abc")) == (std::tuple{std::size_t{3}, 'a'}));

  // --- Composition ---
  std::vector<std::string> ws{"bb", "a", "ccc"};
  assert(std::ranges::count_if(ws, _.size() >= 2u) == 2);
  std::ranges::sort(ws, _.size() < _.size());
  assert(ws[0] == "a");
  auto scaled = (_ + 1) * 2;
  assert(scaled(3) == 8);
  auto head = _[0];
  assert(head(std::vector<int>{4, 5}) == 4);
  assert(((_ + _) + 1)(1, 2) == 4);
  std::list<int> lst{3, 1, 2};
  _.sort(_ < _)(lst);                         // many-fill closure passes as bound comparator
  assert(lst.front() == 1);
  assert(_.front().size()(ws) == 1);          // ws[0] == "a": size(front(x))
  assert(tacit::compose(_ + 1, _ * 2)(3) == 8);

  // --- Application ---
  assert(_(3)(std::negate{}) == -3);
  int hits = 0;
  std::vector<std::function<void()>> thunks{[&] { ++hits; }};
  std::ranges::for_each(thunks, _());
  assert(hits == 1);

  // --- lift ---
  assert(lift(std::vector{1, 2}).size() == 2);
  assert(lift(-42).abs() == 42);
  assert(lift("abc").length() == 3);
  assert((_.abs() > 1)(-5));

  // --- make ---
  auto m1 = make<std::vector>(1, 2, 3);
  static_assert(std::is_same_v<decltype(m1), std::vector<int>>);
  auto m2 = make<std::vector, double>(1.0, 2.0);
  static_assert(std::is_same_v<decltype(m2), std::vector<double>>);
  auto m3 = make<std::set, _, std::greater<>>(3, 1, 2);
  assert(*m3.begin() == 3);
  auto m4 = make<std::map, _, _, std::greater<>>(std::pair{1, 'a'}, std::pair{2, 'b'});
  assert(m4.begin()->first == 2);

  // --- Closures as types ---
  std::set<int, decltype(_ > _)> s{3, 1, 2};
  assert(*s.begin() == 3);
  static_assert(sizeof(std::set<int, decltype(_ > _)>) == sizeof(std::set<int>));

  // --- $ ---
  assert($(-42).abs() == 42);
  assert($("abc").length() == 3);
  assert($(std::vector{1, 2}).size() == 2);
  assert($(sp)->size() == 3);
  assert($(sp).use_count() >= 1);
  assert(($<std::vector>(1, 2, 3)) == (std::vector{1, 2, 3}));
  assert(*($<std::set, _, std::greater<>>(3, 1, 2)).begin() == 3);

  // --- λ ---
  {
    std::vector<std::string> lv{"ccc", "a", "bb"};
    std::ranges::sort(lv, λ(a, b) { return a.size() < b.size(); });
    assert(lv[0] == "a");
    assert(std::ranges::count_if(lv, λ(s) { return s.size() * s.size() > 4u; }) == 1);
    assert(\u{3BB}(s) { return s.front(); }(lv[1]) == lv[1].front()); // ASCII spelling, same macro
    λ(s) -> decltype(auto) { return s.front(); }(lv[0]) = 'z';
    assert(lv[0] == "z");
  }

  // --- TACIT_VERBS ---
  Account account;
  _.make_deposit(_)(account, 100);
  assert(account.bal == 100);
  std::vector<Account> accounts{{200}, {100}};
  std::ranges::sort(accounts, {}, _.balance());
  assert(accounts[0].bal == 100);
  assert(std::ranges::count_if(accounts, _.is_frozen()) == 0);
  assert((_.balance() < _.balance())(accounts[0], accounts[1]));

  return 0;
}
