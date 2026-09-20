#ifndef WASMGOCOS_INCLUDE_GOCOS_DISPATCH_H_
#define WASMGOCOS_INCLUDE_GOCOS_DISPATCH_H_

// WASMGocOS is the wasigocvm edge kernel (browser → desktop).
// Guest binding is gocvm.Call("gocos", …) on gocvm.wasm.
//
//   GocKrnl  — kernel (objects, process, thread, vm, io, session)
//   GocSys   — subsystem (loader, window, draw, registry, path)
//
// Every hop is gocvm hypervision: k32 (~/WASMWin32) or nix (~/WASMNix)
// for WSL. Memory is the gocvm model (EPT vmem / k32 VirtualAlloc /
// nix mmap) — not a local heap and not a copy of ntoskrnl or Wine.
#ifdef __cplusplus
extern "C" {
#endif

// api: Boot, Version, CreateProcess, Alloc, LoadModule, CreateWindow,
//      Present, Launch, Cmd, …
// args: UTF-8, 0x1F-separated parameters (may be empty).
// out: UTF-8 reply, always NUL-terminated when cap > 0.
// returns 0 on success; nonzero is errno or -1 for unknown API.
int wasmgocos_call(const char* api, const char* args, char* out, unsigned cap);

#ifdef __cplusplus
}
#endif

#endif  // WASMGOCOS_INCLUDE_GOCOS_DISPATCH_H_
