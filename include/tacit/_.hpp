// SPDX-License-Identifier: BSL-1.0
#pragma once

// Version as a comparable integer: MAJOR*10000 + MINOR*100 + PATCH. A clean-path macro (kept), so
// downstream code can `#if TACIT_VERSION >= 200` feature-test.
#define TACIT_VERSION_MAJOR 0
#define TACIT_VERSION_MINOR 3
#define TACIT_VERSION_PATCH 0
#define TACIT_VERSION                                                                              \
  (TACIT_VERSION_MAJOR * 10000 + TACIT_VERSION_MINOR * 100 + TACIT_VERSION_PATCH)
// ============================================================================================
//  _.hpp  —  a point-free "_" object with a first-class standard-library
//  vocabulary, and a
//            reusable core for deriving your own domain-specific placeholders
// ============================================================================================
//
//  `_` (a stateless global object of its own type `tacit::_`) whose members
//  return closures that forward to a same-named operation on whatever they are
//  later applied to:
//
//      _.size()        ==  [](auto&& x){ return std::ranges::size(x); }
//      _.push_back(y)  ==  [y](auto&& x){ return x.push_back(y); }
//      (_ == y)        ==  [y](auto&& x){ return x == y; }
//
//  BLANKS (partial application).  Each `_` token is one blank; the arity of the
//  closure is the number of blanks; blanks are filled left to right. The
//  receiver counts:
//
//      _.push_back(y)   -> 1 blank  (c)             c.push_back(y)
//      _.push_back(_)   -> 2 blanks (c, v)          c.push_back(v)
//      _.replace(_, _)  -> 3 blanks (c, a, b)       c.replace(a, b)
//      _ + _            -> 2 blanks (a, b)          a + b
//
//  Bound (non-`_`) arguments are stored by value; the receiver and blank-fills
//  are perfect- forwarded. Repeated `_` are DISTINCT blanks (there are no
//  positional `_1`/`_2` sigils — reach for a named lambda the moment you need
//  to reorder or reuse an argument).
//
//  COMPARISON CHAINS.  C++ parses `0 < _ < 10` as `(0 < _) < 10` — comparing a
//  *bool* against 10, so the closure would be silently always true. Comparison
//  sections instead chain, rewriting into the conjunction the notation means:
//
//      0 < _ < 10          -> x -> (0 < x) && (x < 10)
//      1 <= _.size() < 4   -> x -> (1 <= size(x)) && (size(x) < 4)
//
//  for any length and any mix of `== != < > <= >=`; every other operator ends
//  the chain. See "comparison chains" in namespace detail for the mechanism.
//
//  COMMA TUPLES.  `,` is the one section that builds data instead of calling
//  something, and the one that is n-ary — each further `,` appends an operand:
//
//      _, _          -> (a, b)    -> std::pair{a, b}
//      _, y          -> x         -> std::pair{x, y}
//      _, _, _       -> (a, b, c) -> std::tuple{a, b, c}
//      _.size(), _   -> (a, b)    -> std::pair{size(a), b}
//
//  Two operands are a `pair`, three or more a `tuple`; the list is flat, so
//  parenthesising concatenates rather than nests. Blanks stay distinct, so
//  `(_.size(), _.front())` takes TWO arguments — the same-input tuple is
//  `tacit::fanout(_.size(), _.front())`. A comma section composes onward through
//  the value it builds, keeping its arity: the vocabulary and the six comparisons
//  apply to the pair/tuple that comes out (`(_, _) == p`, `(_, _) < p`).
//
//  TEACH `_` YOUR OWN NAMES.  `_` is the one placeholder — there is no separate
//  derived object. Domain names are added to `_` in place, pre-#defined before
//  the include as bare comma lists: TACIT_VERBS (value-level member calls) and
//  TACIT_NOUNS (type-level nested-type projections):
//
//      #define TACIT_VERBS deposit, balance, freeze
//      #include <tacit/_.hpp>
//      using tacit::_;
//      // now first-class on the same `_`:
//      ranges::sort(accounts, {}, _.balance());
//      _.deposit(_)(acct, 100);            // blanks work, as everywhere
//
//  Each verb becomes a member on `_` (and on its projections and the arrow
//  proxy), guarded so a name a given type lacks is a clean SFINAE miss. Nouns are
//  the type-level twin, reached as `_::name::of<X>`.
//
//  THREE LAYERS
//    1. First-class vocabulary — generated from the curated tables (or your
//    own). Pure C++23
//       (builds on g++ 13 and clang 18, -std=c++23).
//    2. Operator sections — `_ == y`, `x + _`, `_ + _`. Finite and lexical.
//    3. Reflective hatch — `_.m<"custom">(a...)`, `_.field<"x">()`,
//    `_.enum_name()` for names not
//       in a table. C++26 (P2996); auto-detected, otherwise compiled out.
//
//  RANGE ACCESS goes through the customization points (`std::ranges::NAME(x)`,
//  not `x.NAME()`), so
//  `_.size()` / `_.begin()` also work on C arrays, string views, and
//  third-party ranges.
//
//  ON THE NAME.  "tacit" is point-free programming; `_` is a "lieutenant" (French
//  "lieu tenant" — literally "place-holding"), a stand-in, sidestepping the loaded
//  English word "placeholder" (already spoken for by std::placeholders and `auto`).
// ============================================================================================

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <functional>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

// Detect reflection (P2996). `__cpp_impl_reflection` is the language facility
// (the `^^` operator and `[: :]` splicers); `__cpp_lib_reflection` is the
// <meta> library. Define TACIT_HAS_REFLECTION yourself before including to
// force it (e.g. on the clang-p2996 fork behind -freflection-latest).
#ifndef TACIT_HAS_REFLECTION
#if defined(__cpp_impl_reflection) && defined(__cpp_lib_reflection)
#define TACIT_HAS_REFLECTION 1
#else
#define TACIT_HAS_REFLECTION 0
#endif
#endif
#if TACIT_HAS_REFLECTION
#include <meta>
#include <string_view>
#endif

namespace tacit {
namespace detail {

// A blank is any tacit placeholder in argument position — detected via the
// `is_tacit_placeholder` tag that TACIT_CORE injects, so blanks work for the
// default `_` and for any derived placeholder.
template <class T>
constexpr bool is_blank_v = [] {
  if constexpr (requires { std::remove_cvref_t<T>::is_tacit_placeholder; })
    return bool(std::remove_cvref_t<T>::is_tacit_placeholder);
  else
    return false;
}();

// The composable projection wrapper `fn` (defined in full below, after the vocabulary tables). The
// forward declarations let the section machinery treat an `fn` argument as a *projected* blank, and
// let the operator/section macros constrain against it.
//
// `fn`'s second parameter is its CHAIN STATE — see "comparison chains" below. Everything that is not
// a comparison section builds `fn{f}`, i.e. `Last == nochain`: an ordinary, unchained projection.
struct nochain {};
// A closure of more than one fill (`_ < _`, `_.size() < _`, `_ , _`'s operator siblings). It rides
// in the same slot as the chain state because the two never coexist: a two-input form has no
// rightmost-operand to fold against. The point is `is_slot_v` below — a two-input closure in
// argument position is a bound VALUE (`_.sort(_ < _)` passes a comparator), not a projected blank,
// which only a one-fill closure can be.
struct nary {};
template <class> constexpr bool is_fn_v = false;
template <class, class = nochain> struct fn;
template <class F, class L> constexpr bool is_fn_v<fn<F, L>> = true;
template <class> constexpr bool is_nary_v = false;
template <class F> constexpr bool is_nary_v<fn<F, nary>> = true;
template <class T>
concept not_fn = !is_fn_v<std::remove_cvref_t<T>>;

// ---- comparison chains ------------------------------------------------------------------------
// C++ parses `0 < _ < 10` as `(0 < _) < 10`, which compares a *bool* against 10 — always true. A
// comparison section therefore carries one extra piece of state, `last`: a projection recovering the
// value of the comparison's RIGHTMOST operand as a function of the eventual fill. The next
// comparison then rewrites itself into the conjunction that the notation means:
//
//     (… op0 m) op y   ==   (… op0 m) && (m op y)        with m == last(x)
//
// so `0 < _ < 10` is `x -> (0 < x) && (x < 10)`, `0 < _.size() <= n` is
// `x -> (0 < size(x)) && (size(x) <= n)`, and the rule iterates for any length or mix of
// `== != < > <= >=`. Only those six build chain state; every other operator yields a plain `fn`, so
// a chain ends the moment the expression stops being a comparison.
//
// The rightmost operand is one of three things, hence three `last` shapes:
template <class Y> struct always { // a bound value: independent of the fill  (`_ < y`)
  Y y;
  constexpr Y const &operator()(auto &&...) const noexcept { return y; }
};
template <class Y> always(Y) -> always<Y>;
struct same { // the blank itself: the fill, untouched  (`y < _`)
  constexpr decltype(auto) operator()(auto &&x) const noexcept {
    return static_cast<decltype(x)>(x);
  }
};
// (the third is a projection — `y < _.size()` stores the `fn` itself as `last`.)

// Chain state for a bound operand: none if it is a placeholder (`_ < _` is a two-input comparator,
// not a link), otherwise the constant projection.
template <class Y> [[nodiscard]] constexpr auto last_of(Y const &y) {
  if constexpr (is_blank_v<Y>)
    return nochain{};
  else
    return always<Y>{y};
}

// A "slot" the section fills from a supplied argument: a plain blank (`_`, identity) or an `fn` —
// a *projected* blank, whose projection is applied to the fill before the call.
template <class T>
constexpr bool is_slot_v = is_blank_v<T> || (is_fn_v<std::remove_cvref_t<T>> &&
                                             !is_nary_v<std::remove_cvref_t<T>>);

// Where the blanks are in an operand list, and how a supplied fill reaches each one. Shared by the
// two things built out of operand lists: `section` (a partially-applied member call) and
// `comma_section` (a comma tuple). Each operand is a plain blank, a projected blank (an `fn`, whose
// projection is applied to the fill), or a bound value; fills land in the slots left to right.
template <class... Operand> struct slot_map {
  static constexpr std::size_t arity = sizeof...(Operand);
  static constexpr std::array<bool, arity> slot_at{is_slot_v<Operand>...};
  static constexpr std::array<bool, arity> proj_at{is_fn_v<std::remove_cvref_t<Operand>>...};
  static constexpr std::size_t slots = [] {
    std::size_t n = 0;
    for (bool s : slot_at)
      n += s;
    return n;
  }();
  static constexpr std::size_t slots_before(std::size_t i) {
    std::size_t n = 0;
    for (std::size_t j = 0; j < i; ++j)
      n += slot_at[j];
    return n;
  }

  // Operand I as a one-element tuple, ready to be `tuple_cat`ed into the resolved argument list.
  template <std::size_t I, class Operands, class Fills>
  static constexpr decltype(auto) pick(Operands const &ops, Fills &&fills) {
    if constexpr (proj_at[I]) // projected blank: apply the stored fn to the fill (materialised)
      return std::make_tuple(std::get<I>(ops)(std::get<slots_before(I)>(std::forward<Fills>(fills))));
    else if constexpr (slot_at[I]) // plain blank: the fill, untouched
      return std::forward_as_tuple(std::get<slots_before(I)>(std::forward<Fills>(fills)));
    else // bound value
      return std::forward_as_tuple(std::get<I>(ops));
  }
};

// A partially-applied operation carrying its bound args and blank markers.
// `invoke` performs the call once every argument is materialised; `bound` holds
// each original argument (a value, or a placeholder standing for a blank).
// Applying it to (subject, fills...) splices the fills into the blank
// positions, left to right, and calls `invoke`.
template <class Invoke, class... Bound> struct section {
  using map = slot_map<Bound...>;
  Invoke invoke;
  std::tuple<Bound...> bound;

  static constexpr std::size_t slots = map::slots;

  template <class X, class... F>
    requires(sizeof...(F) == slots)
  constexpr decltype(auto) operator()(X &&x, F &&...f) const {
    auto fills = std::forward_as_tuple(std::forward<F>(f)...);
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
      return std::apply(
          [&](auto &&...args) -> decltype(auto) {
            return invoke(std::forward<X>(x), std::forward<decltype(args)>(args)...);
          },
          std::tuple_cat(map::template pick<Is>(bound, fills)...));
    }(std::make_index_sequence<map::arity>{});
  }
};

template <class Invoke, class... A>
[[nodiscard]] constexpr auto make_section(Invoke invoke, A &&...a) {
  return section<Invoke, std::decay_t<A>...>{invoke, {std::forward<A>(a)...}};
}

// ---- comma sections ----------------------------------------------------------------------------
// `_, _` and friends. A comma section is an operand list that *builds data* rather than calling
// something: applied to one fill per blank, it returns the operands as a `std::pair` (two of them)
// or a `std::tuple` (three or more) — `(_, _)` is `(a, b) -> {a, b}`, `(_, 9)` is `x -> {x, 9}`.
//
// Unlike every other section it is n-ary and it ACCUMULATES: because `,` is left-associative,
// `_, _, _` parses as `(_, _), _`, so each further `,` appends an operand to the list rather than
// pairing with the closure built so far. That is why this is its own type and not an `fn` — an `fn`
// is unary, and the plain lambda the two-blank form used to return could neither grow nor compose,
// which is what left `_, _, _` a shape that only errored when called.
template <class... Ops> struct comma_section;
template <class> constexpr bool is_comma_v = false;
template <class... Ops> constexpr bool is_comma_v<comma_section<Ops...>> = true;
template <class T>
concept not_comma = !is_comma_v<std::remove_cvref_t<T>>;

// The definition lands after the vocabulary tables (as `fn`'s does), since a comma section carries
// that vocabulary too; these declarations are what the section machinery and the `,` overloads on
// `_` and `fn` need before then.
template <class... A> [[nodiscard]] constexpr comma_section<std::decay_t<A>...> make_comma(A &&...a);

#if TACIT_HAS_REFLECTION
template <std::size_t N> struct fixed_string {
  char v[N]{};
  consteval fixed_string(char const (&s)[N]) { std::copy_n(s, N, v); }
  constexpr std::string_view view() const noexcept { return {v, N - 1}; }
};
template <std::size_t N> fixed_string(char const (&)[N]) -> fixed_string<N>;
#endif

} // namespace detail

