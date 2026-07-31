# Asks

Things we wanted and could not get. Each comes with what was tried and where the attempt is written
up (`tacit_extras.md` is the lab notebook). If you can do one of these — or can prove one more of
them impossible — open an issue or a PR; being proven wrong here is the good outcome.

## 1. Make `_<>` work

The white whale. `_` is an object; we want the *same name* to also work at the type level —
`_<int>`, or `std::map<_, int>` with a bare `_`. Every route we know hits the same wall: **a name is
one kind of entity per scope**, and `<` after a bare name demands that the name be a template. What
exists today is the conforming consolation tier — `_::blank<>`, `bind`/`apply`/`quote`, and
`std::map<struct _, int>::with<char>` (experimental) — all documented under *Four quadrants* in
`tacit_extras.md`, along with everything that failed and why.

What a win looks like: any spelling where `_` (that `_`, not a cousin) names both the value and a
type-level blank, on any conforming compiler — or a language-evolution path that gets there
(universal template parameters à la P1985 would; is anything moving?). Partial credit for a trick we
haven't tried, even if it dies interestingly.

## 2. A conforming `$`

`$(x)` and `$<F>(a…)` are the canonical spellings of the term wrapper, but `$` in an identifier is a
GCC/Clang extension — `-pedantic-errors` rejects it at the lexer, and MSVC-conforming mode likewise.
Dead ends so far: `$` (a UCN may not designate `$`-as-identifier — `$` has no XID property, and
C++23's UAX #31 identifiers make that final); C++26's P2558 added `$` to the basic character set for
*literals*, not identifiers. The conforming fallbacks are `lift`/`make`, which work but are two
names for one idea.

What a win looks like: any conforming route to the glyph — or a committee-facing argument that
`$` in identifiers should be standardized (it is the most requested identifier character in every
adjacent language). We would happily co-author that paper.

## 3. Toolchains beyond the three

CI now covers clang, GCC, MSVC, clang-cl, and AppleClang; Intel ICX and EDG are spot-checked green
via Compiler Explorer. Every one of those additions found something or confirmed something, so the
ask stands for whatever is left: **embedded and cross toolchains** (ARM/AArch64 GCC, QNX, Green
Hills), **older-but-alive compilers** anyone is stuck on, **libstdc++ vs libc++ vs MSVC STL edge
cases** we haven't hit, and **nvcc** if you actually use CUDA. A green report is useful; a red one
with the error text is better.

## 4. Stateless *composed* closures

`decltype(_ > _)` is empty and default-constructible — a drop-in comparator type for
`std::set<int, decltype(_ > _)>`. But a *composed* closure (`_.size() < _.size()`) is not: sections
are built from capturing lambdas, and any capture deletes the default constructor, even when
everything captured is itself stateless and default-constructible. The information is all there at
the type level; we just lose it at the first capture.

What a win looks like: composition machinery where stateless × stateless stays stateless (empty
members plus `[[no_unique_address]]`? closure types rebuilt via NTTPs?), without giving up the
capture-based fast path for stateful operands. This one smells solvable and is probably the highest
value-per-effort item on the list.

## 5. Positional blanks — do you need them?

By design every `_` is a distinct blank, so `x * x` (one argument used twice) and `flip`-style
reordering are inexpressible point-free — that's what `λ` is for. A designed-but-unbuilt extension
would add placeholders with identity (`$1`/`$2`, `$x`/`$y`, or the conforming gem `1_`/`2_` via
`operator""_` — all three spellings are analyzed at the end of the sigil-sweep notes in
`tacit_extras.md`, along with the slot-unification cost in `fn`). The ask is half demand-signal,
half design review: would you use them, which spelling, and do you see a cheaper unification
mechanism than a slot mask on every section?

## 6. Modules beyond clang

`import tacit;` works and is CI-proven — on clang. GCC's modules implementation has not handled the
wrap-a-header-in-the-GMF pattern for us; we haven't retried recent GCC seriously, and MSVC modules
are unexplored. If you can make `tacit.cppm` build on either, the CI leg is yours to write.

## Standing invitation: break a proof

The lab notebook contains impossibility arguments we act on. If any is wrong, everything downstream
of it improves, so refutations are first-class contributions:

- **λ can only be a macro** (name introduction is lexical, so no named module can ever carry it),
  **its braces cannot be elided** (a macro cannot transform tokens that follow it), and **the
  `return` cannot go** without body-as-argument comma rules — see "λ: the lambda head".
- **`$` can never participate in an operator sigil** (it lexes as an identifier character:
  juxtaposition or operand renaming, always) — see the sigil sweep's second pass.
- **Haskell's `|||`, `>>>`, `<<<`, `<*>`, `<|>`, `<$>`, `>=>` are unreachable as C++ sigils**
  (maximal munch or missing prefix forms) — same section, full table.
- **Bare `\(…)` cannot exist** (after `\` the grammar permits exactly one thing outside literals: a
  universal-character-name) — though `\u{3BB}(x)` gets surprisingly close.
