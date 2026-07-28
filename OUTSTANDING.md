OUTSTANDING
-----------

Repo state as of the last session: working tree clean, `master` at `2661ece`, CI green on all four
jobs (clang++-18, g++-13, modules, packaging) as of `f815969`. 24 tests; `typeapply` fails locally on
Apple clang only — a pre-existing `-Winvalid-specialization` rejection of the experimental
`TACIT_STD_BLANKS` tier, which the CI compilers accept.


1. ~~blank vs hole terminology.~~ **DONE** — `blank` everywhere; `hole<A...>` is now `blank<A...>`,
   `TACIT_STD_HOLES` is `TACIT_STD_BLANKS`, and the header states the settled vocabulary: `_` is the
   *placeholder* (the standard's own word, already used by the `is_tacit_placeholder` tag), a *blank*
   is the gap it leaves, at term or type level.

2. ~~120-column formatting.~~ **DONE** — `.clang-format` ColumnLimit is 120, backslash continuations
   re-padded to column 120, prose comment paragraphs re-wrapped, tests and `tacit.cppm` formatted.
   Not a preference: 238 lines already exceeded the nominal 100 and the longest was exactly 119, so
   105 or 110 would not have covered what the file already needed.

3. ~~`_.get<...>()` treatment; field-style names.~~ **DONE** — added `to<C>` (ranges::to, both kinds
   of argument), `any_cast`, `holds_alternative`, `duration_cast`, and the three pointer casts, all
   reached by ADL so the header gains no includes. Field-style `_.first` / `_.second` added.
   Deliberately NOT added: chrono `floor`/`ceil`/`round` (`<cmath>`'s `floor` is visible, so
   `floor<seconds>(x)` misbehaves — `duration_cast` covers it); `span::first<N>`/`last<N>` (would
   collide with the field-style names, and the runtime `_.subspan(o, n)` already covers it);
   `variant::emplace<T>(a...)` (would be ambiguous with the existing runtime `emplace`).

4. ~~Synthetic sigil operators.~~ **DONE (opt-in: `TACIT_COMBINATORIAL_OPERATORS`)** — `->*` compose
   left-to-right, `<<*` compose right-to-left, `&&&` fanout, `***` product. Exhaustive sweep in
   `tacit_extras.md`: 7194 candidates, 615 stolen by maximal munch, 368 of the 391 practical ones
   compile. Haskell's `&&&`/`***`/`+++` survive C++'s lexer; `|||`/`>>>`/`<<<` do not, which is why
   compose is `->*` (a real, single, unclaimed operator). `->.*` is not a token sequence at all —
   `->` needs an id-expression after it. The named forms (compose/fanout/first/second) already exist
   behind `TACIT_COMBINATORS`; moving them to a sub-namespace is still open.

5. README. **Namespaces fixed** — every range-taking algorithm is now `std::ranges::`; the headline
   example did not compile before. Still outstanding: inferred std types to cut noise, and documenting
   whatever lands from #4 and #6.

6. Need to figure out the best way to split `_` and `$` into `_.hpp` and `$.hpp` with minimal repetition and ideally not needing a shared header (meaning no intra-includes) - potentially a build step produces the two exposed headers.
   *(Parked: "decide later". The fork is whether `$.hpp` must be usable WITHOUT including `_.hpp`. If
   it may include it, the split is ~15 lines and needs no build step; if it must stand alone, a build
   step has to generate both from one source, since the vocabulary tables and `fn` would otherwise be
   duplicated verbatim.)*

7. **OPEN — naming: `tacit::make`.** Unconditional public name, introduced without sign-off, by
   analogy with the standard's factory vocabulary (`make_shared`, `make_pair`, `make_optional`). It
   is the term-level counterpart to `bind`, and like `bind`/`apply`/`quote` it is qualified-only, so
   it never enters anyone's scope — its only oddity is being a common English word.

   The one real argument for changing it: `$` covers both forms under one symbol, while the
   conforming spelling needs two names — `$(x)` == `lift(x)` but `$<F>(a…)` == `make<F>(a…)`. Folding
   the builder into `lift` would make the two surfaces isomorphic, at the cost of `lift` carrying two
   meanings (its stated rule, `lift(x).f(a…) == _.f(a…)(x)`, does not extend to `lift<F>(a…)`).

   Ruled out on inspection: `_::make` (the "one name" promise was never threatened, since
   `tacit::make` is qualified-only — and a static member is reachable through the object, so
   `_.make<V>(1,2,3)` would compile and return a *value*, breaking the invariant that `_.f()` is a
   closure); `tacit::of` (says nothing standalone, and would make `of` mean a third thing beside the
   `::of` applier and the noun projectors); anything of the form `$name` (`$` is an identifier
   *character*, so `$std` lexes as ONE token — `$` can never prefix a name).

8. Named combinators (compose / fanout / first / second, currently behind `TACIT_COMBINATORS`) into a
   sub-namespace — still open, carried over from #4.
