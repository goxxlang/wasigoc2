// WASIGO_GOCVM_BRIDGE hook for the wasm in-module VM.
//
// Generated wasigoc main() calls wasigo::set_os_args(), which calls
// wasigo_gocvm_install_bridge() when built with -DWASIGO_GOCVM_BRIDGE=1.
// Install the net async bridge first (sockets, os/exec, tls), then stack
// the Win32 topic bridge (win32 / wsl / nix -> wasmwin32_call) on top of
// it. Order matters: the Win32 bridge captures the net bridge as its
// fall-through, so unowned topics still reach net.
#include "runtime.hpp"

#include "win32/gocvm_bridge.hpp"

extern "C" void wasigo_gocvm_install_bridge() {
#if defined(WASIGO_GOCVM) && WASIGO_GOCVM
  wasigo::gocvm::install_wasigocvm_net_bridge();
  win32::install_wasigocvm_win32_bridge();
#endif
}
