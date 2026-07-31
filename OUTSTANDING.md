OUTSTANDING
-----------

Repo state: CI matrix is clang++-18/22 and g++-13/16 plus modules and packaging jobs. The
`typeapply` local failure on Apple clang is resolved: libc++ 21 hard-bans specializing `std::tuple`
(`[[clang::no_specializations]]`), so the experimental std-blanks header skips the tuple cell there
and exposes `TACIT_HAS_STD_TUPLE_BLANKS` to feature-test it. Only `tuple` is marked as of libc++ 21;
`pair`/`vector`/`set`/`map` are not.


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

4. ~~Synthetic sigil operators.~~ **DONE (opt-in: `TACIT_COMBINATORIAL_OPERATORS`)** — `>>*` compose
   left-to-right, `<<*` compose right-to-left, `&&&` fanout, `***` product. Exhaustive sweep in
   `tacit_extras.md`: 7194 candidates, 615 stolen by maximal munch, 368 of the 391 practical ones
   compile. Haskell's `&&&`/`***`/`+++` survive C++'s lexer; `|||`/`>>>`/`<<<` do not, so compose is
   the mirrored `>>*`/`<<*` pair off the one `*` marker. (`->*` briefly held the compose slot; it has
   been returned to its natural, ungated meaning — member-pointer projection, the `.*` gap-filler.)
   `->.*` is not a token sequence at all — `->` needs an id-expression after it. The named forms
   (compose/fanout/first/second) already exist behind `TACIT_COMBINATORS`; moving them to a
   sub-namespace is still open.

5. README. **Namespaces fixed** — every range-taking algorithm is now `std::ranges::`; the headline
   example did not compile before. Still outstanding: inferred std types to cut noise, and documenting
   whatever lands from #4 and #6.

6. ~~Split `_` and `$` into `_.hpp` and `$.hpp`.~~ **DONE** — the fork resolved toward `$.hpp`
   including `_.hpp`: `$`-without-`_` was never a real use case (`$<std::set, _, ...>` has `_` in its
   own examples), so no build step and no duplication. The one mechanical obstacle — `_.hpp` #undefs
   its generators — is solved by `detail/make_overloads.hpp`, a guard-free macro-only header both
   public headers include and clean up; include order is immaterial and each header stays macro-clean.
   The module story then SIMPLIFIED rather than mirrored: a separate `tacit.dollar` module existed
   briefly, but the split's reason — `#include` injects tokens, and lexing `$` is what
   `-pedantic-errors` rejects — does not survive `import`, which injects only names. So one `tacit`
   module exports `$` too; a strict consumer just never spells it (CI compiles such a consumer with
   `-pedantic-errors` against the `$`-bearing interface), and `-DTACIT_NO_DOLLAR` strips the
   interface for builds that must compile even it strictly. The std `namespace` deviancy
   moved out the same way: `<tacit/experimental/std_blanks.hpp>`, opt-in by include, no macro.
   `TACIT_DOLLAR`, `TACIT_STD_BLANKS` and `TACIT_USING_UNDERSCORE` are all gone — the first two
   replaced by their headers, the last dropped (`using tacit::_;` is good enough).

7. ~~Naming: `tacit::make`.~~ **RESOLVED** — `$` is now the CANONICAL name for both halves of the
   term wrapper (`$(x)` == `lift(x)`, `$<F>(a…)` == `make<F>(a…)`), documented as such; `lift` and
   `make` stay as the conforming spellings for `-pedantic-errors`/MSVC worlds, since `$` in an
   identifier is a GCC/Clang extension. The asymmetry that motivated the question (one symbol vs two
   names) is therefore accepted on the conforming side rather than papered over by overloading
   `lift`. Still ruled out, for the record: `_::make` (a static member is reachable through the
   object, so `_.make<V>(1,2,3)` would return a *value*, breaking the invariant that `_.f()` is a
   closure); `tacit::of` (says nothing standalone); anything of the form `$name` (`$` is an
   identifier *character*, so `$std` lexes as ONE token — `$` can never prefix a name).

8. Named combinators (compose / fanout / first / second, currently behind `TACIT_COMBINATORS`) into a
   sub-namespace — still open, carried over from #4.

9. ~~`λ.hpp`.~~ **DONE** — `<tacit/λ.hpp>`, completely standalone (includes nothing), conforming
   (`λ` is a legal C++23 identifier per UAX #31, `-pedantic-errors`-clean, needs UTF-8 source). The
   macro emits the lambda HEAD only — `λ(a, b)` == `[&](auto&& a, auto&& b)` — body in ordinary
   braces, trailing-return slot open. The three impossibility results that fix the design (macro-only,
   braces stay, `return` stays), and the parked named-placeholder idea (`$x`/`$y` or `$1`/`$2` —
   NOT `a`..`z`, which shadow) are in `tacit_extras.md` under "λ: the lambda head".

10. ~~README reframing.~~ **DONE** — the opening no longer pitches point-free programming; it now
    matches the repo description ("a pithy C++ library to write pithy C++"): one vocabulary, three
    grammars — `_` for expressions, `$` for values, `λ` for statements — with the two opt-in headers
    introduced right after the headline example. The stale "ON THE NAME" gag at the end of the
    `_.hpp` preamble (point-free + the already-removed lieutenant etymology) is gone, as is the
    banner's promise of "deriving your own domain-specific placeholders" (also long removed).
    "Point-free" survives only as a technical adjective deep in extras/tests where it describes
    pipeline style, not identity.

11. **OPEN — single-file distribution.** Decision pending (discussed, not settled): keep
    `include/tacit/` as canonical source and add a generated, committed `single/` with standalone
    `_.hpp` and `$.hpp` (core inlined behind a shared content guard so both coexist in one TU), a
    ~30-line amalgamation script, and a CI regenerate-and-diff check. λ.hpp needs none of this (born
    single-file); std_blanks stays repo-only. Modules are orthogonal — the `.cppm` GMFs always
    resolve includes at interface-build time.
