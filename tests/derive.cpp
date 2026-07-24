// Derive-your-own placeholder from TACIT_CORE (opt in to keep the generator macros).
#define TACIT_KEEP_MACROS
#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <tacit/_.hpp>
#include <vector>

namespace bank {
struct Account {
  std::string owner;
  long cents = 0;
  bool frozen = false;
  void deposit(long c) {
    if (!frozen)
      cents += c;
  }
  long balance() const { return cents; }
  Account &freeze() {
    frozen = true;
    return *this;
  }
  bool has_value() const { return cents != 0; } // reuse a std-vocabulary name too
};
struct teller {
  TACIT_MEMBER(deposit)
  TACIT_MEMBER(balance) TACIT_MEMBER(freeze) TACIT_MEMBER(has_value) TACIT_CORE(teller)
};
inline constexpr teller it; // your own object
} // namespace bank

int main() {
  using bank::it;
  using bank::Account;
  using tacit::_;

  std::vector<Account> a{{"a", 300}, {"b", 100}, {"c", 250}};
  std::ranges::sort(a, {}, it.balance()); // domain projection
  assert(a[0].balance() == 100 && a[2].balance() == 300);

  it.deposit(50)(a[0]); // bound-arg closure
  assert(a[0].balance() == 150);

  it.deposit(_)(a[1], 25); // a BLANK in a derived placeholder
  assert(a[1].balance() == 275);

  assert(std::ranges::count_if(a, it.has_value()) == 3);
  return 0;
}
