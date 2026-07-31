// SPDX-License-Identifier: BSL-1.0
// C++20 module interface for `$` — the module mirror of the <tacit/_.hpp> / <tacit/$.hpp> header
// split. `import tacit;` is the strictly-conforming surface; `import tacit.dollar;` is the opt-in
// that adds `tacit::$`, exactly as including <tacit/$.hpp> is in the header world. A separate module
// because that is what keeps the opt-in PER-CONSUMER: a macro gate cannot cross an `import` (it
// would have to be decided once, when the interface is built), but a second import is a choice each
// importer makes. Named `tacit.dollar` since a module name is dots-and-identifiers only — `$` cannot
// appear in it.
//
// The consumer's TU still has to lex `$`, so the same portability note applies as for the header:
// GCC/Clang accept it; `-pedantic-errors` does not (use `import tacit;` and `lift`/`make` there).
module;
#include <tacit/$.hpp>
export module tacit.dollar;

// Only `$` itself: `_`, `lift`, `make` and the rest come from `import tacit;`, which any user of `$`
// will want anyway (the two modules' global-module entities are the same, so the imports compose).
export namespace tacit {
using tacit::$;
} // namespace tacit
