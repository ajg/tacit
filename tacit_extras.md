# tacit — extras & design notes

Design notes for the pieces that sit *around* the core `_` object: what's implemented, why it's
shaped the way it is, and what's still on the table. The default public surface is `tacit::_` plus the
opt-in type-level `tacit::bind`; the free-function combinators are gated behind `TACIT_COMBINATORS`.
Everything here is either already in `<tacit/_.hpp>` or a candidate for it.

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
Opt-in behind `#define TACIT_COMBINATORS` (see the exported-surface note below).
A `template for` (C++26, `__cpp_expansion_statements`) path can later extend them to arbitrary
aggregates and reflection ranges behind a `TACIT_HAS_EXPANSION` flag — no API change.

## Hybrid: member chaining + projected blanks (implemented)

Prototyping the items below (standalone, on g++ 13 / clang 18) showed that three of them are one
change, not three. Today `_` (of its own type `_`) and a composed projection (`detail::fn`) are separate
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

## Exported surface (decision: gate the free helpers)

The default surface is `_` plus the opt-in type-level `bind`, and it earns the "one symbol" promise on
*scope*: `using tacit::_;` brings in only `_`; the operator forms (sections, `|`) arrive via ADL as
hidden friends of `fn`; and the type-level blank reuses `_` as `struct _`, adding no new name. `bind` is
the sole extra qualified name, and it's inert unless you write it.

The free-function combinators (`fanout`, `first`, `second`, the `*_element` family) are different in
kind: as free functions in `tacit`, they are *not* ADL-reachable — their arguments associate `std` and
`tacit::detail`, never `tacit` — so they can only be called qualified, and each is a genuine extra
symbol. Rather than move them to a sub-namespace (which fights the flat-namespace preference) or drop
the cleverness, they sit behind `#define TACIT_COMBINATORS`: still in-tree, still tested, but off the
default surface. Decision (pre-v1, experimental): gate, don't move or delete; revisit once real usage
tells us whether they should graduate to the default surface (or become hidden friends of `fn`, which
would make them ADL-reachable like `|` and moot the gate).

**If it ever comes to two symbols.** The gate above is *expression interdiction* via the preprocessor —
we forbid a symbol at the macro level rather than spend a name. If the design ever genuinely wants a
*second* first-class name (not just a hidden helper), the alternative is to spend a real sigil rather
than another macro: `$`, a natural twin to `_`, as a *generalized bind* — a value+type-level hole like
`_`, unifying binding under one visible symbol instead of the qualified `tacit::bind`.

The clean justification for the glyph is the *zero-hole* framing: define `$`-as-bind so the no-hole
case degenerates to `$(f, x) == f(x)` — exactly Haskell's `$` (application) — and each positional hole
is then one step away from `$` toward a section. So Haskell's `$` is the zero-hole special case; you're
generalizing application to leave gaps, not borrowing the glyph by coincidence. Three caveats keep it
honest. (1) The mechanism can't transfer: in C++, `$` is only ever an *identifier* (a GCC/Clang
extension, rejected under `-pedantic`), never an operator — the overloadable-operator set is fixed and
excludes it — so there is no infix `f $ x` and none of Haskell's `infixr 0` paren-dropping; you get the
glyph, not the mechanism, and that portability cost is exactly why the early `$` usage was removed
(commit `f1e25db`). (2) The literal Haskell-`$` analog already exists as `_`'s application form —
`_(3)` is the `($ 3)` section, `_()` invokes a thunk — so a `$` symbol would really be doing
sections/bind (currying / `flip` / `.` territory), a different job wearing `$`'s coat. (3) Least
astonishment: a Haskeller reading C++ `$` expects apply-glue and would find bind-with-holes, so it
shouldn't be sold as "C++'s `$`". Net: parked, not adopted — a live option for the "two symbols" world,
priced in portability, to weigh against staying macro-gated at one.

## Still on the table

**Compose combinators** *(implemented)* — `f | g` (left-to-right compose, an `operator|` on `fn`,
always available), plus the opt-in `tacit::fanout(f, g, …)` (Haskell `&&&`) and `tacit::first` /
`tacit::second` (behind `TACIT_COMBINATORS`). Each returns an `fn`, so results keep composing;
`operator|` does not collide with the ranges pipe (its left operand is a range, not an `fn`).
`f *** g` is just `first(f) | second(g)`.

**Template-argument members** — `_.get<0>()`, `_.as<int>()`. After the hybrid these go into the
*shared* vocabulary, so they would work on `_` and every projection at once. A template parameter pack
is single-kind, so one macro cannot take both `foo<int>` (type) and `foo<0>` (value) — it needs two
overloads plus the runtime path, offered as an opt-in generator (`TACIT_TMEMBER` / `TACIT_VMEMBER`).

**Type-level tacit** *(implemented)* — `bind<F, args...>::with<Xs...>` partially applies a class
template. `_` reuses its own identifier at the type level via the elaborated `struct _` (an old C
trick: a class and a variable can share a name), so the blank is `struct _` and fixed args stay plain
types — `bind<std::map, int, struct _>::with<double>` == `std::map<int, double>`. A P2996 build can
generalize substitution to alias templates / non-type params via `std::meta::substitute` (gated hook).

**Type-level projection** *(implemented)* — the dual of `bind`: where `bind` wraps the hole in an
*outer* template, `_::name::of<X>` pulls a nested member *out* of X (`_::value_type::of<vector<int>>`
== `int`), the type-level twin of the value-level member vocabulary. It works because a name before
`::` is looked up considering only types, namespaces, and templates ([basic.lookup.qual]), so `_::name`
reaches `struct _` past the value that hides it — `_` now serves as value placeholder, bind blank, and
projection namespace under one symbol. Three constraints, all inherent rather than incidental: there is
no `operator()` at the type level, so a projection is *applied* with `::of<X>` (or by nesting, or a
`transform`), never called — the deep asymmetry with value-level `_.foo()`; the vocabulary is closed
(each name is declared in `struct _`, extend via `TACIT_EXTRA_TYPE_MEMBERS`), since `::` demands a real
member; and it inherits `bind`'s parameter-kind split — nested *templates* need a separate
`TACIT_TYPE_TEMPLATE` entry, reached as `_::name<A...>::of<X>`. Reflection (P2996) could open the
vocabulary, but only through a `_::member<"name">`-style spelling or a splice, never `_::name` for an
arbitrary name.

**Adoption / packaging** *(in progress)* — added `install()` + a generated `tacitConfig.cmake` so
`find_package(tacit)` and FetchContent work, and a `TACIT_VERSION` macro. Still worth doing: a Godbolt
"try it" link, clearer diagnostics (named concepts), and a short recipes section.
