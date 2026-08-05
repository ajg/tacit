// The three properties every closure form has to have, asserted for every form rather than assumed from one.
//
// These are the claims a caller can rely on but that no test spelled out, so all three had gone unchecked:
//
//   CONST-CORRECTNESS. Every closure is callable through a const lvalue and a const rvalue. This has always held
//   — and holds one step further than std::function, whose call operator is const but invokes a non-const target:
//   `fn` constrains on `F const&` being invocable, so a non-const-callable target is never wrapped in the first
//   place. It is asserted here because a regression would be SILENT: a stray non-const `operator()` in a generator
//   would compile and pass every other test, and only bite someone reaching through a const reference.
//
//   SFINAE-FRIENDLINESS. Asking whether a closure is invocable must ANSWER, not explode. `is_invocable_v` on a
//   non-matching argument used to be a hard error for the operator sections, because they were built from
//   unconstrained lambdas and the failure escaped the immediate context. That broke concept-based dispatch,
//   `std::ranges`' own constraints, and every wrapper's constructibility check. Each `static_assert` below is a
//   double claim: that the answer is `false`, and — by compiling at all — that asking was legal.
//
//   NOEXCEPT. A call is `noexcept` exactly when the underlying operation is, which is the only honest setting:
//   claiming it when the operation can throw would turn a propagating exception into `std::terminate`. Note the
//   measurements bind the closure FIRST and query the call alone; `noexcept(build(...)(call))` would fold in the
//   construction, which is not `noexcept` and never claimed to be.
#include <tacit/_.hpp>

#include <cassert>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using tacit::_;

namespace {

// Two operands identical but for the exception specification, so noexcept can be shown to TRACK the operation
// rather than merely being present or absent.
struct nothrows {
  int v;
  friend int operator+(nothrows, int) noexcept { return 1; }
  friend bool operator<(nothrows, int) noexcept { return true; }
  nothrows &operator+=(int) noexcept { return *this; }
};
struct throws {
  int v;
  friend int operator+(throws, int) { return 1; }
  friend bool operator<(throws, int) { return true; }
  throws &operator+=(int) { return *this; }
};

} // namespace

