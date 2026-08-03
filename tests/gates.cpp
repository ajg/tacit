// Every opt-in at once: TACIT_SIGILS + TACIT_VIEWS + TACIT_VERBS + TACIT_NOUNS,
// through <tacit/$.hpp>, with the std blanks and λ riding along. The individual gates each have
// their own test; what this file pins is that they COMPOSE — no gate's machinery (sigil markers,
// view verbs, taught names) breaks another's.
#define TACIT_SIGILS
#define TACIT_VIEWS
#define TACIT_VERBS balance
#define TACIT_NOUNS shape
#include <tacit/$.hpp>
#include <tacit/_.hpp>
#include <tacit/experimental/std_blanks.hpp>
#include <tacit/λ.hpp>

#include <cassert>
#include <ranges>
#include <type_traits>
#include <vector>

using tacit::_;
using tacit::$;

struct Account {
  using shape = int;
  int balance() const { return 7; }
};

int main() {
  assert(_.balance()(Account{}) == 7);                            // taught verb
  static_assert(std::is_same_v<_::shape::of<Account>, int>);      // taught noun
  assert((_.size() &&& _.front())(std::vector{1, 2}).second == 1); // sigil fanout
  assert((_.front() >>* (_ * 2))(std::vector{3, 4}) == 6);        // sigil compose
  assert(tacit::compose(_ + 1, _ * 2)(3) == 8);                   // named combinator
  std::vector<int> v{1, 2, 3};
  auto piped = _.filter(_ > 1).take(1)(v);                        // view verbs chain
  assert(*std::ranges::begin(piped) == 2);
  static_assert(std::is_same_v<std::vector<_::blank<>>::with<int>, std::vector<int>>); // std blank
  assert($(-1).abs() == 1);                                       // $, same TU
  assert(λ(x) { return x + 1; }(41) == 42);                       // λ, same TU
  return 0;
}
