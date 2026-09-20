# WASMPELoader

PE mapper for **wasigocvm** / **WASMWin32**. `LoadLibraryW` / `LdrLoadDll`
on the wasm hop call this module. It is not Microsoft's loader and not a
PE-to-wasm JIT.

Maps MZ / PE32 / PE32+ into linear memory:

- headers and sections
- relocs (types 0, 1, 2, 3, 4, 10)
- exports, including forwards
- import and delay-load IAT (via a resolve callback)
- bound imports, resources, TLS callback RVAs (as data)
- exception / debug / load-config / security directories as data

x86/x64 bytes stay data here. MainDLL (`DllMain`) and TLS callbacks
run on the **WASMWin32** `LoadLibraryW` hop through
`WHvRunVirtualProcessor`. Authenticode is **not** invented here
(`WinVerifyTrust` stays a different hop).

WASMWin32 `pe_map.hpp` includes this header. Do not copy the mapper
back into WASMWin32.

```
include/wasmpe/loader.hpp   header-only mapper
tests/loader_test.cc        minimal PE map / export / reject
```

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
