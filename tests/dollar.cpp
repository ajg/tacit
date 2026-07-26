// `$` — the term wrapper, behind `TACIT_DOLLAR`. It is `tacit::lift` under a shorter name, so the
// rule is the same: `$(x).f(a...)` == `_.f(a...)(normalize(x))`. What this file pins is that `$` is a
// *function* — namespaced, qualifiable, ADL-obeying — rather than a macro, and that the gate keeps it
// out of a default build entirely.
#define TACIT_DOLLAR
#include <tacit/_.hpp>

#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <vector>

using tacit::_;
using tacit::$;

int main() {
  // ---- the three dispatch kinds, through `$` ----
  {
    std::vector<int> v{1, 2, 3};
    assert($(v).size() == 3);      // range CPO — v.size() would also work, but this routes the CPO
    assert($(v).front() == 1);     // member
    assert($(-42).abs() == 42);    // free function — a bare value has no members at all
    assert($(2.7).floor() == 2.0);
  }

  // ---- normalize: the literal's raw type is never what you mean ----
  {
    assert($("").length() == 0);
    assert($("abc").length() == 3);
    assert($("abc").size() == 3);   // 3, not 4 — the NUL is not counted
    assert($("abc").starts_with("ab"));
  }

  // ---- reaches what a bare value has no members for ----
  {
    int carr[4]{};
    assert($(carr).size() == 4);    // carr.size() does not exist
    assert(!$(carr).empty());
  }

  // ---- lvalues held by reference, so mutation reaches the subject ----
  {
    std::vector<int> v{1, 2};
    $(v).push_back(3);
    assert(v.size() == 3);
    std::string s = "hello";
    assert($(s).substr(1, 3) == "ell");
  }

  // ---- it is a function, not a macro: qualification works, and so does passing it around ----
  {
    assert(tacit::$(-1).abs() == 1);
    assert(tacit::lift(-1).abs() == tacit::$(-1).abs());   // same thing, two spellings
  }

  // ---- the governing rule, and the term world it sits beside ----
  {
    std::vector<int> v{-3, 1, -2};
    assert($(v).size() == _.size()(v));
    assert($(-42).abs() == _.abs()(-42));
    assert(std::ranges::count_if(v, _.abs() > 1) == 2);
    assert((_ < _)(1, 2));
  }

  return 0;
}
