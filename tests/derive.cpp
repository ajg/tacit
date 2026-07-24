// Derive-your-own placeholder (opt in to keep the generator macros).
#define TACIT_KEEP_MACROS
#include <algorithm>
#include <cassert>
#include <ranges>
#include <tacit/_.hpp>
#include <vector>

struct Account {
  long cents = 0;
  bool frozen = false;
  void deposit(long c) {
    if (!frozen)
      cents += c;
  }
  long balance() const { return cents; }
  bool has_value() const { return cents != 0; } // reuse a std-vocabulary name too
};

// One-liner: defines the `teller` type and the `it` object in a single statement.
namespace bank {
TACIT_LIEUTENANT(teller, it, deposit, balance, has_value);
}

// Manual form: one member per line (or TACIT_MEMBERS(a, b, ...) for a compact list), then the core.
namespace store {
struct clerk {
  TACIT_MEMBER(deposit);
  TACIT_MEMBER(balance);
  TACIT_CORE(clerk);
};
inline constexpr clerk it;
}

int main() {
  using tacit::_;
  std::vector<Account> a{{300}, {100}, {250}};

  std::ranges::sort(a, {}, bank::it.balance()); // domain projection
  assert(a[0].balance() == 100 && a[2].balance() == 300);

  bank::it.deposit(50)(a[0]); // bound-arg closure
  assert(a[0].balance() == 150);

  bank::it.deposit(_)(a[1], 25); // a BLANK in a derived placeholder
  assert(a[1].balance() == 275);

  assert(std::ranges::count_if(a, bank::it.has_value()) == 3);

  std::vector<Account> b{{5}, {2}, {9}};
  std::ranges::sort(b, {}, store::it.balance()); // manual-form placeholder works the same
  assert(b[0].balance() == 2 && b[2].balance() == 9);
  return 0;
}
