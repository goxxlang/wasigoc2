// Native Windows backend — same runtime OS hop as wasm.
#include "gocos/host.hpp"
#if defined(WASMGOCOS_HAS_JITRT)
#include <windows.h>

#include "gocos/klock.hpp"

namespace {

// Runs on its own OS thread (Windows starts one per control event), so it
// reaches kernel_interrupt_foreground through the atomic ForegroundGuard
// publishes, not a thread-local. Handled, so the default (process exit)
// does not run.
BOOL WINAPI CtrlHandler(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
    wasmgocos::kernel_interrupt_foreground();
    return TRUE;
  }
  return FALSE;
}

struct InstallCtrlHandler {
  InstallCtrlHandler() { SetConsoleCtrlHandler(&CtrlHandler, TRUE); }
} g_install_ctrl_handler;

}  // namespace
#endif

extern "C" int wasmgocos_call(const char* api, const char* args, char* out,
                              unsigned cap) {
#if defined(WASMGOCOS_HAS_JITRT)
  // Ctrl-C reaches this call's kill flag only for its outermost duration
  // on this thread -- not a guest's own gocos.Call reentering the host.
  wasmgocos::ForegroundGuard foreground;
  // One thread at a time in the kernel; a guest's gocos.Call re-enters.
  wasmgocos::KernelLock lock;
#endif
  std::string reply = wasmgocos::gocos_call(api, args);
  return wasmgocos::fill_out(out, cap, reply);
}
