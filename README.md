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
reached *through* the object, and the operator forms (sections, `|`) are hidden friends found by ADL,
so they need no `using`. The only other public name is the opt-in type-level `tacit::bind`, and it
reuses the same identifier as `struct _`, so it adds no name of its own. (The free-function combinators
live behind `#define TACIT_COMBINATORS` and are off by default — see [Composition](#composition).)

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

The comparison and arithmetic operators are finite and lexical: `_ == y`, `x + _`, `_ + _` all build
the obvious closure.

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

`f | g` composes closures left-to-right (`x -> g(f(x))`); it's a hidden friend of `fn`, always
available, and never clashes with the ranges pipe (whose left operand is a range, not an `fn`).

A few more `_`-agnostic combinators are available behind `#define TACIT_COMBINATORS` (off by default,
to keep the surface at `_` + `bind`): `tacit::fanout(f, g, …)` maps a value to a tuple of projections
(`x -> {f(x), g(x)}`), `tacit::first` / `tacit::second` transform one component of a pair, and the
`*_element` family (`transform_elements`, `any_of_element`, …) drives a closure over a tuple-like.
Each returns an `fn`, so results keep composing. They're free `tacit::` functions rather than hidden
friends, so they take qualification and a `#define` — the operators (`|`, sections) don't.

### Application

There's a third way to apply. Where `_.size()` applies a *named member* and `_ == y` applies an
*operator*, `_(args...)` applies the **subject itself** — it builds `[args...](f){ return f(args...); }`,
the closure that calls its argument. It mirrors mapping a function over data: `_(3)` fans the value `3`
across a set of callables, while `_()` (no args) simply invokes — handy for forcing a thunk.

```cpp
_(3)(std::negate{});                 // -> -3  (applies negate to 3)
std::ranges::for_each(thunks, _());  // invoke each nullary callable
```

## Derive your own placeholder

The vocabulary-independent machinery (operator sections, application, reflection) lives in
`TACIT_CORE(Self)`. The shortest way to make a placeholder is `TACIT_LIEUTENANT`, which declares the
type and its object in one statement. Opt into keeping the generator macros with `TACIT_KEEP_MACROS`
before including:

```cpp
#define TACIT_KEEP_MACROS
#include <tacit/_.hpp>
#include <algorithm>
using tacit::_;
using namespace std::ranges;

namespace bank {
TACIT_LIEUTENANT(teller, it, deposit, balance, freeze);
}

sort(accounts, {}, bank::it.balance());
bank::it.deposit(_)(account, 100);   // blanks work in derived placeholders too
```

Need more control — hand-written members, or the whole std vocabulary? Write the struct yourself:
one member per line (or `TACIT_MEMBERS(a, b, c);` for a compact list), and/or
`TACIT_STD_MEMBERS(TACIT_MEMBER)` to pull the whole vocabulary, then drop in `TACIT_CORE(Self)`:

```cpp
namespace bank {
struct teller {
  TACIT_MEMBER(deposit);   // one line per member, one semicolon per line
  TACIT_MEMBER(balance);
  TACIT_MEMBER(freeze);
  TACIT_CORE(teller);
};
inline constexpr teller it;
}
```

To add names to the built-in `_` instead of deriving a new placeholder, pre-`#define`
`TACIT_EXTRA_MEMBERS` before the include (it only ever *adds* to the std vocabulary — and needs no
`TACIT_KEEP_MACROS`):

```cpp
#define TACIT_EXTRA_MEMBERS(X) X(area); X(perimeter);
#include <tacit/_.hpp>
using tacit::_;

std::ranges::sort(shapes, {}, _.area());   // _.area() is now first-class on _
```

Blank detection is trait-based, so `_` is recognised as a blank in any placeholder's arguments.

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
and re-exports `_` and the type-level `bind`:

```cpp
import tacit;
using tacit::_;
```

Macros don't cross a module boundary, so the derive generators (`TACIT_LIEUTENANT`, `TACIT_CORE`, …)
stay with `#include <tacit/_.hpp>` — `import` is enough to *use* `_`, `#include` to derive your own
(just as `import std;` exports no macros). For the same reason `TACIT_COMBINATORS` can't be switched
on from the consumer side; build the interface with `-DTACIT_COMBINATORS` to have it export the
combinators too. Verified on clang; GCC's `-fmodules-ts` isn't reliable for this pattern yet, so
prefer `#include` there.

## Type-level `_`

`_` doubles as a *type-level* blank for partially applying a class template. Because a template
argument list can't hold the value `_`, the blank is written `struct _` (its tag-namespace twin), so
fixed arguments stay plain types:

```cpp
tacit::bind<std::vector, struct _>::with<int>       // std::vector<int>
tacit::bind<std::map, int, struct _>::with<double>  // std::map<int, double>
```

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

To run the test suite:

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## On the name

`tacit` names the paradigm (point-free / tacit programming). The type of `_` is `tacit::lieutenant` —
French *lieu tenant*, literally "place-holder," a stand-in — which is exactly what `_` is, while
sidestepping the loaded English word "placeholder" (already spoken for by `std::placeholders` and by
the grammar term *placeholder type specifier* for `auto`).

## License

Boost Software License 1.0 — see [LICENSE](LICENSE). Chosen for header-only friendliness: the notice
is required only in source distributions, not in binaries.
