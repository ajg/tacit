// SPDX-License-Identifier: BSL-1.0
#pragma once
// ============================================================================================
//  _.hpp  —  a point-free "_" object with a first-class standard-library
//  vocabulary, and a
//            reusable core for deriving your own domain-specific placeholders
// ============================================================================================
//
//  `_` (of type `tacit::lieutenant`) is a stateless global object whose members
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
//  DERIVE YOUR OWN.  The vocabulary-independent machinery (operator sections,
//  application, the reflective hatch) lives in TACIT_CORE(Self). The shortest
//  way to make a placeholder is TACIT_LIEUTENANT, which declares the type and
//  its object in one statement:
//
//      #define TACIT_KEEP_MACROS
//      #include <tacit/_.hpp>
//      namespace bank {
//        TACIT_LIEUTENANT(teller, it, deposit, balance, freeze);   // type +
//        object + your methods
//      }
//      // now:  ranges::sort(accounts, {}, bank::it.balance());
//      bank::it.deposit(_)(acct, 100);
//
//  Or write the struct yourself — one member per line, then the core (each line
//  semicolon-clean):
//
//      struct teller {
//        TACIT_MEMBER(deposit);              // one name per line (or
//        TACIT_MEMBERS(a, b, c); ) TACIT_MEMBER(balance);
//        TACIT_STD_MEMBERS(TACIT_MEMBER)     // and/or pull the whole std
//        vocabulary (bulk, no ;) TACIT_CORE(teller);
//      };
//
//  These need the generator macros, undefined by default (so a plain include
//  exports only `_`);
//  `#define TACIT_KEEP_MACROS` before including to keep them. To instead add
//  names to the built-in
//  `_`, pre-#define TACIT_EXTRA_MEMBERS(X) (no TACIT_KEEP_MACROS needed) — see
//  below.
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
//  ON THE NAME.  "tacit" is point-free programming; "lieutenant" is French
//  "lieu tenant" — literally "place-holding" — sidestepping the loaded English
//  "placeholder".
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

//  Apply TACIT_MEMBER to a list (one name or many, up to 64). Semicolon-terminated and clean:
//      TACIT_MEMBERS(deposit, balance, freeze);
//      TACIT_LIEUTENANT(teller, it, deposit, balance);   // whole placeholder: type + instance
#define TACIT_PARENS ()
#define TACIT_EXPAND(...)   TACIT_EXPAND_C(TACIT_EXPAND_C(TACIT_EXPAND_C(TACIT_EXPAND_C(__VA_ARGS__))))
#define TACIT_EXPAND_C(...) TACIT_EXPAND_B(TACIT_EXPAND_B(TACIT_EXPAND_B(TACIT_EXPAND_B(__VA_ARGS__))))
#define TACIT_EXPAND_B(...) TACIT_EXPAND_A(TACIT_EXPAND_A(TACIT_EXPAND_A(TACIT_EXPAND_A(__VA_ARGS__))))
#define TACIT_EXPAND_A(...) __VA_ARGS__
#define TACIT_FE(a, ...) TACIT_MEMBER(a) __VA_OPT__(; TACIT_FE_AGAIN TACIT_PARENS (__VA_ARGS__))
#define TACIT_FE_AGAIN() TACIT_FE
#define TACIT_MEMBERS(...) __VA_OPT__(TACIT_EXPAND(TACIT_FE(__VA_ARGS__)))
#define TACIT_LIEUTENANT(Type, Obj, ...)                                                           \
  struct Type {                                                                                    \
    TACIT_MEMBERS(__VA_ARGS__) __VA_OPT__(;)                                                        \
    TACIT_CORE(Type);                                                                              \
  };                                                                                               \
  inline constexpr Type Obj

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
    return tacit::detail::fn{[x = std::forward<X>(x)](auto&& y) -> decltype(auto)                  \
             { return x op std::forward<decltype(y)>(y); }};                                       \
  }                                                                                                \
  [[nodiscard]] friend constexpr auto operator op(self, self) {                                    \
    return [](auto&& x, auto&& y) -> decltype(auto)                                                \
             { return std::forward<decltype(x)>(x) op std::forward<decltype(y)>(y); };             \
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

// Additive extension hook for the default `_`: pre-#define this before
// including to add your own first-class names (semicolon-separated, like the
// table above), e.g.
//   #define TACIT_EXTRA_MEMBERS(X) X(area); X(perimeter);
#ifndef TACIT_EXTRA_MEMBERS
#define TACIT_EXTRA_MEMBERS(X)
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
  template <not_fn X> [[nodiscard]] friend constexpr auto operator op(X x, fn g) {                 \
    return tacit::detail::fn{                                                                      \
        [g, x](auto &&y) -> decltype(auto) { return x op g(std::forward<decltype(y)>(y)); }};      \
  }                                                                                                \
  template <class G> [[nodiscard]] friend constexpr auto operator op(fn g, fn<G> h) {              \
    return [g, h](auto &&a, auto &&b) -> decltype(auto) {                                          \
      return g(std::forward<decltype(a)>(a)) op h(std::forward<decltype(b)>(b));                   \
    };                                                                                             \
  }
  TACIT_FN_OP(==) TACIT_FN_OP(!=) TACIT_FN_OP(<) TACIT_FN_OP(>) TACIT_FN_OP(<=) TACIT_FN_OP(>=)
  TACIT_FN_OP(+) TACIT_FN_OP(-) TACIT_FN_OP(*) TACIT_FN_OP(/) TACIT_FN_OP(%) TACIT_FN_OP(^)
#undef TACIT_FN_OP
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
  TACIT_EXTRA_MEMBERS(TACIT_FN_MEMBER)
  TACIT_STD_CPOS1(TACIT_FN_CPO1)
#undef TACIT_FN_MEMBER
#undef TACIT_FN_CPO1
};
} // namespace detail
// clang-format on

