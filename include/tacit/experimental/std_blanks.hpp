// SPDX-License-Identifier: BSL-1.0
#pragma once

// EXPERIMENTAL: natural-spelling type-level blanks, `std::map<struct _, int>::with<char>`. Injects
// partial specializations of common std containers into `namespace std`, keyed on the blank type
// `tacit::_`, each exposing a `::with<...>` that reconstructs the container with the blank filled.
//
// This lives in its own header, off the default surface, because it is [namespace.std] deviancy: a
// specialization of a std class template for `tacit::_` does not meet the original template's
// requirements (a blank is not a key/value/allocator), so it is technically ill-formed, no
// diagnostic required — it works on the tested toolchains, but it is a spelling convenience, not a
// standards guarantee. Including this file is the opt-in. The portable, always-on surface is
// `tacit::apply` / `tacit::bind` in <tacit/_.hpp>. Notes on the shape:
//   - A trailing pack is not "more specialized" than a fixed-arity primary, so the defaulted
//     Compare/Allocator params can't hide behind `...`; each SHAPE macro names them explicitly.
//   - `::with` reconstructs FRESH defaults (map<K,T>, not map<K,T,C,A>): carrying the blank-derived
//     less<blank>/allocator<...blank...> along would silently poison the result. Cost: you can't thread
//     an explicit non-default Compare/Allocator through a blank (drop to `apply`/`bind` for that).
//   - `tuple` is variadic: a single LEADING blank + trailing pack is legal at any arity and portable;
//     interior blanks and multiple leading-blank specs are not (see tacit_extras.md).
//   - `array` / `span` are value-parameterized (`<class, size_t>`): only the ELEMENT type is blanked and
//     the extent rides along as a literal (`array<struct _, 5>::with<int>`). No wrapper is needed
//     because the value is a normal template argument — the reason this stays natural where the
//     general primitive would demand a value wrapper. Holing the extent itself has no natural spelling
//     and is intentionally not offered.
#include "../_.hpp"

#include <array>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <utility>
#include <vector>
namespace std {
#define TACIT_HOLE ::tacit::blank<> // the type-level blank; `_::blank<>` in user code
#define TACIT_SPEC_1_1(F)                                                                                              \
  template <class A0> class F<TACIT_HOLE, A0> {                                                                        \
  public:                                                                                                              \
    template <class X> using with = F<X>;                                                                              \
  };
#define TACIT_SPEC_1_2(F)                                                                                              \
  template <class C, class A0> class F<TACIT_HOLE, C, A0> {                                                            \
  public:                                                                                                              \
    template <class X> using with = F<X>;                                                                              \
  };
#define TACIT_SPEC_2_2(F)                                                                                              \
  template <class T, class C, class A0> class F<TACIT_HOLE, T, C, A0> {                                                \
  public:                                                                                                              \
    template <class K> using with = F<K, T>;                                                                           \
  };                                                                                                                   \
  template <class K, class C, class A0> class F<K, TACIT_HOLE, C, A0> {                                                \
  public:                                                                                                              \
    template <class V> using with = F<K, V>;                                                                           \
  };                                                                                                                   \
  template <class C, class A0> class F<TACIT_HOLE, TACIT_HOLE, C, A0> {                                                \
  public:                                                                                                              \
    template <class K, class V> using with = F<K, V>;                                                                  \
  };
TACIT_SPEC_1_1(vector)
TACIT_SPEC_1_2(set)
TACIT_SPEC_2_2(map)
#undef TACIT_SPEC_1_1
#undef TACIT_SPEC_1_2
#undef TACIT_SPEC_2_2
// pair: two type params, no defaults — by hand.
template <class B> struct pair<TACIT_HOLE, B> {
  template <class X> using with = pair<X, B>;
};
template <class A0> struct pair<A0, TACIT_HOLE> {
  template <class Y> using with = pair<A0, Y>;
};
template <> struct pair<TACIT_HOLE, TACIT_HOLE> {
  template <class X, class Y> using with = pair<X, Y>;
};
// tuple: variadic, single leading blank + trailing pack — legal at any arity, portable in the
// language — but libc++ 21 marks std::tuple itself [[clang::no_specializations]], turning the NDR
// ill-formedness into a hard error there. The cell is skipped on that library; feature-test with
// TACIT_HAS_STD_TUPLE_BLANKS. (pair and the containers carry no such marking as of libc++ 21.)
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 210000
#define TACIT_HAS_STD_TUPLE_BLANKS 0
#else
#define TACIT_HAS_STD_TUPLE_BLANKS 1
template <class... R> class tuple<TACIT_HOLE, R...> {
public:
  template <class X> using with = tuple<X, R...>;
};
#endif
// value-parameterized containers: blank the ELEMENT type; the extent (a non-type parameter) rides along as an ordinary
// literal — std::array<struct _, 5>::with<int> == std::array<int, 5>. This is the user-free grain: the value is a
// normal template argument, no wrapper, no sentinel. (The mirror — holing the *value* and fixing the type — has no
// natural spelling: a type blank can't sit in a size_t slot, so it's intentionally absent; reach for a hand-written
// metafunction to vary an extent.)
template <std::size_t N> struct array<TACIT_HOLE, N> {
  template <class T> using with = array<T, N>;
};
template <std::size_t E> class span<TACIT_HOLE, E> {
public:
  template <class T> using with = span<T, E>;
};
#undef TACIT_HOLE
} // namespace std
