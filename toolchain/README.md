# wasigocvm toolchain

**Everything lives under `go++/toolchain/`.** Not `~/wasi-sdk`, not
`~/wasigocvm-wasi-sdk-*`, not `%USERPROFILE%\wasi-sdk` as the product
install. Those home paths were a bootstrap mistake for WSL `/mnt/c`
speed; they are no longer the default.

| Path | What |
| --- | --- |
| `toolchain/` | `WASIGO_TOOLCHAIN` / install prefix |
| `toolchain/bin/` | `wasm32-wasip2-clang++` (+ wasigocvm wrapper) |
| `toolchain/sysroot/` | eh libc++ / wasi-libc (Windows-visible copy) |
| `toolchain/share/wasi-sysroot/` | same sysroot as wasi-sdk installs it |
| `toolchain/wasigocvm-toolchain.json` | stamp (`WASIGO_GOCVM`, exceptions, threads) |
| `toolchain/wasi-sdk-src/` | WebAssembly/wasi-sdk git checkout |
| `toolchain/wasi-sdk-build/` | cmake build tree |

Architecture: [docs/architecture.md](../docs/architecture.md). Product:
[docs/wasigocvm.md](../docs/wasigocvm.md).

| Pillar | Where it lives |
| --- | --- |
| Full **libc++** (exceptions + RTTI) | eh sysroot + `wasigocvm.bat` / `.sh` |
| **Oilpan** GC | `src/runtime.hpp` `wasigo::gc` |
| **type_key** / go/types interning | `type_key_of<T>()`, `go/types` Object Type Identifier |
| **Sockets** | sysroot libc + `src/wasigocvm_net.hpp` (poll workers) |
| **TLS** | OpenSSL 3 wasm (`toolchain/openssl-wasm`) via `src/wasigocvm_tls.hpp` |
| **Threads** | stamp `threads: true`; child memory is WASMSafeSpace cage |

## Bootstrap

```bash
# From go++ repo root (Linux / macOS / WSL)
./toolchain/bootstrap.sh --jobs 8

# Windows → WSL (still installs into go++/toolchain on /mnt/c)
.\toolchain\bootstrap.ps1 -Jobs 8
```

Defaults (always in-repo):

```
SRC    = <repo>/toolchain/wasi-sdk-src
BUILD  = <repo>/toolchain/wasi-sdk-build
PREFIX = <repo>/toolchain   # or $WASIGO_TOOLCHAIN
```

Optional overrides only: `WASIGO_WASI_SDK_SRC`, `WASIGO_WASI_SDK_BUILD`,
`WASIGO_TOOLCHAIN`. Do not set these to `$HOME` unless you mean it.

Needs **Clang >=22** on the WSL/Linux host (`apt.llvm.org` / `llvm.sh 22+`).
`bootstrap.sh` also fetches `wasm-component-ld` when missing.

If you still have leftover WSL trees from the old default:

```
~/wasigocvm-wasi-sdk-src
~/wasigocvm-wasi-sdk-build
```

those are **orphans**. Safe to delete after `toolchain/` is complete.
Drivers never look there.

## Driver probe order

`wasigocvm.bat` / `.sh` / CMake:

1. `$WASIGO_TOOLCHAIN` or `<repo>/toolchain` (sysroot + bin)
2. Fallback only: `$WASI_SDK_PATH` / `%USERPROFILE%\wasi-sdk`

Product path is (1). (2) is stock wasi-sdk borrow when bootstrap has not
run.

## Layout drivers expect

```
$WASIGO_TOOLCHAIN/          # = go++/toolchain
  wasigocvm-toolchain.json
  bin/wasm32-wasip2-clang++
  bin/wasm32-wasigocvm-clang++   # optional wrapper → wasip2
  sysroot/
    include/wasm32-wasip2/eh/c++/v1/...
    lib/wasm32-wasip2/eh/...
```

## Host / run

```
wasigocvm.bat examples\httppkg\main.go -o httppkg.wasm
wasitime httppkg.wasm
```
