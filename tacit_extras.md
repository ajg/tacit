# tacit — extras & design notes

Design notes for the pieces that sit *around* the core `_` object: what's implemented, why it's
shaped the way it is, and what's still on the table. The default public surface is `tacit::_` plus the
opt-in type-level `tacit::bind` / `tacit::apply` / `tacit::quote`; the free-function combinators are
gated behind `TACIT_COMBINATORS`, and the natural-spelling std holes behind `TACIT_STD_HOLES`.
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

## Operator surface (implemented)

Beyond comparison/arithmetic, the sections now cover bitwise `& |`, shift/stream `<< >>`, logical
`&& ||`, comma `,`; the unary operators `* - + ! ~ &` and `++ --` (pre/post); assignment `=` and
compound `+= -= *= /= %= ^= &= |= <<= >>=`; and `->`. Notes and decisions:

- **`&&` / `||` are two-input combiners in the two-blank form** (`_ && _` == `(a,b) -> a && b`), like
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
- **Comma pairs (not the built-in comma).** `_ , _` is `(a, b) -> std::pair{a, b}`, and `_ , y` / `x , _`
  bind the fixed side — comma reads as the tuple/pairing glyph rather than "evaluate-and-discard". It's a
  custom overload (not `TACIT_SECTION`, whose `x op y` body would be the built-in comma = return `y`),
  guarded to `tacit::_` operands, so it only fires in a comma-*operator* context; argument-list and
  init-list commas are separators and untouched. Overloading `operator,` is normally a footgun, so this
  was added only after confirming the full suite (folds over tuples, `std::apply`, etc.) still passes.
  Binary only for now — `_ , _ , _` needs an `fn ,` accumulator to flatten, deferred.
- **Deferred / experimental:** `operator->*` and `_[&Member]` (the `.*` gap — `.*` isn't overloadable).

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
class template `F` and holes among its *arguments*; it cannot hole the template itself. `apply` closes
that gap with a single op. Quote a template into a type — `quote<F>` — so it can occupy a slot next to
ordinary type args, then `apply<Slots...>::with<Fills...>` fills each `struct _` slot (template *or*
argument) left to right:

    apply<quote<std::map>, struct _, struct _>::with<int, char>   // std::map<int,char>  (fix template)
    apply<struct _, int, char>::with<quote<std::map>>             // std::map<int,char>  (fix args)
    apply<struct _, int, struct _>::with<quote<std::map>, char>   // std::map<int,char>  (hole both)

So `bind<F, A...>::with<X...>` is exactly `apply<quote<F>, A...>::with<X...>` — `bind` is the
template-pinned special case, kept as the ergonomic spelling; `apply` is the general fallback and the
only one that spells the arg-first grain. The `quote<>` wrapper is the tax C++'s single-kind parameter
packs impose (a template can't sit in a type slot unquoted); a C++26 reflection build erases it, since
templates and types both become `std::meta::info` and the slot list stops needing the wrapper. Naming
(`apply` / `quote`) is provisional — the mechanism is what's settled, not the spelling.

**Type-level: natural spelling via std holes** *(implemented, gated `TACIT_STD_HOLES`, experimental)* —
the sugar tier: `std::map<struct _, int>::with<char>` reading as itself, no `quote`/`apply` ceremony.
It works by injecting partial specializations of common containers (`vector`, `set`, `map`, `pair`,
`tuple`) into `namespace std`, each keyed on the hole type and exposing a `::with<...>`. Four facts,
all verified on g++-13 / clang-18, fix its exact shape and are why it's off by default:

- *The hole must be elaborated.* A template argument names the value `_` first, so a bare `_` is a
  non-type argument where a type is wanted; the specializations key on `struct ::tacit::_` (and callers
  write `std::map<struct _, int>`), the same tag-namespace trick `bind` already leans on.
- *Explicit arity, not a trailing pack.* A trailing `...` is not "more specialized" than a fixed-arity
  primary, so the defaulted `Compare`/`Allocator` params can't hide behind a pack — each shape is a
  SHAPE macro (`SPEC_1_1` / `SPEC_1_2` / `SPEC_2_2`) that names them. One macro per (fillable, trailing)
  shape plus a short table; medium curation weight.
- *Reconstruct fresh defaults.* `map<struct _, T, C, A>::with<K>` yields `map<K, T>`, **not**
  `map<K, T, C, A>` — carrying the hole-derived `less<hole>` / `allocator<…hole…>` along silently
  poisons the result (`is_same` fails). The price: you can't thread an explicit non-default
  Compare/Allocator through a hole; drop to `apply`/`bind` for that.
- *Variadic templates take one leading hole, portably.* `tuple<struct _, R...>` is legal at any arity,
  but interior holes (`tuple<int, struct _, char>`) are ill-formed (pack must be last) and a *second*
  leading-hole spec makes g++ (not clang) call it ambiguous — so: prefix hole, one at a time.
- *Value-parameterized containers hole the type, the value rides.* `array` / `span` are `<class,
  size_t>`; `array<struct _, 5>::with<int>` == `array<int, 5>`, with the extent `5` a plain literal in
  a real template-id — no wrapper, because a non-type template argument is already a first-class thing
  to write. That's the whole reason this stays natural: a *probe* (`value_param_probes.cpp`) confirmed
  every value-parameterized template is also reachable through the general primitive — `<class, auto>`,
  `<auto>`, even `integer_sequence`'s dependent `<class T, T...>` — but only by making the caller pick
  a kind-matched `quote` *and* wrap each value as `val<5>`. Two visible taxes for the general case; the
  natural type-hole grain pays neither, so it's the only value-param grain shipped. The mirror (holing
  the *extent*, fixing the type) has no natural spelling — a type hole can't sit in a `size_t` slot —
  and is intentionally absent; reach for a one-line metafunction if you must vary an extent.

The politics: specializing a std class template for a hole type doesn't meet the original's
requirements, so `[namespace.std]` makes it ill-formed *no diagnostic required* — it compiles and does
the right thing on tested toolchains, but it's a spelling convenience, not a standards guarantee (the
`std::hash`-for-your-type precedent covers "specialize for your type," not "specialize into something
that isn't the thing"). Hence the gate: off by default, `apply`/`bind` is the portable always-on
surface, and Tier 1 is opt-in for those who want the natural read and accept the caveat.

**Type-level projection** *(implemented)* — the dual of `bind`: where `bind` wraps the hole in an
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
