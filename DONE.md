DONE
----

The decision ledger: what was done, what was declined, and why — newest at the bottom. Open work,
when it exists, lives here too until it resolves; right now everything is settled. (Public asks —
the things we *couldn't* do — are in ASKS.md.)

Repo state: private, v0.4.0 tagged, pre-announcement fixes landed (#12). CI matrix: clang++-18/22,
g++-13/16, modules (clang, with strict-consumer and strict-interface legs), packaging (with the
single/ freshness gate). 27 ctest targets. The
`typeapply` local failure on Apple clang is resolved: libc++ 21 hard-bans specializing `std::tuple`,
so the std-blanks header skips that cell and exposes `TACIT_HAS_STD_TUPLE_BLANKS`.


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

4. ~~Synthetic sigil operators.~~ **DONE (opt-in: `TACIT_SIGILS`, né `TACIT_COMBINATORIAL_OPERATORS`)** — `>>*` compose
   left-to-right, `<<*` compose right-to-left, `&&&` fanout, `***` product. Exhaustive sweep in
   `tacit_extras.md`: 7194 candidates, 615 stolen by maximal munch, 368 of the 391 practical ones
   compile. Haskell's `&&&`/`***`/`+++` survive C++'s lexer; `|||`/`>>>`/`<<<` do not, so compose is
   the mirrored `>>*`/`<<*` pair off the one `*` marker. (`->*` briefly held the compose slot; it has
   been returned to its natural, ungated meaning — member-pointer projection, the `.*` gap-filler.)
   `->.*` is not a token sequence at all — `->` needs an id-expression after it. The named forms
   (compose/fanout/first/second) already exist behind `TACIT_COMBINATORS`; moving them to a
   sub-namespace is still open.

5. ~~README.~~ **SUPERSEDED** — the namespace fixes landed, and everything else this item wanted
   was absorbed by #10 (the reframe) and the announcement pass (#12): #4 and #6 are documented (then
   deliberately demoted to `tacit_extras.md` pointers), and the compile-checked `tests/readme.cpp`
   discipline now guards every code block.

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

8. ~~Named combinators into a sub-namespace.~~ **DECLINED** — the gate (`TACIT_COMBINATORS`) is
   already the fence; a sub-namespace would be a second fence around the same field, and every use
   would grow a qualifier for no added safety. Carried from #4, considered twice, dropped.

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

11. ~~Single-file distribution.~~ **DONE (option A)** — `include/tacit/` stays canonical;
    `single/` holds generated, committed standalone forms of `_`, `$`, and `λ` (the shared core
    wrapped in `TACIT_SINGLE_CORE_SEEN`, so `_`+`$` coexist in one TU either order; λ is
    pass-through). `tools/amalgamate` (python3) is the only writer; CI regenerates + diffs (drift
    impossible), compiles the reverse include order and a `-pedantic-errors` probe, and the ctest
    `single_check` target builds against `single/` alone (not linked to tacit::tacit, so the include
    path proves standalone-ness) on every matrix leg. std_blanks stays repo-only; modules are
    orthogonal (`.cppm` GMFs resolve at interface-build time; `single/` never participates).

12. ~~Pre-announcement readiness review.~~ **DONE** — two fresh-eyes reviews (README-as-first-contact
    and a hostile header read) plus mechanical probes (compile cost ~0.1s over a std baseline,
    ~10-line error messages, C++26 `_` coexistence verified). All three blockers fixed with
    regression tests: fills are genuinely perfect-forwarded now (they collapsed to lvalues — rvalue
    fills copied, move-only fills didn't compile); a bool folded into a comparison chain
    (`(_ < 10) == false`, an always-false closure) is a static_assert with the fix in the message;
    a non-copyable rvalue operand (`std::ostringstream{} << _`, silently dangling) is a
    static_assert saying "name it first". Should-fixes landed too: the six vocabulary SFINAE guards
    now test the exact call their bodies make (`std::decay_t<A> const&...`), `_()`/`_(a...)` return
    `fn` like every other builder, and λ.hpp states the Greek-identifier collision and its bounds.
    README rewritten as an announcement document (why-this-exists with the C++20 addressability
    argument, costs/limits/coexistence with measured numbers, comma defense, Lambda2 comparison);
    ASKS.md added as the outward-facing challenge list.

13. ~~Review leftovers.~~ **CLOSED** — `_ = _` is fixed: it is now the two-input assignment
    section, `(a, b) -> (a = b)`, exactly as `_ += _` always was (declaring `operator=(self)` also
    suppresses the implicit copy assignment, which a stateless tag never needed); documented in the
    README and tested. The rest of the litter was reviewed and deliberately let go: the chain's
    middle operand running once per link is already shown by the doc's own expansion; the dead
    `tacit_mark_inner_` local is invisible in practice; `apply`/`quote` naming waits for 1.0 by
    definition; named-concept diagnostics and a recipes section are future polish, not debts. The
    duplicate `<string_view>` include is gone.

15. ~~Gate the named combinators?~~ **UNGATED** — `TACIT_COMBINATORS` is gone; `compose`, `fanout`,
    `first`, `second`, and the `*_element` family are plain qualified `tacit::` functions, exported
    unconditionally by the module too. The gate was a philosophy fence, and every argument that
    justifies a gate elsewhere fails here: `$` is a lexer extension (its fence is the header), `λ`
    is a macro (ditto), the sigils spend real operator readings (their fence is the macro) — but a
    qualified name costs a caller nothing until spelled, and across a module boundary a gate
    degrades to a build-time `-D` on the interface, the wrong granularity entirely (the same
    argument that folded `$` into the one module). The premise stands: `_` is the core and the
    README opens with it; `$`, `λ`, and the named combinators exist beside it, each fenced only by
    what actually needs fencing. This also retroactively grounds #8's decline — the sub-namespace
    would have been a fence around a field that needed none.

16. ~~MSVC: any signal at all?~~ **SANITY-CHECKED** — via the Compiler Explorer API (no Windows
    machine involved): MSVC v19.latest compiles `single/_.hpp`, `single/$.hpp`, and `single/λ.hpp`
    with ZERO warnings at `/W4` and executes representative probes correctly (sections, chaining,
    comma, composition, partial CTAD, lift, `$` — so MSVC accepts `$` identifiers — and λ under
    `/utf-8`). Required flags: `/std:c++latest /EHsc /utf-8 /Zc:preprocessor` — the conforming
    preprocessor is NOT implied by /std:c++latest, and without it the X-macro engine's `__VA_OPT__`
    misfires (warning C5109). ASKS #3 narrowed accordingly: what remains is the full suite on a
    real Windows runner (a `windows-2022` GitHub Actions leg is the cheap durable route). The
    single/ files earned their keep here: self-contained one-file payloads are exactly what a
    compile API wants.
