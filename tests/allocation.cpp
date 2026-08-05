// What a closure COSTS on the heap, pinned rather than assumed.
//
// The claim tacit makes is parity: building a closure allocates what writing the equivalent lambda by hand
// allocates, and CALLING one allocates nothing at all. Neither half was checked anywhere until this file, and both
// had drifted — a comparison section used to store its bound operand twice over (once in the closure, once again as
// chain state), and `first`/`second` deep-copied the pair element they never even look at.
//
// HOW THE ASSERTIONS ARE WRITTEN, and why they are not counts of allocations. The absolute number depends on the
// standard library: the small-string threshold is 15 characters on libstdc++ and the MSVC STL, 22 on libc++, and a
// Debug MSVC build can allocate for its own bookkeeping besides. So every claim here is RELATIVE — tacit's count
// against the hand-written count in the same build — except "zero", which is portable because it is zero. The
// strings are long enough to defeat every implementation's SSO, so a copy always shows up as an allocation.
#include <tacit/_.hpp>

#include <cassert>
#include <cstdlib>
#include <new>
#include <string>
#include <utility>

using tacit::_;

namespace {
long allocs = 0;
// Long enough that a copy allocates on every implementation; not `constexpr`, so it is a real heap string.
std::string const LONG = "a string comfortably longer than every small-string buffer in every standard library";
} // namespace

void *operator new(std::size_t n) {
  ++allocs;
  if (void *p = std::malloc(n)) return p;
  throw std::bad_alloc{};
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void *p) noexcept { ::operator delete(p); }
void operator delete[](void *p, std::size_t) noexcept { ::operator delete(p); }

// Run `f` and report how many allocations it caused.
template <class F> long cost(F &&f) {
  long const before = allocs;
  static_cast<F &&>(f)();
  return allocs - before;
}

int main() {
  // ---- calling a closure allocates NOTHING ----
  // The absolute claim, and the one that matters most: a closure in a loop must not allocate per call. Bound
  // operands are held by the closure and passed by reference to the operator; fills are forwarded, never copied.
  {
    auto eq = (_ == LONG);
    auto cmp = (_.size() < _.size());
    auto add = (_ + 1);
    auto sub = _.substr(0); // returns a copy of the whole string — that DOES allocate
    assert(cost([&] {
             for (int i = 0; i < 100; ++i) {
               (void)eq(LONG);
               (void)cmp(LONG, LONG);
               (void)add(41);
             }
           }) == 0);
    // a chain that genuinely builds a string allocates — that is the substr's result, not the closure
    assert(cost([&] { (void)sub(LONG); }) > 0);
  }

  // ---- binding an operand stores it ONCE ----
  // Asserted on SIZE, at compile time, because that is what the property actually is. Storing an operand twice
  // doubles the closure; it does not reliably double an allocation count, and counting was the wrong instrument:
  // the fn-side `g op y` operator takes its operand by value and then moves it into a by-value constructor
  // parameter, one move more than the mirror path. That move is free everywhere it matters and costs an
  // iterator-debugging proxy under MSVC's Debug CRT, so the two paths legitimately differ in allocations while
  // storing exactly the same thing. `sizeof` says what is stored and says it identically on every implementation.
  //
  // The bug this guards: a comparison section kept the operand in the closure AND again as chain state, so these
  // were twice the size of the operand they held.
  static_assert(sizeof(decltype(_ == LONG)) == sizeof(std::string));
  static_assert(sizeof(decltype(LONG == _)) == sizeof(std::string));
  static_assert(sizeof(decltype(_ + LONG)) == sizeof(std::string));
  // The projected forms hold the operand plus the projection, so an exact size is not predictable — alignment
  // decides the rest. The claim that matters survives anyway, stated as an upper bound: whatever else is in
  // there, the OPERAND is not in it twice.
  static_assert(sizeof(decltype(_.substr(0) == LONG)) < 2 * sizeof(std::string));
  static_assert(sizeof(decltype(LONG == _.substr(0))) < 2 * sizeof(std::string));

  // ---- and binding really does cost a copy of it, not nothing ----
  {
    long const one_copy = cost([&] {
      auto s = LONG;
      (void)s;
    });
    long const bound = cost([&] {
      auto f = (_ == LONG);
      (void)f;
    });
    assert(one_copy > 0 && bound >= one_copy);
    // a section over an operand that never touches the heap costs nothing at all
    assert(cost([&] {
             auto f = (_.size() == LONG.size());
             (void)f;
           }) == 0);
  }

  // ---- an RVALUE operand is MOVED into the closure, not copied ----
  // Stated as "strictly cheaper than the lvalue case" rather than "costs nothing". Under MSVC's Debug CRT every
  // string OBJECT carries an iterator-debugging proxy that is allocated per construction, so even a move costs
  // something there and a flat `== 0` would be asserting the absence of an allocation that the standard library,
  // not tacit, is making. Cheaper-than-copying is the property being claimed, it is what forwarding buys, and it
  // holds on every implementation.
  {
    auto lv = LONG;
    long const by_copy = cost([&] {
      auto f = (_ == lv);
      (void)f;
    });
    auto rv = LONG;
    long const by_move = cost([&] {
      auto f = (_ == std::move(rv));
      (void)f;
    });
    assert(by_move < by_copy);

    auto lv2 = LONG;
    long const cat_copy = cost([&] {
      auto f = (_ + lv2);
      (void)f;
    });
    auto rv2 = LONG;
    long const cat_move = cost([&] {
      auto f = (_ + std::move(rv2));
      (void)f;
    });
    assert(cat_move < cat_copy);
  }

  // ---- combinators do not copy what they do not touch ----
  // `first`/`second` forward the pair, so the untouched element moves out of an rvalue rather than being copied.
  // Same formulation: the rvalue path must be strictly cheaper than the lvalue path. Un-forwarding either of them
  // makes the two equal, which is what this catches.
  {
    auto p1 = std::pair<int, std::string>{1, LONG};
    auto p2 = std::pair<int, std::string>{1, LONG};
    assert(cost([&] { (void)tacit::first(_ + 1)(std::move(p1)); }) < cost([&] { (void)tacit::first(_ + 1)(p2); }));

    auto q1 = std::pair<std::string, int>{LONG, 1};
    auto q2 = std::pair<std::string, int>{LONG, 1};
    assert(cost([&] { (void)tacit::second(_ + 1)(std::move(q1)); }) < cost([&] { (void)tacit::second(_ + 1)(q2); }));

    // the comma section, likewise
    auto a1 = LONG, b1 = LONG, a2 = LONG, b2 = LONG;
    assert(cost([&] { (void)(_, _)(std::move(a1), std::move(b1)); }) < cost([&] { (void)(_, _)(a2, b2); }));
  }

  // ---- a stateless closure is free ----
  // Not a heap claim but the same family: these hold nothing, so there is nothing to copy anywhere. Only the leaf
  // forms are asserted EMPTY — a closure wrapping a projection is two bytes, not one, because the projection is a
  // distinct empty subobject of its own (see the note in properties.cpp). Its portable claim is default
  // constructibility, which lives there.
  static_assert(std::is_empty_v<decltype(_ > _)>);
  static_assert(std::is_empty_v<decltype(-_)>);

  return 0;
}
