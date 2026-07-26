# tacit

[![ci](https://github.com/ajg/tacit/actions/workflows/ci.yml/badge.svg)](https://github.com/ajg/tacit/actions/workflows/ci.yml)

> **Status: experimental, pre-1.0.** The API may change, and it isn't released or announced yet.

A tiny, header-only library for **point-free (tacit) programming** in C++23. Its whole public surface
is one object, `tacit::_`, whose members return closures that forward to a same-named operation on
whatever they're later applied to — so you can hand operations to algorithms without writing lambdas.

```cpp
#include <tacit/_.hpp>
using tacit::_;

std::sort(nums, _ < _);
std::count_if(nums, _ == 0);
std::transform(words, out, _.size());
```

`_` is the one name that enters your scope: `using tacit::_;` imports exactly `_` — the vocabulary is
reached *through* the object, and the operator sections are hidden friends found by ADL,
so they need no `using`. Everything else is a qualified `tacit::` helper that never enters your scope.
(The free-function combinators live behind `#define TACIT_COMBINATORS`, off by default — see
[Composition](#composition).)

Prefer `#include` alone? `#define TACIT_USING_UNDERSCORE` before including and the header does the
`using` for you — opt-in, so it never imposes a global `_` on anyone who didn't ask.

## Requirements

- **C++23** (tested on g++ 13 and clang 18, `-std=c++23`). No dependencies beyond the standard library.
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

A blank always means "another fill", including where that reading is less obvious — `_[_]` is
`(x, i) -> x[i]`, `_ += _` is `(a, b) -> a += b`, `_(_)` is `(f, x) -> f(x)`, and `_.size() < _` is
`(v, n) -> size(v) < n`. Where a blank *cannot* be filled, the expression is rejected where you write
it rather than building a closure nothing can call: a chained call binds its arguments, so
`_.front().substr(_)` is a compile error, not a dead closure. (`_.substr(_)` is fine — that section
does fill its own blanks.)

### Vocabulary

`_` carries a curated first-class vocabulary of standard-library member names (`at`, `push_back`,
`substr`, `value_or`, `find`, `emplace`, …), kept in one editable table. Range access
(`size`, `begin`, `end`, `empty`, `data`, …) routes through the `std::ranges` customization points, so
`_.size()` / `_.begin()` also work on C arrays, string views, and third-party ranges.

The table reaches past containers, since the names that pay off in point-free code are the ones you
project or test with: diagnostics (`what`, `code`, `message`), `filesystem::path` (`extension`,
`stem`, `filename`, `parent_path`, `is_absolute`, …), the monadic family (`and_then`, `transform`,
`or_else`, `value_or`, `error_or`, `transform_error`), concurrency (`join`, `joinable`, `load`,
`store`, `fetch_add`, `wait`, `valid`), streams (`good`, `eof`, `is_open`, `flush`), `bitset`
(`test`, `all`, `any`, `none`, `flip`), `regex` match results (`position`, `prefix`, `suffix`,
`ready`), `complex` (`real`, `imag`), `chrono` (`count`, `time_since_epoch`), and `span`
(`subspan`, `size_bytes`). Names cost nothing until used — each is a member template, so a wider
table is a longer declaration list, not a bigger binary.

The type-level table is the twin, and reaches the same way: `_::rep::of<duration>`,
`_::hasher::of<unordered_map<…>>`, `_::deleter_type::of<unique_ptr<…>>`,
`_::container_type::of<stack<…>>`, `_::iterator_category::of<It>`.

### Operator sections

The comparison, arithmetic, bitwise, shift, and logical operators are finite and lexical: `_ == y`,
`x + _`, `_ + _` all build the obvious closure (a one-sided form is unary; `_ op _` is a two-input
combiner, like the `_.size() < _.size()` comparator). Unary forms work too — `-_`, `!_`, `~_`,
`*_` (deref), `++_` — as does streaming (`os << _`, so `for_each(v, std::cout << _)`) and member access
through a pointer, `_->size()`, which uses the pointee's real `operator->` (distinct from `(*_).size()`).

**Comparisons chain.** C++ parses `0 < _ < 10` as `(0 < _) < 10` — a *bool* compared against 10, so
the closure is silently always true. A comparison section therefore remembers its rightmost operand,
and a comparison applied to one rewrites itself into the conjunction the notation means:

```cpp
0 < _ < 10        // x -> (0 < x) && (x < 10)      not  ((0 < x) < 10)
1 <= _.size() < 4 // x -> (1 <= size(x)) && (size(x) < 4)
0 <= _ <= 10 < 20 // chains to any length, any mix of == != < > <= >=
```

The middle term is evaluated once per link (so keep a projection cheap and pure) and `&&`
short-circuits, exactly as in the spelled-out form. Only those six operators build a chain; any other
operator ends it — `(0 < _) + 0 < 2` is still arithmetic on the bool, and `!(_ < 10)` is a plain
negated predicate. The flip side of the rule is that a comparison *of* a comparison chains too:
`(_ < 10) == false` reads as `(x < 10) && (10 == false)`, not `x >= 10` — spell that one `_ >= 10`.
`_ < _` is unaffected: with two blanks it's the two-input comparator, not a link.

Assignment is included and **mutates**: `_ = 0` and compound forms like `_ += 1` build sections that
bind the argument by reference, so `for_each(v, _ += 1)` updates `v` in place. Bitwise `|` is an
ordinary section too (`_ | 4`, `_ | _`), symmetric with `&`; general function composition lives in
`tacit::compose`, not in `|`.

**Comma builds tuples.** `,` is the one section that makes data rather than calling something, and
the only n-ary one — each further `,` appends an operand rather than pairing with what came before:

```cpp
(_, _)                 // (a, b)    -> std::pair{a, b}
(_, 9)                 // x         -> {x, 9}      one-sided binds
(_, _, _)              // (a, b, c) -> std::tuple{a, b, c}
(_.size(), _.front())  // (a, b)    -> {size(a), front(b)}
(_, 5, _)              // (a, b)    -> {a, 5, b}   bound: no fill
```

Two operands stay a `std::pair` — `.first` / `.second` are strictly extra, since a pair is a
two-tuple for `get`, `tuple_size`, structured bindings, and `std::apply` alike. Three or more have no
pair to be, so they're a `std::tuple`. The operand list is **flat**: `(_, (_, _))` and
`((_, _), _)` are the same three-slot tuple, and there's no nested-pair spelling — reach for a
lambda if you want one. The parens around a comma section are load-bearing everywhere `,` would
otherwise read as a separator (argument lists, init-lists, declarations) — that's the built-in comma
doing its usual job, untouched.

The usual blank rule applies, and it's the thing to watch: each `_` is a **distinct** blank, so
`(_.size(), _.front())` takes *two* arguments — it is not a one-argument key function. For the
same-input tuple (the lexicographic projection you probably want) that's `tacit::fanout`:

```cpp
tacit::fanout(_.size(), _.front())  // x      -> {size(x), front(x)}
(_.size(), _.front())               // (a, b) -> {size(a), front(b)}
```

A comma section composes onward through the value it builds, keeping its arity — the vocabulary and
the comparisons both apply to the `pair`/`tuple` that comes out:

```cpp
(_, _) == std::pair{1, 2}   // (a, b) -> {a, b} == {1, 2}
(_, _) < std::pair{2, 0}    // (a, b) -> lexicographic, as pair defines it
(_, _, _) < std::tuple{…}   // likewise at three
```

Comparisons only: the six are the operators `pair` and `tuple` actually have, so arithmetic and
bitwise stay off a data builder. The result is an ordinary `fn`, so it keeps composing
(`!((_, _) == p)`). Bound arguments only in a chained call — a blank inside one (`(_, _).foo(_)`) is
not a further slot, exactly as it isn't for a projection, where `_.front().substr(_)` has never taken
one: the fills belong to the operand list. Worth knowing that the member half is dormant on the
default surface, since `std::pair` and `std::tuple` expose no name from the vocabulary table; it
comes alive for a `TACIT_VERBS` name that they do have.

### Composition

The closure `_` hands back is itself composable, so a projection and a section chain without ever
naming a lambda — sections, subscript (`_[i]`), and arithmetic all build a new closure:

```cpp
std::count_if(v, _.size() >= 2);        // size(x) >= 2
std::sort(v, _.size() < _.size());      // order by size

auto scaled = (_ + 1) * 2;         // x -> (x + 1) * 2
auto head   = _[0];                // x -> x[0]
```

Composition is **arity-preserving**, so a two-input form composes exactly like a one-input one:
`(_ + _) + 1` is `(a, b) -> (a + b) + 1`, and `(_ < _).size()` is `(a, b) -> size(a < b)`. The one
place arity is load-bearing is argument position, where a *one-fill* closure is a projected blank
(`_.push_back(_.size())`) while a *many-fill* one is an ordinary bound value — which is what lets
`_.sort(_ < _)` pass a comparator. The genuinely partial forms (`_.foo(_)`, which is still filling
its own blanks) stay partial applications.

Member access chains, too: a projection keeps the vocabulary, so `_.front().size()` is
`x -> size(front(x))` — handy as a projection: `sort(words, {}, _.front().size())`.

For composing *arbitrary* closures left-to-right there's `tacit::compose(f, g, …)` (`x -> …(g(f(x)))`),
behind `#define TACIT_COMBINATORS` — `compose(_ + 1, _ * 2)(3)` is `8`. (There's no `f | g` compose:
`|` is a bitwise section like `&`. Member chaining still composes vocabulary on the default surface.)

A few more `_`-agnostic combinators live behind the same `#define TACIT_COMBINATORS` (off by default,
to keep the surface at `_`): `tacit::fanout(f, g, …)` maps a value to a tuple of projections
(`x -> {f(x), g(x)}`), `tacit::first` / `tacit::second` transform one component of a pair, and the
`*_element` family (`transform_elements`, `any_of_element`, …) drives a closure over a tuple-like.
Each returns an `fn`, so results keep composing. They're free `tacit::` functions, so they take
qualification and the `#define`; the operator sections don't.

### Application

There's a third way to apply. Where `_.size()` applies a *named member* and `_ == y` applies an
*operator*, `_(args...)` applies the **subject itself** — it builds `[args...](f){ return f(args...); }`,
the closure that calls its argument. It mirrors mapping a function over data: `_(3)` fans the value `3`
across a set of callables, while `_()` (no args) simply invokes — handy for forcing a thunk.

```cpp
_(3)(std::negate{}); // -> -3

// or:

std::vector<std::function<void()>> thunks{};
std::for_each(thunks, _()); // invoke each thunk
```

## Values and types

`_` is a hole in the **term** world, awaiting its subject. Two things sit beside it: a hole in the
**type** world, and a way to hand either world a subject it already has.

`tacit::hole<A...>` is the type-level twin, with two duals — `of` fixes the arguments and awaits the
template, `as` fixes the template and awaits the arguments:

```cpp
hole<int>::of<std::vector>            // std::vector<int>      head is the hole
hole<>::as<std::map>::with<int, char> // std::map<int, char>   head is given
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

`tacit::lift(x)` is the term-level counterpart: it gives a plain value the vocabulary it may not have
as members, and applies it **eagerly**. The rule is exactly `lift(x).f(a…)` == `_.f(a…)(x)`, so there
is one vocabulary and one dispatch, not two to keep in step:

```cpp
lift(v).size()        // ranges::size(v) — even where v.size() doesn't exist
lift(-42).abs()       // 42 — a bare value has no members at all
lift("abc").length()  // 3
```

Two things worth knowing. A string literal's raw type is never what you mean — `const char[4]` has no
members, and `ranges::size("abc")` is **4** because it counts the NUL — so a char array is normalized
to `string_view` on the way in. And a lifted call hands back the operation's own result, not a wrapper,
so chaining continues on that result's own type.

The vocabulary grew a third dispatch kind for this: beside member calls and the range CPOs, names that
are *free functions* in the standard library (`abs`, `sqrt`, `floor`, `ceil`, `round`, `log`, `isnan`, …)
route to `std::`. They work on `_` as well — `count_if(v, _.abs() > 1)`.

### The `$` wrapper (opt-in)

`$` is `tacit::lift` under a shorter name, behind `#define TACIT_DOLLAR`:

```cpp
#define TACIT_DOLLAR
#include <tacit/_.hpp>
using tacit::_;
using tacit::$;

$(-42).abs()        // 42
$("abc").length()   // 3
$(v).size()         // ranges::size(v)
```

It is a **function**, not a macro — so it keeps its namespace, obeys ADL, can be written
`tacit::$(x)`, and claims nothing from the rest of the translation unit.

It is gated because `$` is not an identifier in standard C++ — a GCC/Clang extension, rejected under
`-pedantic-errors`. Everything it spells is reachable conformingly as `tacit::lift`; nothing is
`$`-only, and a default build never sees it.

## Teach `_` your own names

`_` is the only placeholder — there is no separate derived object to learn or spell. To hand it a
domain vocabulary, pre-`#define` **`TACIT_VERBS`** (a comma list of member-call names) before the
include, and each name becomes first-class on the same `_`:

```cpp
#define TACIT_VERBS make_deposit, balance, is_frozen
#include <tacit/_.hpp>
using tacit::_;

_.make_deposit(_)(account, 100);        // blanks work here too
std::sort(accounts, {}, _.balance());   // now first-class on _
std::count_if(accounts, _.is_frozen()); // also on projections and _->
```

Each verb is `requires`-guarded, so a name a given type lacks is a clean SFINAE miss rather than a hard
error — a domain verb sits safely alongside the standard vocabulary. The same list also lands on `_`'s
composable projections (`_.balance() < _.balance()`) and on the arrow proxy (`_->balance()`), so a
verb behaves everywhere the built-in names do.

Blank detection is trait-based, so `_` is recognised as a blank in any argument position.

## Reflective hatch (C++26)

When a P2996 toolchain is present (`__cpp_impl_reflection` + `__cpp_lib_reflection`), `TACIT_CORE`
also provides, for names not in a table:

- `_.m<"method">(args...)` — call an arbitrary member resolved by name;
- `_.field<"x">()` — project a data member by name;
- `_.enum_name()` — enumerator → `string_view`;
- `_.each_field(f)` — fold `f` over a value's data members.

These are compiled out otherwise. `TACIT_HAS_REFLECTION` (the one macro kept on the clean include
path) lets you `#if` on whether they exist.

## Modules

`import tacit;` is available as an experimental C++20 module (`tacit.cppm`), which wraps the header
and re-exports `_`:

```cpp
import tacit;
using tacit::_;
```

Macros don't cross a module boundary, so the `TACIT_VERBS` extension hook is consumed at include time
and stays with `#include <tacit/_.hpp>` — `import` is enough to *use* `_`, `#include` to teach it your
own names (just as `import std;` exports no macros). For the same reason `TACIT_COMBINATORS` can't be
switched on from the consumer side; build the interface with `-DTACIT_COMBINATORS` to have it export
the combinators too. Verified on clang; GCC's `-fmodules-ts` isn't reliable for this pattern yet, so
prefer `#include` there.

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

Or fetch it with [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) (no extra setup needed — the
`add_subdirectory` path defines `tacit::tacit` and skips tacit's own tests when consumed):

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

To run **CI** — every job in `.github/workflows/ci.yml`: the suite, the `import tacit;` module build,
and both packaging paths — there's `ci.sh`, and a `shell.nix` that pins the compilers it uses:

```sh
nix-shell --run ./ci.sh                    # clang (CI's clang++-18 leg)
nix-shell --argstr cc gcc --run ./ci.sh    # gcc 13 (CI's g++-13 leg)
CXX=g++-13 ./ci.sh                         # or your own compiler, no nix
```

It prints which compiler it actually used, and skips (rather than silently drops) anything the local
platform can't run. On Linux the pins are exactly CI's. On aarch64-darwin two are forced — gcc isn't
in the binary cache there, and clang 18's stdenv doesn't build — so the shell lands on the nearest
working clang and says so; `shell.nix` documents each substitution and why.

## On the name

`tacit` names the paradigm (point-free / tacit programming). `_`'s own type is just `tacit::_`; `_`
itself is the *lieutenant* — French *lieu tenant*, literally "place-holding," a stand-in —
sidestepping the loaded English word "placeholder" (already spoken for by `std::placeholders` and by
the grammar term *placeholder type specifier* for `auto`); the irony is not lost entirely that
point-free style almost necessarily involves more points in the literal sense (periods/dots),
whether through composition syntax like `f . g` in Haskell or member access function objects for
partial application like `_.m(...)` here.

## License

Boost Software License 1.0 — see [LICENSE](LICENSE). Chosen for header-only friendliness: the notice
is required only in source distributions, not in binaries.
