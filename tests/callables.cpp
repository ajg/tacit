// Closures and the standard function wrappers, both directions: `_()` invokes whatever callable a
// range hands it, and a tacit closure is an ordinary callable, so the wrappers hold it in turn.
// Per wrapper: std::function takes the copyable ones (most sections are); std::move_only_function
// (C++23) takes a section that bound a move-only VALUE — which makes the closure itself move-only,
// exactly that wrapper's case; std::function_ref (C++26) refers, so it wants a NAMED closure; and
// std::copyable_function (C++26) is std::function with const-correctness. The C++23/26 wrappers are
// feature-tested — absent on Apple's libc++ as of 21, present on the CI legs' libstdc++.
#include <tacit/_.hpp>

#include <cassert>
#include <functional>
#include <memory>
#include <ranges>
#include <vector>
#include <version>

using tacit::_;

int main() {
  // the README's thunk shape, and std::function holding a section
  using thunk_t = std::function<void()>;
  int hits = 0;
  auto thunks = std::vector<thunk_t>{[&] { ++hits; }, [&] { ++hits; }};
  std::ranges::for_each(thunks, _()); // invoke each thunk
  assert(hits == 2);
  std::function<bool(int)> pred = _ > 3; // a copyable closure, type-erased as usual
  assert(pred(4) && !pred(3));

  // a section binding a move-only VALUE is itself a move-only closure (the comparison's chain
  // state degrades to unchained for move-only operands — a fold needs a copy by construction)
  struct Key {
    std::unique_ptr<int> p;
    bool operator==(int v) const { return *p == v; }
  };
  auto is7 = (_ == Key{std::make_unique<int>(7)});
  static_assert(!std::is_copy_constructible_v<decltype(is7)>); // so std::function CANNOT hold it...
  assert(is7(7) && !is7(8));

#ifdef __cpp_lib_move_only_function
  // ...but std::move_only_function can — exactly its case
  std::move_only_function<bool(int) const> erased7 = (_ == Key{std::make_unique<int>(7)});
  assert(erased7(7) && !erased7(8));
  // and `_()` drives move-only callables too
  std::vector<std::move_only_function<void()>> mthunks;
  mthunks.emplace_back([&] { ++hits; });
  std::ranges::for_each(mthunks, _());
  assert(hits == 3);
#endif

#ifdef __cpp_lib_function_ref
  auto gt = _ > 3; // function_ref refers — a NAMED closure, never a temporary
  std::function_ref<bool(int)> ref = gt;
  assert(ref(4) && !ref(3));
#endif

#ifdef __cpp_lib_copyable_function
  std::copyable_function<bool(int) const> cf = _ > 3;
  assert(cf(4) && !cf(3));
#endif

  return 0;
}
