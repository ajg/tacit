// The vocabulary *file* hook: richer than the TACIT_VERBS comma list because each entry chooses its
// dispatch kind — member call, free function, customization point, or nested type. The file is
// expanded once per surface (`_`, projections, `->`, comma sections, the lift), X-macro style.
#include <string>
#include <vector>

namespace bank {
struct account {
  using money_type = long;
  money_type cents = 0;
  void deposit(money_type n) { cents += n; }
  money_type balance() const { return cents; }
};
// free function and CPO-ish: an `account` has no members for these
inline int risk(account const &a) { return a.cents < 0 ? 2 : 0; }
inline int tier(account const &a) { return a.cents >= 1000 ? 1 : 0; }
} // namespace bank

#define TACIT_VOCABULARY "vocab/bank_vocabulary.hpp"
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>

using tacit::_;

int main() {
  bank::account a{500};

  // verbs: member calls, on `_` and on projections alike
  _.deposit(250)(a);
  assert(a.balance() == 750);
  assert(_.balance()(a) == 750);

  // free function and customization point — the thing a bare verb list cannot express
  assert(_.risk()(a) == 0);
  assert(_.tier()(a) == 0);
  _.deposit(500)(a);
  assert(_.tier()(a) == 1);

  // the noun, at type level
  static_assert(std::is_same_v<_::money_type::of<bank::account>, long>);

  // it reaches every surface: sections, projections, algorithms, and the lift
  std::vector<bank::account> v{{100}, {5000}, {-20}};
  assert(std::ranges::count_if(v, _.tier() == 1) == 1);
  assert(std::ranges::count_if(v, _.risk() > 0) == 1);
  assert(std::ranges::count_if(v, _.balance() > 50) == 2);
  assert(tacit::lift(v[1]).balance() == 5000); // the lift surface
  assert(tacit::lift(v[1]).tier() == 1);       // ...including the free/CPO entries

  // and composes like any other vocabulary
  assert((_.balance() + 1)(a) == 1251);
  return 0;
}
