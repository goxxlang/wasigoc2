#ifndef WASMGOCOS_INCLUDE_GOCOS_CATALOG_H_
#define WASMGOCOS_INCLUDE_GOCOS_CATALOG_H_

// GocKrnl.* / GocSys.* / GocDesk.* / GocNix.* / GocShell.* — the edge
// kernel catalog. Not a dump of ntoskrnl, wine.git, or lxcore.sys.
// Calls run through gocvm hypervision (k32 / nix). Memory is gocvm
// vmem (EPT + k32 VirtualAlloc / nix mmap).
#ifdef __cplusplus
extern "C" {
#endif

typedef struct WasmGocOSApi {
  const char* ns;   // GocKrnl.Proc, GocSys.Wnd, GocDesk, GocNix, GocShell, …
  const char* lib;  // gockrnl, gocsys, gocdesk, gocnix, gocshell
  const char* name; // Boot / CreateProcess / Alloc / LoadModule / Present / …
} WasmGocOSApi;

const WasmGocOSApi* wasmgocos_catalog(int* count);

#ifdef __cplusplus
}
#endif

#endif  // WASMGOCOS_INCLUDE_GOCOS_CATALOG_H_
