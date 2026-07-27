# tacit — extras & design notes

Design notes for the pieces that sit *around* the core `_` object: what's implemented, why it's
shaped the way it is, and what's still on the table. The default public surface is `tacit::_` plus the
opt-in type-level `tacit::bind` / `tacit::apply` / `tacit::quote`; the free-function combinators are
gated behind `TACIT_COMBINATORS`, and the natural-spelling std blanks behind `TACIT_STD_BLANKS`.
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
*scope*: `using tacit::_;` brings in only `_`; the operator sections arrive via ADL as
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
than another macro: `$`, a natural twin to `_`, as a *generalized bind* — a value+type-level blank like
`_`, unifying binding under one visible symbol instead of the qualified `tacit::bind`.

The clean justification for the glyph is the *zero-blank* framing: define `$`-as-bind so the no-blank
case degenerates to `$(f, x) == f(x)` — exactly Haskell's `$` (application) — and each positional blank
is then one step away from `$` toward a section. So Haskell's `$` is the zero-blank special case; you're
generalizing application to leave gaps, not borrowing the glyph by coincidence. Three caveats keep it
honest. (1) The mechanism can't transfer: in C++, `$` is only ever an *identifier* (a GCC/Clang
extension, rejected under `-pedantic`), never an operator — the overloadable-operator set is fixed and
excludes it — so there is no infix `f $ x` and none of Haskell's `infixr 0` paren-dropping; you get the
glyph, not the mechanism, and that portability cost is exactly why the early `$` usage was removed
(commit `f1e25db`). (2) The literal Haskell-`$` analog already exists as `_`'s application form —
`_(3)` is the `($ 3)` section, `_()` invokes a thunk — so a `$` symbol would really be doing
sections/bind (currying / `flip` / `.` territory), a different job wearing `$`'s coat. (3) Least
astonishment: a Haskeller reading C++ `$` expects apply-glue and would find bind-with-blanks, so it
shouldn't be sold as "C++'s `$`". Net: parked, not adopted — a live option for the "two symbols" world,
priced in portability, to weigh against staying macro-gated at one.

## Operator surface (implemented)

Beyond comparison/arithmetic, the sections now cover bitwise `& |`, shift/stream `<< >>`, logical
`&& ||`, comma `,`; the unary operators `* - + ! ~ &` and `++ --` (pre/post); assignment `=` and
compound `+= -= *= /= %= ^= &= |= <<= >>=`; and `->`. Notes and decisions:

- **Comparisons chain (`0 < _ < 10`).** C++ parses that as `(0 < _) < 10`, comparing the bool against
  10 — a closure that is silently always true, the one place where the section surface produced a
  quiet wrong answer rather than a compile error. Fixed by giving the six comparison sections one
  extra piece of state: `fn`'s second template parameter, `last`, a projection recovering the
  *rightmost operand* of the comparison as a function of the eventual fill (`always{y}` for a bound
  value, `same{}` for the blank, the `fn` itself for a projection; `nochain` — the default — for
  everything else). A comparison whose left operand already carries chain state then folds into
  `(… op0 m) && (m op y)` with `m == last(x)`, which iterates to any length and any mix of
  `== != < > <= >=`. Three consequences worth stating: the middle term is evaluated once per link
  (keep projections pure and cheap), `&&` short-circuits as in the spelled-out form, and comparing a
  comparison chains too — `(_ < 10) == false` is `(x < 10) && (10 == false)`, not `x >= 10`. That last
  one is the price of the rewrite being purely lexical (Python's chained comparisons pay it too);
  `_ >= 10` is the spelling that means it. Non-comparison operators drop the state, so the chain ends
  wherever the expression stops being a comparison, and `_ < _` — two blanks — stays the two-input
  comparator rather than a link.
- **`&&` / `||` are two-input combiners in the two-blank form** (`_ && _` == `(a, b) -> a && b`), like
  `_.size() < _.size()`. Short-circuit is preserved (it lives in the generated body) but there is no
  one-input "both predicates on x" — that's the distinct-blank stance; reach for a lambda.
- **Streaming binds the left operand by reference.** `os << _` can't copy the stream, so the `X op _`
  section captures a non-copy-constructible left operand by reference (copyable ones stay by value, so
  `2 - _` is unchanged). Enables `for_each(v, std::cout << _)`.
- **Assignment mutates by reference.** The argument binds by forwarding reference, so `_ = 0` / `_ += 1`
  update the caller's lvalue in place (`reference_wrapper` unnecessary, and would misbehave). Left-only
  and single-blank (`operator=` must be a member; `_ = _` falls to the deleted copy-assign).
- **`->` is a real arrow, not `(*_).`** — `operator->` returns an `arrow` proxy whose vocabulary
  forwards through `x->name()`, using the pointee's actual `operator->` (which a type may define
  independently of, or without, unary `*`). The arrow proxy carries the same vocabulary as `_`, verbs
  included, so `_->name()` tracks `_.name()`.
- **`&` unary is included** despite the address-of caveat (`&_` builds `x -> &x`, not the placeholder's
  address); it may mean something type-specific, per the same reasoning as `->`.
- **Bitwise `|` is a plain section (resolved).** `|` used to be left-to-right compose (`f | g`), which
  left bitwise `&` present but bitwise `|` absent — an asymmetry `|=` (a distinct token, always bitwise)
  kept surfacing. Resolved in favour of symmetry: `|` is now an ordinary section (`_ | 4`, `_ | _`) just
  like `&`, and general composition moved to `tacit::compose` (below). The trigger was the range-adaptor
  verbs (`_.filter(_!=0).take(2)`, opt-in `TACIT_VIEWS`): once a *pipeline* reads as member chaining
  through `_`, the pipe-shaped `|` had little left to do, so it went back to meaning bitwise-or. Nothing
  is lost — member chaining still composes vocabulary, `tacit::compose` composes arbitrary closures —
  and the ranges pipe is untouched (`nums | views::filter(_!=0)` has a range on the left, not an `fn`).
- **Comma pairs (not the built-in comma).** `_, _` is `(a, b) -> std::pair{a, b}`, and `_, y` / `x, _`
  bind the fixed side — comma reads as the tuple/pairing glyph rather than "evaluate-and-discard". It's a
  custom overload (not `TACIT_SECTION`, whose `x op y` body would be the built-in comma = return `y`),
  guarded to `tacit::_` operands, so it only fires in a comma-*operator* context; argument-list and
  init-list commas are separators and untouched. Overloading `operator,` is normally a footgun, so this
  was added only after confirming the full suite (folds over tuples, `std::apply`, etc.) still passes.
