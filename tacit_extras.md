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

## On the table

**Template-argument members** — `_.get<0>()`, `_.as<int>()`. A member template *can* parse on the
concrete `_`, but a template parameter *pack* is single-kind, so one macro cannot accept both
`foo<int>` (type) and `foo<0>` (value) — it needs two overloads plus the runtime path. Best offered
as an opt-in generator (`TACIT_TMEMBER`) reached for on domain member-templates, not baked into the
~60-name std vocabulary. A reflection build could instead expose `_.call<"get", 0>()` via
`std::meta::substitute`.

**Projected blanks** — generalize a blank from the identity `_` to any `fn`, so `_.foo(_.size())`
would mean `(obj, x) -> obj.foo(size(x))`. Falls out of teaching `make_section` to apply each blank's
wrapped function; the identity `_` stays the plain blank.

**Function composition combinator** — an explicit `f | g` / `compose` spelling for chaining two
projections, beyond the operator/subscript composition above.

**Type-level tacit** — `_` as a placeholder in a *template* argument list (`bind<std::vector, _>`
yields a one-argument metafunction), the type-level twin of value blanks. Feasible because
`lieutenant` is an empty structural type usable as a non-type template argument, but it is a separate
sublibrary.