// clang-format off
//  Member-call forwarder. Variadic, so one entry covers every overload/arity. No-blank calls take a
//  guarded fast path (unary closure, requires-guarded so a bad name is a clean SFINAE rejection); a
//  call containing `_` builds a `section` whose arity grows by one per blank. The guard uses fresh
//  requires-parameters (xx, aa...) — it must not name the captured pack (clang rejects that).
#define TACIT_MEMBER(NAME)                                                                         \
  template <class... A>                                                                            \
  [[nodiscard]] static constexpr auto NAME(A&&... a) {                                             \
    if constexpr ((tacit::detail::is_slot_v<A> || ...))                                           \
      return tacit::detail::make_section(                                                          \
          [](auto&& x, auto&&... g) -> decltype(auto) {                                            \
            return std::forward<decltype(x)>(x).NAME(std::forward<decltype(g)>(g)...);             \
          },                                                                                       \
          std::forward<A>(a)...);                                                                  \
    else                                                                                           \
      return tacit::detail::fn{[... a = std::forward<A>(a)]<class X>(X&& x) -> decltype(auto)      \
               requires requires(X&& xx, A&&... aa) {                                              \
                 std::forward<X>(xx).NAME(std::forward<A>(aa)...);                                 \
               } { return std::forward<X>(x).NAME(a...); }};                                       \
  }                                                                                                \
  static_assert(true)

//  Arrow-member forwarder: the twin of TACIT_MEMBER but through `->`, so `_->NAME(args)` builds
//  x -> x->NAME(args) (the pointee's member, via the real operator->). Lives on the `arrow` proxy
//  that `_`'s operator-> hands back. No projected-blank path — a plain call forwarder.
#define TACIT_ARROW_MEMBER(NAME)                                                                   \
  template <class... A>                                                                            \
    requires(!(tacit::detail::is_blank_v<A> || ...))                                               \
  [[nodiscard]] static constexpr auto NAME(A&&... a) {                                             \
    return tacit::detail::fn{[... a = std::forward<A>(a)]<class X>(X&& x) -> decltype(auto)         \
             requires requires(X&& xx, A&&... aa) {                                                 \
               std::forward<X>(xx)->NAME(std::forward<A>(aa)...);                                   \
             } { return std::forward<X>(x)->NAME(a...); }};                                         \
  }                                                                                                \
  static_assert(true)

//  Range-adaptor verb (opt-in, behind TACIT_VIEWS). Unlike TACIT_MEMBER it BINDS its arguments as
//  values — a predicate/transformer is a value, not a projected blank — and routes through
//  std::views::ADAPT, so `_.filter(pred)` is x -> views::filter(x, pred): a *unary* fn that keeps
//  chaining. `_.filter(p).take(2)` is then x -> views::take(views::filter(x, p), 2), a point-free lazy
//  pipeline, and `(nums)` applies it. Binding (not projecting) the callable is exactly what keeps the
//  result unary and chainable — a projected blank would make it a two-input section that can't chain.
#define TACIT_VIEW(NAME, ADAPT)                                                                     \
  template <class... A> [[nodiscard]] static constexpr auto NAME(A&&... a) {                        \
    return tacit::detail::fn{[... a = std::forward<A>(a)]<class X>(X&& x) -> decltype(auto)         \
             requires requires(X&& xx, A&&... aa) {                                                 \
               std::views::ADAPT(std::forward<X>(xx), std::forward<A>(aa)...);                      \
             } { return std::views::ADAPT(std::forward<X>(x), a...); }};                            \
  }

//  The range-adaptor table (opt-in). `transform` is deliberately absent — it already means the
//  optional/ranges monadic member in the value vocabulary, so it needs a precedence call before it can
//  double as a view. Add more here (join, common, keys, values, ...) as they earn their place.
#define TACIT_VIEW_VERBS(X)                                                                         \
  X(filter, filter) X(take, take) X(drop, drop)                                                     \
  X(take_while, take_while) X(drop_while, drop_while) X(reverse, reverse)

//  Apply a forwarder macro M to a comma-separated list of names, each terminated with `;`. This is
//  the engine behind the TACIT_VERBS / TACIT_NOUNS extension hooks: the same list is driven through a
//  different forwarder for `_`, its projections (`fn`), and the arrow proxy. Empty list -> nothing.
//  Handles up to 256 names via the classic recursive-rescan trick.
#define TACIT_PARENS ()
#define TACIT_EXPAND(...)   TACIT_EXPAND_C(TACIT_EXPAND_C(TACIT_EXPAND_C(TACIT_EXPAND_C(__VA_ARGS__))))
#define TACIT_EXPAND_C(...) TACIT_EXPAND_B(TACIT_EXPAND_B(TACIT_EXPAND_B(TACIT_EXPAND_B(__VA_ARGS__))))
#define TACIT_EXPAND_B(...) TACIT_EXPAND_A(TACIT_EXPAND_A(TACIT_EXPAND_A(TACIT_EXPAND_A(__VA_ARGS__))))
#define TACIT_EXPAND_A(...) __VA_ARGS__
#define TACIT_FE(M, a, ...) M(a); __VA_OPT__(TACIT_FE_AGAIN TACIT_PARENS (M, __VA_ARGS__))
#define TACIT_FE_AGAIN() TACIT_FE
#define TACIT_FOR_EACH(M, ...) __VA_OPT__(TACIT_EXPAND(TACIT_FE(M, __VA_ARGS__)))

//  Unary / binary customization-point forwarders (route through std::ranges niebloids).
#define TACIT_CPO1(NAME, CPO)                                                                      \
  [[nodiscard]] static constexpr auto NAME() {                                                     \
    return tacit::detail::fn{[]<class X>(X&& x) -> decltype(auto)                                  \
             requires requires(X&& xx) { CPO(xx); } { return CPO(x); }};                           \
  }
#define TACIT_CPO2(NAME, CPO)                                                                      \
  [[nodiscard]] static constexpr auto NAME() {                                                     \
    return tacit::detail::fn{[]<class X, class Y>(X&& x, Y&& y) -> decltype(auto)                  \
             requires requires(X&& xx, Y&& yy) { CPO(xx, yy); } {                                  \
      return CPO(std::forward<X>(x), std::forward<Y>(y));                                          \
    }, tacit::detail::nary{}};                                                                     \
  }

//  One operator section (both one-sided forms and the two-blank form). Uses the enclosing `self`.
#define TACIT_SECTION(op)                                                                          \
  template <class Y> requires tacit::detail::not_fn<Y> [[nodiscard]] friend constexpr auto operator op(self, Y&& y) { \
    return tacit::detail::fn{[y = std::forward<Y>(y)](auto&& x) -> decltype(auto)                  \
             { return std::forward<decltype(x)>(x) op y; }};                                       \
  }                                                                                                \
  template <class X> requires tacit::detail::not_fn<X> [[nodiscard]] friend constexpr auto operator op(X&& x, self) { \
    if constexpr (std::is_copy_constructible_v<std::remove_cvref_t<X>>)                             \
      return tacit::detail::fn{[x = std::forward<X>(x)](auto&& y) -> decltype(auto)                 \
               { return x op std::forward<decltype(y)>(y); }};                                      \
    else /* non-copyable left operand (e.g. a stream): bind by reference so `os << _` works */      \
      return tacit::detail::fn{[&x](auto&& y) -> decltype(auto)                                     \
               { return x op std::forward<decltype(y)>(y); }};                                      \
  }                                                                                                \
  [[nodiscard]] friend constexpr auto operator op(self, self) {                                    \
    return tacit::detail::fn{[](auto&& x, auto&& y) -> decltype(auto)                              \
             { return std::forward<decltype(x)>(x) op std::forward<decltype(y)>(y); },             \
             tacit::detail::nary{}};                                                               \
  }

//  One comparison section: TACIT_SECTION plus chain state, so a following comparison can rewrite
//  itself into the conjunction the notation means (`0 < _ < 10` == `(0 < x) && (x < 10)`). The
//  one-sided forms differ only in what the rightmost operand is: the bound value (`_ op y`) or the
//  blank (`x op _`). The two-blank form stays a two-INPUT comparator — nothing to chain onto.
#define TACIT_COMPARE(op)                                                                          \
  template <class Y> requires tacit::detail::not_fn<Y> [[nodiscard]] friend constexpr auto operator op(self, Y&& y) { \
    auto last = tacit::detail::last_of(y); /* before the capture below moves from y */             \
    return tacit::detail::fn{[y = std::forward<Y>(y)](auto&& x) -> decltype(auto)                  \
             { return std::forward<decltype(x)>(x) op y; }, std::move(last)};                      \
  }                                                                                                \
  template <class X> requires tacit::detail::not_fn<X> [[nodiscard]] friend constexpr auto operator op(X&& x, self) { \
    if constexpr (std::is_copy_constructible_v<std::remove_cvref_t<X>>)                             \
      return tacit::detail::fn{[x = std::forward<X>(x)](auto&& y) -> decltype(auto)                 \
               { return x op std::forward<decltype(y)>(y); }, tacit::detail::same{}};               \
    else /* non-copyable left operand: bind by reference, as TACIT_SECTION does */                  \
      return tacit::detail::fn{[&x](auto&& y) -> decltype(auto)                                     \
               { return x op std::forward<decltype(y)>(y); }, tacit::detail::same{}};               \
  }                                                                                                \
  [[nodiscard]] friend constexpr auto operator op(self, self) {                                    \
    return tacit::detail::fn{[](auto&& x, auto&& y) -> decltype(auto)                              \
             { return std::forward<decltype(x)>(x) op std::forward<decltype(y)>(y); },             \
             tacit::detail::nary{}};                                                               \
  }

