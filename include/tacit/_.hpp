// SPDX-License-Identifier: BSL-1.0
#pragma once

// Version as a comparable integer: MAJOR*10000 + MINOR*100 + PATCH. A clean-path macro (kept), so
// downstream code can `#if TACIT_VERSION >= 200` feature-test.
#define TACIT_VERSION_MAJOR 0
#define TACIT_VERSION_MINOR 2
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

#include <array>
#include <cstddef>
#include <functional>
#include <ranges>
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
#include <algorithm>
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
template <class> constexpr bool is_fn_v = false;
template <class F> struct fn;
template <class F> constexpr bool is_fn_v<fn<F>> = true;
template <class T>
concept not_fn = !is_fn_v<std::remove_cvref_t<T>>;

// A "slot" the section fills from a supplied argument: a plain blank (`_`, identity) or an `fn` —
// a *projected* blank, whose projection is applied to the fill before the call.
template <class T> constexpr bool is_slot_v = is_blank_v<T> || is_fn_v<std::remove_cvref_t<T>>;

// A partially-applied operation carrying its bound args and blank markers.
// `invoke` performs the call once every argument is materialised; `bound` holds
// each original argument (a value, or a placeholder standing for a blank).
// Applying it to (subject, fills...) splices the fills into the blank
// positions, left to right, and calls `invoke`.
template <class Invoke, class... Bound> struct section {
  Invoke invoke;
  std::tuple<Bound...> bound;

  static constexpr std::size_t arity = sizeof...(Bound);
  static constexpr std::array<bool, arity> slot_at{is_slot_v<Bound>...};
  static constexpr std::array<bool, arity> proj_at{is_fn_v<std::remove_cvref_t<Bound>>...};
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

  template <std::size_t I, class Fills> constexpr decltype(auto) pick(Fills &&fills) const {
    if constexpr (proj_at[I]) // projected blank: apply the stored fn to the fill (materialised)
      return std::make_tuple(
          std::get<I>(bound)(std::get<slots_before(I)>(std::forward<Fills>(fills))));
    else if constexpr (slot_at[I]) // plain blank: the fill, untouched
      return std::forward_as_tuple(std::get<slots_before(I)>(std::forward<Fills>(fills)));
    else // bound value
      return std::forward_as_tuple(std::get<I>(bound));
  }

  template <class X, class... F>
    requires(sizeof...(F) == slots)
  constexpr decltype(auto) operator()(X &&x, F &&...f) const {
    auto fills = std::forward_as_tuple(std::forward<F>(f)...);
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
      return std::apply(
          [&](auto &&...args) -> decltype(auto) {
            return invoke(std::forward<X>(x), std::forward<decltype(args)>(args)...);
          },
          std::tuple_cat(pick<Is>(fills)...));
    }(std::make_index_sequence<arity>{});
  }
};

