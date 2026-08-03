// `$` — the canonical short name of the term wrapper, in its own header: including <tacit/$.hpp> is
// the opt-in (no macro), and `<tacit/_.hpp>` alone never sees the identifier. The rule is
// `$(x).f(a...)` == `_.f(a...)(normalize(x))` == `lift(x).f(a...)`. What this file pins is that `$`
// is a *function* — namespaced, qualifiable, ADL-obeying — rather than a macro.
#include <tacit/$.hpp>
#include <tacit/_.hpp> // `_` is used below, so it is included: never lean on $.hpp's own include

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <ranges>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

using tacit::$;
using tacit::_;

int main() {
  // ---- the three dispatch kinds, through `$` ----
  {
    std::vector<int> v{1, 2, 3};
    assert($(v).size() == 3);   // range CPO — v.size() would also work, but this routes the CPO
    assert($(v).front() == 1);  // member
    assert($(-42).abs() == 42); // free function — a bare value has no members at all
    assert($(2.7).floor() == 2.0);
  }

  // ---- normalize: the literal's raw type is never what you mean ----
  {
    assert($("").length() == 0);
    assert($("abc").length() == 3);
    assert($("abc").size() == 3); // 3, not 4 — the NUL is not counted
    assert($("abc").starts_with("ab"));
  }

  // ---- reaches what a bare value has no members for ----
  {
    int carr[4]{};
    assert($(carr).size() == 4); // carr.size() does not exist
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

  // ---- the arrow surface: for a value with no useful members of its own ----
  {
    auto p = std::make_shared<std::vector<int>>(std::vector<int>{1, 2, 3});
    assert($(p)->size() == 3); // the POINTEE's member, via the real operator->
    assert($(p)->at(1) == 2);
    assert(!$(p)->empty());
    $(p)->push_back(4);
    assert(p->size() == 4);               // reaches the subject
    assert($(p)->size() == _->size()(p)); // mirrors `_->` exactly
    // while the dot surface routes the vocabulary at the holder: `get` is shared_ptr's own verb
    assert($(p).get() == p.get());
    assert($(p).use_count() == 1);
    assert($(p).subject() == p); // the escape hatch, named out of the vocabulary's way
    auto s = std::make_unique<std::string>("hello");
    assert($(s)->length() == 5);
    assert($(s)->substr(1, 3) == "ell");
  }

  // ---- it is a function, not a macro: qualification works, and so does passing it around ----
  {
    assert(tacit::$(-1).abs() == 1);
    assert(tacit::lift(-1).abs() == tacit::$(-1).abs()); // same thing, two spellings
  }

  // ---- and because it is a function TEMPLATE, `$<F>(a...)` is `make<F>(a...)`: the other closed
  // cell. `$(x)` adopts a value that exists; `$<F>(...)` builds one. They cannot collide — a call
  // with no explicit template arguments cannot deduce `F`, so it only ever reaches the lift.
  {
    auto v = $<std::vector>(1, 2, 3); // plain CTAD
    static_assert(std::is_same_v<decltype(v), std::vector<int>>);
    auto d = $<std::vector, double>(1.0, 2.0); // arguments given
    static_assert(std::is_same_v<decltype(d), std::vector<double>>);
    auto s = $<std::set, _, std::greater<>>(3, 1, 2); // partial: deduce the element
    static_assert(std::is_same_v<decltype(s), std::set<int, std::greater<>>>);
    assert(*s.begin() == 3);
    assert(($<std::vector>(1, 2, 3) == tacit::make<std::vector>(1, 2, 3))); // the same function
    assert($(-1).abs() == 1);                                               // ...and the lift is untouched

    // the result is the value itself, not a lift of it: `$(x)` is a view of a subject that outlives
    // it, `$<F>(...)` creates the subject and so must own it
    static_assert(!std::is_same_v<decltype($<std::vector>(1)), decltype($(v))>);
    assert($($<std::vector>(1, 2, 3)).size() == 3); // wrap it if you want the vocabulary
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