//  One unary operator (prefix). Coexists with a same-token binary section (`*_` vs `_ * y`) — they
//  differ by arity. `&_` builds `x -> &x`, not the placeholder's address (use std::addressof if ever
//  needed); overloading unary `&` is the one to keep in mind.
#define TACIT_UNARY(op)                                                                            \
  [[nodiscard]] friend constexpr auto operator op(self) {                                           \
    return tacit::detail::fn{[](auto&& x) -> decltype(auto)                                         \
             { return op std::forward<decltype(x)>(x); }};                                          \
  }
//  Postfix form (the trailing int disambiguates), for ++ / --.
#define TACIT_UNARY_POST(op)                                                                       \
  [[nodiscard]] friend constexpr auto operator op(self, int) {                                      \
    return tacit::detail::fn{[](auto&& x) -> decltype(auto)                                         \
             { return std::forward<decltype(x)>(x) op; }};                                          \
  }
//  One compound-assignment section: `_ op y` == x -> (x op y). Mutating and left-operand-only (there
//  is no `y op _` form — that would assign into y). The argument binds by reference, so it mutates
//  the caller's lvalue: e.g. std::ranges::for_each(v, _ += 1).
#define TACIT_ASSIGN(op)                                                                            \
  template <class Y> requires tacit::detail::not_fn<Y> [[nodiscard]] friend constexpr auto operator op(self, Y&& y) { \
    if constexpr (tacit::detail::is_blank_v<Y>) /* `_ op= _` is two-input, like `_ op _` */          \
      return tacit::detail::fn{[](auto&& a, auto&& b) -> decltype(auto)                              \
               { return std::forward<decltype(a)>(a) op std::forward<decltype(b)>(b); },             \
               tacit::detail::nary{}};                                                               \
    else                                                                                             \
      return tacit::detail::fn{[y = std::forward<Y>(y)](auto&& x) -> decltype(auto)                  \
               { return std::forward<decltype(x)>(x) op y; }};                                        \
  }

//  Reflective members (defined empty when reflection is off, so TACIT_CORE need not branch).
#if TACIT_HAS_REFLECTION
#  define TACIT_REFLECT(Self)                                                                      \
    template <tacit::detail::fixed_string Name, class... A>                                        \
    [[nodiscard]] static constexpr auto m(A&&... a) {                                              \
      return [... a = std::forward<A>(a)]<class X>(X&& x) -> decltype(auto) {                      \
        constexpr std::meta::info r = [] consteval {                                               \
          for (std::meta::info e : std::meta::members_of(                                          \
                   ^^std::remove_cvref_t<X>, std::meta::access_context::current()))                \
            if (std::meta::is_function(e) && std::meta::has_identifier(e) &&                       \
                std::meta::identifier_of(e) == Name.view())                                        \
              return e;                                                                            \
          return std::meta::info{};                                                                \
        }();                                                                                       \
        return std::forward<X>(x).[:r:](a...);                                                     \
      };                                                                                           \
    }                                                                                              \
    template <tacit::detail::fixed_string Name>                                                    \
    [[nodiscard]] static constexpr auto field() {                                                  \
      return []<class X>(X&& x) -> decltype(auto) {                                                \
        constexpr std::meta::info r = [] consteval {                                               \
          for (std::meta::info e : std::meta::nonstatic_data_members_of(                           \
                   ^^std::remove_cvref_t<X>, std::meta::access_context::current()))                \
            if (std::meta::identifier_of(e) == Name.view()) return e;                              \
          return std::meta::info{};                                                                \
        }();                                                                                       \
        return std::forward<X>(x).[:r:];                                                           \
      };                                                                                           \
    }                                                                                              \
    [[nodiscard]] static constexpr auto enum_name() {                                              \
      return [](auto e) -> std::string_view {                                                      \
        std::string_view name{};                                                                   \
        template for (constexpr std::meta::info en :                                               \
                      std::meta::enumerators_of(^^std::remove_cvref_t<decltype(e)>))               \
          if (e == [:en:]) name = std::meta::identifier_of(en);                                    \
        return name;                                                                               \
      };                                                                                           \
    }                                                                                              \
    [[nodiscard]] static constexpr auto each_field(auto f) {                                       \
      return [f]<class X>(X&& x) {                                                                 \
        template for (constexpr std::meta::info m : std::meta::nonstatic_data_members_of(          \
                          ^^std::remove_cvref_t<X>, std::meta::access_context::current()))         \
          f(std::forward<X>(x).[:m:]);                                                             \
      };                                                                                           \
    }
#else
#  define TACIT_REFLECT(Self)
#endif

//  The reusable, vocabulary-independent core: drop into any placeholder struct after its members.
#define TACIT_CORE(Self)                                                                           \
  using self = Self;                                                                               \
  static constexpr bool is_tacit_placeholder = true;                                               \
  TACIT_COMPARE(==) TACIT_COMPARE(!=) TACIT_COMPARE(<) TACIT_COMPARE(>)                            \
  TACIT_COMPARE(<=) TACIT_COMPARE(>=) TACIT_SECTION(+) TACIT_SECTION(-)                            \
  TACIT_SECTION(*) TACIT_SECTION(/) TACIT_SECTION(%) TACIT_SECTION(^)                              \
  TACIT_SECTION(&) TACIT_SECTION(|) TACIT_SECTION(&&) TACIT_SECTION(||) TACIT_SECTION(<<) TACIT_SECTION(>>) \
  TACIT_UNARY(*) TACIT_UNARY(-) TACIT_UNARY(+) TACIT_UNARY(!) TACIT_UNARY(~) TACIT_UNARY(&)        \
  TACIT_UNARY(++) TACIT_UNARY(--) TACIT_UNARY_POST(++) TACIT_UNARY_POST(--)                        \
  TACIT_ASSIGN(+=) TACIT_ASSIGN(-=) TACIT_ASSIGN(*=) TACIT_ASSIGN(/=) TACIT_ASSIGN(%=)             \
  TACIT_ASSIGN(^=) TACIT_ASSIGN(&=) TACIT_ASSIGN(|=) TACIT_ASSIGN(<<=) TACIT_ASSIGN(>>=)           \
  template <class Y>                                                                               \
    requires tacit::detail::not_fn<Y> && (!std::is_same_v<std::remove_cvref_t<Y>, self>)          \
  [[nodiscard]] constexpr auto operator=(Y&& y) const {                                            \
    return tacit::detail::fn{[y = std::forward<Y>(y)](auto&& x) -> decltype(auto)                  \
             { return std::forward<decltype(x)>(x) = y; }};                                        \
  }                                                                                                \
  /* comma is the tupling section: `_, _` == (a, b) -> {a, b}, `_, y` == x -> {x, y}, and each     \
     further `,` appends an operand — see `comma_section`. Operands that are an `fn` or an already \
     accumulated comma section are excluded here; their own overloads take those. */               \
  template <class Y>                                                                               \
    requires tacit::detail::not_fn<Y> && tacit::detail::not_comma<Y> &&                            \
             (!std::is_same_v<std::remove_cvref_t<Y>, self>)                                       \
  [[nodiscard]] friend constexpr auto operator,(self s, Y y) {                                     \
    return tacit::detail::make_comma(s, std::move(y));                                             \
  }                                                                                                \
  template <class X>                                                                               \
    requires tacit::detail::not_fn<X> && tacit::detail::not_comma<X> &&                            \
             (!std::is_same_v<std::remove_cvref_t<X>, self>)                                       \
  [[nodiscard]] friend constexpr auto operator,(X x, self s) {                                     \
    return tacit::detail::make_comma(std::move(x), s);                                             \
  }                                                                                                \
  /* a template, not a plain friend: a non-template body with a deduced return type is compiled     \
     with the class, which would instantiate comma_section<self, self> — and so std::tuple<_, _> —  \
     during the header parse, ambiguating the TACIT_STD_HOLES specialisation of std::tuple declared \
     below it. As a template it is instantiated on use, like every other comma overload. */         \
  template <class = void>                                                                          \
  [[nodiscard]] friend constexpr auto operator,(self a, self b) {                                  \
    return tacit::detail::make_comma(a, b);                                                        \
  }                                                                                                \
  [[nodiscard]] static constexpr auto operator()() {                                               \
    return [](auto&& x) -> decltype(auto) { return std::invoke(std::forward<decltype(x)>(x)); };   \
  }                                                                                                \
  template <class... Y>                                                                            \
  [[nodiscard]] static constexpr auto operator()(Y&&... ys) {                                      \
    if constexpr ((tacit::detail::is_slot_v<Y> || ...)) /* `_(_)` == (f, x) -> f(x) */             \
      return tacit::detail::make_section(                                                          \
          [](auto&& f, auto&&... as) -> decltype(auto)                                             \
            { return std::invoke(std::forward<decltype(f)>(f),                                     \
                                 std::forward<decltype(as)>(as)...); },                            \
          std::forward<Y>(ys)...);                                                                 \
    else                                                                                           \
      return [... ys = std::forward<Y>(ys)](auto&& x, auto&&... zs) -> decltype(auto) {            \
        return std::invoke(std::forward<decltype(x)>(x), ys..., std::forward<decltype(zs)>(zs)...);\
      };                                                                                           \
  }                                                                                                \
  template <class... Ts>                                                                           \
  [[nodiscard]] static constexpr auto get_types() {                                                \
    return [](auto&& x) -> decltype(auto) {                                                        \
      return std::forward<decltype(x)>(x).template get<Ts...>();                                   \
    };                                                                                             \
  }                                                                                                \
  template <int I>                                                                                 \
  [[nodiscard]] static constexpr auto get_at() {                                                   \
    return [](auto&& x) -> decltype(auto) {                                                        \
      return std::forward<decltype(x)>(x).template get<I>();                                       \
    };                                                                                             \
  }                                                                                                \
  template <class I>                                                                               \
  [[nodiscard]] constexpr auto operator[](I&& i) const {                                           \
    if constexpr (tacit::detail::is_slot_v<I>) /* `_[_]` == (x, i) -> x[i] */                      \
      return tacit::detail::make_section(                                                          \
          [](auto&& x, auto&& j) -> decltype(auto)                                                 \
            { return std::forward<decltype(x)>(x)[std::forward<decltype(j)>(j)]; },                \
          std::forward<I>(i));                                                                     \
    else                                                                                           \
      return tacit::detail::fn{[i = std::forward<I>(i)](auto&& x) -> decltype(auto)                \
               { return std::forward<decltype(x)>(x)[i]; }};                                       \
  }                                                                                                \
  TACIT_REFLECT(Self)                                                                              \
  static_assert(true)

