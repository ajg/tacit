// SPDX-License-Identifier: BSL-1.0
// C++20 module interface for tacit. Wraps the headers in the global module fragment and re-exports
// the public names, so `import tacit;` brings in `tacit::_`, the term wrapper `tacit::$` with its
// conforming spellings `tacit::lift` / `tacit::make`, and the type-level `tacit::bind` /
// `tacit::apply` / `tacit::quote` — no second copy of the code.
//
// ONE module, deliberately — the `_.hpp` / `$.hpp` header split does NOT get a module mirror,
// because the reason for the split does not survive `import`. The headers are split since
// `#include` injects TOKENS: a TU that includes <tacit/$.hpp> lexes `$`, and clang's
// -pedantic-errors rejects that at the lexer. An `import` injects only NAMES, and a name costs
// nothing until the consumer spells it — a strictly-conforming TU can `import tacit;` and keep to
// `lift`/`make`, never lexing `$` at all (the modules CI job proves exactly this, compiling a
// consumer with -pedantic-errors against this interface). So the opt-in that must be per-consumer
// in the header world is simply free in the module world.
//
// The one place strictness still bites is THIS translation unit: whoever builds the interface
// lexes `$.hpp`. For a build that must compile even the interface with -pedantic-errors, define
// TACIT_NO_DOLLAR — the skipped `$` lines are only ever preprocessing tokens, which is
// pedantic-clean (also CI-proven).
//
// Macros do not cross a module boundary: the extension hooks (TACIT_VERBS, TACIT_NOUNS,
// TACIT_COMBINATORS, ...) are reachable only through `#include`; build this interface with
// -DTACIT_COMBINATORS to compile and re-export the combinators. For the same reason `λ` — a macro —
// can never be carried by any named module: `#include <tacit/λ.hpp>` is its permanent vehicle.
//
// Verified with clang. GCC's -fmodules-ts (as of 13) does not yet handle this pattern reliably;
// prefer `#include` there.
module;
#ifdef TACIT_NO_DOLLAR
#include <tacit/_.hpp>
#else
#include <tacit/$.hpp>
#endif
export module tacit;

export namespace tacit {
using tacit::_;
using tacit::apply;
using tacit::bind;
using tacit::lift;
using tacit::make;
using tacit::quote;
#ifndef TACIT_NO_DOLLAR
using tacit::$;
#endif
#ifdef TACIT_COMBINATORS
using tacit::all_of_element;
using tacit::any_of_element;
using tacit::compose;
using tacit::fanout;
using tacit::first;
using tacit::for_each_element;
using tacit::none_of_element;
using tacit::second;
using tacit::transform_elements;
#endif
} // namespace tacit
