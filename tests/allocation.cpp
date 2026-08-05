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

  // ---- binding an operand costs what the hand-written capture costs ----
  {
    long const hand = cost([&] {
      auto f = [y = LONG](std::string const &x) { return x == y; };
      (void)f;
    });
    assert(cost([&] {
             auto f = (_ == LONG);
             (void)f;
           }) == hand);
    // the mirror form is symmetric — it used to be HALF the cost of the above, which was the tell
    assert(cost([&] {
             auto f = (LONG == _);
             (void)f;
           }) == hand);
    // and so is the projected form, which used to store the operand twice over
    assert(cost([&] {
             auto f = (_.size() == LONG.size());
             (void)f;
           }) == 0); // size_t operand: nothing to allocate at all
    assert(cost([&] {
             auto f = (_.substr(0) == LONG);
             (void)f;
           }) == hand);
  }

  // ---- an RVALUE operand is MOVED into the closure, not copied ----
  {
    auto s = LONG;
    assert(cost([&] {
             auto f = (_ == std::move(s));
             (void)f;
           }) == 0);
    auto t = LONG;
    assert(cost([&] {
             auto f = (_ + std::move(t));
             (void)f;
           }) == 0);
  }

  // ---- combinators do not copy what they do not touch ----
  // `first`/`second` forward the pair, so the untouched element moves out of an rvalue rather than being copied.
  {
    auto p = std::pair<int, std::string>{1, LONG};
    assert(cost([&] { (void)tacit::first(_ + 1)(std::move(p)); }) == 0);
    auto q = std::pair<std::string, int>{LONG, 1};
    assert(cost([&] { (void)tacit::second(_ + 1)(std::move(q)); }) == 0);
    // the comma section, likewise
    auto a = LONG, b = LONG;
    assert(cost([&] { (void)(_, _)(std::move(a), std::move(b)); }) == 0);
  }

  // ---- a stateless closure is free ----
  // Not a heap claim but the same family: these hold nothing, so there is nothing to copy anywhere.
  static_assert(std::is_empty_v<decltype(_ > _)>);
  static_assert(std::is_empty_v<decltype(_.size() < _.size())>);
  static_assert(std::is_empty_v<decltype(-_)>);

  return 0;
}