// The standard-library vocabulary: one editable table, grouped by role with line breaks. A derived
// placeholder pulls it all with TACIT_STD_MEMBERS(TACIT_MEMBER), or just lists the names it wants.
#define TACIT_STD_MEMBERS(X)                                                                       \
  /* access   */ X(at); X(front); X(back); X(top);                                                 \
  /* capacity */ X(length); X(capacity); X(reserve); X(resize); X(shrink_to_fit); X(max_size);     \
                 X(size_bytes);                                                                    \
  /* modify   */ X(clear); X(push_back); X(pop_back); X(push_front); X(pop_front); X(push); X(pop);\
                 X(emplace); X(emplace_back); X(emplace_front); X(emplace_hint); X(insert);        \
                 X(insert_or_assign); X(try_emplace); X(erase); X(extract); X(remove); X(remove_if);\
                 X(splice); X(merge); X(unique); X(sort); X(append); X(assign); X(replace);        \
                 X(before_begin); X(insert_after); X(emplace_after); X(erase_after);               \
  /* range23  */ X(assign_range); X(append_range); X(insert_range); X(prepend_range); X(push_range);\
  /* lookup   */ X(find); X(count); X(contains); X(lower_bound); X(upper_bound); X(equal_range);   \
  /* string   */ X(substr); X(compare); X(starts_with); X(ends_with); X(rfind); X(find_first_of);  \
                 X(find_last_of); X(find_first_not_of); X(find_last_not_of); X(c_str); X(str);     \
                 X(remove_prefix); X(remove_suffix); X(to_string);                                 \
  /* optional */ X(has_value); X(value); X(value_or); X(and_then); X(transform); X(or_else); X(error);\
                 X(error_or); X(transform_error); X(index); X(reset);                              \
  /* pointer  */ X(get); X(release); X(use_count); X(expired); X(lock); X(owner_before);           \
  /* error    */ X(what); X(code); X(message); X(category); X(name);                               \
  /* path     */ X(filename); X(stem); X(extension); X(parent_path); X(relative_path); X(root_path);\
                 X(has_filename); X(has_extension); X(is_absolute); X(is_relative);                \
                 X(replace_extension); X(replace_filename); X(remove_filename);                    \
                 X(lexically_normal); X(string); X(generic_string); X(native);                     \
  /* stream   */ X(flush); X(is_open); X(open); X(close); X(good); X(eof); X(fail); X(bad); X(rdbuf);\
  /* thread   */ X(join); X(detach); X(joinable); X(wait); X(valid); X(share); X(load); X(store);  \
                 X(exchange); X(fetch_add); X(fetch_sub); X(unlock); X(try_lock); X(notify_one);   \
                 X(notify_all); X(request_stop); X(stop_requested);                                \
  /* numeric  */ X(real); X(imag); X(time_since_epoch);                                            \
  /* bitset   */ X(test); X(set); X(flip); X(all); X(any); X(none);                                \
  /* view     */ X(base); X(subspan);                                                              \
  /* regex    */ X(position); X(prefix); X(suffix); X(ready);                                      \
  /* source   */ X(file_name); X(function_name); X(line); X(column);

//  Free-function forwarder: the third dispatch kind, beside member calls and the range CPOs. Some
//  vocabulary is spelled as a free function in the standard library and can never be a member — a
//  bare `-42` has no members at all — so those names route to `std::` instead of to `x.NAME(...)`.
#define TACIT_FREE1(NAME, FN)                                                                      \
  [[nodiscard]] static constexpr auto NAME() {                                                     \
    return tacit::detail::fn{[]<class X>(X&& x) -> decltype(auto)                                  \
             requires requires(X&& xx) { FN(xx); } { return FN(std::forward<X>(x)); }};            \
  }

//  The free-function table: names that are free functions in std, kept small and numeric-leaning.
#define TACIT_STD_FREES(X)                                                                         \
  X(abs, std::abs)     X(sqrt, std::sqrt)   X(cbrt, std::cbrt)   X(floor, std::floor)              \
  X(ceil, std::ceil)   X(round, std::round) X(trunc, std::trunc) X(exp, std::exp)                  \
  X(log, std::log)     X(log2, std::log2)   X(log10, std::log10) X(signbit, std::signbit)          \
  X(isnan, std::isnan) X(isinf, std::isinf) X(isfinite, std::isfinite)

#define TACIT_STD_CPOS1(X)                                                                         \
  X(begin,  std::ranges::begin)  X(end,   std::ranges::end)                                        \
  X(cbegin, std::ranges::cbegin) X(cend,  std::ranges::cend)                                       \
  X(rbegin, std::ranges::rbegin) X(rend,  std::ranges::rend)                                       \
  X(crbegin, std::ranges::crbegin) X(crend, std::ranges::crend)                                    \
  X(size,   std::ranges::size)   X(ssize, std::ranges::ssize)                                      \
  X(empty,  std::ranges::empty)  X(data,  std::ranges::data)                                       \
  X(cdata,  std::ranges::cdata)
// clang-format on

// Additive extension hook for the one `_`: pre-#define a comma list of bare member-call names before
// including, and each becomes a first-class *verb* on `_` — reachable as `_.name(...)`, composing and
// blank-taking like the built-in vocabulary (`_.name(_)`), on `_` and its projections alike:
//   #define TACIT_VERBS area, perimeter, scale
// This is how you teach `_` a domain vocabulary — there is no separate placeholder object; `_` stays
// the one interface. (Its type-level twin is TACIT_NOUNS, below.)
// VOCABULARY FILE (the richer twin of the TACIT_VERBS / TACIT_NOUNS comma lists). Point
// TACIT_VOCABULARY at a header, or just drop a `tacit_vocabulary.hpp` on the include path and it is
// found by `__has_include`:
//
//     #define TACIT_VOCABULARY <bank/vocabulary.hpp>
//     #include <tacit/_.hpp>
//
// The file is a list of entries, and — unlike a bare comma list — each entry chooses its *dispatch
// kind*, so a domain free function or customization point is reachable, not just member calls:
//
//     TACIT_VERB(deposit)               // x.deposit(a...)
//     TACIT_FREE(area, geom::area)      // geom::area(x)          — a bare value has no members
//     TACIT_CPO(extent, geom::extent)   // geom::extent(x)        — a customization point
//     TACIT_NOUN(payload_type)          // _::payload_type::of<X> == X::payload_type
//
// The file is included ONCE PER SURFACE (`_`, its projections, the arrow proxy, comma sections, the
// lift), each time with the entry macros bound to that surface's forwarder — the X-macro pattern. So
// it must NOT have an include guard or `#pragma once`, and must contain nothing but entries.
//
// Two entries, one list, one place: every translation unit that includes tacit sees the same
// vocabulary by construction. That is the point over `#define TACIT_VERBS`, which every TU has to
// repeat identically before the include or else end up with a different `_`. It is not a *guarantee*
// — a TU that points TACIT_VOCABULARY somewhere else still gets a different `_`, and mixing those in
// one program is an ODR violation like any other — but it removes the per-TU ritual that made the
// mismatch easy to cause by accident.
#if defined(TACIT_VOCABULARY)
#define TACIT_VOCABULARY_FILE TACIT_VOCABULARY
#elif defined(__has_include)
#if __has_include(<tacit_vocabulary.hpp>)
#define TACIT_VOCABULARY_FILE <tacit_vocabulary.hpp>
#endif
#endif

#ifndef TACIT_VERBS
#define TACIT_VERBS
#endif

// ------------------------------------------------------------------------------------------------
//  Type-level projection vocabulary: the twin of the value-level member table above, but for types.
//  Each entry is a metafunction living inside `struct _`, pulling a nested member out of a
//  later-supplied type X and applied as `_::name::of<X>`. TACIT_TYPE_MEMBER projects a nested
//  type/alias (`X::name`); TACIT_TYPE_TEMPLATE projects a nested *template* (`X::template name<A...>`,
//  the rebind family), reached as `_::name<A...>::of<X>` — you supply the template's own arguments
//  before applying. `of` is the applier (distinct from bind's hole-filling `with`).
#define TACIT_TYPE_MEMBER(NAME)                                                                     \
  struct NAME { template <class X> using of = typename X::NAME; };                                  \
  static_assert(true)
#define TACIT_TYPE_TEMPLATE(NAME)                                                                   \
  template <class... A> struct NAME {                                                               \
    template <class X> using of = typename X::template NAME<A...>;                                  \
  };                                                                                                \
  static_assert(true)

// clang-format off
//  The standard nested-type vocabulary (one editable table, grouped by role). Every name here is a
//  nested type some standard component exposes; a projection instantiates only on use, so listing a
//  name a given X lacks costs nothing until `_::name::of<X>` is actually asked for.
#define TACIT_STD_TYPE_MEMBERS(X)                                                                  \
  /* element */ X(value_type); X(element_type); X(reference); X(const_reference);                  \
                X(pointer); X(const_pointer);                                                      \
  /* size    */ X(size_type); X(difference_type);                                                  \
  /* iterate */ X(iterator); X(const_iterator); X(reverse_iterator); X(const_reverse_iterator);    \
                X(iterator_category); X(iterator_concept);                                         \
                X(local_iterator); X(const_local_iterator);                                        \
  /* assoc   */ X(key_type); X(mapped_type); X(key_compare); X(value_compare);                     \
                X(hasher); X(key_equal); X(node_type); X(insert_return_type);                      \
  /* alloc   */ X(allocator_type); X(void_pointer); X(const_void_pointer);                         \
  /* pair    */ X(first_type); X(second_type);                                                     \
  /* traits  */ X(char_type); X(traits_type); X(int_type);                                         \
                X(pos_type); X(off_type); X(state_type); X(string_type);                           \
  /* monad   */ X(error_type); X(unexpected_type); X(result_type);                                 \
  /* pointer */ X(deleter_type); X(weak_type);                                                     \
  /* time    */ X(rep); X(period); X(duration); X(time_point); X(clock);                           \
  /* adaptor */ X(container_type);                                                                 \
  /* thread  */ X(native_handle_type); X(id);                                                      \
  /* meta    */ X(type);
// clang-format on

//  Additive extension hook for the type-level `_`: the twin of TACIT_VERBS, a comma list of bare
//  nested-type names, each a first-class *noun* projected as `_.name` — reached as `_::name::of<X>`:
//    #define TACIT_NOUNS shape_tag, payload_type
//  TACIT_NOUN_TEMPLATES is the parameterized variant, for nested *templates* (`X::name<A...>`, the
//  rebind family), reached as `_::name<A...>::of<X>`. Kept a separate list so TACIT_NOUNS stays a
//  clean bare-name list; use it only when you actually project a nested template:
//    #define TACIT_NOUN_TEMPLATES rebind
#ifndef TACIT_NOUNS
#define TACIT_NOUNS
#endif
#ifndef TACIT_NOUN_TEMPLATES
#define TACIT_NOUN_TEMPLATES
#endif