- **Comma is n-ary now, and reaches projections (implemented).** The binary-only version had two
  blanks, both silent. `fn` carried no `operator,` at all, so `(_.size(), _.front())` fell to the
  *built-in* comma: the left operand evaluated and vanished, leaving just the right — caught only by
  `[[nodiscard]]`, and only as a warning. And `_, _, _` parses `((_, _), _)`, where the two-blank
  form returned a plain lambda that no further `,` could grow, so it built a closure of the wrong
  shape that failed at the call. Both are fixed by making a comma expression its own type,
  `comma_section<Ops...>` — an operand list where each operand is a blank, a projection, or a bound
  value, exactly the vocabulary `section` already uses for member calls (the slot bookkeeping is now
  factored into `slot_map`, shared by both). Each further `,` appends an operand; applying it fills
  one blank per argument, left to right. Consequences worth stating: two operands stay a `std::pair`
  (a pair *is* a two-tuple for `get`/`tuple_size`/structured bindings/`apply` — `.first`/`.second` are
  strictly extra) and three or more become a `std::tuple`; the list is flat, so `(_, (_, _))` and
  `((_, _), _)` are the same three-slot tuple and nesting has no spelling; and a comma section is a
  terminal builder rather than an `fn`, so it doesn't carry the vocabulary onward — it makes data.
  Widening `operator,` from `_` to `fn` widens the footgun surface too, which is why
  `for_each_element`'s fold now spells its discard `void(f(x))` rather than leaning on `,`.
- **Composition became arity-preserving (implemented).** A comma section carries the vocabulary and
  the six comparisons, applied to the value it builds — `(_, _) == p` is `(a, b) -> {a, b} == p`.
  Making that chain further (`(_, _).bar().baz()`) needed one change in `fn`: its composition lambdas
  took a single `X&& x`, so anything they produced collapsed to arity one. They now take `X&&... x`,
  which costs nothing for the unary case (a unary `g` still only accepts one fill — `g(xs...)` is
  simply a substitution failure otherwise) and lets an n-ary closure keep composing. `fn`'s
  `operator()` was already variadic, so the wrapper was never the unary part; only the vocabulary
  was. Two limits kept deliberately: a chained call takes bound arguments only, since a blank inside
  one would have to interleave fills across two arity systems (`_.front().substr(_)` has never taken
  one either), and only the comparisons are lifted — they are the operators `pair`/`tuple` actually
  have, whereas arithmetic on a data builder would be noise. The member half is dormant against the
  standard vocabulary, which names nothing `std::pair` or `std::tuple` has; it is live for
  TACIT_VERBS.
