// Type-level projection: `_::name::of<X>` pulls a nested member out of a type — the type-level twin
// of the value-level member vocabulary, and the dual of `bind` (which wraps rather than projects).
// Custom projections are added to `_` via the extension hooks: TACIT_NOUNS for a nested type, and
// TACIT_NOUN_TEMPLATES for a nested *template* (the `_::name<A...>::of<X>` rebind form).
#define TACIT_NOUNS tag
#define TACIT_NOUN_TEMPLATES rebound
#include <tacit/_.hpp>

#include <map>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

using tacit::_; // the value `_`; `_::name` reaches the twin type past it via qualified lookup

// nested-type projections over the standard vocabulary
static_assert(std::is_same_v<_::value_type::of<std::vector<int>>, int>);
static_assert(std::is_same_v<_::size_type::of<std::vector<int>>, std::size_t>);
static_assert(std::is_same_v<_::difference_type::of<std::vector<int>>, std::ptrdiff_t>);
static_assert(std::is_same_v<_::key_type::of<std::map<int, char>>, int>);
static_assert(std::is_same_v<_::mapped_type::of<std::map<int, char>>, char>);
static_assert(std::is_same_v<_::value_type::of<std::map<int, char>>, std::pair<const int, char>>);
static_assert(std::is_same_v<_::element_type::of<std::shared_ptr<double>>, double>);
static_assert(std::is_same_v<_::first_type::of<std::pair<int, char>>, int>);
static_assert(std::is_same_v<_::second_type::of<std::pair<int, char>>, char>);
static_assert(std::is_same_v<_::allocator_type::of<std::vector<int>>, std::allocator<int>>);

// chaining is just nesting `of` — vector<vector<char>> -> vector<char> -> char
static_assert(std::is_same_v<_::value_type::of<_::value_type::of<std::vector<std::vector<char>>>>, char>);

// a projection is a first-class type: hand it to a higher-order metafunction
template <class L, class Proj> struct map_proj;
template <template <class...> class L, class... Xs, class Proj> struct map_proj<L<Xs...>, Proj> {
  using type = L<typename Proj::template of<Xs>...>;
};
template <class...> struct list {};
static_assert(std::is_same_v<map_proj<list<std::vector<int>, std::map<char, bool>>, _::value_type>::type,
                             list<int, std::pair<const char, bool>>>);

// extension hooks: a user-declared nested-type (TACIT_NOUNS) and nested-*template* (TACIT_NOUN_TEMPLATES)
// projection, both first-class on the same `_`
struct Widget {
  using tag = int;
  template <class U> struct rebound {
    using type = U *;
  };
};
static_assert(std::is_same_v<_::tag::of<Widget>, int>);
static_assert(std::is_same_v<_::rebound<char>::of<Widget>::type, char *>);

// projection coexists with bind, both reached through `_`
static_assert(std::is_same_v<tacit::bind<std::vector, _::blank<>>::with<int>, std::vector<int>>);

int main() { return 0; }
