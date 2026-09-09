# wasigocvm

**wasigocvm** is the web-native Go++ machine: our compiler driver +
sysroot contract. It is **not** "stock wasip2 with better flags."

Stock Bytecode Alliance wasip2 is a Component Model world and a
triple that has sockets but no pthread. Flag-tuning that triple cannot
grow Oilpan + type_key + full libc++ + threads as one product. So the
product gate is `-DWASIGO_GOCVM=1` and `wasigocvm.bat`; the clang
wrapper may still be `wasm32-wasip2-clang++` until `toolchain/` ships
our own sysroot.

```
Go++  →  wasigoc  →  C++  →  wasigocvm clang++ (full libc++)  →  .wasm
                              │
                              ├─ Oilpan (wasigo::gc)
                              ├─ type_key interning (no itables)
                              ├─ gocvm net (wasigocvm_net.hpp + poll)
                              └─ threads when toolchain/sysroot has them
```

## Four pillars

| Pillar | Status |
| --- | --- |
| **Full libc++** | eh headers/libs, `-fexceptions -frtti`, standard WASM EH |
| **Oilpan** | `runtime.hpp` (`GarbageCollected` / `Member` / `Persistent`) |
| **Type-id interning** | `type_key_of<T>()` + `go/types` Object Type Identifier |
| **Threads** | Needs our sysroot; until then gocvm uses `poll()` workers |

## Build / run

```
wasigocvm.bat examples\httppkg\main.go -o httppkg.wasm
..\shim_sandbox\tools\w2g-run.bat httppkg.wasm
```

`compile.bat --wasip2` is **retired**. Prefer `WASIGO_TOOLCHAIN` /
`toolchain/sysroot`; fallback is still a wasi-sdk with `eh/`.

ctest: `hello_wasigocvm`, `netpkg_wasigocvm`, `httppkg_wasigocvm`.

## Rewrite note

Guest net used to key off `__wasip2__` and live in `wasip2_net.hpp`.
That made the stock triple the product identity. It now keys off
`WASIGO_GOCVM` and `wasigocvm_net.hpp`. Next: rebuild wasi-sdk into
`toolchain/` (exceptions ON, then pthread / shared-everything-threads).
Next: rebuild wasi-sdk into `toolchain/` (exceptions ON, then pthread).
Bootstrap: `toolchain/bootstrap.sh` / `bootstrap.ps1`, CI workflow
`wasigocvm-toolchain`. Feature probes: `src/wasigocvm_config.hpp`.
