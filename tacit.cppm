// SPDX-License-Identifier: BSL-1.0
// C++20 module interface for tacit. Wraps <tacit/_.hpp> in the global module fragment and re-exports
// the public names, so `import tacit;` brings in `tacit::_` and the type-level `tacit::bind` — no
// second copy of the code.
//
// Macros do not cross a module boundary: the derive generators (TACIT_LIEUTENANT, TACIT_MEMBER,
// TACIT_CORE, TACIT_KEEP_MACROS, TACIT_EXTRA_MEMBERS, ...) are reachable only through
// `#include <tacit/_.hpp>`. `import tacit;` is enough to *use* `_`; deriving your own placeholder
// still uses the header. (This mirrors `import std;`, which likewise exports no C macros.)
//
// The opt-in combinators sit behind TACIT_COMBINATORS. Since that macro must be seen when the header
// is included — here, in the global module fragment — a consumer's `#define` cannot reach them across
// the `import`; build this interface with -DTACIT_COMBINATORS to compile and re-export them.
//
// Verified with clang. GCC's -fmodules-ts (as of 13) does not yet handle this pattern reliably;
// prefer `#include` there.
module;
#include <tacit/_.hpp>
export module tacit;

export namespace tacit {
using tacit::_;
using tacit::bind;
#ifdef TACIT_COMBINATORS
using tacit::fanout;
using tacit::first;
using tacit::second;
using tacit::for_each_element;
using tacit::any_of_element;
using tacit::all_of_element;
using tacit::none_of_element;
using tacit::transform_elements;
#endif
} // namespace tacit
