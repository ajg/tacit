// Proves the default include exports exactly one name: `tacit::_`.
#include <algorithm>
#include <cassert>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <tacit/_.hpp>
#include <vector>

using tacit::_; // the ONLY name imported from tacit

int main() {
  std::vector<int> v{3, 1, 2};
  std::string s = "hello";
  std::optional<int> o{};

  assert(_.size()(v) == 3);
  assert(_.at(1)(v) == 1);
  assert(_.substr(0, 3)(s) == "hel");
  assert(_.value_or(-1)(o) == -1);

  // operator sections are hidden friends, found by ADL — no `using` of operators needed
  assert((_ > 1)(2) == true);
  assert((1 + _)(4) == 5);
  assert((_ + _)(3, 4) == 7);

  // blanks + application
  std::vector<int> w;
  _.push_back(_)(w, 5);
  _.push_back(9)(w);
  assert(w[0] == 5 && w[1] == 9);
  assert(_(2)(std::plus<>{}, 40) == 42);

  // projection into an algorithm
  std::ranges::sort(v, _ < _);
  assert((v == std::vector<int>{1, 2, 3}));

  return 0;
}

// Policy checks (preprocessor): generators gone on the clean path, feature flag kept.
#if defined(TACIT_MEMBER) || defined(TACIT_CORE) || defined(TACIT_STD_MEMBERS) ||                   \
    defined(TACIT_FOR_EACH) || defined(TACIT_LIEUTENANT) || defined(TACIT_VERBS) ||                 \
    defined(TACIT_NOUNS)
#error "a generator macro leaked into the clean include"
#endif
#ifndef TACIT_HAS_REFLECTION
#error "TACIT_HAS_REFLECTION should remain defined on the clean path"
#endif