// clang-format off
namespace detail {
// The composable projection wrapper (forward-declared above). Beyond call, subscript, and the
// operator sections, it carries the same std vocabulary as `_` — but every member COMPOSES through
// the wrapped projection, so `_.front().size()` == `x -> size(front(x))`: that is member chaining.
template <class F, class Last> struct fn {
  F f;
  // Chain state: `nochain` for every projection that is not a comparison section. See "comparison
  // chains" above — `last` recovers the rightmost operand of the comparison this `fn` represents.
  [[no_unique_address]] Last last{}; // (the initializer keeps plain `fn{f}` warning-clean)
  static constexpr bool chained = !std::is_same_v<Last, nochain> && !std::is_same_v<Last, nary>;

  template <class... A>
    requires requires(F const &g, A &&...a) { g(std::forward<A>(a)...); }
  constexpr decltype(auto) operator()(A &&...a) const { return f(std::forward<A>(a)...); }
  template <class I> [[nodiscard]] constexpr auto operator[](I i) const {
    return tacit::detail::fn{[g = *this, i = std::move(i)](auto &&...x) -> decltype(auto) {
      return g(std::forward<decltype(x)>(x)...)[i];
    }};
  }
  // Operator sections as hidden friends — found by ADL, including across a module boundary
  // (`import tacit;`): `g op value` / `value op g` compose to a unary fn; `g op h` is binary.
#define TACIT_FN_OP(op)                                                                            \
  template <not_fn Y> [[nodiscard]] friend constexpr auto operator op(fn g, Y y) {                 \
    if constexpr (is_blank_v<Y>) /* `g op _` is two-input, like `_ op _` — not a bound value */    \
      return tacit::detail::fn{[g](auto &&a, auto &&b) -> decltype(auto) {                         \
        return g(std::forward<decltype(a)>(a)) op std::forward<decltype(b)>(b); }, nary{}};        \
    else                                                                                           \
      return tacit::detail::fn{                                                                    \
        [g, y](auto &&...x) -> decltype(auto) { return g(std::forward<decltype(x)>(x)...) op y; }};\
  }                                                                                                \
  template <not_fn X> [[nodiscard]] friend constexpr auto operator op(X &&x, fn g) {               \
    if constexpr (is_blank_v<std::remove_cvref_t<X>>) /* `_ op g`, the mirror */                   \
      return tacit::detail::fn{[g](auto &&a, auto &&b) -> decltype(auto) {                         \
        return std::forward<decltype(a)>(a) op g(std::forward<decltype(b)>(b)); }, nary{}};        \
    else if constexpr (std::is_copy_constructible_v<std::remove_cvref_t<X>>)                       \
      return tacit::detail::fn{                                                                    \
          [g, x = std::forward<X>(x)](auto &&...y) -> decltype(auto) {                             \
            return x op g(std::forward<decltype(y)>(y)...); }};                                    \
    else /* non-copyable (stream): bind by reference, e.g. cout << _.size() */                     \
      return tacit::detail::fn{                                                                    \
          [g, &x](auto &&...y) -> decltype(auto) { return x op g(std::forward<decltype(y)>(y)...); }};\
  }                                                                                                \
  template <class G, class L> [[nodiscard]] friend constexpr auto operator op(fn g, fn<G, L> h) {  \
    return tacit::detail::fn{[g, h](auto &&a, auto &&b) -> decltype(auto) {                        \
      return g(std::forward<decltype(a)>(a)) op h(std::forward<decltype(b)>(b));                   \
    }, nary{}};                                                                                    \
  }
  TACIT_FN_OP(+) TACIT_FN_OP(-) TACIT_FN_OP(*) TACIT_FN_OP(/) TACIT_FN_OP(%) TACIT_FN_OP(^)
  TACIT_FN_OP(&) TACIT_FN_OP(|) TACIT_FN_OP(&&) TACIT_FN_OP(||) TACIT_FN_OP(<<) TACIT_FN_OP(>>)
#undef TACIT_FN_OP
  // The six comparisons are TACIT_FN_OP plus chain state, so `0 < _ < 10` means what it says: when
  // the left operand is itself a comparison section, the new one folds into a conjunction against
  // that section's rightmost operand rather than against its bool RESULT. Every link records its own
  // rightmost operand, so chains of any length and any mix of the six operators compose.
#define TACIT_FN_COMPARE(op)                                                                       \
  template <not_fn Y> [[nodiscard]] friend constexpr auto operator op(fn g, Y y) {                 \
    if constexpr (is_blank_v<Y>) /* `g op _` is the two-input comparator, like `_ op _` */        \
      return tacit::detail::fn{[g](auto &&a, auto &&b) -> decltype(auto) {                         \
        return g(std::forward<decltype(a)>(a)) op std::forward<decltype(b)>(b); }, nary{}};        \
    else if constexpr (fn::chained) /* `(… op0 m) op y` == `(… op0 m) && (m op y)` */              \
      return tacit::detail::fn{[g, y](auto &&...x) -> bool {                                       \
                                 return static_cast<bool>(g(x...)) &&                              \
                                        static_cast<bool>(g.last(x...) op y);                      \
                               },                                                                  \
                               tacit::detail::always{y}};                                          \
    else /* first link: unchanged behaviour, plus the state a following comparison folds against */ \
      return tacit::detail::fn{                                                                    \
          [g, y](auto &&...x) -> decltype(auto) { return g(std::forward<decltype(x)>(x)...) op y; },\
          tacit::detail::always{y}};                                                               \
  }                                                                                                \
  template <not_fn X> [[nodiscard]] friend constexpr auto operator op(X &&x, fn g) {               \
    if constexpr (is_blank_v<std::remove_cvref_t<X>>) /* `_ op g`, the mirror */                   \
      return tacit::detail::fn{[g](auto &&a, auto &&b) -> decltype(auto) {                         \
        return std::forward<decltype(a)>(a) op g(std::forward<decltype(b)>(b)); }, nary{}};        \
    else if constexpr (std::is_copy_constructible_v<std::remove_cvref_t<X>>)                       \
      return tacit::detail::fn{                                                                    \
          [g, x = std::forward<X>(x)](auto &&...y) -> decltype(auto) {                             \
            return x op g(std::forward<decltype(y)>(y)...); }, g}; /* rightmost operand: g */      \
    else /* non-copyable (stream): bind by reference */                                            \
      return tacit::detail::fn{                                                                    \
          [g, &x](auto &&...y) -> decltype(auto) { return x op g(std::forward<decltype(y)>(y)...); }, g};\
  }                                                                                                \
  template <class G, class L> [[nodiscard]] friend constexpr auto operator op(fn g, fn<G, L> h) {  \
    return tacit::detail::fn{[g, h](auto &&a, auto &&b) -> decltype(auto) {                        \
      return g(std::forward<decltype(a)>(a)) op h(std::forward<decltype(b)>(b));                   \
    }, nary{}};                                                                                    \
  }
  TACIT_FN_COMPARE(==) TACIT_FN_COMPARE(!=) TACIT_FN_COMPARE(<)
  TACIT_FN_COMPARE(>)  TACIT_FN_COMPARE(<=) TACIT_FN_COMPARE(>=)
#undef TACIT_FN_COMPARE
  // Comma: a projection joins a comma section as an operand like any other, so
  // `(_.size(), _.front())` is `(a, b) -> {size(a), front(b)}`. Without these the built-in comma
  // applies instead — it evaluates and DISCARDS the left operand, silently yielding just the right
  // one. (`[[nodiscard]]` on the vocabulary makes that a warning, not an error; a section that
  // quietly loses half of what you wrote is worth an overload.)
  template <class Y>
    requires(not_fn<Y> && not_comma<Y>)
  [[nodiscard]] friend constexpr auto operator,(fn g, Y y) {
    return tacit::detail::make_comma(g, std::move(y));
  }
  template <class X>
    requires(not_fn<X> && not_comma<X>)
  [[nodiscard]] friend constexpr auto operator,(X x, fn g) {
    return tacit::detail::make_comma(std::move(x), g);
  }
  template <class G, class L> [[nodiscard]] friend constexpr auto operator,(fn g, fn<G, L> h) {
    return tacit::detail::make_comma(g, h);
  }
  // Unary operators compose through the projection: `op g` == x -> op g(x).
#define TACIT_FN_UNARY(op)                                                                         \
  [[nodiscard]] friend constexpr auto operator op(fn g) {                                          \
    return tacit::detail::fn{                                                                      \
        [g](auto &&...x) -> decltype(auto) { return op g(std::forward<decltype(x)>(x)...); }};     \
  }
#define TACIT_FN_UNARY_POST(op)                                                                    \
  [[nodiscard]] friend constexpr auto operator op(fn g, int) {                                     \
    return tacit::detail::fn{                                                                      \
        [g](auto &&...x) -> decltype(auto) { return g(std::forward<decltype(x)>(x)...) op; }};     \
  }
  TACIT_FN_UNARY(*) TACIT_FN_UNARY(-) TACIT_FN_UNARY(+) TACIT_FN_UNARY(!) TACIT_FN_UNARY(~) TACIT_FN_UNARY(&)
  TACIT_FN_UNARY(++) TACIT_FN_UNARY(--) TACIT_FN_UNARY_POST(++) TACIT_FN_UNARY_POST(--)
#undef TACIT_FN_UNARY
#undef TACIT_FN_UNARY_POST
  // (`|` is a bitwise section like `&`, above — general composition is `tacit::compose`, not `f | g`.)
  // Vocabulary — each name composes through the projection (member chaining). No-blank args only.
#define TACIT_FN_MEMBER(NAME)                                                                      \
  template <class... A>                                                                            \
    requires(!(tacit::detail::is_blank_v<A> || ...)) /* see "no dead closures" below */            \
  [[nodiscard]] constexpr auto NAME(A &&...a) const {                                              \
    return tacit::detail::fn{                                                                      \
        [g = *this, ... a = std::forward<A>(a)]<class... X>(X &&...x) -> decltype(auto)            \
          requires requires(X &&...xx, A &&...aa) {                                                \
            std::declval<fn const &>()(std::forward<X>(xx)...).NAME(std::forward<A>(aa)...);       \
          } { return g(std::forward<X>(x)...).NAME(a...); }};                                      \
  }                                                                                                \
  static_assert(true)
#define TACIT_FN_CPO1(NAME, CPO)                                                                   \
  [[nodiscard]] constexpr auto NAME() const {                                                      \
    return tacit::detail::fn{[g = *this]<class... X>(X &&...x) -> decltype(auto)                   \
             requires requires(X &&...xx) { CPO(std::declval<fn const &>()(std::forward<X>(xx)...)); }\
             { return CPO(g(std::forward<X>(x)...)); }};                                           \
  }
  // `._()` — the application form, composed. On `_` itself, application is spelled `_(a, b)`
  // (`operator()` builds `f -> f(a, b)`); on a projection that spelling is taken, since calling an
  // `fn` means "call this closure". So the vocabulary carries it under the placeholder's own name:
  // `_.x()._()` invokes what `_.x()` produced, and `._(a, b)` calls it with those arguments —
  // `x -> invoke(x.x(), a, b)`. Returns an `fn`, so a callable-returning chain keeps going.
  template <class... Y>
    requires(!(tacit::detail::is_blank_v<Y> || ...))
  [[nodiscard]] constexpr auto _(Y &&...ys) const {
    return tacit::detail::fn{
        [g = *this, ... ys = std::forward<Y>(ys)]<class... X>(X &&...x) -> decltype(auto)
          requires requires(X &&...xx, Y &&...yy) {
            std::invoke(std::declval<fn const &>()(std::forward<X>(xx)...), std::forward<Y>(yy)...);
          } { return std::invoke(g(std::forward<X>(x)...), ys...); }};
  }
  TACIT_STD_MEMBERS(TACIT_FN_MEMBER)
  TACIT_FOR_EACH(TACIT_FN_MEMBER, TACIT_VERBS)
  TACIT_STD_CPOS1(TACIT_FN_CPO1)
#define TACIT_FN_FREE1(NAME, FN)                                                                   \
  [[nodiscard]] constexpr auto NAME() const {                                                      \
    return tacit::detail::fn{[g = *this]<class... X>(X &&...x) -> decltype(auto)                   \
             requires requires(X &&...xx) { FN(std::declval<fn const &>()(std::forward<X>(xx)...)); }\
             { return FN(g(std::forward<X>(x)...)); }};                                            \
  }
  TACIT_STD_FREES(TACIT_FN_FREE1)
#ifdef TACIT_VOCABULARY_FILE
#define TACIT_VERB(NAME) TACIT_FN_MEMBER(NAME);
#define TACIT_FREE(NAME, FN) TACIT_FN_FREE1(NAME, FN)
#define TACIT_CPO(NAME, FN) TACIT_FN_CPO1(NAME, FN)
#define TACIT_NOUN(NAME)
#include TACIT_VOCABULARY_FILE
#undef TACIT_VERB
#undef TACIT_FREE
#undef TACIT_CPO
#undef TACIT_NOUN
#endif
#undef TACIT_FN_FREE1
  // Range-adaptor verbs compose through the projection, so a pipeline keeps chaining (opt-in).
#ifdef TACIT_VIEWS
#define TACIT_FN_VIEW(NAME, ADAPT)                                                                  \
  template <class... A> [[nodiscard]] constexpr auto NAME(A&&... a) const {                         \
    return tacit::detail::fn{                                                                       \
        [g = *this, ... a = std::forward<A>(a)]<class X>(X&& x) -> decltype(auto)                   \
          requires requires(X&& xx, A&&... aa) {                                                    \
            std::views::ADAPT(std::declval<fn const&>()(std::forward<X>(xx)),                       \
                              std::forward<A>(aa)...);                                               \
          } { return std::views::ADAPT(g(std::forward<X>(x)), a...); }};                            \
  }
  TACIT_VIEW_VERBS(TACIT_FN_VIEW)
#undef TACIT_FN_VIEW
#endif
#undef TACIT_FN_MEMBER
#undef TACIT_FN_CPO1
};
// `fn{f}` is an unchained projection; `fn{f, last}` a comparison section. Spelled out (rather than
// left to aggregate deduction) so the one-argument form unambiguously picks up the `nochain` default.
template <class F> fn(F) -> fn<F, nochain>;
template <class F, class L> fn(F, L) -> fn<F, L>;

// The comma section proper (declared above) — its vocabulary needs the tables, like `fn`'s.
template <class... Ops> struct comma_section {
  using map = slot_map<Ops...>;
  std::tuple<Ops...> ops;

  static constexpr std::size_t slots = map::slots;

  template <class... F>
    requires(sizeof...(F) == slots)
  [[nodiscard]] constexpr auto operator()(F &&...f) const {
    auto fills = std::forward_as_tuple(std::forward<F>(f)...);
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return std::apply([](auto &&...xs) { return build(std::forward<decltype(xs)>(xs)...); },
                        std::tuple_cat(map::template pick<Is>(ops, fills)...));
    }(std::make_index_sequence<map::arity>{});
  }