template <class Invoke, class... A>
[[nodiscard]] constexpr auto make_section(Invoke invoke, A &&...a) {
  return section<Invoke, std::decay_t<A>...>{invoke, {std::forward<A>(a)...}};
}

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
    return []<class X, class Y>(X&& x, Y&& y) -> decltype(auto)                                    \
             requires requires(X&& xx, Y&& yy) { CPO(xx, yy); } {                                  \
      return CPO(std::forward<X>(x), std::forward<Y>(y));                                          \
    };                                                                                             \
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
    return [](auto&& x, auto&& y) -> decltype(auto)                                                \
             { return std::forward<decltype(x)>(x) op std::forward<decltype(y)>(y); };             \
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
    return tacit::detail::fn{[y = std::forward<Y>(y)](auto&& x) -> decltype(auto)                   \
             { return std::forward<decltype(x)>(x) op y; }};                                         \
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
  TACIT_SECTION(==) TACIT_SECTION(!=) TACIT_SECTION(<) TACIT_SECTION(>)                            \
  TACIT_SECTION(<=) TACIT_SECTION(>=) TACIT_SECTION(+) TACIT_SECTION(-)                            \
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
  /* comma is the pairing section: `_ , _` == (a,b) -> {a,b}; one-sided forms bind the value. */   \
  template <class Y>                                                                               \
    requires tacit::detail::not_fn<Y> && (!std::is_same_v<std::remove_cvref_t<Y>, self>)          \
  [[nodiscard]] friend constexpr auto operator,(self, Y y) {                                        \
    return tacit::detail::fn{[y](auto&& x) { return std::pair{std::forward<decltype(x)>(x), y}; }};\
  }                                                                                                \
  template <class X>                                                                               \
    requires tacit::detail::not_fn<X> && (!std::is_same_v<std::remove_cvref_t<X>, self>)          \
  [[nodiscard]] friend constexpr auto operator,(X x, self) {                                        \
    return tacit::detail::fn{[x](auto&& y) { return std::pair{x, std::forward<decltype(y)>(y)}; }};\
  }                                                                                                \
  [[nodiscard]] friend constexpr auto operator,(self, self) {                                       \
    return [](auto&& a, auto&& b) {                                                                 \
      return std::pair{std::forward<decltype(a)>(a), std::forward<decltype(b)>(b)}; };              \
  }                                                                                                \
  [[nodiscard]] static constexpr auto operator()() {                                               \
    return [](auto&& x) -> decltype(auto) { return std::invoke(std::forward<decltype(x)>(x)); };   \
  }                                                                                                \
  template <class... Y>                                                                            \
  [[nodiscard]] static constexpr auto operator()(Y&&... ys) {                                      \
    return [... ys = std::forward<Y>(ys)](auto&& x, auto&&... zs) -> decltype(auto) {              \
      return std::invoke(std::forward<decltype(x)>(x), ys..., std::forward<decltype(zs)>(zs)...);  \
    };                                                                                             \
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
    return tacit::detail::fn{[i = std::forward<I>(i)](auto&& x) -> decltype(auto)                  \
             { return std::forward<decltype(x)>(x)[i]; }};                                         \
  }                                                                                                \
  TACIT_REFLECT(Self)                                                                              \
  static_assert(true)

// The standard-library vocabulary: one editable table, grouped by role with line breaks. A derived
// placeholder pulls it all with TACIT_STD_MEMBERS(TACIT_MEMBER), or just lists the names it wants.
#define TACIT_STD_MEMBERS(X)                                                                       \
  /* access   */ X(at); X(front); X(back); X(top);                                                 \
  /* capacity */ X(length); X(capacity); X(reserve); X(resize); X(shrink_to_fit); X(max_size);     \
  /* modify   */ X(clear); X(push_back); X(pop_back); X(push_front); X(pop_front); X(push); X(pop);\
                 X(emplace); X(emplace_back); X(emplace_front); X(emplace_hint); X(insert);        \
                 X(insert_or_assign); X(erase); X(extract); X(remove); X(remove_if); X(splice); X(merge);\
                 X(unique); X(sort); X(append); X(assign); X(replace);                             \
  /* lookup   */ X(find); X(count); X(contains); X(lower_bound); X(upper_bound); X(equal_range);   \
  /* string   */ X(substr); X(compare); X(starts_with); X(ends_with); X(rfind); X(find_first_of);  \
                 X(find_last_of); X(find_first_not_of); X(find_last_not_of); X(c_str); X(str);     \
  /* optional */ X(has_value); X(value); X(value_or); X(and_then); X(transform); X(or_else); X(error);\
                 X(index); X(reset);                                                               \
  /* pointer  */ X(get); X(release); X(use_count); X(expired); X(lock); X(owner_before);

#define TACIT_STD_CPOS1(X)                                                                         \
  X(begin,  std::ranges::begin)  X(end,   std::ranges::end)                                        \
  X(cbegin, std::ranges::cbegin) X(cend,  std::ranges::cend)                                       \
  X(rbegin, std::ranges::rbegin) X(rend,  std::ranges::rend)                                       \
  X(size,   std::ranges::size)   X(ssize, std::ranges::ssize)                                      \
  X(empty,  std::ranges::empty)  X(data,  std::ranges::data)
// clang-format on

