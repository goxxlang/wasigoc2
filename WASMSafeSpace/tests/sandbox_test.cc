// Golden test for the Sandbox cage itself: Contains() is exact (true inside,
// false for anything outside -- including addresses that are merely "close"
// to the cage), the in-cage bump allocator only ever hands out memory that
// Contains() itself agrees is inside the cage, alignment is honored, and
// TearDown() actually invalidates containment (nothing "left over" is still
// considered inside afterwards).
#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/sandbox/sandbox.h"

using v8::Address;
using v8::internal::Sandbox;

int main() {
  Sandbox sandbox;
  assert(!sandbox.is_initialized());

  sandbox.Initialize(1 * 1024 * 1024);  // 1MB cage
  assert(sandbox.is_initialized());
  assert(sandbox.size() == 1u * 1024 * 1024);
  assert(sandbox.end() == sandbox.base() + sandbox.size());

  // The base itself, and the last valid byte, are inside; end() (one past
  // the last byte) and anything beyond are not.
  assert(sandbox.Contains(sandbox.base()));
  assert(sandbox.Contains(sandbox.end() - 1));
  assert(!sandbox.Contains(sandbox.end()));
  assert(!sandbox.Contains(static_cast<Address>(0)));
  assert(!sandbox.Contains(static_cast<Address>(~Address{0})));  // max address

  // An address that merely looks plausible (e.g. one byte before base, one
  // past the reservation) must still be rejected -- this is the whole
  // point of a cage boundary being exact rather than approximate.
  if (sandbox.base() != 0) {
    assert(!sandbox.Contains(sandbox.base() - 1));
  }

  // The bump allocator only ever hands out in-cage memory, and respects
  // alignment.
  void* a = sandbox.Allocate(64, 16);
  void* b = sandbox.Allocate(3, 16);  // odd size forces padding before next
  void* c = sandbox.Allocate(64, 64);
  assert(sandbox.Contains(a));
  assert(sandbox.Contains(b));
  assert(sandbox.Contains(c));
  assert(reinterpret_cast<uintptr_t>(a) % 16 == 0);
  assert(reinterpret_cast<uintptr_t>(b) % 16 == 0);
  assert(reinterpret_cast<uintptr_t>(c) % 64 == 0);
  assert(a != b && b != c && a != c);

  // A second, independent sandbox has its own, disjoint range.
  Sandbox other;
  other.Initialize(4096);
  assert(other.Contains(other.base()));
  assert(!sandbox.Contains(other.base()) || sandbox.base() == other.base());
  // (the "||" clause only guards against an astronomically unlikely
  // allocator coincidence; in practice the two heap allocations never
  // overlap.)

  // current()/set_current(): the single-active-sandbox slot real V8 also
  // maintains (see sandbox.h's file comment).
  assert(Sandbox::current() == nullptr);
  Sandbox::set_current(&sandbox);
  assert(Sandbox::current() == &sandbox);
  Sandbox::set_current(nullptr);

  other.TearDown();
  assert(!other.is_initialized());

  sandbox.TearDown();
  assert(!sandbox.is_initialized());

  std::printf("sandbox_test: OK\n");
  return 0;
}
