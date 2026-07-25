# tacit

[![ci](https://github.com/ajg/tacit/actions/workflows/ci.yml/badge.svg)](https://github.com/ajg/tacit/actions/workflows/ci.yml)

> **Status: experimental, pre-1.0.** The API may change, and it isn't released or announced yet.

A tiny, header-only library for **point-free (tacit) programming** in C++23. Its whole public surface
is one object, `tacit::_`, whose members return closures that forward to a same-named operation on
whatever they're later applied to — so you can hand operations to algorithms without writing lambdas.

```cpp
#include <tacit/_.hpp>
#include <algorithm>  // the ranges algorithms live here, not in <tacit/_.hpp>
#include <ranges>     // std::views
using tacit::_;
using namespace std::ranges;

sort(nums, _ < _);                              // ascending (two-blank comparator)
count_if(nums, _ == 0);                         // count zeros
transform(words, out, _.size());                // string lengths
nums | views::filter(_ != 0) | views::take(2);  // predicate drops into std::views
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

### Vocabulary

`_` carries a curated first-class vocabulary of standard-library member names (`at`, `push_back`,
`substr`, `value_or`, `find`, `emplace`, …), kept in one editable table. Range access
(`size`, `begin`, `end`, `empty`, `data`, …) routes through the `std::ranges` customization points, so
`_.size()` / `_.begin()` also work on C arrays, string views, and third-party ranges.

### Operator sections

The comparison, arithmetic, bitwise, shift, and logical operators are finite and lexical: `_ == y`,
`x + _`, `_ + _` all build the obvious closure (a one-sided form is unary; `_ op _` is a two-input
combiner, like the `_.size() < _.size()` comparator). Unary forms work too — `-_`, `!_`, `~_`,
`*_` (deref), `++_` — as does streaming (`os << _`, so `for_each(v, std::cout << _)`) and member access
through a pointer, `_->size()`, which uses the pointee's real `operator->` (distinct from `(*_).size()`).

Assignment is included and **mutates**: `_ = 0` and compound forms like `_ += 1` build sections that
bind the argument by reference, so `std::ranges::for_each(v, _ += 1)` updates `v` in place. Bitwise `|`
is an ordinary section too (`_ | 4`, `_ | _`), symmetric with `&`; general function composition lives
in `tacit::compose`, not in `|`.

### Composition

The closure `_` hands back is itself composable, so a projection and a section chain without ever
naming a lambda — sections, subscript (`_[i]`), and arithmetic all build a new closure:

```cpp
std::ranges::count_if(v, _.size() >= 2);     // size(x) >= 2
std::ranges::sort(v, _.size() < _.size());   // order by size
auto scaled = (_ + 1) * 2;                   // x -> (x + 1) * 2
auto head   = _[0];                          // x -> x[0]
```

Every single-argument closure `_` produces is a small composable `fn`; the multi-blank forms
(`_.foo(_)`, `_ < _`) stay partial applications, where composition would not mean anything.

Member access chains, too: a projection keeps the vocabulary, so `_.front().size()` is
`x -> size(front(x))` — handy as a projection: `std::ranges::sort(words, {}, _.front().size())`.

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
_(3)(std::negate{});                 // -> -3  (applies negate to 3)

std::vector<std::function<void()>> thunks{ []{}, []{} };
std::ranges::for_each(thunks, _());  // invoke each nullary callable
```

## Teach `_` your own names

`_` is the only placeholder — there is no separate derived object to learn or spell. To hand it a
domain vocabulary, pre-`#define` **`TACIT_VERBS`** (a comma list of member-call names) before the
include, and each name becomes first-class on the same `_`:

```cpp
#define TACIT_VERBS deposit, balance, freeze
#include <tacit/_.hpp>
#include <algorithm>
using tacit::_;
using namespace std::ranges;

sort(accounts, {}, _.balance());     // _.balance() is now first-class on _
_.deposit(_)(account, 100);          // blanks work, exactly as with the built-in vocabulary
count_if(accounts, _.frozen());      // ... and a verb reaches _'s projections and _-> too
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
