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

// The same generator <tacit/_.hpp> consumed for `make`, replayed for `$` (it is macro-only and
// guard-free precisely so it can be included again here), then cleaned up the same way.
#include "detail/make_overloads.hpp"

namespace tacit {
template <class X> [[nodiscard]] constexpr auto $(X &&x) { return lift(static_cast<X &&>(x)); }
TACIT_MAKE_OVERLOADS($)
} // namespace tacit

#undef TACIT_UNPAREN
#undef TACIT_MAKE_BLANK
#undef TACIT_MAKE_SLOT
#undef TACIT_MAKE_ONE
#undef TACIT_MAKE_OVERLOADS
