// SPDX-License-Identifier: BSL-1.0
// The macro-hygiene policy, in one place: no generator macro may survive an include of a public
// tacit header. Deliberately GUARD-FREE and declaration-free — it is a pure preprocessor assertion,
// so it may be included at as many points in a TU as there are claims to make.
//
// Two enforcement points, because two headers now define generators: `strict_using.cpp` checks the
// conforming core alone (<tacit/_.hpp>), and `order_body.hpp` checks after all three public headers
// in either order — which is what covers `$.hpp`'s own copy of the `make` overload table. Keeping
// the NAME LIST here rather than in each of them is the point: a new generator macro is one edit,
// and both claims tighten at once.
//
// `λ` is deliberately absent: <tacit/λ.hpp> exists to define it, so it is a product, not a leak.
#if defined(TACIT_MEMBER) || defined(TACIT_CORE) || defined(TACIT_STD_MEMBERS) || defined(TACIT_FOR_EACH) ||            \
    defined(TACIT_LIEUTENANT) || defined(TACIT_VERBS) || defined(TACIT_NOUNS) || defined(TACIT_NOUN_TEMPLATES) ||      \
    defined(TACIT_TYPE_TEMPLATE) || defined(TACIT_VIEW) || defined(TACIT_VIEW_VERBS) ||                                \
    defined(TACIT_SECTION) || defined(TACIT_COMPARE) || defined(TACIT_ASSIGN) || defined(TACIT_UNARY) ||               \
    defined(TACIT_MEMPTR) || defined(TACIT_FN_TFREE1) || defined(TACIT_CPO1) || defined(TACIT_MKU) ||                  \
    defined(TACIT_PC) || defined(TACIT_PS) || defined(TACIT_FC) || defined(TACIT_FS) || defined(TACIT_MK) ||           \
    defined(TACIT_MK1) || defined(TACIT_MK2) || defined(TACIT_MK3) || defined(TACIT_MK4) || defined(TACIT_MKN)
#error "a generator macro leaked out of a public tacit header"
#endif
