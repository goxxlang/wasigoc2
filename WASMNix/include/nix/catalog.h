#ifndef WASMNIX_INCLUDE_NIX_CATALOG_H_
#define WASMNIX_INCLUDE_NIX_CATALOG_H_

// Curated Linux.POSIX.* / Linux.Syscall.* / Linux.Kernel.* rows from
// public man-pages and include/linux/*.h EXPORT_SYMBOL (ntoskrnl analog),
// plus WSL Linux-side, Nix CLI, and kvm.h/vfio/vhost/mshv. Not a dump
// of unistd.h or of vmlinux.o.
#ifdef __cplusplus
extern "C" {
#endif

typedef struct WasmNixApi {
  const char* ns;   // Linux.POSIX.unistd, Linux.Syscall, WSL, Nix, ...
  const char* lib;  // libc, libpthread, libdl, linux, wsl, nix, ...
  const char* name; // syscall / libc / CLI entry
} WasmNixApi;

const WasmNixApi* wasmnix_catalog(int* count);

#ifdef __cplusplus
}
#endif

#endif  // WASMNIX_INCLUDE_NIX_CATALOG_H_
