// The closed cells: `tacit::lift` (term) and `tacit::hole` (type), plus the `$` spellings of both.
// The governing rule for the lift is `lift(x).f(a...) == _.f(a...)(normalize(x))` — the closed cell is
// the open one applied now, over the same vocabulary and the same three dispatch kinds.
#include <tacit/_.hpp>

#include <cassert>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

using tacit::_;
using tacit::lift;

int main() {
  // ---- the type world: two duals of one template ----
  {
    static_assert(std::is_same_v<tacit::hole<int>::of<std::vector>, std::vector<int>>);
    static_assert(std::is_same_v<tacit::hole<int, char>::of<std::map>, std::map<int, char>>);
    static_assert(std::is_same_v<tacit::hole<>::as<std::vector>::with<int>, std::vector<int>>);
    static_assert(std::is_same_v<tacit::hole<>::as<std::map>::with<int, char>, std::map<int, char>>);
    // `of` fixes the arguments and awaits the template; `as` fixes the template and awaits the
    // arguments — the same construction from either side
    static_assert(std::is_same_v<tacit::hole<int>::of<std::vector>,
                                 tacit::hole<>::as<std::vector>::with<int>>);
    // plain types and plain templates throughout: nothing quoted, nothing lifted
    static_assert(std::is_same_v<tacit::hole<std::string>::of<std::vector>,
                                 std::vector<std::string>>);
  }

  // ---- the lift: a plain value gets the vocabulary, applied eagerly ----
  {
    std::vector<int> v{1, 2, 3};
    assert(lift(v).size() == 3);        // range CPO, not a member call
    assert(lift(v).front() == 1);       // member
    assert(lift(-42).abs() == 42);      // free function — a bare value has no members at all
    assert(lift(2.7).floor() == 2.0);
    assert(lift(-3).abs() + 1 == 4);    // the result is the natural one, not a wrapper
  }

  // ---- normalize: a string literal's raw type is never what you mean ----
  {
    assert(lift("").length() == 0);
    assert(lift("abc").length() == 3);
    assert(lift("abc").size() == 3);          // 3, not 4 — the NUL is not counted
    assert(_.size()("abc") == 4);             // ...which is what the un-normalized subject gives
    assert(lift("abc").starts_with("ab"));
  }

  // ---- reaches what a bare value has no members for ----
  {
    int carr[4]{};
    assert(lift(carr).size() == 4);           // carr.size() does not exist
    assert(!lift(carr).empty());
  }

  // ---- lvalues are held by reference (no copy), so mutation reaches the subject ----
  {
    std::vector<int> v{1, 2};
    lift(v).push_back(3);
    assert(v.size() == 3);
    std::string s = "hello";
    assert(lift(s).substr(1, 3) == "ell");
  }

  // ---- the governing rule, stated as tests ----
  {
    std::vector<int> v{1, 2, 3};
    assert(lift(v).size() == _.size()(v));
    assert(lift(-42).abs() == _.abs()(-42));
    assert(lift(v).front() == _.front()(v));
  }

  // ---- and the same vocabulary still composes at term level ----
  {
    std::vector<int> v{-3, 1, -2};
    assert(std::ranges::count_if(v, _.abs() > 1) == 2);
    assert((_.abs() + 1)(-41) == 42);
  }

  return 0;
}
