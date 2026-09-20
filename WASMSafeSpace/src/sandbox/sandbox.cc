#include "src/sandbox/sandbox.h"

#include <cstdlib>

namespace v8 {
namespace internal {

Sandbox* Sandbox::current_ = nullptr;

Sandbox::~Sandbox() {
  if (initialized_) TearDown();
}

void Sandbox::Initialize(size_t size) {
  CHECK(!initialized_);
  CHECK_GT(size, 0u);
  backing_ = std::make_unique<uint8_t[]>(size);
  base_ = reinterpret_cast<Address>(backing_.get());
  size_ = size;
  end_ = base_ + size_;
  bump_offset_ = 0;
  initialized_ = true;
}

void Sandbox::TearDown() {
  CHECK(initialized_);
  if (current_ == this) current_ = nullptr;
  backing_.reset();
  base_ = kNullAddress;
  end_ = kNullAddress;
  size_ = 0;
  bump_offset_ = 0;
  initialized_ = false;
}

void* Sandbox::Allocate(size_t size, size_t alignment) {
  CHECK(initialized_);
  // Align the *absolute* address, not just the running offset: the cage's
  // own base address has no alignment guarantee beyond whatever the host
  // allocator happened to hand back for `backing_` (see this method's file
  // history -- an earlier version aligned `bump_offset_` alone, which only
  // produced a correctly-aligned pointer by coincidence when `base()`
  // itself happened to already be aligned; wasmtime's own allocator
  // returned a `base()` that wasn't, so aligned_offset=64 tests started
  // failing under wasm32-wasip1 -- see the top-level README's own
  // "real bug" section for the same "only actually running on the target
  // catches this" story).
  uintptr_t base_addr = reinterpret_cast<uintptr_t>(backing_.get());
  uintptr_t unaligned = base_addr + bump_offset_;
  uintptr_t aligned = (unaligned + alignment - 1) & ~(alignment - 1);
  size_t aligned_offset = aligned - base_addr;
  CHECK_LE(aligned_offset + size, size_);
  void* result = backing_.get() + aligned_offset;
  bump_offset_ = aligned_offset + size;
  return result;
}

}  // namespace internal
}  // namespace v8
