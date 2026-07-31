// Heterogeneous element combinators driven by `_`-closures over tuple-likes.
// The *_element combinators are opt-in — request them before including the header.
#include <array>
#include <cassert>
#include <deque>
#include <string>
#include <tacit/_.hpp>
#include <tuple>
#include <vector>

using tacit::_;

int main() {
  auto t = std::tuple{std::vector<int>{1, 2, 3}, std::string("ab"), std::array<int, 4>{}};

  // map a heterogeneous tuple through _.size()  ->  tuple of sizes
  auto sizes = tacit::transform_elements(t, _.size());
  assert(std::get<0>(sizes) == 3 && std::get<1>(sizes) == 2 && std::get<2>(sizes) == 4);

  // predicates, short-circuiting
  assert(tacit::any_of_element(t, _.empty()) == false);
  assert(tacit::none_of_element(t, _.empty()) == true);
  auto empties = std::tuple{std::vector<int>{}, std::string("")};
  assert(tacit::all_of_element(empties, _.empty()) == true);

  // for_each with side effects across DIFFERENT container types sharing one op
  auto cs = std::tuple{std::vector<int>{}, std::deque<int>{}};
  tacit::for_each_element(cs, _.push_back(7));
  assert(std::get<0>(cs).at(0) == 7 && std::get<1>(cs).at(0) == 7);
  return 0;
}
