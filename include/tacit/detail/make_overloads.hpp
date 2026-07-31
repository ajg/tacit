// SPDX-License-Identifier: BSL-1.0
// The `make` overload-set generator, shared verbatim by <tacit/_.hpp> (as `tacit::make`) and
// <tacit/$.hpp> (as `tacit::$`). Macros only — no declarations — so it has NO include guard: each
// consumer includes it, expands TACIT_MAKE_OVERLOADS(name), and #undefs the five macros again,
// keeping the clean-path promise that no generator macro survives an include. That is also why the
// two public headers can be included in either order: neither depends on the other having left
// macros behind. (The inner #ifndef only suppresses redefinition if a consumer forgets the #undef.)
//
// TACIT_MAKE_OVERLOADS(NAME) emits `NAME<F, ...>(a...)` == partial CTAD: every `tacit::blank<>` (a
// `_` among the explicit template arguments arrives as one) means "deduce this position", to depth
// four. The engine is `tacit::detail::make_` in <tacit/_.hpp>; these macros only spell the sixteen
// type/value parameter shapes the language cannot express as one variadic signature (a single pack
// cannot mix type and non-type parameters — the same kind wall the type-level notes describe).
#ifndef TACIT_MAKE_OVERLOADS
#define TACIT_UNPAREN(...) __VA_ARGS__
#define TACIT_MAKE_BLANK tacit::blank<>                 /* the fixed-list marker: deduce here */
#define TACIT_MAKE_SLOT tacit::detail::blank_value auto /* the parameter slot a `_` argument binds */
#define TACIT_MAKE_ONE(NAME, PARAMS, FIXED)                                                                            \
  template <template <class...> class F, TACIT_UNPAREN PARAMS, class... B>                                             \
  [[nodiscard]] constexpr auto NAME(auto &&...a) {                                                                     \
    return tacit::detail::make_<F, TACIT_UNPAREN FIXED, B...>(static_cast<decltype(a)>(a)...);                         \
  }
#define TACIT_MAKE_OVERLOADS(NAME)                                                                                     \
  template <template <class...> class F, class... B> [[nodiscard]] constexpr auto NAME(auto &&...a) {                  \
    return tacit::detail::make_<F, B...>(static_cast<decltype(a)>(a)...);                                              \
  }                                                                                                                    \
  TACIT_MAKE_ONE(NAME, (TACIT_MAKE_SLOT A0), (TACIT_MAKE_BLANK))                                                       \
  TACIT_MAKE_ONE(NAME, (class A0, TACIT_MAKE_SLOT A1), (A0, TACIT_MAKE_BLANK))                                         \
  TACIT_MAKE_ONE(NAME, (TACIT_MAKE_SLOT A0, TACIT_MAKE_SLOT A1), (TACIT_MAKE_BLANK, TACIT_MAKE_BLANK))                 \
  TACIT_MAKE_ONE(NAME, (class A0, class A1, TACIT_MAKE_SLOT A2), (A0, A1, TACIT_MAKE_BLANK))                           \
  TACIT_MAKE_ONE(NAME, (TACIT_MAKE_SLOT A0, class A1, TACIT_MAKE_SLOT A2), (TACIT_MAKE_BLANK, A1, TACIT_MAKE_BLANK))   \
  TACIT_MAKE_ONE(NAME, (class A0, TACIT_MAKE_SLOT A1, TACIT_MAKE_SLOT A2), (A0, TACIT_MAKE_BLANK, TACIT_MAKE_BLANK))   \
  TACIT_MAKE_ONE(NAME, (TACIT_MAKE_SLOT A0, TACIT_MAKE_SLOT A1, TACIT_MAKE_SLOT A2),                                   \
                 (TACIT_MAKE_BLANK, TACIT_MAKE_BLANK, TACIT_MAKE_BLANK))                                               \
  TACIT_MAKE_ONE(NAME, (class A0, class A1, class A2, TACIT_MAKE_SLOT A3), (A0, A1, A2, TACIT_MAKE_BLANK))             \
  TACIT_MAKE_ONE(NAME, (TACIT_MAKE_SLOT A0, class A1, class A2, TACIT_MAKE_SLOT A3),                                   \
                 (TACIT_MAKE_BLANK, A1, A2, TACIT_MAKE_BLANK))                                                         \
  TACIT_MAKE_ONE(NAME, (class A0, TACIT_MAKE_SLOT A1, class A2, TACIT_MAKE_SLOT A3),                                   \
                 (A0, TACIT_MAKE_BLANK, A2, TACIT_MAKE_BLANK))                                                         \
  TACIT_MAKE_ONE(NAME, (TACIT_MAKE_SLOT A0, TACIT_MAKE_SLOT A1, class A2, TACIT_MAKE_SLOT A3),                         \
                 (TACIT_MAKE_BLANK, TACIT_MAKE_BLANK, A2, TACIT_MAKE_BLANK))                                           \
  TACIT_MAKE_ONE(NAME, (class A0, class A1, TACIT_MAKE_SLOT A2, TACIT_MAKE_SLOT A3),                                   \
                 (A0, A1, TACIT_MAKE_BLANK, TACIT_MAKE_BLANK))                                                         \
  TACIT_MAKE_ONE(NAME, (TACIT_MAKE_SLOT A0, class A1, TACIT_MAKE_SLOT A2, TACIT_MAKE_SLOT A3),                         \
                 (TACIT_MAKE_BLANK, A1, TACIT_MAKE_BLANK, TACIT_MAKE_BLANK))                                           \
  TACIT_MAKE_ONE(NAME, (class A0, TACIT_MAKE_SLOT A1, TACIT_MAKE_SLOT A2, TACIT_MAKE_SLOT A3),                         \
                 (A0, TACIT_MAKE_BLANK, TACIT_MAKE_BLANK, TACIT_MAKE_BLANK))                                           \
  TACIT_MAKE_ONE(NAME, (TACIT_MAKE_SLOT A0, TACIT_MAKE_SLOT A1, TACIT_MAKE_SLOT A2, TACIT_MAKE_SLOT A3),               \
                 (TACIT_MAKE_BLANK, TACIT_MAKE_BLANK, TACIT_MAKE_BLANK, TACIT_MAKE_BLANK))
#endif // TACIT_MAKE_OVERLOADS
