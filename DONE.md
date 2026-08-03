DONE
----

The decision ledger: what was done, what was declined, and why — newest at the bottom. Open work,
when it exists, lives here too until it resolves; right now everything is settled. (Public asks —
the things we *couldn't* do — are in ASKS.md.)

Repo state: private, v0.5.0 tagged. CI matrix: clang++-18/22, g++-13/16, MSVC v143, clang-cl,
AppleClang, plus modules (clang, with strict-consumer and strict-interface legs) and packaging
(with the vendor-by-copy gate). 29 ctest targets; Intel ICX and EDG spot-checked via Compiler
Explorer. `include/tacit/` is exactly three files — `_.hpp`, `$.hpp`, `λ.hpp` — and they ARE the
distribution: no generator, no second copy, nothing to keep fresh.


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
   its generators — was solved by `detail/make_overloads.hpp`, a guard-free macro-only header both
   public headers included and cleaned up; include order was immaterial and each header stayed
   macro-clean. (That shared file is gone as of #21; the table is now duplicated on purpose.)
   The module story then SIMPLIFIED rather than mirrored: a separate `tacit.dollar` module existed
   briefly, but the split's reason — `#include` injects tokens, and lexing `$` is what
   `-pedantic-errors` rejects — does not survive `import`, which injects only names. So one `tacit`
   module exports `$` too; a strict consumer just never spells it (CI compiles such a consumer with
   `-pedantic-errors` against the `$`-bearing interface). `-DTACIT_NO_DOLLAR` briefly also stripped
   the interface for builds that must compile even IT strictly; that knob is gone as of #21. The std `namespace` deviancy
   moved out the same way at the time — `<tacit/experimental/std_blanks.hpp>`, opt-in by include,
   no macro — though that header has since been removed outright (see #20).
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

11. ~~Single-file distribution.~~ **DONE, then SUPERSEDED by #21** — the shipped answer was option
    A: `include/tacit/` canonical, `single/tacit/` holding generated, committed standalone forms of
    `_`, `$`, and `λ`, written only by `tools/amalgamate` (python3) and kept honest by a CI
    regenerate-and-diff. It worked, and the `tacit/` subdirectory in the output was the right call
    (one include spelling, always). It stopped being worth its weight once `detail/` was inlined and
    the canonical headers became standalone themselves — see #21.

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

17. ~~Windows CI leg.~~ **DONE, BLOCKING** — `windows-2022`, VS2022 MSVC, Debug config (Release's
    NDEBUG would vaporize every assert and pass the suite vacuously), `ctest --timeout 60` because
    a failing Debug assert pops a MODAL DIALOG and the runner waits out the whole job — 25 minutes,
    observed. Four rounds to green, and the first two findings were real: MSVC parses and silently
    IGNORES `[[no_unique_address]]` (ABI freeze), so closures were not empty there and the
    stateless-comparator guarantee was broken until `TACIT_NO_UNIQUE_ADDRESS` started spelling
    `[[msvc::no_unique_address]]`; and MSVC has not implemented C++23 P2314 (UCN/glyph macro
    equivalence), so λ works there but `\u{3BB}` does not — documented, tests scoped. The last two
    failures were POSIX assumptions in the tests (`filesystem::path::string_type` is `wstring` on
    Windows; `/tmp/x` is a RELATIVE path there), fixed by asserting against what the type itself
    reports. 29/29 on all three compiler families.

18. ~~Toolchain breadth after MSVC.~~ **DONE** — CI legs added for **clang-cl** (the `windows` job
    is now a matrix over front ends, `-T v143` / `-T ClangCL`, no vcvars shell and so no
    third-party action) and **AppleClang** (`macos-15`, green first try — and the toolchain whose
    libc++ produced the `std::tuple` surprise, so it belongs in CI rather than on a laptop).
    clang-cl immediately earned its slot: it targets the MSVC ABI and therefore also ignores
    `[[no_unique_address]]`, but it defines `__clang__`, so the compiler-based guard written for
    MSVC handed it the ignored spelling and the emptiness guarantee broke again. Detection is now
    `__has_cpp_attribute(msvc::no_unique_address)` — ask what a toolchain SUPPORTS, not what it is
    called. Spot-checked green through the Compiler Explorer API, no CI cost: **Intel ICX 2026**
    (compiles and runs the whole `$` surface) and **EDG 6.9** (the front end behind Visual Studio's
    IntelliSense — so no red squiggles). Intel ICX was considered for a standing leg and declined:
    a ~1 GB install would make it the slowest job by 3× for near-certain "clang-based, it works".

19. ~~"Concepts" as a README section title.~~ **RENAMED to "Notation"** — `concept` is a C++
    keyword, and a section called Concepts in a C++ library's README reads as a page about
    `requires`-clauses. "Notation" is also just more accurate: blanks, vocabulary, sections,
    composition, and application are the notation the library adds.

20. ~~The std blanks (`experimental/`).~~ **REMOVED** — `std::map<struct _, int>::with<char>` was
    the sugar tier of the type level, and it worked by specializing std class templates for the
    blank type. That is [namespace.std] deviancy — ill-formed NDR however well it runs — and the
    library is now enforcing it: libc++ 21's `[[clang::no_specializations]]` on `std::tuple` had
    already forced a `TACIT_HAS_STD_TUPLE_BLANKS` carve-out, and nothing suggests that trend
    reverses. It was also charging rent in the core, where `operator,(self, self)` had to be a
    template purely so the header parse would not instantiate `std::tuple<_, _>` and ambiguate the
    specialization. Removing it takes `include/tacit/experimental/` with it, and the portable
    `bind`/`apply`/`quote` grain still reaches every shape it did, at the cost of a `quote<>` at the
    call site. The one thing genuinely lost is value-parameterized templates (`std::array<struct _,
    5>::with<int>`): a template-template parameter is `template <class...>`, so a `<class, size_t>`
    head has no slot in the general primitive — noted in ASKS. The design notes are kept in
    `tacit_extras.md`, marked as removed, because the findings outlive the code.

21. ~~`detail/`, `single/`, and the build step.~~ **ALL GONE** — with `experimental/` removed (#20),
    the whole intra-library include graph was three edges: `$.hpp → _.hpp`, and both public headers
    → `detail/make_overloads.hpp`. That last file is the sixteen-shape partial-CTAD generator, and
    it was shared rather than duplicated because C++ gives no way to hand an existing
    function-template overload set a second name (`using` preserves the name, and `$` cannot forward
    to `make` through one signature for the same reason there are sixteen shapes: a single pack
    cannot mix type and non-type parameters). So the table is now a VERBATIM COPY in each public
    header — the only deliberate duplication in the library — and `tests/shapes.cpp` pins the copies
    to identical shapes and identical result types, so divergence fails loudly at the offending
    shape instead of waiting for the first user to write shape eleven. (Verified by deleting one row
    from `$.hpp`'s copy: `shapes.cpp` failed at exactly that line.)

    Paying that let the rest fall: `_.hpp` and `λ.hpp` became standalone, leaving `$.hpp → _.hpp`
    (siblings, one directory) as the only edge — which is the thing `single/` existed to erase, and
    #6 had already established that `$`-without-`_` is not a real use case. So `single/` (326 KB of
    committed duplicate), `tools/amalgamate`, `tests/single_check.cpp`, and the CI freshness gate
    are all deleted. `include/tacit/` is three files and they are the distribution. The guarantee
    `single_check` used to give is now given directly, and more honestly, by a packaging step that
    copies the headers into a bare directory and builds with the repo's include path absent.

22. ~~`TACIT_NO_DOLLAR`.~~ **REMOVED** — it let `tacit.cppm` be built under `-pedantic-errors` by
    dropping `$` from the interface. The objection that killed it: a macro cannot cross a module
    boundary, so this was never a consumer's choice — it was the *producer's*, and it produced two
    different modules both answering to `import tacit;`, one exporting `tacit::$` and one not. A
    header may vary per TU; that is what a header is. A named module varying per build is a trap,
    and all it bought was the ability to put `-pedantic-errors` on a file that is not yours. The
    strict-CONSUMER leg stays in CI — that one proves the actual claim, that `import` injects names
    rather than tokens. While there, `tacit.cppm` also picked up the one public name its export list
    was missing (`tacit::blank`) and grouped the rest.

23. ~~Include-order permutations.~~ **COVERED EXHAUSTIVELY** — `order_{cdl,cld,dcl,dlc,lcd,ldc}.cpp`
    (c = core, d = dollar, l = lambda) are all six permutations of the three public headers, sharing
    one body, `order_body.hpp`, so the only variable across the set is the order. The first instinct
    was to sample two of the six; six IS the whole space and each TU costs ~0.4s, so covering it
    exhaustively is both cheaper to justify and leaves no judgment call about which orders matter.
    Ordered subsets are deliberately NOT enumerated — the triples put every header in first, middle
    and last position — with one exception, `order_d_only.cpp`, below.

    Three things this flushed out. First, the suite was accidentally blind: clang-format sorts
    includes alphabetically and `$` < `_` < `λ`, so left to itself EVERY test lands in that one
    order, and `_.hpp` before `$.hpp` — the order a person would write by hand — was compiled
    nowhere once the `single/` reverse-order probe was deleted. Hence `clang-format off` in all six;
    the order is the fixture, not a style slip. Second, `strict_using.cpp` was the only macro-leak
    check and it includes the core ALONE, so nothing verified that `$.hpp` cleans up its own
    (duplicated, per #21) copy of the generator table. The policy list moved to
    `tests/no_generator_macros.hpp` — guard-free and declaration-free, so it can assert at as many
    points as there are claims — and is now checked after `_.hpp` alone, after `$.hpp` alone, and
    after all three in every order. Verified by deleting one `#undef` from `$.hpp`: `strict_using`
    stayed green, the order tests failed. Third, `order_d_only.cpp` restores a case the IWYU pass
    had silently removed: `dollar.cpp` used to be the only TU including `$.hpp` by itself, and
    adding an explicit `<tacit/_.hpp>` to it (correctly) deleted the sole proof that `$.hpp` brings
    its own core. It now has a file that exists for it on purpose.
