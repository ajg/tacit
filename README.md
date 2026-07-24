# tacit

[![ci](https://github.com/ajg/tacit/actions/workflows/ci.yml/badge.svg)](https://github.com/ajg/tacit/actions/workflows/ci.yml)

A tiny, header-only library for **point-free (tacit) programming** in C++23. Its whole public surface
is one object, `tacit::_`, whose members return closures that forward to a same-named operation on
whatever they're later applied to — so you can hand operations to algorithms without writing lambdas.

```cpp
#include <tacit/_.hpp>
#include <algorithm>  // the ranges algorithms live here, not in <tacit/_.hpp>
#include <ranges>     // std::views
using tacit::_;

std::ranges::sort(nums, _ < _);                     // ascending (two-blank comparator)
std::ranges::count_if(nums, _ == 0);                // count zeros
std::ranges::transform(words, out, _.size());       // string lengths
nums | std::views::filter(_ != 0) | std::views::take(2);  // predicate drops into std::views
```

`_` is a single exported name: a plain `using tacit::_;` is all you ever need — the vocabulary is
reached through the object, and the operator forms are hidden friends found by ADL. (Prefer
`#include` alone? `#define TACIT_USING_UNDERSCORE` before including and the header does the `using`
for you — opt-in, so it never imposes a global `_` on anyone who didn't ask.)

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

### Vocabulary

`_` carries a curated first-class vocabulary of standard-library member names (`at`, `push_back`,
`substr`, `value_or`, `find`, `emplace`, …), kept in one editable table. Range access
(`size`, `begin`, `end`, `empty`, `data`, …) routes through the `std::ranges` customization points, so
`_.size()` / `_.begin()` also work on C arrays, string views, and third-party ranges.

### Operator sections

The comparison and arithmetic operators are finite and lexical: `_ == y`, `x + _`, `_ + _` all build
the obvious closure.

## Derive your own placeholder

The vocabulary-independent machinery (operator sections, application, reflection) lives in
`TACIT_CORE(Self)`. A domain placeholder is a struct that lists the method names it wants and drops in
the core. Opt into keeping the generator macros with `TACIT_KEEP_MACROS` before including:

```cpp
#define TACIT_KEEP_MACROS
#include <tacit/_.hpp>
#include <algorithm>
#include <vector>
using tacit::_;

namespace bank {
struct teller {
  TACIT_MEMBER(deposit) TACIT_MEMBER(balance) TACIT_MEMBER(freeze)  // your methods
  TACIT_CORE(teller)
};
inline constexpr teller $;
}

std::ranges::sort(accounts, {}, bank::$.balance());
bank::$.deposit(_)(account, 100);   // blanks work in derived placeholders too
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

## Build & test

Header-only — just add `include/` to your include path, or use CMake:

```cmake
add_subdirectory(tacit)
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
French *lieu tenant*, literally "place-holding," a stand-in — which is exactly what `_` is, while
sidestepping the loaded English word "placeholder" (already spoken for by `std::placeholders` and by
the grammar term *placeholder type specifier* for `auto`).

## License

Boost Software License 1.0 — see [LICENSE](LICENSE). Chosen for header-only friendliness: the notice
is required only in source distributions, not in binaries.
