// SPDX-FileCopyrightText: 2026 Alvaro J. Genial
// SPDX-License-Identifier: BSL-1.0
#pragma once

// `$` — the canonical short name of the term wrapper:
//
//     $(42).abs()        $("abc").length()        $(v).size()       // == tacit::lift
//     $<std::vector>(1, 2, 3)                                       // == tacit::make
//     $<std::set, _, std::greater<>>(3, 1, 2)                       //    ...partial CTAD included
//
// `$(x)` is exactly `tacit::lift(x)` — `$(x).f(a...)` is `_.f(a...)(normalize(x))`, the closed cell
// of the term world — and `$<F>(a...)` is exactly `tacit::make<F>(a...)`, the other closed cell:
// `$(x)` adopts a value that exists, `$<F>(...)` builds one. The two can never collide, because a
// call with no explicit template arguments cannot deduce `F`, so `$(42)` only ever reaches the lift.
//
// `$` is a *function*, not a macro — it keeps its namespace, obeys ADL, can be qualified
// `tacit::$(x)`, and claims nothing from the rest of the translation unit. But `$` in an identifier
// is a GCC/Clang extension, rejected under `-pedantic-errors`, which is why it lives in this header
// rather than <tacit/_.hpp>: INCLUDING THIS FILE IS THE OPT-IN — no macro, no gate — and the main
// header stays strictly conforming. Nothing is `$`-only: `tacit::lift` and `tacit::make` are the
// same functions under their conforming names, and a `-pedantic-errors` build simply keeps to those.
//
// This is as far as `$` can go into the type world, and the boundary is the language's, not a
// choice: `$` is a function, so `$<std::map>::anything` is ill-formed — a qualified name cannot
// refer into a specialisation of a function template. `$<F>` yields values; naming a type still
// wants `blank`/`bind`/`apply`/`rebind`, or `decltype` around a `make`.
#include "_.hpp"

// A VERBATIM COPY of the generator block in <tacit/_.hpp> (which consumed and #undef'd it for
// `make`), replayed here to emit the same sixteen partial-CTAD shapes as `$`. The duplication is
// forced: C++ cannot give an existing function-template overload set a second name, and `$` cannot
// forward to `make` through one signature because a single pack cannot mix type and non-type
// parameters — which is why there are sixteen shapes in the first place. `tests/callables.cpp`
// pins the two copies to identical behaviour, so drift is caught rather than discovered.
#define TACIT_MKU(...) __VA_ARGS__ /* strip one layer of parens */
/* Position n of a shape, in the two places it has to appear. C = a fixed Class, S = a deducing
   Slot; P spells the template PARAMETER, F the entry in the FIXED list handed to `make_`. */
#define TACIT_PC(n) class A##n
#define TACIT_PS(n) tacit::detail::blank_value auto A##n
#define TACIT_FC(n) A##n
#define TACIT_FS(n) tacit::blank<>
#define TACIT_MK(NAME, PARAMS, FIXED) \
  template <template <class...> class F, TACIT_MKU PARAMS, class... B> \
  [[nodiscard]] constexpr auto NAME(auto &&...a) { \
    return tacit::detail::make_<F, TACIT_MKU FIXED, B...>(static_cast<decltype(a)>(a)...); \
  }
#define TACIT_MK1(N, a) TACIT_MK(N, (TACIT_P##a(0)), (TACIT_F##a(0)))
#define TACIT_MK2(N, a, b) TACIT_MK(N, (TACIT_P##a(0), TACIT_P##b(1)), (TACIT_F##a(0), TACIT_F##b(1)))
#define TACIT_MK3(N, a, b, c) \
  TACIT_MK(N, (TACIT_P##a(0), TACIT_P##b(1), TACIT_P##c(2)), (TACIT_F##a(0), TACIT_F##b(1), TACIT_F##c(2)))
#define TACIT_MK4(N, a, b, c, d) \
  TACIT_MK(N, (TACIT_P##a(0), TACIT_P##b(1), TACIT_P##c(2), TACIT_P##d(3)), \
           (TACIT_F##a(0), TACIT_F##b(1), TACIT_F##c(2), TACIT_F##d(3)))
/* The table: arity, then one letter per position. Every row ends in S — a TRAILING fixed type needs
   no overload of its own, it rides the variadic tail — so the rows are the binary count over the
   positions before it: 1 + 2 + 4 + 8 = 15, plus the no-prefix base case = the sixteen shapes. */
#define TACIT_MKN(NAME) \
  template <template <class...> class F, class... B> [[nodiscard]] constexpr auto NAME(auto &&...a) { \
    return tacit::detail::make_<F, B...>(static_cast<decltype(a)>(a)...); \
  } \
  TACIT_MK1(NAME, S) \
  TACIT_MK2(NAME, C, S) \
  TACIT_MK2(NAME, S, S) \
  TACIT_MK3(NAME, C, C, S) \
  TACIT_MK3(NAME, S, C, S) \
  TACIT_MK3(NAME, C, S, S) \
  TACIT_MK3(NAME, S, S, S) \
  TACIT_MK4(NAME, C, C, C, S) \
  TACIT_MK4(NAME, S, C, C, S) \
  TACIT_MK4(NAME, C, S, C, S) \
  TACIT_MK4(NAME, S, S, C, S) \
  TACIT_MK4(NAME, C, C, S, S) \
  TACIT_MK4(NAME, S, C, S, S) \
  TACIT_MK4(NAME, C, S, S, S) \
  TACIT_MK4(NAME, S, S, S, S)

namespace tacit {
template <class X> [[nodiscard]] constexpr auto $(X &&x) { return lift(static_cast<X &&>(x)); }
TACIT_MKN($)
} // namespace tacit

#undef TACIT_MKU
#undef TACIT_PC
#undef TACIT_PS
#undef TACIT_FC
#undef TACIT_FS
#undef TACIT_MK
#undef TACIT_MK1
#undef TACIT_MK2
#undef TACIT_MK3
#undef TACIT_MK4
#undef TACIT_MKN