- **Deferred / experimental:** `operator->*` and `_[&Member]` (the `.*` gap — `.*` isn't overloadable).

## Vocabulary sweep + arity (implemented)

A pass over the standard library for names worth first-class treatment, and the arity rules that fell
out of it.

- **Tables roughly doubled** — verbs 66 → 149, nouns 23 → 50, range CPOs 10 → 13 (`crbegin`, `crend`,
  `cdata` complete that family). The curation test was *projection value*: names you sort, filter or
  test with. That is why the additions cluster where they do — diagnostics (`what`, `code`, `message`,
  `category`, `name`), `filesystem::path` (17 names; `sort(paths, {}, _.extension())` is the whole
  argument), the monadic family completed (`error_or`, `transform_error`), concurrency (`join`,
  `joinable`, `load`, `store`, `fetch_add`, `wait`, `valid` — `for_each(threads, _.join())` is the
  shape), streams, `bitset`, `regex` match results, `complex`, `chrono`, `span`, `try_emplace`, the
  `forward_list` `*_after` family, and the C++23 `*_range` members.
- **The cost is compile-time only, and small.** A name is a member template on four surfaces (`_`,
  `fn`, `arrow`, `comma_section`), so a wider table is a longer declaration list — instantiated only
  on use. Measured: the tiny-TU binary is byte-identical at 16840 either way, front-end time +20% on
  a trivial TU (0.18s → 0.22s) and +11% on the heaviest test (0.34s → 0.38s).
- **Rejected, with reasons.** `swap` — the free/`ranges` form is already `TACIT_CPO2`, and the member
  form is a libc++ landmine (`x.swap(pair)` fails to instantiate there, pre-existing and independent
  of tacit). `reverse` — would collide with `TACIT_VIEW(reverse)` under `TACIT_VIEWS`. `first`/`last`
  (span) — they read as `pair::first`/`second`, which are *data* members a call vocabulary can never
  reach, so the name would promise something it can't do. `any::type` — would shadow the `type` noun.
  The bucket/`load_factor`/`rehash` and allocator `allocate`/`deallocate` families — real, but nobody
  goes point-free there.
- **Composition became arity-preserving, and arity became load-bearing in exactly one place.** Every
  two-input form (`_ op _`, `g op _`, `g op h`, the binary CPO) used to return a raw lambda, which
  carried nothing: `(_ < _).size()` and `(_ + _) + 1` did not compile. They now return an `fn`, so
  they compose like anything else. That created one conflict worth naming: an `fn` in argument
  position is a *projected blank*, so making `_ < _` an `fn` would have turned `_.sort(_ < _)` from
  "pass a comparator" into "project the fill", breaking it. Hence the `nary` tag — it rides in the
  chain-state slot (the two never coexist, since a two-input form has no rightmost operand to fold
  against) and `is_slot_v` excludes it. One-fill closures project; many-fill closures are values.
- **No dead closures.** A blank captured where nothing can fill it now fails where it is written
  rather than building a closure that no call can satisfy: `_.front().substr(_)`, `_->substr(_)`,
  `_.front()._(_)` are rejected, because a chained call binds its arguments. The cases that *do* have
  a sensible reading got one instead of an error — `_[_]` is `(x, i) -> x[i]`, `_ += _` is
  `(a, b) -> a += b`, `_(_)` is `(f, x) -> f(x)`, all routed through the existing `section`, and
  `_.size() < _` is the two-input comparator it always looked like (it used to build `x -> g(x) < _`,
  which nothing could call).
- **`._()` — the application form on a projection.** On `_`, application is `_(a, b)`; on a projection
  that spelling is taken, since calling an `fn` means "call this closure". So the vocabulary carries
  it under the placeholder's own name: `_.x()._()` invokes what `_.x()` produced and `._(a, b)` calls
  it with those arguments. Returns an `fn`, so a callable-returning chain keeps going.

## Range-adaptor verbs (implemented, opt-in `TACIT_VIEWS`)

A pipeline written point-free *through* `_`, so the dots sit on `_` rather than on the range:

    _.filter(_ != 0).take(2)(nums)   ==   nums | views::filter(_ != 0) | views::take(2)

The verbs (`filter take drop take_while drop_while reverse`) route through `std::views::*` — the same
customization-point posture as `_.size()` going through `std::ranges::size` — and each returns an `fn`,
so a pipeline is literally function composition of adaptors and the existing member-chaining machinery
does the chaining for free. It sidesteps the "`std::vector` has no `.filter`" problem precisely because
the vocabulary belongs to `_`, not the range; the range shows up only at the trailing `(nums)`, which
also means the pipeline is a *reusable* closure (apply it to many ranges) where `nums | …` binds its
range eagerly. Laziness is preserved end-to-end — `take` short-circuits an unbounded `iota` source.

The load-bearing rule: a range-adaptor verb **binds its callable argument as a value**, it does not
treat it as a projected blank. `is_slot_v` includes `fn`, so under the ordinary member rule
`_.filter(_ != 0)` would read `_ != 0` as an extra input and build a two-input *section* — and sections
carry no vocabulary, so `.take(2)` wouldn't even be reachable. Binding (not projecting) keeps the result
a *unary* `fn` that chains. That makes range-adaptor verbs their own forwarder category (a predicate is
a value that happens to be callable), sitting beside `TACIT_MEMBER` / `TACIT_CPO1` — the codebase
already runs several forwarder kinds with different argument rules, so it's a new category, not an
inconsistency. Two reasons it's gated rather than default: it's range-specific vocabulary that pulls in
`<ranges>` semantics, and `transform` already means the optional/ranges monadic member — so `transform`
is deliberately *absent* from the view table pending a precedence call (member vs view). A prototype
(`view_verbs_probe.cpp`) proved the mechanism on g++-13 / clang-18 before it went in.

## Still on the table

**Compose combinators** *(implemented)* — `tacit::compose(f, g, …)` (left-to-right compose,
`x -> …(g(f(x)))`), plus `tacit::fanout(f, g, …)` (Haskell `&&&`) and `tacit::first` / `tacit::second`,
all behind `TACIT_COMBINATORS`. Each returns an `fn`, so results keep composing. `compose` replaced the
old `f | g` operator when `|` went back to being a bitwise section (see the operator surface note); the
ranges pipe is unaffected (its left operand is a range, not an `fn`). `f *** g` is
`compose(first(f), second(g))`.

**More Haskell combinators** *(planned, agreed — not yet built)* — a named set to add behind
`TACIT_COMBINATORS`, all returning an `fn` so they keep composing:

- `dup(f)` = Reader's `join` / the **W** combinator — `x -> f(x, x)`. The *sanctioned* answer to
  "repeated `_` are distinct blanks, reach for a lambda to reuse": `dup(_ * _)` is `x -> x*x`. It
  collapses any two-blank combiner to its diagonal.
- `on(binop, proj)` — `(a, b) -> binop(proj(a), proj(b))`. The comparator-maker; `_.size() < _.size()`
  is an inline `on`, and `on(std::less{}, _.size())` generalizes it.
- `flip(f)` = **C** — `(a, b) -> f(b, a)`.
- `constant(v)` = `pure` / **K** — `x -> v`.
- `liftA2(h, f, g)` — `x -> h(f(x), g(x))` (the practical face of Applicative `<*>` / **S**).
- `all_of(p, q, …)` / `any_of(p, q, …)` — one-input predicate conjunction/disjunction
  (`x -> (p(x) && …)`). Fills the gap the distinct-blank stance creates: `_ && _` is a *two-input*
  combiner, so there is otherwise no way to spell "both predicates on the same x". Negation already
  works — `!p` on an `fn` gives `x -> !p(x)`.

Skipped as curiosities: Reader `>>=`, the pointwise function `Monoid`, `fix`. Sum-type Arrow
(`+++` / `|||`, dispatch over `variant`/`expected`) is feasible but deferred until a real use appears.

**`operator->*` = member-pointer projection** *(planned, agreed)* — give `->*` its natural meaning, not
a combinator: `_ ->* &Widget::x` == `p -> (*p).x`, i.e. "deref, then select the pointed-to member". A
hidden friend, ungated, works on anything dereferenceable (raw / `shared_ptr` / iterator); verified on
g++-13 / clang-18. It's the only way to spell member-pointer projection at all, since `.*` isn't
overloadable — the deferred "`.*` gap" filler. Clean for **data** members; **member-function** pointers
are awkward (`obj.*pmf` is only valid as a call head, can't carry args), so methods stay with the
`_->f(args)` arrow proxy. The value side (non-pointer) would be `_[&Widget::x]` via subscript.

**Operator-glyph combinators** *(parked — explored, not adopted)* — whether a multi-character glyph
like `_ &&& _` could carry a combinator (fanout / parallel / compose). Findings, all proven on
g++-13 / clang-18 (probes: `invent.cpp`, `amp.cpp`, `arrow.cpp`):

You cannot invent a new operator *token* — the set is fixed and maximal-munch lexing is forced. But a
glued glyph that lexes into `[postfix ++/-- on the left] · [one binary "hinge"] · [prefix unaries on the
right]` **can** be given an arbitrary meaning: the unary pieces return a *marker* type, and the binary
hinge is overloaded on that marker to mean anything (ordinary uses still resolve, since overload
resolution splits on the operand type). The mechanism, in miniature:

    template <class F> struct amp { F f; };                       // marker from unary &
    friend constexpr amp<F> operator&(fn self) { return {self.f}; }
    template <class G> friend constexpr auto operator&&(fn f, amp<G> m) { /* fanout(f, g) */ }
    // f &&& g  ==  f && (&g)  ==  operator&&(f, amp<g>)  ==  fanout(f, g)

Three glyphs read as genuine Arrow combinators; the rest of the space (~dozens) is semantic noise:

- `&&&` = `&&`·`&` → **fanout**. Chains cleanly (right-side marker → result is a plain `fn`). Cost:
  consumes unary `&` (currently shipped as `&_` = address-of — the most expendable of the three).
- `***` = `*`·`*`·`*` → **parallel**. Chains cleanly. Cost: consumes unary `*` = deref (`*_`), a core
  feature — expensive.
- `-->` = `--`·`>` → **compose**. Reads best, but the marker is on the *left* (postfix `--`), so a chain
  `f --> g --> h` fights `>`-associativity and needs extra `marker > marker` overloads; and it consumes
  postfix `--`.

Hard exclusions: a trailing piece with no unary form is a parse error — `|||` (`||`·`|`, no unary `|`),
`>>>` (`>>`·`>`), `<<<` all dead. Single-token glues (`++ -- && || << >> == != <= >=`) are just that
operator. `/*` and `//` are comment traps.

Verdict: real, but names win — `fanout` / `both` / `compose` are clearer and cost no operator. `&&&`
for fanout is the *only* glyph worth reconsidering, since it chains and unary-`&` address-of is the
cheapest thing to spend. Parked at that.

**Template-argument members** — `_.get<0>()`, `_.as<int>()`. After the hybrid these go into the
*shared* vocabulary, so they would work on `_` and every projection at once. A template parameter pack
is single-kind, so one macro cannot take both `foo<int>` (type) and `foo<0>` (value) — it needs two
overloads plus the runtime path, offered as an opt-in generator (`TACIT_TMEMBER` / `TACIT_VMEMBER`).

**Type-level tacit** *(implemented)* — `bind<F, args...>::with<Xs...>` partially applies a class
template. `_` reuses its own identifier at the type level via the elaborated `struct _` (an old C
trick: a class and a variable can share a name), so the blank is `struct _` and fixed args stay plain
types — `bind<std::map, int, struct _>::with<double>` == `std::map<int, double>`. A P2996 build can
generalize substitution to alias templates / non-type params via `std::meta::substitute` (gated hook).

**Type-level: one primitive that curries both grains** *(implemented)* — `bind<F, args...>` fixes the
class template `F` and blanks among its *arguments*; it cannot blank the template itself. `apply` closes
that gap with a single op. Quote a template into a type — `quote<F>` — so it can occupy a slot next to
ordinary type args, then `apply<Slots...>::with<Fills...>` fills each `struct _` slot (template *or*
argument) left to right:

    apply<quote<std::map>, struct _, struct _>::with<int, char>   // std::map<int,char>  (fix template)
    apply<struct _, int, char>::with<quote<std::map>>             // std::map<int,char>  (fix args)
    apply<struct _, int, struct _>::with<quote<std::map>, char>   // std::map<int,char>  (blank both)

So `bind<F, A...>::with<X...>` is exactly `apply<quote<F>, A...>::with<X...>` — `bind` is the
template-pinned special case, kept as the ergonomic spelling; `apply` is the general fallback and the
only one that spells the arg-first grain. The `quote<>` wrapper is the tax C++'s single-kind parameter
packs impose (a template can't sit in a type slot unquoted); a C++26 reflection build erases it, since
templates and types both become `std::meta::info` and the slot list stops needing the wrapper. Naming
(`apply` / `quote`) is provisional — the mechanism is what's settled, not the spelling.

**Type-level: natural spelling via std blanks** *(implemented, gated `TACIT_STD_BLANKS`, experimental)* —
the sugar tier: `std::map<struct _, int>::with<char>` reading as itself, no `quote`/`apply` ceremony.
It works by injecting partial specializations of common containers (`vector`, `set`, `map`, `pair`,
`tuple`) into `namespace std`, each keyed on the blank type and exposing a `::with<...>`. Four facts,
all verified on g++-13 / clang-18, fix its exact shape and are why it's off by default:

- *The blank must be elaborated.* A template argument names the value `_` first, so a bare `_` is a
  non-type argument where a type is wanted; the specializations key on `struct ::tacit::_` (and callers
  write `std::map<struct _, int>`), the same tag-namespace trick `bind` already leans on.
- *Explicit arity, not a trailing pack.* A trailing `...` is not "more specialized" than a fixed-arity
  primary, so the defaulted `Compare`/`Allocator` params can't hide behind a pack — each shape is a
  SHAPE macro (`SPEC_1_1` / `SPEC_1_2` / `SPEC_2_2`) that names them. One macro per (fillable, trailing)
  shape plus a short table; medium curation weight.
- *Reconstruct fresh defaults.* `map<struct _, T, C, A>::with<K>` yields `map<K, T>`, **not**
  `map<K, T, C, A>` — carrying the blank-derived `less<blank>` / `allocator<…blank…>` along silently
  poisons the result (`is_same` fails). The price: you can't thread an explicit non-default
  Compare/Allocator through a blank; drop to `apply`/`bind` for that.
- *Variadic templates take one leading blank, portably.* `tuple<struct _, R...>` is legal at any arity,
  but interior blanks (`tuple<int, struct _, char>`) are ill-formed (pack must be last) and a *second*
  leading-blank spec makes g++ (not clang) call it ambiguous — so: prefix blank, one at a time.
- *Value-parameterized containers blank the type, the value rides.* `array` / `span` are `<class,
  size_t>`; `array<struct _, 5>::with<int>` == `array<int, 5>`, with the extent `5` a plain literal in
  a real template-id — no wrapper, because a non-type template argument is already a first-class thing
  to write. That's the whole reason this stays natural: a *probe* (`value_param_probes.cpp`) confirmed
  every value-parameterized template is also reachable through the general primitive — `<class, auto>`,
  `<auto>`, even `integer_sequence`'s dependent `<class T, T...>` — but only by making the caller pick
  a kind-matched `quote` *and* wrap each value as `val<5>`. Two visible taxes for the general case; the
  natural type-blank grain pays neither, so it's the only value-param grain shipped. The mirror (holing
  the *extent*, fixing the type) has no natural spelling — a type blank can't sit in a `size_t` slot —
  and is intentionally absent; reach for a one-line metafunction if you must vary an extent.

The politics: specializing a std class template for a blank type doesn't meet the original's
requirements, so `[namespace.std]` makes it ill-formed *no diagnostic required* — it compiles and does
the right thing on tested toolchains, but it's a spelling convenience, not a standards guarantee (the
`std::hash`-for-your-type precedent covers "specialize for your type," not "specialize into something
that isn't the thing"). Hence the gate: off by default, `apply`/`bind` is the portable always-on
surface, and Tier 1 is opt-in for those who want the natural read and accept the caveat.

**Type-level projection** *(implemented)* — the dual of `bind`: where `bind` wraps the blank in an
*outer* template, `_::name::of<X>` pulls a nested member *out* of X (`_::value_type::of<vector<int>>`
== `int`), the type-level twin of the value-level member vocabulary. It works because a name before
`::` is looked up considering only types, namespaces, and templates ([basic.lookup.qual]), so `_::name`
reaches `struct _` past the value that hides it — `_` now serves as value placeholder, bind blank, and
projection namespace under one symbol. Two constraints, both inherent rather than incidental: there is
no `operator()` at the type level, so a projection is *applied* with `::of<X>` (or by nesting, or a
`transform`), never called — the deep asymmetry with value-level `_.foo()`; and the vocabulary is
closed (each name is declared in `struct _`; teach it your own with `TACIT_NOUNS`, the type-level twin
of `TACIT_VERBS`), since `::` demands a real member. Reflection (P2996) could open the vocabulary, but
only through a `_::member<"name">`-style spelling or a splice, never `_::name` for an arbitrary name.

`TACIT_NOUNS` is a plain comma list of nested-type names (`_::name::of<X>` == `X::name`). Nested-
*template* projection — `_::name<A...>::of<X>` == `X::template name<A...>`, the rebind family — is its
own hook, **`TACIT_NOUN_TEMPLATES`**, kept separate so `TACIT_NOUNS` stays a clean bare-name list. The
split is inherent, not incidental: a bare name can't carry the template's own parameters, and a nested
*template* isn't a type until you supply them (`_::rebound<char>` first, then `::of<X>`) — the same
parameter-kind split that makes `bind` need `quote<F>`, surfacing on the projection side. Two lists
rather than one is the price of that asymmetry; the common (nullary) case pays nothing for it, and
nothing in the std table needs the templated form, so it stays opt-in and rarely reached for.

### Type level: what exists and what it spells

Moved out of the README, which now says only "pain, avoided for now" — the surface works, but the
notation never became pleasant enough to lead with. What is built and tested:

`_` is a blank in the **term** world, awaiting its subject. Two things sit beside it: a blank in the
**type** world, and a way to hand either world a subject it already has.

`tacit::blank<A...>` is the type-level twin, with two duals — `of` fixes the arguments and awaits the
template, `as` fixes the template and awaits the arguments:

```cpp
blank<int>::of<std::vector>            // std::vector<int>      head is the blank
blank<>::as<std::map>::with<int, char> // std::map<int, char>   head is given
```

Plain types and plain templates throughout — nothing quoted, nothing wrapped.

`_::rebind` works the other direction: it **decomposes** a specialisation you already have and
re-applies its template, so you never name the template at all —

```cpp
_::rebind<double>::of<std::vector<float>>   // std::vector<double>
_::rebind<double>::of<std::array<float, 5>> // std::array<double, 5>
```

Arguments are replaced wholesale, which is what defaulted parameters want: `std::vector<float>` is
really `vector<float, allocator<float>>`, so `rebind<double>` re-defaults the allocator instead of
carrying `allocator<float>` across. This is the shape `std::simd`'s `rebind_t` has and that
[P3971](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3971r0.html) is standardising.

`bind`/`apply` remain for the arg-blank grain with the blank spelled `_::blank<>`; `as` is the spelling
that needs no marker. See `tests/lift.cpp`, `tests/typelevel.cpp`, `tests/typeproject.cpp`,
`tests/typeapply.cpp`.

### Four quadrants: term/type × open/closed (design notes; not yet built)

#### Where it landed (decision)

> **Superseded in part — read this first.** The grid below was the plan; what shipped is narrower and
> better founded. `$` is a plain **function template in `namespace tacit`**, not an alias template and
> not a macro, and it is **term-only**: `$(x)` lifts a value, `$<F>(a…)` builds one (`make`). The type
> world kept its conforming spelling (`blank`/`bind`/`apply`/`rebind`) and never moved onto `$`.
>
> The reason is a language rule the plan below did not account for: **`$` is a function, so
> `$<std::map>::anything` is ill-formed** — a qualified name cannot refer into a specialization of a
> function template. A function can *return* a value but can never *be* a scope, so no amount of
> cleverness makes `$<…>::…` name a type. The alias-template-plus-macro scheme in the plan was the only
> way to get both, and it was dropped when `$` became a function (which bought namespacing, ADL,
> qualification, and freedom from include-order rules).
>
> **What a function template buys instead**, and it is not nothing: function templates are the one
> construct that can be **overloaded on parameter kind**, which class templates cannot be. So an
> explicit argument list may mix a *value* `_` with *types* — `$<std::map, _, int>` — if the overloads
> are enumerated, one per arrangement of blanks. That is what makes `make`'s partial CTAD spellable
> (`make<std::set, _, std::greater<>>(3,1,2)`), and it is the only place in the library where `_` sits
> among types in a template-argument list. It resolves the "blanks among arguments" problem below at
> *term* level — where a blank can be enumerated cheaply because the result is a value — while the type
> level, needing a class template, stays walled off exactly as described.

```
                open (blank)                     closed (given)
  term          _    _.f()  _ < _  _(x)         $(42).f()
  type          $<int>::of<F>                   $<>::as<F>::with<X>
```

**The assignment is derived, not chosen.** The name-kind wall only bites a name that must be usable
*bare*. `_` must be — `_ < _`, `_.f()`, `_(x)`, `_[i]` — which forces it to be a variable, which forbids
it from ever being a template. `$` is never bare: every use carries a bracket, `$<…>` or `$(…)`. So it
is free to be an alias template *and* a function-like macro at once, since one keys on `<` and the
other on `(`. `$` therefore needs no variable, no storage and no ADL surface — it is purely a name in
two syntactic slots, and the closest thing to a standalone `$` is `$<>`, which still wears its brackets.

`_` stays **macro-free and term-only**: an ordinary variable, so `_ < _`, `_.f()` and the application
form `_(x)` all survive untouched, along with scoping, shadowing and any local named `_`. The type
world is `$`, an alias template over the core's `tacit::blank<A...>`; `decltype(_)` is `blank<>`, so the
two worlds are still one template even though they no longer share a spelling. The term lift `$(42)` is
a **function-like macro**, which coexists with `$<int>` for free — a function-like macro fires only on
`(`, and `$<int>` has no paren. No probe, no region, no delimiters.

**`_<T>` is given up.** That is the whole cost, and it is a cost in symmetry rather than in practice:
the head-blank is rarely what you reach for, and everything it expressed is available as `$<T>`. The
grid stays orthogonal in meaning; only the notation stops rhyming.

All four cells are occupied, and the two type cells are **duals of one template**: `of` fixes the
arguments and awaits the template, `as` fixes the template and awaits the arguments —

```cpp
$<int>::of<std::vector>            // std::vector<int>   head is the blank
$<>::as<std::vector>::with<int>    // std::vector<int>   head is given
static_assert(std::is_same_v<$<int>::of<std::vector>, $<>::as<std::vector>::with<int>>);
```

which settles what looked like an open question: today's `bind`/`apply` is not a separate mechanism to
reconcile with the head-blank — it *is* the closed/type cell, reached through `as`. Both work on plain
types and plain templates, with no `quote<>` and no lifting.

So the whole design is one line: **one template `$<A...>`; `_` is its bare instance at value level
(`decltype(_)` is `$<>`), `of`/`as` are its two appliers, `$(…)` lifts a plain value.**

Because `$` is an extension identifier, the type world ships as an opt-in `<tacit/$.hpp>` and the core
must keep a conforming spelling of the same template (`tacit::blank<int>::of<F>`, or a user's own
`_t` alias) so a `-pedantic` project is never locked out. The `$(…)` macro makes that header
include-last.

Settled by exhaustion rather than taste — the sections below are the evidence:

- one kind per name, and no trick reaches it (conversions, inheritance, `constexpr` all act too late)
- `_ < _` stops parsing the instant `_` becomes a template — the constraint that ended most branches
- `_` and `$` are the *entire* non-alphanumeric ASCII palette; `@`/`#` are sealed, UCNs included
- non-ASCII (`τ`, `ℓ`) is legal and *more* conforming than either, but untypeable
- macros rescue call-shaped names, never bare ones

**Still open**: whether the term lift is worth a macro at all versus a plain word in the core. What was
given up is only the *spelling* `_<T>` — the concept survives as `$<T>` — leaving one scar: the open
column uses two symbols, `_` for terms and `$<>` for types, which is irreducible, since one name cannot
be both a variable and a template.

#### The wall: one name, one kind

Everything tried, all rejected by the compiler:

| attempt | result |
| --- | --- |
| class template `_` + variable `_` | `redefinition of '_' as different kind of symbol` |
| …+ variable of an unrelated type, or a function `_()` | same |
| variable template `_` + variable `_` | same |
| `namespace _` + variable `_` | same |
| concept `_`, typedef-name `_`, alias template `_`, each + variable `_` | same |
| member named `_` inside `struct _` (`_::_`) | `member '_' has the same name as its class` |
| two inline namespaces merging both | `reference to '_' is ambiguous` |
| bare use of a variable template | `requires template arguments` |
| `struct _` with **templated constructors**, hoping for `_<int>` | `expected unqualified-id` — see below |

Only a **class** name and an **enum** name may share a name with a variable — the C struct-hack, which
is exactly what today's `struct _` exploits. It does not extend to templates. Note how narrow this is:
even a plain `using _ = H;` beside a variable `_` is a redefinition, so the exemption is not about
"type names" generally — it is specifically about a **class-head name**, one introduced by
`class`/`struct`/`union`/`enum`. Conversions, inheritance and `constexpr` are all irrelevant: the
conflict resolves at *name lookup*, before types or values exist, so there is nothing yet to convert
from. No future language version changes this.

**Templated constructors do not open a back door**, and it is worth being precise about why, since the
idea is a natural one: keep `_` a class (so the exemption survives) and put the template parameters on
its constructors instead of on the class, hoping to buy `_<int>` without becoming a template. Three
independent walls, any one of them fatal:

1. **A constructor's template arguments can never be written explicitly.** There is no syntax for it —
   not `_<int>(x)` (`expected unqualified-id`), not `_::_<int>(x)` (`qualified reference to '_' is a
   constructor name rather than a type`). They are always deduced from the arguments. So a constructor
   template gives `_(x)`, never `_<int>`.

   Worth being exact, because the declaration itself is perfectly legal and looks promising:

   ```cpp
   struct _ {
     template <auto...> constexpr _() {}     // compiles fine — and is unreachable
   } inline constexpr _;
   ```

   That *does* compile. What compiles is the **struct-hack** (a class-head name beside a variable of
   the same name), which the library already relies on; the constructor template rides along without
   objecting. But every way of supplying its arguments is rejected — `_<1>`, `_<1>{}`, `_<1>()`,
   `decltype(_)::_<1>()` — and since the pack is non-deducible it is only ever deduced *empty*. So the
   declaration is not a foothold: it is a default constructor with unreachable parameters.
2. Even granting (1), `_<int>` requires `_` to *name a template* at the point of parse, which is the
   redefinition in row 1 of the table.
3. Even granting (1) and (2), the variable `_` **hides** the class in expression contexts, so `_(42)`
   is a call on the variable and can never reach a constructor at all. Reaching the class in an
   expression needs an elaborated-type-specifier, which has no expression form.

The general shape: `<…>` after a **bare** name always demands that the name be a template, and that is
the one thing `_` can never be. What survives is `<…>` after a `.` or a `::` — member function
templates and member class templates *do* take explicit template arguments, which is exactly the route
`_::blank<int>`, `_::rebind<double>` and `_.of<F>()` already take. That is the open axis; the bare one is
closed for good.

**The clincher**: if `_` were a template, `_ < _` stops parsing — the compiler reads `_<` as a
template-argument list (`expected '>'`), and `_ + _` fails because a template-name is not an
expression. Making `_` a template does not cost notation at the margin; it deletes the term world.

#### The palette: `_` and `$`, and nothing else

Non-alphanumeric ASCII, tested exhaustively as identifiers:

```
whole identifier:   $ _
rejected:           ! " # % & ' ( ) * + , - . / : ; < = > ? @ [ \ ] ^ ` { | } ~
inside one (x?y):   $        (nothing else besides _)
```

The sneaky paths are sealed too: `@` is rejected with *"character '@' cannot be specified by a
universal character name"* — the standard forbids UCNs for basic-charset characters precisely to
prevent this — and `@` cannot be a macro name (*"macro name must be an identifier"*). `$` *can* be a
macro name, object-like or function-like, which the macro section relies on.

Non-ASCII is legal but does not help. Measured on clang:

| tier | characters | `-pedantic-errors` | warnings |
| --- | --- | --- | --- |
| letters (`XID_Start`) | `λ α τ θ Ω Σ Δ` · `ℓ ℝ ℤ ℕ ℂ ℚ ℘ ℯ` · `ª º µ ı ˆ ᵗ` · `型 値 空` | ok | 0 |
| math notation | `∂ ∇ ∞` | **rejected** | 2, *"mathematical notation character"* |
| not identifiers at all | `∘ ∑ √ → ⇒ □ ● ¢ £ € · ± × ÷ § © ® ° ¹ ² ½ ✓ ★ ‿ ⁀` | — | — |

The first tier is the surprise: `τ<int>` or `ℓ<int>` is **strictly more conforming than either `__` or
`$`** — fully standard, zero warnings, and collision-proof for a reason no ASCII name can match, since
nobody declares a variable named `τ`. What rules it out is untypeability, not legality. Math *symbols*
are a trap that looks like the opposite: `∂`/`∇`/`∞` compile by default but are a clang extension,
dying under `-pedantic` exactly like `$`. (GCC and MSVC untested; C++23 mandates UTF-8 source (P2295)
and UAX #31 identifiers (P1949), so a conforming C++23 compiler should accept the first tier.)

Where the ASCII candidates sit:

| spelling | legal identifier? | `-pedantic-errors` | `-Wall -Wextra -Werror` |
| --- | --- | --- | --- |
| `_`, `_t` | yes; reserved only *in the global namespace* | compiles | passes |
| `__` | yes, but reserved **everywhere, for any use** | compiles | passes |
| `$`, `$$` | **not an identifier in standard C++** | **rejected** | passes |

Each is caught only by its own opt-in warning — `-Wreserved-identifier` for the underscore forms,
`-Wdollar-in-identifier-extension` for `$`. `__` is a conforming program grabbing a name the
implementation owns (ill-formed, no diagnostic required): it survives strict mode and breaks only if a
stdlib ever claims `__`, which none does today. `$` is not C++ at all and dies under `-pedantic`
deterministically. Two different bets, one rung apart.

Ordinary word-shaped names (`t`, `ty`) are worse than either: any local of the same name hides the
template and the error lands at the *use* site (`no template named 't'`). The reservation that makes
`__`/`_t` formally risky is exactly what makes them practically collision-proof — conformance risk and
collision risk point in opposite directions, and collision is the one that bites daily.

`$` inherits the same wall: `$(42)` is a function and `$<std::map>` a template, so they collide just as
`_` did, and a second name (`$$`) would be needed. The 2×2 of names is forced by the language.

#### One template, two worlds

The type-level blank is `_::blank<>`, and it is the only spelling. `struct _` and `decltype(_)` were
both discarded: the placeholder's own type is the *term*-level object, and conflating it with the
type-level blank is what made the old spelling need a `struct` crutch. `bind`/`apply`/`TACIT_STD_BLANKS`
all take `blank<>` now. Watch for silent failure when adding a spelling — an unrecognised blank does not
error, it quietly becomes a fixed argument (`bind<std::vector, _::blank<>>` compiled and produced
`std::vector<blank<>>` before the slot-fillers were taught about it).

Every spelling below is notation over the same core: one ordinary, conforming name — `tacit::blank<A...>`
(or `blank`) — whose bare specialisation is also the type of the value, so `decltype(_)` *is* `blank<>`.
Users who want a short alias write one line of their own:

```cpp
template <class... A> using __ = tacit::blank<A...>;   // or _t, or Ty, or whatever
```

Verified transparent: trait matching, partial specialisations written in the alias spelling, and
deduction through it all see `blank`. `using __ = tacit::blank<>;` is *not* enough — a plain alias gives
only the bare blank, and `__<int>` then fails; it must be the alias-template form.

That the alias is the user's own line is the point: the library never declares a reserved or
non-conforming identifier, so the core stays strictly conforming and the gamble, where there is one, is
visible in the file that takes it.

#### What macros rescue, and what they don't

They **cannot** make `_` both a value and a template: `_ < _` and `_ < int >` are lexically identical at
`_ <`, with no paren to key on. Both directions were tried — object-like `_` expanding to a template
name, and to a value — and both fail. "Define it object-like *and* function-like" is a redefinition
(`-Wmacro-redefined`, second wins), not coexistence.

They **can** put a call-shaped name beside a template-shaped one, since a function-like macro fires only
on `(`:

```cpp
template <class... A> using $ = tacit::blank<A...>;   // $<int>, $<>
#define $(...) tacit::lift(__VA_ARGS__)              // $(42)
```

Verified across `$<int>::of<F>`, `$<>`, `$(x)`, `$ (x)` (space still expands), nested `$($(x))`,
multi-argument `$(a, b)`, `$<int>{}`, and an unrelated `$x`; definition order is irrelevant. The cost is
the usual macro cost — no ADL, no overloading, no namespace, no scope — so such a header must be
included **last**, a genuine ordering constraint unlike the order-independent name collisions elsewhere.

The same trick puts the *whole grid* on `_`, and it is fully conforming, but it forks the surface:

```cpp
template <class... A> using _ = tacit::blank<A...>;
#define _(...) tacit::blank<>{} __VA_OPT__(.apply(__VA_ARGS__))
//  _<>  _<int>  types        _()  _(3)  terms
```

Clean under `-Wall -Wextra -Werror -pedantic-errors`, and the only fully-conforming way to get all four
cells on one symbol. Rejected anyway: `_ < _` becomes `_() < _()`, taxing the hot path; `_` as a
function-like macro claims `_(` TU-wide, colliding with gettext's `_("...")` — the very hazard the
header already cites; and it is a *second surface*, not a spelling, so docs, tests and examples fork.

#### Preprocessor techniques, and their limits

The preprocessor's *only* lookahead is "is the next token `(`". Every Boost.PP idiom — `CAT` dispatch,
`IS_BEGIN_PARENS`, the probe/`CHECK` family — is built on that one bit, and it is also the exact reason
a macro can never split `_ < _` from `_ < int >`: no paren to key on.

What it does buy, all verified:

- **A call-shaped name beside a template-shaped one.** `#define $(...)` and `template <class... A> using
  $ = …` coexist, since the macro fires only on `(`. This is what the decision above rests on, and it
  needs no probe at all.
- **Token pasting eats a leading token.** `a##__VA_ARGS__` joins with the *first* token of the argument,
  so a macro can consume a leading `_` and leave the rest intact:
  `_T(_<int>::of<std::vector>)` → `tacit::blank<int>::of<std::vector>`. `CAT` must be variadic, since
  `<int, char>` contains commas the preprocessor treats as argument separators. Limits: only the
  *leading* `_` is replaced, so nested blanks (`_T(_<_<int>>)`) fail, and input not starting with `_`
  produces a garbage identifier. Scanning arbitrary token soup for every `_` is not something the
  preprocessor can do — Boost.PP rewrites only *enumerated* structures (sequences, tuples).
- **Paren-probe dispatch**, if one macro must serve both worlds:
  `_(int)` → `blank<int>`, `_((3))` → the application form, chosen by `IS_PAREN` on the first argument.
  Verified working end to end. Not needed under the decision above, since `$<…>` and `$(…)` already
  differ by bracket.

Two subtleties cost real time and are easy to hit again:

- `IS_PAREN` must probe **only the first argument** — a single-parameter version overflows on
  `_(int, char)`.
- The probe name must be juxtaposed with an **already-expanded** token, never with a macro *call*.
  `PROBE FIRST(...)` silently yields 0 forever, because when the scanner considers `PROBE` the next
  token is an identifier, not `(`. Split it: `IS_PAREN(...)` → `IS_PAREN_I(FIRST(...))`. This is why
  `BOOST_PP_IS_BEGIN_PARENS` takes its argument directly.

All of this dissolves under C++26 reflection: `^^int` is a *value*, so a single `template <auto...>`
blank accepts `blank<^^int>` and `blank<3>` alike — one template, no probe, no marker parens, no macro.
The preprocessor work here is the C++23 stand-in for that.

#### Blanks among arguments: why the kind wall bites here too, and what to do instead

The type-level blank cannot be the *value* `_`, for the same reason the term-level one cannot be a
template — but the failure lands one level earlier than expected, and it is worth being precise:

- **Class templates cannot be overloaded at all.** Two primaries with the same name is
  *"too many template parameters in template redeclaration"*, and a partial specialization cannot
  introduce a parameter of a different *kind* than the primary's. So the kind of every argument
  position is fixed once, at the primary.
- **Mixed parameter *lists* are legal**, and `_` can be specialized on as a value:
  `template <class T, auto... V> struct B` accepts `B<int, _>`, and
  `template <class T, auto... R> struct B<T, _, R...>` matches it while `B<int, 42>` takes the
  primary. The specialization machinery is entirely capable — it is never the obstacle.
- **But positions are kind-locked.** `B<int, _, char>` and `B<_, int>` both fail, because a blank may
  appear at *any* position and the arguments around it are types. `bind<map, int, _>` and
  `bind<map, _, int>` need opposite layouts from one template. So the error from `bind<vector, _>` is
  not "no specialization matched" — argument resolution fails before any specialization is consulted.
- **Chaining does reach it**, since each step is its own template with its own kinds:
  `builder<std::map>::a<int>::v<_>::with<char>` works, value `_` and all. Set aside because it puts a
  marker on *every* position to buy a blank at one.

**And when the arguments are all values, do not enumerate — compute.** Pinning `_` positionally by
partial specialization works (`sub<1, _, 3>` selects the blank-at-1 pattern) but costs 2^n patterns:
the last blank at position n needs every subset of the n slots before it, so 63 specializations only
reaches arity 5. It also goes *ambiguous* — `sub<_, _>` matches both the blank-at-0 and blank-at-1
patterns until the `<_, _, R...>` combination is written out too. All of that evaporates by folding
over the pack instead:

```cpp
template <auto... A> struct pattern {
  static constexpr std::array<bool, sizeof...(A)> blank{is_hole_v<A>...};
  static constexpr std::size_t blanks = (0 + ... + (is_hole_v<A> ? 1 : 0));
};
pattern<_, 1, _, 2, _, 3, _, 4, _>::blanks == 5    // any arity, any combination, no macro
```

One primary, no specializations, no ambiguity, no bound — and it is the same algorithm
`detail::slot_map` already runs over type packs for term-level blanks (`slot_at`, `slots_before`,
`pick`), so the value-pack version shares a shape with what is there rather than inventing a second
one. This is the foundation `$1`/`$2` should be built on.

### Synthetic sigils: the exhaustive sweep (implemented, opt-in)

C++'s overloadable-operator set is closed, so a "new operator" can only ever be a **token sequence**
the lexer splits, by maximal munch, into operators that already exist. `f &&& g` is binary `&&`
applied to `f` and unary `&` applied to `g` — one glyph to a reader, two operators to the compiler.

**The sweep.** Every sequence of the form `[postfix-unary]? binary [prefix-unary]*` was generated
over the full punctuator set, run through an exact maximal-munch tokenizer, and then compile-tested:

| | |
| --- | --- |
| generated (postfix × binary × prefix, ≤2 prefixes) | 7194 |
| lex as intended | 6579 |
| **stolen by maximal munch** | **615** |
| of the practical subset (391 lex-valid) — **compile** | **368** |

The 615 are the interesting failures: the sigil *cannot* mean what it looks like because a longer
token eats its prefix. `a + + b` is not `+` twice, it is `++`; `a + & & b` is not `+`,`&`,`&`, it is
`+`,`&&`. Maximal munch is not negotiable, so those spellings are unavailable at any price.

**Against Haskell's arrow vocabulary**, which is the canonical naming for exactly these combinators:

| spelling | in C++ | why |
| --- | --- | --- |
| `&&&` fanout | **available** | `&&` + unary `&` |
| `***` product | **available** | `*` + unary `*` + unary `*` |
| `+++` sum | **available** | postfix `++` + `+` |
| `\|\|\|` fanin | unavailable | lexes `\|\|` `\|`, and `\| g` is not an expression |
| `>>>` compose | unavailable | lexes `>>` `>`, and `> g` is not an expression |
| `<<<` compose | unavailable | lexes `<<` `<`, same reason |

That `>>>` is unavailable is why composition here is spelled **`->*`** — a real, single, overloadable
operator that nothing else in the library claimed.

**What shipped** (behind `TACIT_COMBINATORIAL_OPERATORS`): `->*` compose left-to-right, `<<*` compose
right-to-left, `&&&` fanout, `***` product.

**The mechanism, and its one cost.** Both halves of every sigil are already spoken for: `f && g` is
the logical-and section and `&g` is the address-of section. So a sigil cannot be *added* to the
surface — it can only be carved out of what those already mean. Under the gate, unary `&` and `*` on
a closure return a `marked` type that **derives from `fn`** and additionally carries the un-addressed
operand. Deriving is what makes it free: `(&_)(c) == &c` and `(*_) + 1` are bit-for-bit unchanged,
every section still finds it through its base, and `is_fn_v<marked>` is specialised to `true` so the
`not_fn` constraints throughout the header keep treating it as the closure it is (which is also what
breaks the overload ambiguity — without it the ordinary `fn op value` sections claim a marked right
operand as a bound value and the candidates tie).

The single reading given up is `f && (&g)` — logical-and against an address-of closure. Nobody writes
it. That is why this is affordable, and why it is nonetheless gated.

**Precedence is inherited, and it bites twice.** A sigil has no precedence of its own — it takes the
binary half's on the left, and the *unary* half grabs only the primary expression on its right:

- `->*` sits just below postfix, so **both** operands usually want parentheses: `_ + 1 ->* _ * 2`
  parses as `_ + (1 ->* _) * 2`.
- `&&&` sits near the bottom, so the left operand needs none — but `f &&& _ * 2` is still
  `f && ((&_) * 2)`, because unary `&` binds tighter than `*`. Right operands past a single postfix
  expression want parentheses: `f &&& (_ * 2)`.

#### `$` as a class rather than a function (explored; rejected)

A class would buy back the one thing the function cannot have — **a scope**. `$<int>::of<F>` works for
a class template and is ill-formed for a function template, and CTAD plus a deduction guide keeps the
term form, so a class-`$` sketch compiles with both cells occupied:

```cpp
template <class... A> struct $ {
  template <template <class...> class F> using of = F<A...>;
  template <template <class...> class F> struct as { template <class... X> using with = F<X...>; };
  constexpr explicit $(A... a) requires (sizeof...(A) > 0);   // `$(42)` via a deduction guide
};
$<int>::of<std::vector>   // std::vector<int>   — the `::` a function can never have
$(std::vector{1,2,3})     // CTAD              — the term form survives
```

**But it forfeits kind-overloading entirely, and that is the whole point of the function.** A class
template can be neither overloaded nor kind-polymorphic:

```
error: template parameter has a different kind in template redeclaration
```

So a class `$` cannot take a template-template argument (`$<std::map>` is a hard error — `std::map` is
not a type) and therefore loses `$<std::map>(…)`, `$<std::map, _, int>(…)`, and mixed value/type lists
— i.e. all of `make`, partial CTAD included. The trade is: **a class buys `::`, which `blank`/`bind`
already spell conformingly; a function buys kind-overloading, which nothing else in the language does.**
Since `make` is the only genuinely new capability `$` unlocks, the function keeps it.

Two further costs, both practical rather than fundamental. CTAD on `$(v)` deduces `$<std::vector<int>>`
— by **value**, so the wrapper silently copies its subject unless extra guides (`$(T&) -> $<T&>`) are
written; the function `lift` decides that deliberately, per-argument. And a class makes `$` a bare
**type name**, so `$ x{v};` becomes a declaration — a fourth syntactic role for a symbol that was
chosen partly because it is never bare.

`()` versus `{}` would matter only mildly: both do CTAD, but braces forbid narrowing, prefer any
`initializer_list` constructor, and permit aggregate initialisation without a constructor at all.
`${42}` and `$(42)` would agree; `${1, 2}` would reach an `initializer_list` overload that `$(1, 2)`
would not, which is precisely the ambiguity `make<F>(a…)` inherits from writing `F{a…}` by hand.

#### Spellings considered and set aside

- **`t::_<int>`** (type world in its own namespace) — legal and zero-risk, but a foreign scope reads as
  a different library rather than the same `_`, and `t` is far too common a name to expose.
- **`::_<int>`** (global alias, value nested) — works, but forces every TU to nest its
  `using tacit::_;` inside a namespace or function, breaking `TACIT_USING_UNDERSCORE` and every
  file-scope example. Taxes the common path to beautify the rare one.
- **`__<int>` / `_t<int>`** (user-written alias) — the fallback if regions prove unwieldy; see the
  conformance ladder. Opt-in by include, so purists never see it.
- **`$<int>`** (extension identifier, opt-in header) — shorter than a region, never misread as `_<int>`,
  and its failure mode is better than `__`'s: it fails deterministically at its own line under
  `-pedantic`, today or never, rather than deferring a collision into a stranger's TU years later.
- **`_::t<int>::of<F>`** (member of `struct _`) — one symbol, fully conforming, terms untouched, and
  consistent with the existing `_::name::of<X>` noun grammar; costs four characters of `::t`. The
  runner-up, and the safe choice if the region delimiters prove annoying in practice.
- **Bounded type region** (`#include <tacit/types_begin.hpp>` … `types_end.hpp`, with `_` `#define`d to
  the blank inside) — the only design that kept *both* `_ < _` and `_<int>`, fully conforming, with no
  macro at term level. Set aside because per-region `#include` delimiters are too heavy for something as
  ordinary as writing a type, and a forgotten close leaks the macro into everything downstream.
  `#pragma push_macro`/`pop_macro` nests if it is ever revisited.
- **The crossing** (`_` for types, `$` for terms) — compiles, and both crossed spellings work: `_()` and
  `_{}` are usable term blanks, and `$<>` works if `$` is a variable *template* (which then costs bare
  `$`). Rejected because it inverts the priority — the clean native spelling goes to the rarer world and
  the constant one pays, either with `$` (making the **core** non-conforming, not a quarantined header)
  or with `_()` at every use. Worth keeping in the back pocket: `_()`/`_{}` being valid term blanks means
  that if `_` ever must become a template, the term world degrades rather than vanishes.

### The placeholder is always `_` (decision)

An earlier design let you *derive* a fresh placeholder object — `TACIT_LIEUTENANT(teller, it, …)` minted
a `bank::it` with its own vocabulary. That's gone. The whole appeal of `_` is that it's the one
interface; a second object like `bank::it.deposit(_)` splits the reader's attention between two spellings
of the same idea and reads as opaque. So there is exactly one placeholder, and domain names extend *it*,
in place, via `TACIT_VERBS` / `TACIT_NOUNS`.

This is a forced consequence, not just taste: `_.deposit(…)` needs `deposit` to be a member of `_`'s
type, and a class's members can only be declared at its definition — so a single `_` *must* gain its
verbs at include time from a pre-`#define`, and a genuinely separate/namespaced/restricted vocabulary is
exactly the thing only a separate type could provide. Giving that up is the price of "one `_`", and it's
the price this design chooses to pay. The removal also let the whole `TACIT_KEEP_MACROS` switch go: with
no derive-your-own path, nothing downstream needs the internal generator macros, so the header always
cleans them up — one include path, no knobs. (The pre-C++26 need to pre-register names is what
`_.m<"deposit">(…)` escapes on a reflection toolchain; see the reflective hatch.)

**Adoption / packaging** *(in progress)* — added `install()` + a generated `tacitConfig.cmake` so
`find_package(tacit)` and FetchContent work, and a `TACIT_VERSION` macro. Still worth doing: a Godbolt
"try it" link, clearer diagnostics (named concepts), and a short recipes section.