// Additive extension hook for the one `_`: pre-#define a comma list of bare member-call names before
// including, and each becomes a first-class *verb* on `_` — reachable as `_.name(...)`, composing and
// blank-taking like the built-in vocabulary (`_.name(_)`), on `_` and its projections alike:
//   #define TACIT_VERBS area, perimeter, scale
// This is how you teach `_` a domain vocabulary — there is no separate placeholder object; `_` stays
// the one interface. (Its type-level twin is TACIT_NOUNS, below.)
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
#define TACIT_STD_TYPE_MEMBERS(X)                                                                   \
  /* element */ X(value_type); X(element_type); X(reference); X(const_reference);                  \
                X(pointer); X(const_pointer);                                                       \
  /* size    */ X(size_type); X(difference_type);                                                  \
  /* iterate */ X(iterator); X(const_iterator); X(reverse_iterator); X(const_reverse_iterator);    \
  /* assoc   */ X(key_type); X(mapped_type); X(key_compare); X(value_compare);                     \
  /* alloc   */ X(allocator_type);                                                                 \
  /* pair    */ X(first_type); X(second_type);                                                     \
  /* traits  */ X(char_type); X(traits_type); X(int_type);                                         \
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
template <class F> struct fn {
  F f;
  template <class... A>
    requires requires(F const &g, A &&...a) { g(std::forward<A>(a)...); }
  constexpr decltype(auto) operator()(A &&...a) const { return f(std::forward<A>(a)...); }
  template <class I> [[nodiscard]] constexpr auto operator[](I i) const {
    return tacit::detail::fn{[g = *this, i = std::move(i)](auto &&x) -> decltype(auto) {
      return g(std::forward<decltype(x)>(x))[i];
    }};
  }
  // Operator sections as hidden friends — found by ADL, including across a module boundary
  // (`import tacit;`): `g op value` / `value op g` compose to a unary fn; `g op h` is binary.
#define TACIT_FN_OP(op)                                                                            \
  template <not_fn Y> [[nodiscard]] friend constexpr auto operator op(fn g, Y y) {                 \
    return tacit::detail::fn{                                                                      \
        [g, y](auto &&x) -> decltype(auto) { return g(std::forward<decltype(x)>(x)) op y; }};      \
  }                                                                                                \
  template <not_fn X> [[nodiscard]] friend constexpr auto operator op(X &&x, fn g) {               \
    if constexpr (std::is_copy_constructible_v<std::remove_cvref_t<X>>)                            \
      return tacit::detail::fn{                                                                    \
          [g, x = std::forward<X>(x)](auto &&y) -> decltype(auto) {                                \
            return x op g(std::forward<decltype(y)>(y)); }};                                       \
    else /* non-copyable (stream): bind by reference, e.g. cout << _.size() */                     \
      return tacit::detail::fn{                                                                    \
          [g, &x](auto &&y) -> decltype(auto) { return x op g(std::forward<decltype(y)>(y)); }};   \
  }                                                                                                \
  template <class G> [[nodiscard]] friend constexpr auto operator op(fn g, fn<G> h) {              \
    return [g, h](auto &&a, auto &&b) -> decltype(auto) {                                          \
      return g(std::forward<decltype(a)>(a)) op h(std::forward<decltype(b)>(b));                   \
    };                                                                                             \
  }
  TACIT_FN_OP(==) TACIT_FN_OP(!=) TACIT_FN_OP(<) TACIT_FN_OP(>) TACIT_FN_OP(<=) TACIT_FN_OP(>=)
  TACIT_FN_OP(+) TACIT_FN_OP(-) TACIT_FN_OP(*) TACIT_FN_OP(/) TACIT_FN_OP(%) TACIT_FN_OP(^)
  TACIT_FN_OP(&) TACIT_FN_OP(|) TACIT_FN_OP(&&) TACIT_FN_OP(||) TACIT_FN_OP(<<) TACIT_FN_OP(>>)
#undef TACIT_FN_OP
  // Unary operators compose through the projection: `op g` == x -> op g(x).
#define TACIT_FN_UNARY(op)                                                                         \
  [[nodiscard]] friend constexpr auto operator op(fn g) {                                          \
    return tacit::detail::fn{                                                                      \
        [g](auto &&x) -> decltype(auto) { return op g(std::forward<decltype(x)>(x)); }};           \
  }
#define TACIT_FN_UNARY_POST(op)                                                                    \
  [[nodiscard]] friend constexpr auto operator op(fn g, int) {                                     \
    return tacit::detail::fn{                                                                      \
        [g](auto &&x) -> decltype(auto) { return g(std::forward<decltype(x)>(x)) op; }};           \
  }
  TACIT_FN_UNARY(*) TACIT_FN_UNARY(-) TACIT_FN_UNARY(+) TACIT_FN_UNARY(!) TACIT_FN_UNARY(~) TACIT_FN_UNARY(&)
  TACIT_FN_UNARY(++) TACIT_FN_UNARY(--) TACIT_FN_UNARY_POST(++) TACIT_FN_UNARY_POST(--)
#undef TACIT_FN_UNARY
#undef TACIT_FN_UNARY_POST
  // (`|` is a bitwise section like `&`, above — general composition is `tacit::compose`, not `f | g`.)
  // Vocabulary — each name composes through the projection (member chaining). No-blank args only.
#define TACIT_FN_MEMBER(NAME)                                                                      \
  template <class... A> [[nodiscard]] constexpr auto NAME(A &&...a) const {                        \
    return tacit::detail::fn{                                                                      \
        [g = *this, ... a = std::forward<A>(a)]<class X>(X &&x) -> decltype(auto)                  \
          requires requires(X &&xx, A &&...aa) {                                                   \
            std::declval<fn const &>()(std::forward<X>(xx)).NAME(std::forward<A>(aa)...);          \
          } { return g(std::forward<X>(x)).NAME(a...); }};                                         \
  }                                                                                                \
  static_assert(true)
#define TACIT_FN_CPO1(NAME, CPO)                                                                   \
  [[nodiscard]] constexpr auto NAME() const {                                                      \
    return tacit::detail::fn{[g = *this]<class X>(X &&x) -> decltype(auto)                         \
             requires requires(X &&xx) { CPO(std::declval<fn const &>()(std::forward<X>(xx))); }   \
             { return CPO(g(std::forward<X>(x))); }};                                              \
  }
  TACIT_STD_MEMBERS(TACIT_FN_MEMBER)
  TACIT_FOR_EACH(TACIT_FN_MEMBER, TACIT_VERBS)
  TACIT_STD_CPOS1(TACIT_FN_CPO1)
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
};
inline constexpr arrow arrow_v;
} // namespace detail

