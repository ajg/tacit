// SPDX-FileCopyrightText: 2026 Alvaro J. Genial
// SPDX-License-Identifier: BSL-1.0
// C++20 module interface for tacit. It wraps the headers in the global module fragment and
// re-exports the public names — no second copy of the code, and nothing here that the headers do
// not already define.
//
// ONE module, deliberately: the `_.hpp` / `$.hpp` header split does NOT get a module mirror,
// because the reason for the split does not survive `import`. The headers are split since
// `#include` injects TOKENS — a TU that includes <tacit/$.hpp> lexes `$`, and that is exactly what
// clang's -pedantic-errors rejects, at the lexer, before meaning is involved. An `import` injects
// only NAMES, and a name costs nothing until a consumer spells it: a strictly-conforming TU can
// `import tacit;`, keep to `lift`/`make`, and never lex `$` at all. The modules CI job compiles
// precisely that consumer with -pedantic-errors against this interface. So the opt-in that must be
// per-consumer in the header world is simply free in the module world, and one module suffices.
//
// The one place strictness still bites is THIS translation unit, whose author is building tacit
// rather than their own code: whoever compiles this interface does lex `$.hpp`, so this file will
// not compile under -pedantic-errors. That is deliberate and not worth a knob. An earlier revision
// had a TACIT_NO_DOLLAR opt-out here, which was a mistake for a reason worth recording: a macro
// cannot cross a module boundary, so it could not have been a consumer's choice — it was the
// *producer's*, and it made two different modules that both answer to `import tacit;`, one
// exporting `tacit::$` and one not. A header may vary per TU; that is what a header is. A named
// module varying per build is a trap, and it bought only the ability to put -pedantic-errors on a
// file that is not yours. Build this one TU without it.
//
// Macros do not cross a module boundary in the other direction either: the extension hooks
// (TACIT_VERBS, TACIT_NOUNS, TACIT_SIGILS, TACIT_VIEWS) are reachable only through `#include`, and
// so is `λ` — being a macro, it can never be carried by any named module, which is why
// `#include <tacit/λ.hpp>` is its permanent vehicle.
//
// Verified with clang. GCC's -fmodules-ts (as of 13) does not yet handle this pattern reliably;
// prefer `#include` there.
module;
#include <tacit/$.hpp>
#include <tacit/_.hpp>
export module tacit;

export namespace tacit {
// The blank, and the two closed cells of the term world. `_` names both the object and its type, so
// this one using-declaration carries the type-level surface reached through it (`_::blank<>`,
// `_::value_type::of<...>`) along with the placeholder itself.
using tacit::_;
using tacit::$;
using tacit::lift;
using tacit::make;

// Type level: the blank as a type, and the two grains of partial application over templates.
using tacit::apply;
using tacit::bind;
using tacit::blank;
using tacit::quote;

// Closure combinators, and the tuple-element combinators. Qualified-only names, ungated in the
// headers, so they are ungated here too.
using tacit::compose;
using tacit::fanout;
using tacit::first;
using tacit::second;

using tacit::all_of_element;
using tacit::any_of_element;
using tacit::for_each_element;
using tacit::none_of_element;
using tacit::transform_elements;
} // namespace tacit