  // Two operands stay a `std::pair` — the documented meaning of the pairing section, and `.first` /
  // `.second` are strictly extra: a pair is a two-tuple for `get`, `tuple_size`, structured
  // bindings, and `apply` alike. Three or more have no pair to be, so they are a `std::tuple`.
  template <class... Xs> [[nodiscard]] static constexpr auto build(Xs &&...xs) {
    if constexpr (sizeof...(Xs) == 2)
      return std::pair{std::forward<Xs>(xs)...};
    else
      return std::tuple{std::forward<Xs>(xs)...};
  }

  // Vocabulary, composing through the built value: `(_, _).swap(p)` is (a, b) -> {a, b}.swap(p).
  // Same shape as `fn`'s member chaining, and arity-preserving in the same way — the result is an
  // `fn` over the same fills, so it keeps chaining (`(_, _).bar().baz()`). Bound arguments only:
  // a blank inside a chained call (`(_, _).foo(_)`) is not a further slot, exactly as it isn't for a
  // projection (`_.front().substr(_)` has never taken one) — the fills belong to the operand list.
#define TACIT_COMMA_MEMBER(NAME)                                                                   \
  template <class... A>                                                                            \
    requires(!(tacit::detail::is_blank_v<A> || ...))                                               \
  [[nodiscard]] constexpr auto NAME(A &&...a) const {                                              \
    return tacit::detail::fn{                                                                      \
        [c = *this, ... a = std::forward<A>(a)]<class... X>(X &&...x) -> decltype(auto)            \
          requires requires(X &&...xx, A &&...aa) {                                                \
            std::declval<comma_section const &>()(std::forward<X>(xx)...)                          \
                .NAME(std::forward<A>(aa)...);                                                     \
          } { return c(std::forward<X>(x)...).NAME(a...); }};                                      \
  }                                                                                                \
  static_assert(true)
#define TACIT_COMMA_FREE1(NAME, FN)                                                                \
  [[nodiscard]] constexpr auto NAME() const {                                                      \
    return tacit::detail::fn{                                                                      \
        [c = *this]<class... X>(X &&...x) -> decltype(auto)                                        \
          requires requires(X &&...xx) {                                                           \
            FN(std::declval<comma_section const &>()(std::forward<X>(xx)...));                     \
          } { return FN(c(std::forward<X>(x)...)); }};                                             \
  }
#define TACIT_COMMA_CPO1(NAME, CPO)                                                                \
  [[nodiscard]] constexpr auto NAME() const {                                                      \
    return tacit::detail::fn{                                                                      \
        [c = *this]<class... X>(X &&...x) -> decltype(auto)                                        \
          requires requires(X &&...xx) {                                                           \
            CPO(std::declval<comma_section const &>()(std::forward<X>(xx)...));                    \
          } { return CPO(c(std::forward<X>(x)...)); }};                                            \
  }
  TACIT_STD_MEMBERS(TACIT_COMMA_MEMBER)
  TACIT_FOR_EACH(TACIT_COMMA_MEMBER, TACIT_VERBS)
#ifdef TACIT_VOCABULARY_FILE
#define TACIT_VERB(NAME) TACIT_COMMA_MEMBER(NAME);
#define TACIT_FREE(NAME, FN) TACIT_COMMA_FREE1(NAME, FN)
#define TACIT_CPO(NAME, FN) TACIT_COMMA_CPO1(NAME, FN)
#define TACIT_NOUN(NAME)
#include TACIT_VOCABULARY_FILE
#undef TACIT_VERB
#undef TACIT_FREE
#undef TACIT_CPO
#undef TACIT_NOUN
#endif
  TACIT_STD_CPOS1(TACIT_COMMA_CPO1)
#undef TACIT_COMMA_MEMBER
#undef TACIT_COMMA_CPO1
#undef TACIT_COMMA_FREE1

  // Comparisons, likewise through the built value: `(_, _) == p` is (a, b) -> {a, b} == p, and `<`
  // is the lexicographic order `pair`/`tuple` already define — the six are the operators those types
  // actually have, which is why the arithmetic and bitwise sets stay off a data builder. The far
  // side must be a value: another comma section, an `fn`, or a blank would be a second operand list
  // rather than something to compare against, so those are left to fail rather than guessed at.
#define TACIT_COMMA_COMPARE(op)                                                                    \
  template <class Y>                                                                               \
    requires(not_comma<Y> && not_fn<Y> && !is_blank_v<Y>)                                          \
  [[nodiscard]] friend constexpr auto operator op(comma_section c, Y y) {                          \
    return tacit::detail::fn{[c, y](auto &&...x) -> decltype(auto) {                               \
      return c(std::forward<decltype(x)>(x)...) op y;                                              \
    }};                                                                                            \
  }                                                                                                \
  template <class X>                                                                               \
    requires(not_comma<X> && not_fn<X> && !is_blank_v<X>)                                          \
  [[nodiscard]] friend constexpr auto operator op(X x, comma_section c) {                          \
    return tacit::detail::fn{[c, x](auto &&...y) -> decltype(auto) {                               \
      return x op c(std::forward<decltype(y)>(y)...);                                              \
    }};                                                                                            \
  }
  TACIT_COMMA_COMPARE(==) TACIT_COMMA_COMPARE(!=) TACIT_COMMA_COMPARE(<)
  TACIT_COMMA_COMPARE(>) TACIT_COMMA_COMPARE(<=) TACIT_COMMA_COMPARE(>=)
#undef TACIT_COMMA_COMPARE

  // Growing the list. `not_comma` on the mixed forms keeps them from competing with the concatenating
  // overload when both sides are comma sections (`(_, _), (_, _)` is a four-slot tuple).
  template <class Y>
    requires not_comma<Y>
  [[nodiscard]] friend constexpr auto operator,(comma_section c, Y y) {
    return std::apply([&](auto const &...o) { return make_comma(o..., std::move(y)); }, c.ops);
  }
  template <class X>
    requires not_comma<X>
  [[nodiscard]] friend constexpr auto operator,(X x, comma_section c) {
    return std::apply([&](auto const &...o) { return make_comma(std::move(x), o...); }, c.ops);
  }
  template <class... B>
  [[nodiscard]] friend constexpr auto operator,(comma_section c, comma_section<B...> d) {
    return std::apply(
        [&](auto const &...a) {
          return std::apply([&](auto const &...b) { return make_comma(a..., b...); }, d.ops);
        },
        c.ops);
  }
};

template <class... A> [[nodiscard]] constexpr comma_section<std::decay_t<A>...> make_comma(A &&...a) {
  return {{std::forward<A>(a)...}};
}
} // namespace detail
// clang-format on

// The arrow proxy behind `_->NAME(...)`. `_`'s operator-> hands back a pointer to this; its
// vocabulary forwards through `->`, so `_->size()` == x -> x->size() — the pointee's member via the
// *real* operator-> (which is independent of, and not always equal to, `(*x).size()`).
namespace detail {
struct arrow {
  TACIT_STD_MEMBERS(TACIT_ARROW_MEMBER)
  // Range access: through `->` these are ordinary member calls on the pointee (no CPO routing —
  // the pointee has the members), so they join the arrow vocabulary as plain forwarders.
  TACIT_ARROW_MEMBER(begin);  TACIT_ARROW_MEMBER(end);    TACIT_ARROW_MEMBER(cbegin);
  TACIT_ARROW_MEMBER(cend);   TACIT_ARROW_MEMBER(rbegin); TACIT_ARROW_MEMBER(rend);
  TACIT_ARROW_MEMBER(size);   TACIT_ARROW_MEMBER(ssize);  TACIT_ARROW_MEMBER(empty);
  TACIT_ARROW_MEMBER(data);
  TACIT_FOR_EACH(TACIT_ARROW_MEMBER, TACIT_VERBS)
#ifdef TACIT_VOCABULARY_FILE
#define TACIT_VERB(NAME) TACIT_ARROW_MEMBER(NAME);
#define TACIT_FREE(NAME, FN)
#define TACIT_CPO(NAME, FN)
#define TACIT_NOUN(NAME)
#include TACIT_VOCABULARY_FILE
#undef TACIT_VERB
#undef TACIT_FREE
#undef TACIT_CPO
#undef TACIT_NOUN
#endif
};
inline constexpr arrow arrow_v;
} // namespace detail

// ------------------------------------------------------------------------------------------------
// Structural rebind: same template, different arguments — `_::rebind<double>::of<std::vector<float>>`
// is `std::vector<double>`. This is the one type-level operation with demand behind it outside
// metaprogramming libraries: it is what `std::simd`'s `rebind_t` does, what allocator `rebind_alloc`
// does for its own family, and what P3971 is standardising for containers generally.
//
// It differs from `hole`/`bind` in the direction it works. Those *build* a specialisation from a
// template you name; rebind *decomposes* one you already have and re-applies its template. You never
// name `std::vector` — that is the point, since the caller usually does not know it.
//
// Arguments are replaced wholesale, which is what you want for defaulted parameters: `std::vector<float>`
// is really `vector<float, allocator<float>>`, and `rebind<double>` yields `vector<double>` — the
// allocator re-defaults to `allocator<double>` rather than being carried across, wrongly, as
// `allocator<float>`. A second form handles the `<class, size_t>` shapes (`array`, `span`), whose
// extent is not a type and so rides along untouched.
namespace detail {
template <class X, class... New> struct rebind_;
template <template <class...> class F, class... Old, class... New>
struct rebind_<F<Old...>, New...> {
  using type = F<New...>;
};
template <template <class, std::size_t> class F, class Old, std::size_t N, class New>
struct rebind_<F<Old, N>, New> {
  using type = F<New, N>;
};
} // namespace detail

// ------------------------------------------------------------------------------------------------
// The type-level hole. One template, two duals: `of` fixes the arguments and awaits the template,
// `as` fixes the template and awaits the arguments — the head-hole and arg-hole grains of the same
// thing. Both take plain types and plain templates, so nothing needs lifting or quoting:
//
//   hole<int>::of<std::vector>            // std::vector<int>   — head is the hole
//   hole<>::as<std::vector>::with<int>    // std::vector<int>   — head is given
//
// `bind<F, Args...>::with<X...>` (below) is the same arg-hole grain with holes spelled `_::hole<>`
// among the arguments; `as` is the spelling that needs no hole marker at all. The opt-in
// <tacit/$.hpp> aliases this to `$`, so `$<int>::of<F>` is the short form.
template <class... A> struct hole {
  template <template <class...> class F> using of = F<A...>;
  template <template <class...> class F> struct as {
    template <class... X> using with = F<X...>;
  };
};

