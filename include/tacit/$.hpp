// SPDX-License-Identifier: BSL-1.0
#pragma once

// `$` — the short spelling of the type world and the term lift. Two lines over machinery that already
// lives in <tacit/_.hpp>; nothing here is new capability, only notation:
//
//     $<int>::of<std::vector>            //  tacit::hole<int>::of<std::vector>
//     $<>::as<std::map>::with<int, char> //  tacit::hole<>::as<...>::with<...>
//     $(42).size()                       //  tacit::lift(42).size()
//
// The alias and the macro coexist because a function-like macro fires only on `(`, and `$<…>` has no
// paren — so one name serves both without a probe. `$` never appears bare (there is no `$ > $`), which
// is exactly why it can be a template where `_` cannot: the name-kind rule only bites a name that must
// also be usable on its own, and `_` must be.
//
// WHY THIS IS OPT-IN. `$` is not an identifier in standard C++ — it is a GCC/Clang extension, rejected
// under `-pedantic-errors` (`'$' in identifier`). Including this header therefore trades conformance
// for brevity, which is your call to make per project, not the library's to make for you. Everything
// here is reachable conformingly through `tacit::hole` and `tacit::lift`; nothing is `$`-only.
//
// TWO RULES FOR USING IT.
//   1. Include it LAST. `$(…)` is a function-like macro and claims `$(` for the rest of the
//      translation unit, so it must not be in scope while other headers are parsed.
//   2. Application code only, never a public header. A library that includes this re-imposes both the
//      extension and the macro on every downstream consumer, who never chose either.
#include <tacit/_.hpp>

template <class... A> using $ = tacit::hole<A...>;
#define $(...) tacit::lift(__VA_ARGS__)
