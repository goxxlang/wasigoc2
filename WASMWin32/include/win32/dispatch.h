#ifndef WASMWIN32_INCLUDE_WIN32_DISPATCH_H_
#define WASMWIN32_INCLUDE_WIN32_DISPATCH_H_

// WASMWin32 projection entry: one call, named like win32metadata DllImport
// entry points (Windows.Win32.*). Native Windows hits kernel32/wslapi.
// wasm32 has no PE imports — wasigocvm compiles the libc backend
// (wasi_host.hpp) into the guest so gocvm.Call("win32"|"wsl"|"nix")
// stays in-module. Companion gocvm_host is only leftover exec/TLS.
#ifdef __cplusplus
extern "C" {
#endif

// api: GetCurrentProcessId, GetComputerNameW, WslExec, NixVersion, ...
// args: UTF-8, 0x1F-separated parameters (may be empty).
// out: UTF-8 reply, always NUL-terminated when cap > 0.
// returns 0 on success; nonzero is a Win32 error or -1 for unknown API.
int wasmwin32_call(const char* api, const char* args, char* out, unsigned cap);

#ifdef __cplusplus
}
#endif

#endif  // WASMWIN32_INCLUDE_WIN32_DISPATCH_H_
