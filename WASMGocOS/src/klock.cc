#include "gocos/klock.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include "src/runtime/context.h"
#include "src/runtime/exn.h"

namespace wasmgocos {
namespace {

std::mutex g_mu;
std::condition_variable g_turn;
uint64_t g_next = 0;     // the next ticket
uint64_t g_serving = 0;  // the ticket that holds the lock
// The thread that held the lock last. Only a different one can have left
// its own state in the globals.
std::thread::id g_last;

thread_local int t_depth = 0;
thread_local wasmturbo::ExecContext t_ctx;
thread_local std::atomic<bool>* t_kill = nullptr;
thread_local bool t_killed = false;

// The flag a foreground wasmgocos_call installed, if any -- reached from
// whatever thread the console's Ctrl-C handler runs on, so not
// thread_local. Only ForegroundGuard's own thread ever writes it.
std::atomic<std::atomic<bool>*> g_foreground{nullptr};

bool KillAsked() { return t_kill != nullptr && t_kill->load(); }

void Acquire() {
  std::unique_lock<std::mutex> lock(g_mu);
  uint64_t ticket = g_next++;
  if (ticket != g_serving) wasmturbo_preempt = 1;  // ask the holder to yield
  g_turn.wait(lock, [&] { return g_serving == ticket; });
  // The same thread taking the lock back finds its own state in place --
  // possibly live, when it entered WASM without the lock (jit_* callers).
  if (g_last != std::this_thread::get_id()) wasmturbo::LoadExecContext(t_ctx);
  g_last = std::this_thread::get_id();
  wasmturbo_preempt = (g_next - g_serving > 1 || KillAsked()) ? 1 : 0;
}

void Release() {
  wasmturbo::SaveExecContext(&t_ctx);
  std::lock_guard<std::mutex> lock(g_mu);
  ++g_serving;
  g_turn.notify_all();
}

// wasmturbo_yield's hook: the running guest reached a yield point with
// the flag up. Give the lock to whoever waits, take it back, and end the
// guest if it was killed meanwhile.
void Yield() {
  if (t_depth > 0) {
    int depth = t_depth;
    t_depth = 0;
    Release();
    Acquire();
    t_depth = depth;
  } else {
    wasmturbo_preempt = 0;
  }
  if (KillAsked() && !t_killed) {
    t_killed = true;
    wasmturbo_exn_terminate();
  }
}

struct InstallYield {
  InstallYield() { wasmturbo_set_yield(&Yield); }
} g_install;

}  // namespace

KernelLock::KernelLock() {
  if (t_depth++ == 0) Acquire();
}

KernelLock::~KernelLock() {
  if (--t_depth == 0) Release();
}

void kernel_unlocked(const std::function<void()>& wait) {
  if (t_depth == 0) {
    wait();
    return;
  }
  int depth = t_depth;
  t_depth = 0;
  Release();
  wait();
  Acquire();
  t_depth = depth;
  if (KillAsked() && !t_killed) {
    t_killed = true;
    wasmturbo_exn_terminate();
  }
}

void kernel_set_kill(std::atomic<bool>* flag) {
  t_kill = flag;
  t_killed = false;
}

bool kernel_killed() { return t_killed; }

std::atomic<bool>* kernel_kill_flag() { return t_kill; }

bool kernel_kill_asked() { return KillAsked(); }

void kernel_preempt() { wasmturbo_preempt = 1; }

ForegroundGuard::ForegroundGuard() {
  if (t_depth == 0) {
    installed_ = true;
    kernel_set_kill(&flag_);
    g_foreground.store(&flag_);
  }
}

ForegroundGuard::~ForegroundGuard() {
  if (installed_) {
    g_foreground.store(nullptr);
    kernel_set_kill(nullptr);
  }
}

void kernel_interrupt_foreground() {
  std::atomic<bool>* flag = g_foreground.load();
  if (flag != nullptr) {
    flag->store(true);
    kernel_preempt();
  }
}

}  // namespace wasmgocos
