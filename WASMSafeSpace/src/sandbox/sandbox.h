// This port's Sandbox: the actual "cage" -- real V8's Sandbox (src/sandbox/
// sandbox.h, vendored at third_party/v8/src/sandbox/sandbox.h) reserves a
// large (ideally ~1TB), alignment-fixed region of *virtual* address space
// via the OS, flanked by unmapped guard regions, and places most heap
// objects and all ArrayBuffer/WASM-memory backing stores inside it. The
// premise (real V8's own, stated in that file's doc comment): assume an
// attacker can, via a V8 memory-safety bug, corrupt memory anywhere inside
// the sandbox arbitrarily; the sandbox's job is to stop that corruption
// from reaching anything *outside* it.
//
// wasm32-wasip1 -- the actual target this whole repo exists for, see the
// top-level README -- has no OS virtual-memory reservation primitive to
// call (no mmap/VirtualAlloc with PROT_NONE guard pages): the module's
// entire linear memory already *is* a single flat, bounded 32-bit address
// space, and wasmtime's own sandboxing is what actually stops that linear
// memory from reaching anything outside the process -- a different,
// already-provided boundary, not this one. `Sandbox` here is a narrower,
// second boundary purely for defense-in-depth against a bug *inside* this
// port's own object model (the same threat real V8's Sandbox defends
// against): a single, real heap allocation acting as the cage, with real
// bounds tracking (`base()`/`end()`/`size()`/`Contains()`) and a real
// property that `sandboxed-pointer.h`'s compressed pointers are checked
// against. This port deliberately uses the *same* implementation on native
// builds too, rather than forking in real OS guard-page reservation behind
// a `#ifdef` for native only -- one real, portable mechanism, consistent
// with how CompressedPointer in ../WASMv8bindings/include/cppgc/internal/
// member-storage.h is a real (if degenerate) algorithm applied uniformly
// rather than gated by platform.
//
// What's real: the containment guarantee itself (`Contains()` is exact,
// not approximate), the base/end/size bookkeeping, and the single-active-
// sandbox `current()`/`set_current()` pattern real V8 uses per-thread (this
// port: per-process, single-threaded target, no thread_local needed -- same
// "exactly one live instance" simplification WASMv8bindings' cppgc::Heap
// already uses, see its heap.h file comment). What's cut: guard regions
// (nothing to reserve them from), the partially-reserved-sandbox fallback
// path, the real PageAllocator/VirtualAddressSpace abstraction layer (no
// OS pages to allocate), and multi-cage support
// (V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES) -- exactly one Sandbox at a
// time, like real V8's default (single-cage) configuration.
#ifndef WASMSAFESPACE_SRC_SANDBOX_SANDBOX_H_
#define WASMSAFESPACE_SRC_SANDBOX_SANDBOX_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "src/base/logging.h"
#include "v8-internal.h"

namespace v8 {
namespace internal {

class Sandbox final {
 public:
  // Real V8 aims for ~1TB (or a scaled-down size on 32-bit hosts). A
  // wasm32-wasip1 module's entire address space is 4GB at most, and in
  // practice much less is actually available/practical to commit as one
  // allocation -- 64MB is this port's own default, plenty for the
  // tests/examples in this repo and trivially overridable via Initialize().
  static constexpr size_t kDefaultSize = 64 * MB;

  Sandbox() = default;
  ~Sandbox();
  Sandbox(const Sandbox&) = delete;
  Sandbox& operator=(const Sandbox&) = delete;

  // Reserves the cage. CHECK-fails if already initialized.
  void Initialize(size_t size = kDefaultSize);

  // Releases the cage's backing memory. Any SandboxedPointer/handle into
  // this sandbox is invalid after this call, same as real V8.
  void TearDown();

  bool is_initialized() const { return initialized_; }

  // The start of the cage's address range.
  Address base() const { return base_; }
  // The address just past the end of the cage (base() + size()).
  Address end() const { return end_; }
  size_t size() const { return size_; }

  // Real V8's core containment check -- exact, no approximation.
  bool Contains(Address addr) const {
    return initialized_ && addr >= base_ && addr < end_;
  }
  bool Contains(const void* ptr) const {
    return Contains(reinterpret_cast<Address>(ptr));
  }

  // This port's stand-in for real V8's page_allocator()/
  // in_sandbox_allocator(): a plain bump allocator over the cage, for
  // objects (e.g. an ArrayBuffer-style backing store) that must
  // demonstrably live inside the cage rather than just be validated
  // against it. No Free() -- a bump allocator with no reclamation, the
  // same scope real cppgc's own allocator (../WASMv8bindings/src/heap/
  // internal/) actually needed a real freelist for and this doesn't: nothing
  // in this port's own tests needs to reclaim in-cage bump allocations.
  void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));

  // The single active sandbox. Real V8 keeps one per-thread (or one
  // process-wide default); this port keeps exactly one, process-wide,
  // matching its single-threaded wasm32-wasip1 target.
  static Sandbox* current() { return current_; }
  static void set_current(Sandbox* sandbox) { current_ = sandbox; }

 private:
  Address base_ = kNullAddress;
  Address end_ = kNullAddress;
  size_t size_ = 0;
  std::unique_ptr<uint8_t[]> backing_;
  size_t bump_offset_ = 0;
  bool initialized_ = false;

  static Sandbox* current_;
};

// Real V8's OutsideSandbox()/InsideSandbox() free functions, unchanged
// shape: helpers used to assert that a given trusted object genuinely
// lives outside the cage (see sandbox_integration_test.cc).
V8_INLINE bool InsideSandbox(Address address) {
  Sandbox* sandbox = Sandbox::current();
  return sandbox != nullptr && sandbox->Contains(address);
}
V8_INLINE bool OutsideSandbox(Address address) { return !InsideSandbox(address); }

}  // namespace internal
}  // namespace v8

#endif  // WASMSAFESPACE_SRC_SANDBOX_SANDBOX_H_
