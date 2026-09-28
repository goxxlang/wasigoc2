#ifndef WASMWIN32_INCLUDE_WIN32_CATALOG_H_
#define WASMWIN32_INCLUDE_WIN32_CATALOG_H_

// NOTE — design mistake, do not repeat:
// Example trees were copy-dropped into production catalogs. That was
// wrong. The name is CmdExample. UNIL stays. No model should edit this
// runtime to get a guest into GocVM.
//
// Curated Windows.Win32.* rows from public win32metadata, plus WSL's
// wslapi / wsl.exe surface and Nix inside WSL. Not a dump of
// Windows.Win32.winmd.
#ifdef __cplusplus
extern "C" {
#endif

typedef struct WasmWin32Api {
  const char* ns;   // Windows.Win32.System.Threading, WSL, Nix, ...
  const char* dll;  // kernel32, wslapi, wsl.exe, ...
  const char* name; // DllImport entry / CLI
} WasmWin32Api;

const WasmWin32Api* wasmwin32_catalog(int* count);

#ifdef __cplusplus
}
#endif

#endif  // WASMWIN32_INCLUDE_WIN32_CATALOG_H_
