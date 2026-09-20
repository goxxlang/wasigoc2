# WASMWin32

A **win32metadata projection** for wasigocvm — not a dump of
`Windows.Win32.winmd`, and not a rewrite of kernel32. Names, DLLs, and
HANDLE/RAII shape come from the public catalog:

- https://github.com/microsoft/win32metadata
- Language-projection rules: [docs/projections.md](https://github.com/microsoft/win32metadata/blob/master/docs/projections.md)
- WSL hop: https://github.com/microsoft/WSL (`wslapi.dll` / `wsl.exe` on native Windows)
- Nix: the Nix CLI **inside** a WSL distro on native Windows; not in WASI libc

wasm32 cannot `DllImport` `kernel32`. Query APIs that libc already has
(`getpid`, `getenv`, `getcwd`, `gethostname`, `uname`, `stat`, clocks,
sleep) run **inside the wasm module** via `wasi_host.hpp` — same
`wasmwin32_call` names, no companion host process. `CreateProcessW` is
a `std::thread` child running `WslExec` / catalog work. `LoadLibraryW`
loads named modules this hop implements and files this module can map
(`.wasm` / PE via `~/WASMPELoader`). MainDLL (`DllMain` / TLS) runs on
that hop via `WHvRunVirtualProcessor`. A PE file fails only if mapping itself
fails.

```
wasigocvm guest (wasm32)
win32.GetCurrentProcessId()  →  gocvm.Call("win32", ...)
                                      →  wasmwin32::wasi_call()
                                      →  getpid / getenv / uname / stat
win32.CreateProcess(...)     →  std::thread + WslExec
```

Native Windows tests still hit kernel32 / wsl.exe with no wasm in between.

## Layout

```
include/win32/dispatch.h   wasmwin32_call — one entry, metadata names
include/win32/catalog.h    Windows.Win32.* + WSL + Nix rows
include/win32/wasi_host.hpp libc backend (compiled into wasigocvm guests)
include/win32/wasi_k32.hpp extra kernel32/user32/gdi32/ole32/ntdll/bcrypt/ncrypt/crypt32/shell32/winhttp/iphlpapi/comctl32/wininet/dnsapi/dbghelp/setupapi/pdh/wevtapi/comdlg32/WinHvPlatform
include/win32/pe_map.hpp   include of ~/WASMPELoader (LoadLibrary hop)
src/catalog.cc             the table
src/host_win.cc            real kernel32/user32/gdi32/ole32/ntdll/ws2_32/bcrypt/ncrypt/crypt32/shell32/winhttp/iphlpapi/version/comctl32/wininet/dnsapi/secur32/dbghelp/setupapi/netapi32/pdh/wevtapi/WinHvPlatform, SearchPath(wsl.exe), wslapi
src/host_wasi.cc           wasmwin32_call wrapping wasi_host.hpp
tests/win32_test.cc        PID, tick, CreateProcess, files, registry, tokens, vmem, WSA, ntdll Nt/Zw/Rtl/Ldr/Tp, PE map, VEH, bcrypt, ncrypt, Ldr, crypt32, shell32, winhttp, iphlpapi, version, comctl32, wininet, setupapi, pdh, wevtapi, user32, GDI, COM, WHP, MainDLL
tests/wsl_test.cc          WslList / WslExec / NixVersion (skip if absent)
examples/hello/            catalog dump + pid + wsl/nix probe
```

First hop plus the Environment / FileSystem / Time / Console /
Globalization / Diagnostics / Threading slice that libc or
`std::thread` can back (`SetEnvironmentVariableW`, `CreateDirectoryW`,
`GetUserNameW`, `GetSystemTime`, `CreateProcessW`, …). `LoadLibraryW` maps catalog
DLLs, `.wasm`, and PE images (`~/WASMPELoader`). Broader coverage still
comes from `Microsoft.Windows.SDK.Win32Metadata` (or
[win32json](https://github.com/marlersoft/win32json)) — same
`wasmwin32_call` names, more rows in the catalog.

## Build (native Windows)

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
build/win32_example.exe
```

## wasigocvm

`~/go++/stdlib/win32` is the guest projection (`import "win32"`).
`wasigocvm.bat` compiles `wasi_host.hpp` into the wasm module (`-I ~/WASMWin32/include`).

```
wasigocvm.bat examples\win32\main.go -o examples\win32\win32.wasm
..\shim_sandbox\tools\w2g-run.bat examples\win32\win32.wasm
```

No `--host-bridge` for pid / hostname / cwd / WslList(uname) /
CreateProcessW(`true`). Nix still fails. `LoadLibraryW` of a PE file
fails only when mapping that image fails. Mapping is `~/WASMPELoader`.

## License

New code is BSD-3-Clause. Win32 / WSL remain Microsoft APIs; this repo
does not vendor SDK headers or `Windows.Win32.winmd`.