// The one type `_`: the placeholder's own type. It carries the full std vocabulary (plus any
// TACIT_VERBS / TACIT_NOUNS) and the core for the value side, and doubles as the type-level `_` — a blank
// for `bind` and a projection namespace (`_::name::of<X>`). The value `_` (next) hides the type in
// ordinary lookup, but a name before `::` is looked up considering only types, namespaces, and
// templates, so `_::name` reaches this struct past the value; elsewhere reach the type via `struct _`.
struct _ {
  TACIT_STD_MEMBERS(TACIT_MEMBER)
  TACIT_FOR_EACH(TACIT_MEMBER, TACIT_VERBS)
  TACIT_STD_CPOS1(TACIT_CPO1)
  TACIT_CPO2(swap, std::ranges::swap)
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
// the elaborated-type-specifier `struct _` — the tag-namespace twin of the value (the old C trick
// where a class and a variable share a name). Fixed arguments then stay plain types, no wrapper:
//
//   bind<std::vector, struct _>::with<int>              // std::vector<int>
//   bind<std::map, int, struct _>::with<double>         // std::map<int, double>   (partial)
//   bind<std::map, struct _, struct _>::with<char, int> // std::map<char, int>
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
typedef struct _ blank; // handle on the blank type
// Walk the argument list, replacing each `struct _` blank with the next of Xs...; keep fixed types.
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

template <template <class...> class F, class... Args> struct bind {
  template <class... Xs>
  using with = typename detail::fill_slots<F, std::tuple<>, std::tuple<Xs...>, Args...>::type;
};

// The general primitive that subsumes `bind` and curries BOTH grains under one op. `bind` fixes the
// template and holes among its *arguments*; `apply` additionally lets the *template itself* be a hole.
// Quote a template into a type with `quote<F>` (a template can't sit in a type slot unquoted — packs
// are single-kind), then `apply<Slots...>::with<Fills...>` fills each `struct _` slot — template or
// argument — left to right:
//
//   apply<quote<std::map>, struct _, struct _>::with<int, char>   // std::map<int,char>  (fix template)
//   apply<struct _, int, char>::with<quote<std::map>>             // std::map<int,char>  (fix args)
//   apply<struct _, int, struct _>::with<quote<std::map>, char>   // std::map<int,char>  (hole both)
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
  std::apply([&](auto &&...xs) { (f(std::forward<decltype(xs)>(xs)), ...); }, std::forward<Tup>(t));
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
#define TACIT_HOLE struct ::tacit::_ // elaborated: force type lookup past the shadowing value `_`
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
#undef TACIT_VERBS
#undef TACIT_NOUNS
#undef TACIT_NOUN_TEMPLATES
