# tacit

[![ci](https://github.com/ajg/tacit/actions/workflows/ci.yml/badge.svg)](https://github.com/ajg/tacit/actions/workflows/ci.yml)

> **Status: experimental, pre-1.0.** The API may change, and it isn't released or announced yet.

A tiny, header-only library for **point-free (tacit) programming** in C++23. Its whole public surface
is one object, `tacit::_`, whose members return closures that forward to a same-named operation on
whatever they're later applied to — so you can hand operations to algorithms without writing lambdas.

```cpp
#include <tacit/_.hpp>
using tacit::_;

std::ranges::sort(nums, _ < _);
std::ranges::count_if(nums, _ == 0);
std::ranges::transform(words, out, _.size());
```

`using tacit::_;` imports exactly one name. The vocabulary is reached *through* the object, the
operator sections are hidden friends found by ADL, and everything else is a qualified `tacit::`
helper.

## Requirements

- **C++23** (tested on clang 18 and 22, g++ 13 and 16, `-std=c++23`). No dependencies beyond the
  standard library.
- Optional **C++26 reflection (P2996)** unlocks the reflective members; auto-detected, otherwise
  compiled out. See [Reflective hatch](#reflective-hatch-c26).

## Concepts

### Blanks (partial application)

Each `_` token is one **blank**; the arity of the resulting closure is the number of blanks, filled
left to right. The receiver counts as a blank:

```cpp
_.push_back(y)    // 1 blank  (c)        ->  c.push_back(y)
_.push_back(_)    // 2 blanks (c, v)     ->  c.push_back(v)
_.replace(_, _)   // 3 blanks (c, a, b)  ->  c.replace(a, b)
_ + _             // 2 blanks (a, b)     ->  a + b
```

Repeated `_` are *distinct* blanks — there are no positional `_1`/`_2` sigils. Reach for a named
lambda the moment you need to reorder or reuse an argument.

A blank can also *project*: an `fn` in argument position applies its projection to the fill, so
`_.push_back(_.size())` is `(c, x) -> c.push_back(size(x))`.

A blank always means "another fill", even where that reading is less obvious — `_[_]` is
`(x, i) -> x[i]`, `_(_)` is `(f, x) -> f(x)`, and `_.size() < _` is `(v, n) -> size(v) < n`. Where a
blank *cannot* be filled, the expression is rejected where you write it: a chained call binds its
arguments, so `_.front().substr(_)` is a compile error, not a dead closure. (`_.substr(_)` is fine —
that section fills its own blanks.)

### Vocabulary

`_` carries a curated first-class vocabulary of standard-library member names (`at`, `push_back`,
`substr`, `value_or`, `find`, `emplace`, …), kept in one editable table. Range access
(`size`, `begin`, `end`, `empty`, `data`, …) routes through the `std::ranges` customization points, so
`_.size()` / `_.begin()` also work on C arrays, string views, and third-party ranges.

The table reaches past containers to the names you project or test with: diagnostics (`what`,
`message`), `filesystem::path` (`extension`, `stem`, `filename`, …), the monadic family (`and_then`,
`transform`, `value_or`, …), concurrency (`join`, `load`, `wait`, …), streams, `bitset`, `regex`
match results, `complex`, `chrono`, and `span`. Names cost nothing until used — each is a member
template, so a wider table is a longer declaration list, not a bigger binary.

**Tuple-like projection** takes a *template* argument, so it gets its own spelling — by index or by
type, either of which composes like any other verb:

```cpp
_.get<0>()                              // x -> std::get<0>(x)
_.get<std::string>()                    // by type
_.get<1>().size()                       // composes onward
std::ranges::sort(v, {}, _.get<0>());   // sort by first element
std::ranges::count_if(v, _.get<0>() > 1)
```

It reaches the free `get<…>(x)` first — the real route for `tuple`, `pair`, `array`, `variant` and
`subrange` — and falls back to a member `get<…>()` for types that spell it that way. The plain
`_.get()` (`shared_ptr`, `unique_ptr`, `future`) is untouched: the two overload rather than collide.

The rest of the **type-argument** family follows the same shape:

```cpp
_.to<std::vector>()                       // C++23 ranges::to, pipeline terminator
_.to<std::vector<long>>()                 // ...or spelled out
_.any_cast<int>()
_.holds_alternative<std::string>()
_.duration_cast<std::chrono::seconds>()
_.static_pointer_cast<Derived>()
```

These are reached *unqualified*, by ADL, so the header doesn't have to include `<any>`, `<variant>`,
`<memory>` or `<chrono>` — a caller holding a `std::any` has already included `<any>`. (`ranges::to`
is the exception: `std::ranges` isn't an associated namespace of `std::vector`, so it's qualified.
It's also feature-tested — it shipped in libstdc++ 14 and libc++ 17, so `_.to<C>()` is simply not
declared on g++-13; `#if TACIT_HAS_RANGES_TO` to test for it.)

**Field-style verbs.** `pair`'s components are data members, not calls, and read better without empty
parentheses:

```cpp
std::ranges::sort(v, {}, _.first);           // not _.first()
std::ranges::count_if(v, _.second.size() == 2u)
_.first(p) = 9;                              // a reference, so it writes through
```

Because `first` is a field rather than a call, it doesn't chain **from** a projection
(`_.front().first` has no member to find) and the lift doesn't mirror it — both hops spell the same
access as `.get<0>()`.

There is a type-level table too — see *Type level*, below.

### Operator sections

The comparison, arithmetic, bitwise, shift, and logical operators are finite and lexical: `_ == y`,
`x + _`, `_ + _` all build the obvious closure (a one-sided form is unary; `_ op _` is a two-input
combiner). Unary forms work too — `-_`, `!_`, `~_`, `*_` (deref), `++_` — as does streaming
(`os << _`, so `ranges::for_each(v, std::cout << _)`) and member access through a pointer,
`_->size()`, which uses the pointee's real `operator->`.

Its sibling `->*` keeps its natural meaning, member-pointer projection: `_ ->* &Widget::x` is
`p -> (*p).x`. Since `.*` is not overloadable this is the only point-free spelling there is, so the
section falls back to deref-then-select where no built-in `->*` exists — smart pointers and
iterators work, not just raw pointers. Data members only: `(*p).*pmf` is valid solely as a call
head, so member *functions* stay with `_->f(args)`.

**Comparisons chain.** C++ parses `0 < _ < 10` as `(0 < _) < 10` — a *bool* compared against 10, so
the closure is silently always true. A comparison section therefore remembers its rightmost operand,
and a comparison applied to one rewrites itself into the conjunction the notation means:

```cpp
0 < _ < 10          // x -> (0 < x) && (x < 10)      not  ((0 < x) < 10)
1u <= _.size() < 4u // x -> (1 <= size(x)) && (size(x) < 4)
0 <= _ <= 10 < 20   // chains to any length, any mix of == != < > <= >=
```

The middle term is evaluated once per link (so keep a projection cheap and pure) and `&&`
short-circuits, exactly as in the spelled-out form. Only those six operators build a chain; any other
operator ends it. One gotcha: `(_ < 10) == false` chains too — it reads as
`(x < 10) && (10 == false)`, not `x >= 10`; spell that one `_ >= 10`. `_ < _` is unaffected: with two
blanks it's the two-input comparator, not a link.

Assignment is included and **mutates**: `_ = 0` and compound forms like `_ += 1` bind the argument by
reference, so `ranges::for_each(v, _ += 1)` updates `v` in place. Bitwise `|` is an ordinary section
(`_ | 4`), symmetric with `&`; general function composition lives in `tacit::compose`, not in `|`.

**Comma builds tuples.** `,` is the one section that makes data rather than calling something, and
the only n-ary one — each further `,` appends an operand:

```cpp
(_, _)                 // (a, b)    -> std::pair{a, b}
(_, 9)                 // x         -> {x, 9}      one-sided binds
(_, _, _)              // (a, b, c) -> std::tuple{a, b, c}
(_.size(), _.front())  // (a, b)    -> {size(a), front(b)}
(_, 5, _)              // (a, b)    -> {a, 5, b}   bound: no fill
```

Two operands stay a `std::pair`; three or more are a `std::tuple`. The operand list is **flat** —
`(_, (_, _))` and `((_, _), _)` are the same three-slot tuple. The parens are load-bearing everywhere
`,` would otherwise read as a separator (argument lists, init-lists) — that's the built-in comma
doing its usual job, untouched.

The usual blank rule applies, and it's the thing to watch: each `_` is a **distinct** blank, so
`(_.size(), _.front())` takes *two* arguments — it is not a one-argument key function. For the
same-input tuple (the lexicographic projection you probably want) that's `tacit::fanout`:

```cpp
tacit::fanout(_.size(), _.front())  // x      -> {size(x), front(x)}
(_.size(), _.front())               // (a, b) -> {size(a), front(b)}
```

A comma section composes onward through the value it builds, keeping its arity — the six comparisons
(the operators `pair` and `tuple` actually have) apply to what comes out:

```cpp
(_, _) == std::pair{1, 2}   // (a, b) -> {a, b} == {1, 2}
(_, _) < std::pair{2, 0}    // (a, b) -> lexicographic, as pair defines it
```

### Composition

The closure `_` hands back is itself composable, so a projection and a section chain without ever
naming a lambda:

```cpp
std::ranges::count_if(v, _.size() >= 2u);       // size(x) >= 2
std::ranges::sort(v, _.size() < _.size());      // order by size

auto scaled = (_ + 1) * 2;         // x -> (x + 1) * 2
auto head   = _[0];                // x -> x[0]
```

Composition is **arity-preserving**: `(_ + _) + 1` is `(a, b) -> (a + b) + 1`. Arity is load-bearing
only in argument position, where a *one-fill* closure is a projected blank (`_.push_back(_.size())`)
while a *many-fill* one is an ordinary bound value — which is what lets `_.sort(_ < _)` pass a
comparator.

Member access chains, too: a projection keeps the vocabulary, so `_.front().size()` is
`x -> size(front(x))` — `_.front().size()(words)` is the length of the first word.

A few `_`-agnostic combinators live behind `#define TACIT_COMBINATORS` (off by default, to keep the
surface at `_`): `tacit::compose(f, g, …)` composes arbitrary closures left-to-right
(`compose(_ + 1, _ * 2)(3)` is `8`), `tacit::fanout(f, g, …)` maps a value to a tuple of projections,
`tacit::first` / `tacit::second` transform one component of a pair, and the `*_element` family
(`transform_elements`, `any_of_element`, …) drives a closure over a tuple-like. Each returns an `fn`,
so results keep composing.

### Application

There's a third way to apply. Where `_.size()` applies a *named member* and `_ == y` applies an
*operator*, `_(args...)` applies the **subject itself** — the closure that calls its argument. `_(3)`
fans the value `3` across a set of callables, while `_()` simply invokes — handy for forcing a thunk:

```cpp
_(3)(std::negate{}); // -> -3

std::vector<std::function<void()>> thunks{};
std::ranges::for_each(thunks, _()); // invoke each thunk
```

## The term wrapper

`_` is a blank, awaiting its subject. `tacit::lift(x)` is the other side: it gives a plain value the
vocabulary it may not have as members, and applies it **eagerly**. The rule is exactly
`lift(x).f(a…)` == `_.f(a…)(x)`, so there is one vocabulary and one dispatch, not two to keep in step:

```cpp
lift(v).size()        // ranges::size(v) — even where v.size() doesn't exist
lift(-42).abs()       // 42 — a bare value has no members at all
lift("abc").length()  // 3
```

Two things worth knowing. A string literal's raw type is never what you mean — `const char[4]` has no
members, and `ranges::size("abc")` counts the NUL — so a char array is normalized to `string_view` on
the way in. And a lifted call hands back the operation's own result, not a wrapper, so chaining
continues on that result's own type.

The vocabulary has a third dispatch kind for this: beside member calls and the range CPOs, names that
are *free functions* in the standard library (`abs`, `sqrt`, `floor`, `round`, `isnan`, …) route to
`std::`. They work on `_` as well — `count_if(v, _.abs() > 1)`.

### Making a value: `make` and partial CTAD

`lift` adopts a value that exists. `tacit::make<F>(a…)` builds one:

```cpp
make<std::vector>(1, 2, 3)                  // std::vector<int>{1,2,3}   — plain CTAD
make<std::vector, double>(1.0, 2.0)         // std::vector<double>       — arguments given
make<std::set, _, std::greater<>>(3, 1, 2)  // std::set<int, greater<>>  — PARTIAL CTAD
```

The third line is the one C++ cannot otherwise spell: CTAD is all-or-nothing, so fixing *one*
template argument means writing them all — `std::set<int, std::greater<>>{3,1,2}` re-types that `int`
by hand. A `_` in the list means **deduce this position**; everything else is fixed, and trailing
parameters you never mention re-default as usual:

```cpp
make<std::map, _, _, std::greater<>>(pairs…)          // deduce key and mapped type, fix the order
make<std::unordered_map, _, _, _, _, MyAlloc>(p)      // deduce four, fix the allocator
```

It works by deducing the whole specialization and then overlaying the positions you fixed. Deduction
runs first and unmodified, so the deduced positions are exactly what plain CTAD would have given.

`make` returns the **value itself**, not a lift of it: `auto v = make<std::vector>(1,2,3)` is a
`std::vector` you can hand to anything. Wrap it in `lift`/`$` if you want the vocabulary.

Two limits, both the language's. `F` is a `template <class…> class`, so the `<class, size_t>`
families (`array`, `span`) are out of reach — no loss, since their one type argument is all partial
CTAD could have fixed. And blanks reach four positions deep, which covers every standard container.

#### Closures as types: `decltype(_ > _)` for `std::greater<>`

A closure built purely from `_` holds nothing, so it is **default-constructible and empty** — exactly
what a comparator or hasher template parameter wants. `decltype` is the whole crossing:

```cpp
std::set<int, decltype(_ > _)> s{3, 1, 2};        // descending — *s.begin() == 3
make<std::set, _, decltype(_ > _)>(3, 1, 2);      // deduce the element, order by `>`
static_assert(sizeof(std::set<int, decltype(_ > _)>) == sizeof(std::set<int>));  // costs nothing
```

Binding a *value* correctly forfeits this — `decltype(_ > 3)` is not default-constructible, because
it has to keep the 3. A **composed** closure (`_.size() < _.size()`) isn't stateless today either:
sections are built as capturing lambdas, and any capture deletes the default constructor. Ordering by
a projection wants the value form anyway — `std::ranges::sort(v, {}, _.size())` — which needs no type
at all.

### `$` — the canonical short name

`$` is the canonical spelling of the term wrapper — `lift` and `make` under one symbol — and it
lives in its own header, `<tacit/$.hpp>`. Including it is the opt-in; there is no macro:

```cpp
#include <tacit/$.hpp>   // brings <tacit/_.hpp> with it
using tacit::_;
using tacit::$;

$(-42).abs()        // 42
$("abc").length()   // 3
$(v).size()         // ranges::size(v)
```

It is a **function**, not a macro — it keeps its namespace, obeys ADL, and claims nothing from the
rest of the translation unit. `$(p)->f()` reaches through a handle to the pointee, mirroring `_->f()`:

```cpp
$(ptr)->size()      // ptr->size()
$(ptr).use_count()  // the holder's own vocabulary, on the dot surface
```

`$` is a **one-hop lift**, not a fluent facade: a call hands back the operation's natural result, so
`$(v).front()` is a `std::string&`. If you need two hops you are describing a computation, and that
is `_`'s job — `_.front().size()(v)` says it better, and is reusable.

Because `$` is a function *template*, `$<F>(a…)` is `make<F>(a…)` — the other closed cell, under the
same short name. The two can never collide: a call with no explicit template arguments cannot deduce
`F`, so `$(42)` only ever reaches the lift.

```cpp
$<std::vector>(1, 2, 3)                  // std::vector<int>
$<std::set, _, std::greater<>>(3, 1, 2)  // std::set<int, greater<>>
```

It is a separate header because `$` is not an identifier in standard C++ — a GCC/Clang extension,
rejected under `-pedantic-errors` — so `<tacit/_.hpp>` alone stays strictly conforming and never
sees the character. Everything `$` spells is reachable conformingly as `tacit::lift` and
`tacit::make`; nothing is `$`-only, and a `-pedantic-errors` build simply keeps to those names.

## λ — when you do need a lambda (opt-in)

Some things the expression grammar cannot say: an argument used twice, statements, a name in a
non-projection position. For those, `<tacit/λ.hpp>` sheds the ceremony a hand-written lambda drags
in — the macro expands to exactly the head, `λ(a, b)` == `[&](auto&& a, auto&& b)`, and the body
follows in ordinary braces:

```cpp
#include <tacit/λ.hpp>

std::ranges::sort(v, λ(a, b) { return a.size() < b.size(); });
std::ranges::count_if(v, λ(s) { return s.size() * s.size() > 4u; })  // s used twice: `_` can't
λ(s) -> decltype(auto) { return s.front(); }                         // your own trailing return
```

Because the body never passes through the macro, it is plain C++ — commas, statements, multiple
returns, no escaping rules. Capture is `[&]`. No λ key? `\u{3BB}(x)` is the same identifier —
UCNs are equivalent to the character they name, macros included — so λ works from pure ASCII source. The header is **completely standalone** (it includes
nothing, not even `_.hpp`) and, unlike `$`, fully conforming: `λ` is a legal C++23 identifier (UAX
#31), so it survives `-pedantic-errors`; it only asks for UTF-8 source. One caveat has no cure:
macros cannot cross a module boundary, so `#include <tacit/λ.hpp>` is the permanent vehicle — no
`import` will ever carry it. Why λ can only be a macro at all, and why the `{ return … }` cannot be
elided, is recorded in `tacit_extras.md`.

## Synthetic sigils (opt-in)

`#define TACIT_COMBINATORIAL_OPERATORS` before including:

```cpp
f >>* g     // compose, left to right   x -> g(f(x))
f <<* g     // compose, right to left   x -> f(g(x))
f &&& g     // fanout                   x -> {f(x), g(x)}
f *** g     // product                  (a, b) -> {f(a), g(b)}   (or one pair in)

(_.size() &&& _.front())(std::string("abc"))     // {3, 'a'}
std::ranges::sort(v, {}, _.size() >>* (_ * 2));
```

C++'s overloadable-operator set is closed, so none of these is an operator. Each is a **token
sequence** the lexer splits into operators that already exist: `f &&& g` is binary `&&` applied to
`f` and unary `&` applied to `g`. One glyph to a reader, two operators to the compiler. Which
spellings survive maximal munch is not a matter of taste — Haskell's `&&&` and `***` do; `|||`, `>>>`
and `<<<` do not — and the full sweep is in `tacit_extras.md`. Composition is the mirrored pair
`>>*` / `<<*`, both carved from the one `*` marker; `->*` is deliberately *not* a sigil — it is a
real operator with a real (ungated) job, the member-pointer projection above.

**What it costs.** Under the gate, unary `&` and `*` on a closure return a type that *derives* from
`fn`, so `(&_)(c) == &c` and `(*_) + 1` are unchanged and every section still finds it. One reading
is given up **per sigil** — the closure-against-marked-closure form of its binary half: `f && (&g)`,
`f << (*g)`, `f >> (*g)`, `f * (**g)`. None is an expression anybody writes, which is why the trade
is affordable — and why it is nonetheless gated.

**Precedence is inherited.** A sigil takes the binary half's precedence on the left — `>>` sits
below arithmetic, so `_ + 1 >>* (_ * 2)` needs no parentheses on the left — while the unary half
grabs only the primary expression on its right, so right operands usually want them:
`f >>* _ * 2` is `f >> ((*_) * 2)` and `f &&& _ * 2` is `f && ((&_) * 2)`; write `f &&& (_ * 2)`.

## Type level

Deliberately not on the default surface. A working experimental surface exists — `_::blank<>` with
`of`/`as`, `_::rebind`, the noun projections, `bind`/`apply` — and `tests/typelevel.cpp`,
`tests/typeproject.cpp` and `tests/typeapply.cpp` show what it does. But the notation never came out
pleasant, for reasons that turned out to be language rules rather than taste: a name is one *kind* of
entity per scope, so `_` cannot be both the value and a template.

The full account — everything attempted and why each failed, including the parts that *do* work — is
in `tacit_extras.md` under **Four quadrants**. Read that before reopening it.

## Teach `_` your own names

To hand `_` a domain vocabulary, pre-`#define` **`TACIT_VERBS`** (a comma list of member-call names)
before the include, and each name becomes first-class on the same `_`:

```cpp
#define TACIT_VERBS make_deposit, balance, is_frozen
#include <tacit/_.hpp>
using tacit::_;

_.make_deposit(_)(account, 100);        // blanks work here too
std::ranges::sort(accounts, {}, _.balance());   // now first-class on _
std::ranges::count_if(accounts, _.is_frozen()); // also on projections and _->
```

Each verb is `requires`-guarded, so a name a given type lacks is a clean SFINAE miss rather than a
hard error. The same list also lands on `_`'s composable projections (`_.balance() < _.balance()`)
and on the arrow proxy (`_->balance()`), so a verb behaves everywhere the built-in names do.

### A vocabulary file

The comma list has to be `#define`d before the include, identically, in every translation unit — miss
one and that TU gets a different `_`. A **vocabulary file** avoids the ritual: point
`TACIT_VOCABULARY` at a header, or drop a `tacit_vocabulary.hpp` on the include path and
`__has_include` finds it.

```cpp
// bank/vocabulary.hpp — entries only, and NO include guard
TACIT_VERB(deposit)               // x.deposit(a...)
TACIT_FREE(risk, bank::risk)      // bank::risk(x)
TACIT_CPO(tier, bank::tier)       // bank::tier(x)
TACIT_NOUN(money_type)            // _::money_type::of<X>
```

```cpp
#define TACIT_VOCABULARY <bank/vocabulary.hpp>
#include <tacit/_.hpp>
```

The gain over a bare list is that each entry picks its **dispatch kind**, so a domain free function
or customization point is reachable — `TACIT_VERBS` can only make member calls. The file is expanded
once per surface, X-macro style, which is why it must contain nothing but entries and carry no
include guard. It makes an ODR mismatch far less likely, not impossible: a TU that points
`TACIT_VOCABULARY` elsewhere still gets a different `_`, and mixing those in one program is an ODR
violation like any other.

## Reflective hatch (C++26)

When a P2996 toolchain is present (`__cpp_impl_reflection` + `__cpp_lib_reflection`), `_` also
provides, for names not in a table:

- `_.m<"method">(args...)` — call an arbitrary member resolved by name;
- `_.field<"x">()` — project a data member by name;
- `_.enum_name()` — enumerator → `string_view`;
- `_.each_field(f)` — fold `f` over a value's data members.

These are compiled out otherwise. `TACIT_HAS_REFLECTION` (the one macro kept on the clean include
path) lets you `#if` on whether they exist.

## Modules

`import tacit;` is available as an experimental C++20 module (`tacit.cppm`), which wraps the header
and re-exports `_`, `lift`, `make`, and the type-level names. The header split has a module mirror:
`$` is its own module, `tacit.dollar` (`dollar.cppm` — a module name can't contain `$`), so the
opt-in stays per-consumer, exactly as `#include <tacit/$.hpp>` is:

```cpp
import tacit;         // _, lift, make, bind, apply, quote
import tacit.dollar;  // adds $
using tacit::_;
using tacit::$;
```

Macros don't cross a module boundary, so the `TACIT_VERBS` extension hook stays with
`#include <tacit/_.hpp>` — `import` is enough to *use* `_`, `#include` to teach it your own names.
For the same reason `TACIT_COMBINATORS` can't be switched on from the consumer side; build the
interface with `-DTACIT_COMBINATORS` to have it export the combinators too. Verified on clang; GCC's
`-fmodules-ts` isn't reliable for this pattern yet, so prefer `#include` there.

## Build & test

Header-only — just add `include/` to your include path, or use CMake:

```cmake
add_subdirectory(tacit)
target_link_libraries(your_target PRIVATE tacit::tacit)
```

Or install it and `find_package`:

```cmake
find_package(tacit REQUIRED)   # after `cmake --install`
target_link_libraries(your_target PRIVATE tacit::tacit)
```

Or fetch it with [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake):

```cmake
CPMAddPackage("gh:ajg/tacit#master")   # or pin a tagged release
target_link_libraries(your_target PRIVATE tacit::tacit)
```

To run the test suite:

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

To run **CI** — every job in `.github/workflows/ci.yml` — there's `ci.sh`, and a `shell.nix` that
pins the compilers it uses:

```sh
nix-shell --run ./ci.sh                    # clang (CI's clang++-18 leg)
nix-shell --argstr cc gcc --run ./ci.sh    # gcc 13 (CI's g++-13 leg)
CXX=g++-13 ./ci.sh                         # or your own compiler, no nix
```

It prints which compiler it actually used, and skips (rather than silently drops) anything the local
platform can't run. On aarch64-darwin the shell substitutes the nearest working clang and says so;
`shell.nix` documents each substitution and why.

## License

Boost Software License 1.0 — see [LICENSE](LICENSE). Chosen for header-only friendliness: the notice
is required only in source distributions, not in binaries.
