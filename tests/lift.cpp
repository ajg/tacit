// The closed cells: `tacit::lift` (term) and `tacit::blank` (type), plus the `$` spellings of both.
// The governing rule for the lift is `lift(x).f(a...) == _.f(a...)(normalize(x))` — the closed cell is
// the open one applied now, over the same vocabulary and the same three dispatch kinds.
#include <tacit/_.hpp>

#include <array>
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
    static_assert(std::is_same_v<tacit::blank<int>::of<std::vector>, std::vector<int>>);
    static_assert(std::is_same_v<tacit::blank<int, char>::of<std::map>, std::map<int, char>>);
    static_assert(std::is_same_v<tacit::blank<>::as<std::vector>::with<int>, std::vector<int>>);
    static_assert(std::is_same_v<tacit::blank<>::as<std::map>::with<int, char>, std::map<int, char>>);
    // `of` fixes the arguments and awaits the template; `as` fixes the template and awaits the
    // arguments — the same construction from either side
    static_assert(std::is_same_v<tacit::blank<int>::of<std::vector>, tacit::blank<>::as<std::vector>::with<int>>);
    // plain types and plain templates throughout: nothing quoted, nothing lifted
    static_assert(std::is_same_v<tacit::blank<std::string>::of<std::vector>, std::vector<std::string>>);
    // and the whole surface is reachable through `_` itself — `_::` looks past the variable to the
    // class, the same route the nouns take. No `$`, no macro, no `struct` crutch.
    static_assert(std::is_same_v<_::blank<int>::of<std::vector>, std::vector<int>>);
    static_assert(std::is_same_v<_::blank<>::as<std::map>::with<int, char>, std::map<int, char>>);
    static_assert(std::is_same_v<_::blank<int>::of<std::vector>, _::blank<>::as<std::vector>::with<int>>);
    // alongside the pre-existing type-level surface, on the same symbol
    static_assert(std::is_same_v<_::value_type::of<std::vector<int>>, int>);
    // a blank among the arguments, with no `struct` crutch: `decltype(_)` is the placeholder's own
    // type, and `_::blank<>` the same thing spelled through the class
    static_assert(std::is_same_v<tacit::bind<std::vector, _::blank<>>::with<int>, std::vector<int>>);
    static_assert(std::is_same_v<tacit::bind<std::map, int, _::blank<>>::with<char>, std::map<int, char>>);
    static_assert(std::is_same_v<tacit::bind<std::vector, _::blank<>>::with<int>, std::vector<int>>);
    static_assert(
        std::is_same_v<tacit::apply<tacit::quote<std::map>, _::blank<>, int>::with<char>, std::map<char, int>>);
    static_assert(std::is_same_v<tacit::bind<std::vector, _::blank<>>::with<int>,
                                 std::vector<int>>); // the older spelling still works
  }

  // ---- structural rebind: same template, different arguments ----
  {
    // decomposes a specialisation you already have, rather than building one from a template you
    // name — you never write `std::vector`, which is the point
    static_assert(std::is_same_v<_::rebind<double>::of<std::vector<float>>, std::vector<double>>);
    static_assert(std::is_same_v<_::rebind<char, int>::of<std::map<int, char>>, std::map<char, int>>);
    // arguments are replaced wholesale, so a defaulted allocator RE-defaults rather than being
    // carried across wrongly as allocator<float>
    static_assert(
        std::is_same_v<_::rebind<double>::of<std::vector<float>>, std::vector<double, std::allocator<double>>>);
    // the <class, size_t> shapes keep their extent
    static_assert(std::is_same_v<_::rebind<double>::of<std::array<float, 5>>, std::array<double, 5>>);
    // composes with the rest of the type-level surface
    static_assert(std::is_same_v<_::value_type::of<_::rebind<double>::of<std::vector<float>>>, double>);
  }

  // ---- the lift: a plain value gets the vocabulary, applied eagerly ----
  {
    std::vector<int> v{1, 2, 3};
    assert(lift(v).size() == 3);   // range CPO, not a member call
    assert(lift(v).front() == 1);  // member
    assert(lift(-42).abs() == 42); // free function — a bare value has no members at all
    assert(lift(2.7).floor() == 2.0);
    assert(lift(-3).abs() + 1 == 4); // the result is the natural one, not a wrapper
  }

  // ---- normalize: a string literal's raw type is never what you mean ----
  {
    assert(lift("").length() == 0);
    assert(lift("abc").length() == 3);
    assert(lift("abc").size() == 3); // 3, not 4 — the NUL is not counted
    assert(_.size()("abc") == 4);    // ...which is what the un-normalized subject gives
    assert(lift("abc").starts_with("ab"));
  }

  // ---- reaches what a bare value has no members for ----
  {
    int carr[4]{};
    assert(lift(carr).size() == 4); // carr.size() does not exist
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
