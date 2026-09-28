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

// Optional lower-level syscall lookup (~/WASMReact). WASMWin32 does not
// call this for names it answers. go++ installs it for a syscall the
// metadata layer does not answer, so that call is not a vCPU hypercall.
typedef int (*K32ReactCall)(unsigned index, const char* args, char* out, unsigned cap);
void k32_set_react_call(K32ReactCall fn);
K32ReactCall k32_get_react_call(void);

// Name -> ~/WASMReact catalog index, or -1. k32_pe_resolve consults this
// for a PE import the host catalog does not serve, so LoadLibraryW binds
// its IAT to the React band and the software WHv can run it with no host.
typedef int (*K32ReactIndex)(const char* name);
void k32_set_react_index(K32ReactIndex fn);
K32ReactIndex k32_get_react_index(void);

// A DLL that has reached DllMain(DLL_PROCESS_ATTACH). ntdll, kernel32,
// and WinHvPlatform are direct-loaded and do not come through here.
typedef void (*K32PsRegister)(const char* image);
void k32_set_ps_register(K32PsRegister fn);
K32PsRegister k32_get_ps_register(void);

#ifdef __cplusplus
}
#endif

#endif  // WASMWIN32_INCLUDE_WIN32_DISPATCH_H_