int main() {
  // ---- const-correctness: every form, through const lvalue AND const rvalue ----
  {
    // leaf sections, both orientations and the two-blank form
    static_assert(std::is_invocable_v<decltype(_ + 1) const &, int>);
    static_assert(std::is_invocable_v<decltype(_ + 1) const, int>);
    static_assert(std::is_invocable_v<decltype(1 + _) const &, int>);
    static_assert(std::is_invocable_v<decltype(_ + _) const &, int, int>);
    static_assert(std::is_invocable_v<decltype(_ > 3) const &, int>);
    static_assert(std::is_invocable_v<decltype(3 > _) const &, int>);
    static_assert(std::is_invocable_v<decltype(_ > _) const &, int, int>);
    // unary, postfix, compound assignment
    static_assert(std::is_invocable_v<decltype(-_) const &, int>);
    static_assert(std::is_invocable_v<decltype(_ += 1) const &, int &>);
    // member chains and projections, including the fn-side operator forms
    static_assert(std::is_invocable_v<decltype(_.size()) const &, std::string const &>);
    static_assert(std::is_invocable_v<decltype(_.size() + 1) const &, std::string const &>);
    static_assert(std::is_invocable_v<decltype(1 + _.size()) const &, std::string const &>);
    static_assert(std::is_invocable_v<decltype(_.size() < _.size()) const &, std::string, std::string>);
    static_assert(std::is_invocable_v<decltype(-_.size()) const &, std::string const &>);
    // comma sections, combinators, chains
    static_assert(std::is_invocable_v<decltype((_, _)) const &, int, int>);
    static_assert(std::is_invocable_v<decltype(tacit::compose(_ + 1, _ * 2)) const &, int>);
    static_assert(std::is_invocable_v<decltype(tacit::fanout(_ + 1, _ * 2)) const &, int>);
    static_assert(std::is_invocable_v<decltype(0 < _ < 10) const &, int>);
    // a const OBJECT, not merely a const-qualified type, actually calls
    auto const f = _ + 1;
    auto const g = _.size() < _.size();
    assert(f(41) == 42);
    assert(g(std::string("a"), std::string("bb")));
  }

  // ---- SFINAE-friendliness: the question is legal and the answer is no ----
  {
    static_assert(!std::is_invocable_v<decltype(_ + 1) const &, std::string>);
    static_assert(!std::is_invocable_v<decltype(1 + _) const &, std::string>);
    static_assert(!std::is_invocable_v<decltype(_ > 3) const &, std::string>);
    static_assert(!std::is_invocable_v<decltype(_ + _) const &, std::string, int>);
    static_assert(!std::is_invocable_v<decltype(-_) const &, std::string>);
    static_assert(!std::is_invocable_v<decltype(_.size()) const &, int>);
    static_assert(!std::is_invocable_v<decltype(_.size() + 1) const &, int>);
    static_assert(!std::is_invocable_v<decltype(-_.size()) const &, int>);
    static_assert(!std::is_invocable_v<decltype(_ + 1) const &, int, int>); // wrong ARITY, too
    // `->*` on an operand that is neither natively `->*`-able nor dereferenceable
    struct W {
      int x;
    };
    auto pm = &W::x;
    static_assert(std::is_invocable_v<decltype(_ ->* pm) const &, W *>);
    static_assert(!std::is_invocable_v<decltype(_ ->* pm) const &, W>);
  }

  // ---- noexcept tracks the operation ----
  {
    auto add = (_ + 1);
    auto lt = (_ < 1);
    auto padd = (_.size() + 1); // through a projection
    auto pair = (_, _);
    nothrows n{0};
    throws t{0};

    static_assert(noexcept(add(std::declval<nothrows>())));
    static_assert(!noexcept(add(std::declval<throws>())));
    static_assert(noexcept(lt(std::declval<nothrows>())));
    static_assert(!noexcept(lt(std::declval<throws>())));
    // Member chains do NOT propagate noexcept, and that is a recorded DECISION rather than an oversight, so it is
    // asserted rather than left silent. `_.size()` routes through the section call operator, whose body is
    // forward_as_tuple + tuple_cat + apply — no specification can mirror a multi-statement body, and hand-writing
    // one there is the one place a too-narrow spec would turn a propagating exception into std::terminate. If this
    // line ever starts failing, the gap has been closed and this comment is what needs revisiting.
    static_assert(!noexcept(padd(std::declval<std::string const &>())));
    (void)n;
    (void)t;
    (void)pair;

    // compound assignment, both flavours
    auto inc = (_ += 1);
    static_assert(noexcept(inc(std::declval<nothrows &>())));
    static_assert(!noexcept(inc(std::declval<throws &>())));

    // and it is NOT claimed where it cannot be: concatenation allocates, so it can throw
    // (string COMPARISON, by contrast, is noexcept — which is why that is not the example)
    auto cat = (_ + std::string("x"));
    static_assert(!noexcept(cat(std::declval<std::string const &>())));
    // The same shape over `==` tracks the other way, and is asserted RELATIVE to the raw expression rather than
    // as an absolute: whether `string == string` is noexcept is the standard library's business, and the claim
    // being made here is only that the closure reports whatever the operation reports.
    auto eqs = (_ == std::string("x"));
    static_assert(noexcept(eqs(std::declval<std::string const &>())) ==
                  noexcept(std::declval<std::string const &>() == std::declval<std::string const &>()));
  }

  // ---- statelessness propagates through composition ----
  // DEFAULT-CONSTRUCTIBILITY is the portable claim and the useful one: it is what lets a closure be a container's
  // comparator TYPE rather than only a comparator value, and it is precisely what a capturing lambda can never
  // give — any capture deletes the default constructor, whether or not the captured thing is empty. It now holds
  // for composed closures, which is the KNOWN LIMIT recorded in stateless.cpp.
  //
  // EMPTINESS is a separate and weaker story, asserted only where every front end agrees. The portable rule is
  // narrow: a closure is empty when the functor it wraps has NO data members at all — `_ > _` (a `bind_n`) and
  // `-_` (a unary functor) qualify. The moment a closure holds a PROJECTION it does not, because the projection
  // is a distinct empty subobject and `[[no_unique_address]]` cannot make two same-type subobjects share an
  // address. Clang's `is_empty` answers yes there anyway even though `sizeof` is 2; the MSVC ABI answers no and
  // is the one telling the truth. So `!(_ < _)` belongs above with the composed forms, not here — it wraps
  // `_ < _` rather than being a leaf, which is exactly the distinction that is easy to eyeball wrong.
  {
    static_assert(std::is_empty_v<decltype(_ > _)> && std::is_default_constructible_v<decltype(_ > _)>);
    static_assert(std::is_empty_v<decltype(-_)> && std::is_default_constructible_v<decltype(-_)>);
    static_assert(std::is_default_constructible_v<decltype(!(_ < _))>);
    static_assert(std::is_default_constructible_v<decltype(_.size() < _.size())>); // the limit that closed
    static_assert(std::is_default_constructible_v<decltype(-_.size())>);
    static_assert(!std::is_empty_v<decltype(_ > 3)> && !std::is_default_constructible_v<decltype(_ > 3)>);
    static_assert(!std::is_default_constructible_v<decltype(_ += 1)>);
  }

  // ---- the standard wrappers accept these closures, which is what the properties are FOR ----
  {
    std::function<bool(int)> a = _ > 3;
    assert(a(4) && !a(3));
    std::function<bool(std::string const &, std::string const &)> b = _.size() < _.size();
    assert(b("a", "bb"));
    std::vector<int> v{3, 1, 2};
    std::ranges::sort(v, _ > _); // as a comparator VALUE
    assert(v[0] == 3);
    std::vector<int> w{3, 1, 2};
    std::ranges::sort(w, decltype(_ < _){}); // and as a default-constructed comparator TYPE
    assert(w[0] == 1);
  }

  return 0;
}