// The one type `_`: the placeholder's own type. It carries the full std vocabulary (plus any
// TACIT_VERBS / TACIT_NOUNS) and the core for the value side, and doubles as the type-level `_` — a blank
// for `bind` and a projection namespace (`_::name::of<X>`). The value `_` (next) hides the type in
// ordinary lookup, but a name before `::` is looked up considering only types, namespaces, and
// templates, so `_::name` reaches this struct past the value; elsewhere reach the type via `struct _`.
struct _ {
  TACIT_STD_MEMBERS(TACIT_MEMBER)
  TACIT_FOR_EACH(TACIT_MEMBER, TACIT_VERBS)
#ifdef TACIT_VOCABULARY_FILE
#define TACIT_VERB(NAME) TACIT_MEMBER(NAME);
#define TACIT_FREE(NAME, FN) TACIT_FREE1(NAME, FN)
#define TACIT_CPO(NAME, FN) TACIT_CPO1(NAME, FN)
#define TACIT_NOUN(NAME) TACIT_TYPE_MEMBER(NAME);
#include TACIT_VOCABULARY_FILE
#undef TACIT_VERB
#undef TACIT_FREE
#undef TACIT_CPO
#undef TACIT_NOUN
#endif
  TACIT_STD_CPOS1(TACIT_CPO1)
  TACIT_STD_FREES(TACIT_FREE1)
  TACIT_CPO2(swap, std::ranges::swap)
  /* the type-level hole, reached through the same symbol: `_::hole<int>::of<std::vector>`. `_::`
     is looked up considering only types, namespaces and templates, so it reaches this class past
     the variable `_` — the same route the nouns below take. */
  template <class... A> using hole = tacit::hole<A...>;
  /* structural rebind: `_::rebind<double>::of<std::vector<float>>` == `std::vector<double>`. Named
     for the standard's own vocabulary (simd's `rebind_t`, P3971); a TACIT_NOUN_TEMPLATES entry of
     the same name would collide, since that hook spells the *nested* rebind (`X::template rebind`). */
  template <class... New> struct rebind {
    template <class X> using of = typename tacit::detail::rebind_<X, New...>::type;
  };
  TACIT_STD_TYPE_MEMBERS(TACIT_TYPE_MEMBER)
  TACIT_FOR_EACH(TACIT_TYPE_MEMBER, TACIT_NOUNS)
  TACIT_FOR_EACH(TACIT_TYPE_TEMPLATE, TACIT_NOUN_TEMPLATES)
#ifdef TACIT_VIEWS
  TACIT_VIEW_VERBS(TACIT_VIEW)
#endif
  [[nodiscard]] constexpr const detail::arrow* operator->() const { return &detail::arrow_v; }
  TACIT_CORE(_);
};
inline constexpr _ _;

// Closure combinators (Haskell arrow flavour). `_`-agnostic — any callable works — and each returns
// an fn, so results keep composing (member access, operator sections, ...).
//   compose(f, g, ...) left-to-right compose  x -> ...(g(f(x)))   (general function composition)
//   fanout(f, g, ...)  x -> tuple{f(x), g(x), ...}         (Haskell &&&)
//   first(f) / second(f)   transform one component of a pair   (Haskell first / second)
//
// These are free functions in `tacit`, reached only by qualification. To keep the default surface at
// exactly `_`, they are gated: `#define TACIT_COMBINATORS` before including to enable them (and their
// `*_element` cousins below). Off by default; the machinery still compiles either way.
//
// NOTE: `compose` is where general composition lives now that `operator|` is an ordinary bitwise
// section (`_ | y`), symmetric with `&`, rather than the old `f | g` compose. Member chaining
// (`_.front().size()`) still composes vocabulary on the default surface; `compose` is for composing
// arbitrary closures — `compose(_ + 1, _ * 2)(3)` == `(3 + 1) * 2` == 8.
#ifdef TACIT_COMBINATORS
template <class F, class... Gs> [[nodiscard]] constexpr auto compose(F f, Gs... gs) {
  if constexpr (sizeof...(Gs) == 0)
    return detail::fn{f};
  else
    return detail::fn{[f, rest = tacit::compose(gs...)](auto &&x) -> decltype(auto) {
      return rest(f(std::forward<decltype(x)>(x)));
    }};
}
template <class... Fs> [[nodiscard]] constexpr auto fanout(Fs... fs) {
  return detail::fn{[fs...](auto &&x) { return std::tuple{fs(x)...}; }};
}
template <class F> [[nodiscard]] constexpr auto first(F f) {
  return detail::fn{[f](auto &&p) { return std::pair{f(std::get<0>(p)), std::get<1>(p)}; }};
}
template <class F> [[nodiscard]] constexpr auto second(F f) {
  return detail::fn{[f](auto &&p) { return std::pair{std::get<0>(p), f(std::get<1>(p))}; }};
}
#endif // TACIT_COMBINATORS

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// Type-level tacit: `_` doubles as a type-level blank for partially applying a class template into a
// metafunction. A template argument list can't hold the *value* `_`, so the blank is written with
// the type-level blank `_::hole<>`. Fixed arguments stay plain types, no wrapper:
//
//   bind<std::vector, _::hole<>>::with<int>              // std::vector<int>
//   bind<std::map, int, _::hole<>>::with<double>         // std::map<int, double>  (partial)
//   bind<std::map, _::hole<>, _::hole<>>::with<char, int> // std::map<char, int>
//
// The dual of bind is projection: where bind wraps the hole in an *outer* template, `_::name::of<X>`
// pulls a *member* out of X — the type-level twin of the value-level member vocabulary. It is a
// closed vocabulary (each name is declared in `struct _`, table above; add your own via
// TACIT_NOUNS), applied with `::of` since there is no `operator()` at the type level:
//
//   _::value_type::of<std::vector<int>>     // int
//   _::mapped_type::of<std::map<int, char>> // char
//   _::value_type::of<_::value_type::of<T>> // chains by nesting (T's value_type's value_type)
namespace detail {
// The type-level blank is `hole<>` — spelled `_::hole<>` in user code. Neither `struct _` nor
// `decltype(_)` is a hole: the placeholder's own type is the term-level object, and conflating the
// two is what made the old spelling need a `struct` crutch in the first place.
using blank = tacit::hole<>;
// Walk the argument list, replacing each `hole<>` blank with the next of Xs...; keep fixed types.
template <template <class...> class F, class Done, class Xs, class... Args> struct fill_slots;
template <template <class...> class F, class... D, class Xs>
struct fill_slots<F, std::tuple<D...>, Xs> {
  using type = F<D...>;
};
template <template <class...> class F, class... D, class X, class... Xr, class... A>
struct fill_slots<F, std::tuple<D...>, std::tuple<X, Xr...>, blank, A...>
    : fill_slots<F, std::tuple<D..., X>, std::tuple<Xr...>, A...> {};

template <template <class...> class F, class... D, class Xs, class A0, class... A>
struct fill_slots<F, std::tuple<D...>, Xs, A0, A...>
    : fill_slots<F, std::tuple<D..., A0>, Xs, A...> {};
} // namespace detail

// ------------------------------------------------------------------------------------------------
// The term-level lift: `lift(x)` gives a plain value the vocabulary it may not have as members, and
// applies it eagerly. The rule is exactly
//
//     lift(x).f(a...)   ==   _.f(a...)(normalize(x))
//
// so the closed cell is the open one applied now — same table, same dispatch (member, range CPO, or
// free function), no second vocabulary to keep in step. It is what reaches operations a bare value
// has no members for: `lift(-42).abs()` is 42, `lift(v).size()` is `ranges::size(v)` even where
// `v.size()` does not exist.
//
// NORMALIZE. A string literal's raw type is never what you mean: `const char[4]` has no members at
// all, and `ranges::size("abc")` is 4 because it counts the NUL. So a char array is normalized to
// `string_view` on the way in, making `lift("abc").length()` and `.size()` both 3. Everything else
// is taken as it is: lvalues are held by reference (no copy), rvalues by value (no dangling).
//
// The result of a lifted call is the operation's own natural result, not another lift — `.size()`
// hands back a `size_t`, not a wrapper needing unwrapping. Chaining therefore continues on that
// result's own type; re-lift with `lift(...)` when the next hop needs the vocabulary again.
namespace detail {
template <class T> constexpr decltype(auto) normalize(T &&t) {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::is_array_v<U> &&
                std::is_same_v<std::remove_cv_t<std::remove_extent_t<U>>, char>)
    return std::string_view(t);
  else
    return static_cast<T &&>(t);
}
template <class T> struct held {
  T v;
#define TACIT_HELD_MEMBER(NAME)                                                                    \
  template <class... A>                                                                            \
    requires(!(tacit::detail::is_blank_v<A> || ...)) &&                                            \
            requires(T const &x, A &&...aa) { x.NAME(std::forward<A>(aa)...); }                    \
  [[nodiscard]] constexpr decltype(auto) NAME(A &&...a) const {                                    \
    return v.NAME(std::forward<A>(a)...);                                                          \
  }                                                                                                \
  static_assert(true)
#define TACIT_HELD_CPO1(NAME, CPO)                                                                 \
  [[nodiscard]] constexpr decltype(auto) NAME() const                                              \
    requires requires(T const &x) { CPO(x); } { return CPO(v); }
#define TACIT_HELD_FREE1(NAME, FN)                                                                 \
  [[nodiscard]] constexpr decltype(auto) NAME() const                                              \
    requires requires(T const &x) { FN(x); } { return FN(v); }
  TACIT_STD_MEMBERS(TACIT_HELD_MEMBER)
  TACIT_FOR_EACH(TACIT_HELD_MEMBER, TACIT_VERBS)
#ifdef TACIT_VOCABULARY_FILE
#define TACIT_VERB(NAME) TACIT_HELD_MEMBER(NAME);
#define TACIT_FREE(NAME, FN) TACIT_HELD_FREE1(NAME, FN)
#define TACIT_CPO(NAME, FN) TACIT_HELD_CPO1(NAME, FN)
#define TACIT_NOUN(NAME)
#include TACIT_VOCABULARY_FILE
#undef TACIT_VERB
#undef TACIT_FREE
#undef TACIT_CPO
#undef TACIT_NOUN
#endif
  TACIT_STD_CPOS1(TACIT_HELD_CPO1)
  TACIT_STD_FREES(TACIT_HELD_FREE1)
#undef TACIT_HELD_MEMBER
#undef TACIT_HELD_CPO1
#undef TACIT_HELD_FREE1
  [[nodiscard]] constexpr T const &get() const noexcept { return v; }   // the subject, unchanged
};
template <class T> held(T) -> held<T>;
} // namespace detail

template <class X> [[nodiscard]] constexpr auto lift(X &&x) {
  using N = decltype(detail::normalize(static_cast<X &&>(x)));
  if constexpr (std::is_lvalue_reference_v<X> && std::is_lvalue_reference_v<N>)
    return detail::held<std::remove_reference_t<N> &>{detail::normalize(static_cast<X &&>(x))};
  else
    return detail::held<std::remove_cvref_t<N>>{detail::normalize(static_cast<X &&>(x))};
}

template <template <class...> class F, class... Args> struct bind {
  template <class... Xs>
  using with = typename detail::fill_slots<F, std::tuple<>, std::tuple<Xs...>, Args...>::type;
};

// The general primitive that subsumes `bind` and curries BOTH grains under one op. `bind` fixes the
// template and holes among its *arguments*; `apply` additionally lets the *template itself* be a hole.
// Quote a template into a type with `quote<F>` (a template can't sit in a type slot unquoted — packs
// are single-kind), then `apply<Slots...>::with<Fills...>` fills each `hole<>` slot — template or
// argument — left to right:
//
//   apply<quote<std::map>, _::hole<>, _::hole<>>::with<int, char> // std::map<int,char> (fix template)
//   apply<_::hole<>, int, char>::with<quote<std::map>>            // std::map<int,char> (fix args)
//   apply<_::hole<>, int, _::hole<>>::with<quote<std::map>, char> // std::map<int,char> (hole both)
//
// So `bind<F, A...>::with<X...>` is `apply<quote<F>, A...>::with<X...>`; the arg-first grain is the
// mirror the plain `bind` can't spell. A C++26 reflection build would erase `quote<>` — templates and
// types both become std::meta::info, so the slot list stops needing the wrapper. (Naming provisional.)
template <template <class...> class F> struct quote {};
namespace detail {
template <class... Applied> struct run_slots; // first Applied slot must resolve to a quote<F>
template <template <class...> class F, class... A>
struct run_slots<quote<F>, A...> {
  using type = F<A...>;
};
template <class Acc, class Fills, class... Slots> struct apply_slots;
template <class... Acc, class Fills>
struct apply_slots<std::tuple<Acc...>, Fills> {
  using type = typename run_slots<Acc...>::type;
};
template <class... Acc, class X, class... Xr, class... S>
struct apply_slots<std::tuple<Acc...>, std::tuple<X, Xr...>, blank, S...>
    : apply_slots<std::tuple<Acc..., X>, std::tuple<Xr...>, S...> {};
template <class... Acc, class Fills, class S0, class... S>
struct apply_slots<std::tuple<Acc...>, Fills, S0, S...>
    : apply_slots<std::tuple<Acc..., S0>, Fills, S...> {};
} // namespace detail

