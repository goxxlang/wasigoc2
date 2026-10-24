#ifndef WASMGOCOS_INCLUDE_GOCOS_KLOCK_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_KLOCK_HPP_

// The kernel lock. The kernel runs one thread at a time, the way a
// uniprocessor kernel does: whoever holds this lock runs kernel code and
// WASM, and every other thread waits its turn in FIFO order. A background
// guest (`ctr run ... &`) is such a thread. Each thread keeps its own
// WASM execution state (wasmturbo::ExecContext), saved when it gives the
// lock up and loaded when it takes it back, because JIT'd code reads that
// state at fixed addresses.
//
// A guest gives the lock up at its yield points while another thread is
// waiting (src/runtime/context.h), and around every blocking host call
// (kernel_unlocked). `wasmgocos_call` takes the lock, so a caller outside
// the kernel waits its turn too. A thread may take the lock again while
// it holds it (a guest's gocos.Call comes back through wasmgocos_call).

#include <atomic>
#include <functional>

namespace wasmgocos {

class KernelLock {
 public:
  KernelLock();
  ~KernelLock();
  KernelLock(const KernelLock&) = delete;
  KernelLock& operator=(const KernelLock&) = delete;
};

// Runs `wait` with the lock given up, then takes it back. For a sleep, a
// join, a poll: anything that blocks. Without the lock held it just runs.
void kernel_unlocked(const std::function<void()>& wait);

// The kill flag of the guest this thread runs, or null. When it is set,
// the guest's next yield ends it (wasmturbo_exn_terminate) and
// kernel_killed() is true for the rest of the run.
void kernel_set_kill(std::atomic<bool>* flag);
bool kernel_killed();
// This thread's kill flag, to hand to threads working for the same job,
// and whether it is up. A wait that does not return to a yield point
// (a stream, include/gocos/stream.hpp) checks it.
std::atomic<bool>* kernel_kill_flag();
bool kernel_kill_asked();
// Asks the running guest to reach a yield point soon.
void kernel_preempt();

// A foreground `wasmgocos_call` (no trailing `&`) has no thread of its
// own for `kill` to reach, so it installs its kill flag here for the
// call's outermost duration -- not a guest's own gocos.Call reentering
// the host, which keeps running under the outer call's flag. Ctrl-C on
// the console (kernel_interrupt_foreground) ends it the same way `kill`
// ends a background job.
class ForegroundGuard {
 public:
  ForegroundGuard();
  ~ForegroundGuard();
  ForegroundGuard(const ForegroundGuard&) = delete;
  ForegroundGuard& operator=(const ForegroundGuard&) = delete;

 private:
  bool installed_ = false;
  std::atomic<bool> flag_{false};
};

// Ends whatever foreground call currently owns the console, the way
// `kill` ends a background job. A no-op when nothing foreground is
// running (or it is between yield points -- the next one takes it).
void kernel_interrupt_foreground();

}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_KLOCK_HPP_
