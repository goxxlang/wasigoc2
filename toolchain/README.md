# wasigocvm toolchain

This is **our** C/C++ sysroot for the wasigocvm machine (driver + libc +
`wasitime`). Not stock wasip2 with extra `-f` flags. Architecture: [docs/architecture.md](../docs/architecture.md). Product:
[docs/wasigocvm.md](../docs/wasigocvm.md).

| Pillar | Where it lives |
| --- | --- |
| Full **libc++** (exceptions + RTTI) | eh sysroot + `compile.bat` / `wasigocvm.bat` / `.sh` |
| **Oilpan** GC | `src/runtime.hpp` `wasigo::gc` |
| **type_key** / go/types interning | `type_key_of<T>()`, `go/types` Object Type Identifier |
| **Sockets** | sysroot libc + `src/wasigocvm_net.hpp` (poll workers) |
| **TLS** | OpenSSL 3 wasm (`toolchain/openssl-wasm`) via `src/wasigocvm_tls.hpp` — WASMLime TlsTransport (memory BIO), not Schannel |
| **Threads** | stamp `threads: true`; child memory is WASMSafeSpace cage, not wasm `--shared-memory` |

## Bootstrap

```bash
# Linux / macOS / WSL
./toolchain/bootstrap.sh --jobs 8

# Windows → WSL
.\toolchain\bootstrap.ps1 -Jobs 8
```

Needs a **Clang >=22 host compiler** on PATH (wasi-sdk's own CMake refuses a
GCC host and gates `WASI_SDK_EXCEPTIONS` on >=22) -- `apt.llvm.org`'s
`llvm.sh 22` is the fastest way to get one on a stock Ubuntu box where the
default `clang` package is older. `bootstrap.sh` itself fetches the other
real prerequisite, `wasm-component-ld` (a separate Rust tool clang's
wasm32-wasip2/wasip3 driver shells out to for *any* `-shared` link, even
though it looks unrelated to exceptions/threads -- without it the build
fails deep in, with an opaque `posix_spawn failed: No such file or
directory` on `libc.so`).

By default this only builds the **sysroot** (`--target install`) using
whatever host clang it found -- no `bin/wasm32-*-clang++` wrapper scripts
come out of it, since those are wasi-sdk's `dist` target and may need
building LLVM/clang from scratch on top (`--dist`, unverified by this
project so far, budget real time for it). Pair the sysroot this produces
with an existing wasm32-wasip2-capable clang++ yourself until then.

The wasi-sdk CMake `COOP_THREADS_POSSIBLE` gate still wants **Clang >=23**
for *their* pthread+`--shared-memory` layout. That is not our path: stamp
`threads: true` with `-D_REENTRANT -lpthread`, isolation in WASMSafeSpace,
no wasm `--shared-memory`.

CI: `.github/workflows/wasigocvm-toolchain.yml` (workflow_dispatch) uploads
`wasigocvm-toolchain.tar.gz`. Unpack into `toolchain/` and set
`WASIGO_TOOLCHAIN` to that directory.

Install layout the drivers expect:

```
$WASIGO_TOOLCHAIN/
  wasigocvm-toolchain.json
  bin/wasm32-wasigocvm-clang++   # wrapper → wasip2 until we own a triple
  bin/wasm32-wasip2-clang++
  sysroot/                       # or share/wasi-sysroot (symlinked)
    include/wasm32-wasip2/eh/...
```

Drivers probe `wasm32-wasigocvm` eh layout first, then `wasm32-wasip2`.

## Driver (today)

```
compile.bat examples\httppkg\main.go -o httppkg.wasm
wasitime httppkg.wasm
# or: ./wasigocvm.sh ... && ./wasitime.sh ...
```

Always: full libc++, `-DWASIGO_GOCVM=1`, standard WASM EH.

## Roadmap

1. **Now:** sysroot + eh libc++ + Oilpan + type_key + in-guest libc/gocvm + wasitime.
2. **Next:** land a CI-built sysroot under `toolchain/` on this machine.
3. **Then:** `bin/wasm32-wasigocvm-clang++` as the only wrapper (drop wasip2 filename).
4. **OpenSSL wasm:** `./toolchain/build_openssl_wasm.sh` (or `.\toolchain\build_openssl_wasm.ps1`) installs `toolchain/openssl-wasm` from the vcpkg OpenSSL 3.6.3 tree so `wasigocvm.bat` can `-lssl -lcrypto`.

## Oilpan + type_key (not optional)

Rosetta, not extras: `wasigo::gc::*`, `type_key_of<T>()`, `go/types` OTIs.
RTTI is additive for C++ interop.

## Host

Host instantiate of a wasigocvm module is `wasitime` (ported WASMLoader).
Threads in the *wasi-sdk CMake* gate still want Clang >=23 until the stamp
says `threads: true`; wasm `--shared-memory` is not our path (WASMSafeSpace
cage is).