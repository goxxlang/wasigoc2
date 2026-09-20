#ifndef WASMNIX_INCLUDE_NIX_DISPATCH_H_
#define WASMNIX_INCLUDE_NIX_DISPATCH_H_

// WASMNix projection entry: one call, named like Linux man-pages /
// POSIX / WSL Linux-side / Nix CLI. Native Linux/WSL hits libc and
// the nix binary. Native Windows trampolines through wsl.exe so the
// Linux catalog still runs on the WSL hop. wasm32 has no Linux
// syscalls — posix_host.hpp maps what libc already has (getpid,
// getenv, uname, stat, clocks) into the guest so
// gocvm.Call("linux"|"wsl"|"nix") stays in-module.
#ifdef __cplusplus
extern "C" {
#endif

// api: getpid, uname, WslPath, NixVersion, ...
// args: UTF-8, 0x1F-separated parameters (may be empty).
// out: UTF-8 reply, always NUL-terminated when cap > 0.
// returns 0 on success; nonzero is errno or -1 for unknown API.
int wasmnix_call(const char* api, const char* args, char* out, unsigned cap);

#ifdef __cplusplus
}
#endif

#endif  // WASMNIX_INCLUDE_NIX_DISPATCH_H_
