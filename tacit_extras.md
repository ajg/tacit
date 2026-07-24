# tacit — extras & design notes

Design notes for the pieces that sit *around* the core `_` object: what's implemented, why it's
shaped the way it is, and what's still on the table. The public surface is still just `tacit::_`
(plus the free tuple combinators); everything here is either already in `<tacit/_.hpp>` or a
candidate for it.

## Composition (implemented)

**Problem.** A bare `_.size()` returns a plain lambda, so `_.size() >= 2` tries to compare a lambda
with an `int` and fails. Point-free code wants the *result* of a projection to keep composing.

**Design.** Every single-argument closure `_` produces is wrapped in a tiny composable type,
`detail::fn<F>`. `fn` carries the operator sections, subscript, and call, and each of those builds a
*new* `fn`, so projections, sections, subscript, and arithmetic chain:

    _.size() >= 2            // x -> size(x) >= 2
    (_ + 1) * 2              // x -> (x + 1) * 2
    _[0]                     // x -> x[0]
    _.size() < _.size()      // (a, b) -> size(a) < size(b)   (binary, mirrors `_ < _`)

**The trap.** The first attempt (an earlier `tacit_extras` proof-of-concept) failed because each
operation returned `fn<F>` with the *same* `F`, which cannot hold the new composed lambda. The fix is
to deduce the wrapped type per step via CTAD on the *qualified* template name
(`tacit::detail::fn{...}`), yielding `fn<new-lambda>` each time.

**Scope.** Only *single-argument* closures become `fn`. The multi-blank `section` path
(`_.foo(_)`, `_ < _`, `_.replace(_, _)`) stays a plain partial application — composition there is
meaningless — and `fn` is deliberately *not* a blank (no `is_tacit_placeholder`), so the placeholder
detection that drives blanks is untouched. `fn op value` / `value op fn` compose to a unary closure;
`fn op fn` is a binary closure, mirroring `_ op _`.

## Heterogeneous element combinators (implemented)

`tacit::for_each_element / any_of_element / all_of_element / none_of_element / transform_elements`
drive a callable over the elements of a tuple-like, via `std::apply` + fold-expressions (C++23).
They are `_`-agnostic but pair naturally with `_`'s closures, e.g. `transform_elements(t, _.size())`.
A `template for` (C++26, `__cpp_expansion_statements`) path can later extend them to arbitrary
aggregates and reflection ranges behind a `TACIT_HAS_EXPANSION` flag — no API change.

## Hybrid: member chaining + projected blanks (implemented)

Prototyping the items below (standalone, on g++ 13 / clang 18) showed that three of them are one
change, not three. Today `_` (the `lieutenant`) and a composed projection (`detail::fn`) are separate
types: `fn` composes but carries no vocabulary and is not a blank. Give `fn` the vocabulary and
blank-ness and these fall out together:

- **Member chaining** — `_.trim().size()` == `x -> size(trim(x))`. Member access on a projection
  composes, which *is* function composition; the explicit combinator below then shrinks to a
  convenience.
- **Projected blanks** — `_.foo(_.size())` == `(obj, x) -> obj.foo(size(x))`. An ordinary blank is
  just a projected blank whose projection is identity; `make_section` learns to apply each blank's
  projection to its fill instead of taking it raw.

The catch that also fell out: *full* unification (`_` becomes `ph<identity>`) collides with the
application combinator — if `_` is the identity projection then `_(3)` means `3`, which can't also
mean `λf. f(3)`. So the plan is the **hybrid**: keep `_` as the distinct entry point (it keeps `_()`,
`_[i]`, and being the identity blank) and extend only its *results* — `fn` gains the vocabulary and
projected-blank-ness. That is an extension of v0.2's `fn`, not a rewrite. Decided: **yes to member
chaining** — each `fn<F>` carries the ~60 vocabulary members (declared once, instantiated on use).

**Cost of `fn` carrying the vocabulary.** It is a *compile-time* cost, not runtime or binary size.
The ~60 names are member templates / non-template members of a class template, so they are
instantiated only when used — an unused member emits no code. Measured: a TU that uses `_` but no
chaining produces a byte-identical object file to pre-hybrid v0.2 (16240 bytes either way), and
front-end compile time rose ~4% (~40 ms) on a real TU, negligible for parsing the header alone. At
runtime the composed closures inline away — no dispatch, no allocation.

## Exported surface (decision: document, don't move)

The public surface is now `_` plus a handful of *qualified* free helpers (`fanout`, `first`, `second`,
the `*_element` combinators, `bind`) and the operator forms. The "one symbol" promise is about
*scope*, and it holds: `using tacit::_;` brings in only `_`; operators arrive via ADL; the type-level
hole reuses `_` as `struct _` (no new name); and the free helpers are only ever qualified, so they
never enter a user's scope. Decision (pre-v1, experimental): document this precisely and leave the
helpers flat in `tacit::` — a sub-namespace would fight the flat-namespace preference, and qualifying
them already isolates them. Revisit which helpers stay in once usage tells us what's actually wanted.

## Still on the table

**Compose combinators** *(implemented)* — `f | g` (left-to-right compose, an `operator|` on `fn`),
plus `tacit::fanout(f, g, …)` (Haskell `&&&`) and `tacit::first` / `tacit::second`. Each returns an
`fn`, so results keep composing; `operator|` does not collide with the ranges pipe (its left operand
is a range, not an `fn`). `f *** g` is just `first(f) | second(g)`.

**Template-argument members** — `_.get<0>()`, `_.as<int>()`. After the hybrid these go into the
*shared* vocabulary, so they would work on `_` and every projection at once. A template parameter pack
is single-kind, so one macro cannot take both `foo<int>` (type) and `foo<0>` (value) — it needs two
overloads plus the runtime path, offered as an opt-in generator (`TACIT_TMEMBER` / `TACIT_VMEMBER`).

**Type-level tacit** *(implemented)* — `bind<F, args...>::with<Xs...>` partially applies a class
template. `_` reuses its own identifier at the type level via the elaborated `struct _` (an old C
trick: a class and a variable can share a name), so the hole is `struct _` and fixed args stay plain
types — `bind<std::map, int, struct _>::with<double>` == `std::map<int, double>`. A P2996 build can
generalize substitution to alias templates / non-type params via `std::meta::substitute` (gated hook).

**Adoption / packaging** *(in progress)* — added `install()` + a generated `tacitConfig.cmake` so
`find_package(tacit)` and FetchContent work, and a `TACIT_VERSION` macro. Still worth doing: a Godbolt
"try it" link, clearer diagnostics (named concepts), and a short recipes section.