// The default placeholder: the full std vocabulary (plus any
// TACIT_EXTRA_MEMBERS) and the core.
struct lieutenant {
  TACIT_STD_MEMBERS(TACIT_MEMBER)
  TACIT_EXTRA_MEMBERS(TACIT_MEMBER)
  TACIT_STD_CPOS1(TACIT_CPO1)
  TACIT_CPO2(swap, std::ranges::swap)
  TACIT_CORE(lieutenant);
};

inline constexpr lieutenant _;

// ------------------------------------------------------------------------------------------------
// Heterogeneous element combinators: drive a callable over the elements of a
// tuple-like. Built on std::apply + fold-expressions (C++23); a `template for`
// (C++26) path can later extend them to arbitrary aggregates and reflection
// ranges. They are `_`-agnostic (any callable works) but pair naturally with
// `_`'s closures, e.g. transform_elements(t, _.size()) or any_of_element(t,
// _.empty()).
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

} // namespace tacit

// Opt-in: bring the one symbol into global scope so `#include <tacit/_.hpp>`
// alone suffices (no `using tacit::_;`). Off by default — a header must not
// force a global `_` on every includer (gettext's `#define _`, C++26's
// placeholder `_`, ...). Define it in your own build if you want it.
#ifdef TACIT_USING_UNDERSCORE
using tacit::_;
#endif

// The default path exports exactly one name, `tacit::_`; a single `using
// tacit::_;` (or the opt-in above) is all a caller needs — the vocabulary is
// reached through the object and the operator sections are hidden friends found
// by ADL. To keep that promise the generator macros are undefined below. To
// derive your own placeholder, `#define TACIT_KEEP_MACROS` before including;
// the TACIT_MEMBER / TACIT_CORE / TACIT_STD_MEMBERS macros then stay available.
//
// One macro is kept on the clean path: `TACIT_HAS_REFLECTION`, a feature flag
// (not a generator), so you can `#if` on whether the reflective members (m /
// field / enum_name / each_field) exist; testing that yourself would otherwise
// mean re-deriving tacit's `__cpp_*` condition.
#ifndef TACIT_KEEP_MACROS
#undef TACIT_MEMBER
#undef TACIT_CPO1
#undef TACIT_CPO2
#undef TACIT_SECTION
#undef TACIT_REFLECT
#undef TACIT_CORE
#undef TACIT_STD_MEMBERS
#undef TACIT_STD_CPOS1
#undef TACIT_MEMBERS
#undef TACIT_LIEUTENANT
#undef TACIT_FE
#undef TACIT_FE_AGAIN
#undef TACIT_PARENS
#undef TACIT_EXPAND
#undef TACIT_EXPAND_A
#undef TACIT_EXPAND_B
#undef TACIT_EXPAND_C
#undef TACIT_EXTRA_MEMBERS
#endif