template <class... Slots> struct apply {
  template <class... Fills>
  using with = typename detail::apply_slots<std::tuple<>, std::tuple<Fills...>, Slots...>::type;
};

#if TACIT_HAS_REFLECTION
// C++26 extension point: with a P2996 toolchain, std::meta::substitute generalizes this to alias
// templates and non-type template parameters that a template-template parameter cannot name. Left a
// documented hook rather than shipped blind — untested reflection metaprograms want a real
// toolchain, same posture as the reflective member hatch.
#endif

// Heterogeneous element combinators: drive a callable over the elements of a
// tuple-like. Built on std::apply + fold-expressions (C++23); a `template for`
// (C++26) path can later extend them to arbitrary aggregates and reflection
// ranges. They are `_`-agnostic (any callable works) but pair naturally with
// `_`'s closures, e.g. transform_elements(t, _.size()) or any_of_element(t,
// _.empty()). Free `tacit::` functions, so — like the closure combinators above
// — they sit behind `#define TACIT_COMBINATORS` and stay off the default surface.
#ifdef TACIT_COMBINATORS
template <class Tup, class F> constexpr void for_each_element(Tup &&t, F &&f) {
  // `void(...)` so the fold uses the built-in comma even when f returns something with an
  // `operator,` of its own — an `fn` or a comma section would otherwise accumulate here.
  std::apply([&](auto &&...xs) { (void(f(std::forward<decltype(xs)>(xs))), ...); },
             std::forward<Tup>(t));
}
template <class Tup, class F> [[nodiscard]] constexpr bool any_of_element(Tup &&t, F &&f) {
  return std::apply(
      [&](auto &&...xs) { return (static_cast<bool>(f(std::forward<decltype(xs)>(xs))) || ...); },
      std::forward<Tup>(t));
}
template <class Tup, class F> [[nodiscard]] constexpr bool all_of_element(Tup &&t, F &&f) {
  return std::apply(
      [&](auto &&...xs) { return (static_cast<bool>(f(std::forward<decltype(xs)>(xs))) && ...); },
      std::forward<Tup>(t));
}
template <class Tup, class F> [[nodiscard]] constexpr bool none_of_element(Tup &&t, F &&f) {
  return !tacit::any_of_element(std::forward<Tup>(t), std::forward<F>(f));
}
template <class Tup, class F> [[nodiscard]] constexpr auto transform_elements(Tup &&t, F &&f) {
  return std::apply([&](auto &&...xs) { return std::tuple{f(std::forward<decltype(xs)>(xs))...}; },
                    std::forward<Tup>(t));
}
#endif // TACIT_COMBINATORS

} // namespace tacit

// ------------------------------------------------------------------------------------------------
// Tier 1 (experimental, opt-in): natural-spelling type-level holes, `std::map<struct _, int>::with<char>`.
// Injects partial specializations of common std containers into `namespace std`, keyed on the hole
// type `tacit::_`, each exposing a `::with<...>` that reconstructs the container with the hole filled.
//
// This is deliberately gated behind TACIT_STD_HOLES and OFF by default: a specialization of a std
// class template for `tacit::_` does not meet the original template's requirements (a hole is not a
// key/value/allocator), so per [namespace.std] it is technically ill-formed (no diagnostic required) —
// it works on the tested toolchains, but it is a spelling convenience, not a standards guarantee. The
// portable, always-on surface is `tacit::apply` / `tacit::bind` above. Notes on the shape:
//   - A trailing pack is not "more specialized" than a fixed-arity primary, so the defaulted
//     Compare/Allocator params can't hide behind `...`; each SHAPE macro names them explicitly.
//   - `::with` reconstructs FRESH defaults (map<K,T>, not map<K,T,C,A>): carrying the hole-derived
//     less<hole>/allocator<...hole...> along would silently poison the result. Cost: you can't thread
//     an explicit non-default Compare/Allocator through a hole (drop to `apply`/`bind` for that).
//   - `tuple` is variadic: a single LEADING hole + trailing pack is legal at any arity and portable;
//     interior holes and multiple leading-hole specs are not (see tacit_extras.md).
//   - `array` / `span` are value-parameterized (`<class, size_t>`): only the ELEMENT type is holed and
//     the extent rides along as a literal (`array<struct _, 5>::with<int>`). No wrapper is needed
//     because the value is a normal template argument — the reason this stays natural where the
//     general primitive would demand a value wrapper. Holing the extent itself has no natural spelling
//     and is intentionally not offered.
#ifdef TACIT_STD_HOLES
#include <array>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <utility>
#include <vector>
namespace std {
#define TACIT_HOLE ::tacit::hole<> // the type-level blank; `_::hole<>` in user code
#define TACIT_SPEC_1_1(F)                                                                          \
  template <class A0> class F<TACIT_HOLE, A0> {                                                     \
  public:                                                                                           \
    template <class X> using with = F<X>;                                                           \
  };
#define TACIT_SPEC_1_2(F)                                                                          \
  template <class C, class A0> class F<TACIT_HOLE, C, A0> {                                         \
  public:                                                                                           \
    template <class X> using with = F<X>;                                                           \
  };
#define TACIT_SPEC_2_2(F)                                                                          \
  template <class T, class C, class A0> class F<TACIT_HOLE, T, C, A0> {                             \
  public:                                                                                           \
    template <class K> using with = F<K, T>;                                                        \
  };                                                                                                \
  template <class K, class C, class A0> class F<K, TACIT_HOLE, C, A0> {                             \
  public:                                                                                           \
    template <class V> using with = F<K, V>;                                                        \
  };                                                                                                \
  template <class C, class A0> class F<TACIT_HOLE, TACIT_HOLE, C, A0> {                             \
  public:                                                                                           \
    template <class K, class V> using with = F<K, V>;                                               \
  };
TACIT_SPEC_1_1(vector)
TACIT_SPEC_1_2(set)
TACIT_SPEC_2_2(map)
#undef TACIT_SPEC_1_1
#undef TACIT_SPEC_1_2
#undef TACIT_SPEC_2_2
// pair: two type params, no defaults — by hand.
template <class B> struct pair<TACIT_HOLE, B> {
  template <class X> using with = pair<X, B>;
};
template <class A0> struct pair<A0, TACIT_HOLE> {
  template <class Y> using with = pair<A0, Y>;
};
template <> struct pair<TACIT_HOLE, TACIT_HOLE> {
  template <class X, class Y> using with = pair<X, Y>;
};
// tuple: variadic, single leading hole + trailing pack — legal at any arity, portable.
template <class... R> class tuple<TACIT_HOLE, R...> {
public:
  template <class X> using with = tuple<X, R...>;
};
// value-parameterized containers: hole the ELEMENT type; the extent (a non-type parameter) rides
// along as an ordinary literal — std::array<struct _, 5>::with<int> == std::array<int, 5>. This is the
// user-free grain: the value is a normal template argument, no wrapper, no sentinel. (The mirror —
// holing the *value* and fixing the type — has no natural spelling: a type hole can't sit in a size_t
// slot, so it's intentionally absent; reach for a hand-written metafunction to vary an extent.)
template <std::size_t N> struct array<TACIT_HOLE, N> {
  template <class T> using with = array<T, N>;
};
template <std::size_t E> class span<TACIT_HOLE, E> {
public:
  template <class T> using with = span<T, E>;
};
#undef TACIT_HOLE
} // namespace std
#endif // TACIT_STD_HOLES

// ------------------------------------------------------------------------------------------------
// `$` — the term wrapper (opt-in, `#define TACIT_DOLLAR` before including).
//
//     $(42).abs()        $("abc").length()        $(v).size()
//
// It is exactly `tacit::lift`, under a shorter name: `$(x).f(a...)` is `_.f(a...)(normalize(x))`, the
// closed cell of the term world. `$` is spelled as a *function*, not a macro — the macro form only
// existed to let one name serve both `$<T>` and `$(x)`, and `$` is term-only now. A function keeps
// its namespace, obeys ADL, can be qualified `tacit::$(x)`, and claims nothing from the rest of the
// translation unit, so this header has no include-order rule.
//
// WHY IT IS GATED. `$` is not an identifier in standard C++ — a GCC/Clang extension, rejected under
// `-pedantic-errors`. Everything it spells is reachable conformingly as `tacit::lift`; nothing is
// `$`-only. The gate keeps a strictly-conforming default and makes the trade a per-project choice.
// (It lives here while the surface settles; it wants its own header once it does.)
#ifdef TACIT_DOLLAR
namespace tacit {
template <class X> [[nodiscard]] constexpr auto $(X &&x) { return lift(static_cast<X &&>(x)); }
} // namespace tacit
#ifdef TACIT_USING_UNDERSCORE
using tacit::$;
#endif
#endif

// Opt-in: bring the one symbol into global scope so `#include <tacit/_.hpp>`
// alone suffices (no `using tacit::_;`). Off by default — a header must not
// force a global `_` on every includer (gettext's `#define _`, C++26's
// placeholder `_`, ...). Define it in your own build if you want it.
#ifdef TACIT_USING_UNDERSCORE
using tacit::_;
#endif

// The default path exports exactly one name, `tacit::_`; a single `using tacit::_;` (or the opt-in
// above) is all a caller needs — the vocabulary is reached through the object and the operator
// sections are hidden friends found by ADL. `_` is the only placeholder: domain names are taught to it
// in place with the pre-#define TACIT_VERBS / TACIT_NOUNS hooks (consumed above, at include time), so
// nothing downstream needs the internal generator macros — the header always cleans them up here, one
// path, no TACIT_KEEP_MACROS switch.
//
// One macro survives on the clean path: `TACIT_HAS_REFLECTION`, a feature flag (not a generator), so
// you can `#if` on whether the reflective members (m / field / enum_name / each_field) exist; testing
// that yourself would otherwise mean re-deriving tacit's `__cpp_*` condition.
#undef TACIT_MEMBER
#undef TACIT_ARROW_MEMBER
#undef TACIT_VIEW
#undef TACIT_VIEW_VERBS
#undef TACIT_CPO1
#undef TACIT_CPO2
#undef TACIT_FREE1
#undef TACIT_STD_FREES
#undef TACIT_SECTION
#undef TACIT_UNARY
#undef TACIT_UNARY_POST
#undef TACIT_ASSIGN
#undef TACIT_REFLECT
#undef TACIT_CORE
#undef TACIT_STD_MEMBERS
#undef TACIT_STD_CPOS1
#undef TACIT_TYPE_MEMBER
#undef TACIT_TYPE_TEMPLATE
#undef TACIT_STD_TYPE_MEMBERS
#undef TACIT_FE
#undef TACIT_FE_AGAIN
#undef TACIT_FOR_EACH
#undef TACIT_PARENS
#undef TACIT_EXPAND
#undef TACIT_EXPAND_A
#undef TACIT_EXPAND_B
#undef TACIT_EXPAND_C
#undef TACIT_VOCABULARY_FILE
#undef TACIT_VERBS
#undef TACIT_NOUNS
#undef TACIT_NOUN_TEMPLATES
