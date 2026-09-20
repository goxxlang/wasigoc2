# wasigocvm

**wasigocvm is a complete machine, and the full stack is wasm.** The
sysroot, libc, libc++, Oilpan, gocvm (sockets / exec / OpenSSL TLS),
WASMSafeSpace cage, WASMv8bindings cppgc, WASMWin32 / WASMNix / WASMDroid names, and `wasitime` itself
(`wasitime.wasm`) are one module. It is not a Bytecode Alliance host
engine with a WASI guest, not a Wasmtime flag pack, and not a companion
`gocvm_host`.

Architecture (one linear memory; second address space is EPT / TPT /
CHPT): [architecture.md](architecture.md). Bytecode is a **hard fork of
WASM 2** (opcodes kept; Oilpan, cage, interned CHPT instead of WASM GC /
WIT): [wasm2-fork.md](wasm2-fork.md). Host ABI is in-tree **`WASMWin32/` /
`WASMNix/` / `WASMDroid/`** catalogs (`gocvm.Call`, not WIT):
[host-abi.md](host-abi.md).

```
Go++ source
    │
    ▼
wasigoc                          frontend (this repo)
    │
    ▼
compile.bat / wasigocvm.bat      driver  -DWASIGO_GOCVM=1
    │                            full libc++ EH/RTTI, standard WASM EH
    ▼
toolchain/sysroot                OUR sysroot (bootstrap.sh)
    │                            libc + libc++ eh/ + mman + getpid
    ▼
.wasigocvm.wasm
    │
    ├─ Oilpan, type_key, cooperative goroutines   runtime.hpp
    ├─ libc ABI         sockets, getpid, getenv, mmap, uname, stat
    ├─ gocvm            wasigocvm_net.hpp / libc / exec / tls (OpenSSL)
    ├─ WASMSafeSpace    cage + EPT/TPT (exec + Win32/Nix/Droid)
    ├─ WASMv8bindings   CHPT (exec child, Win32Kernel, NixKernel, DroidKernel)
    ├─ WASMWin32        catalog on EPT; process/thread on TPT
    ├─ WASMNix          catalog on EPT; linux/wsl/nix posix_call
    ├─ WASMDroid        catalog + Binder on EPT; android/binder/kvm bionic_call
    └─ WASMGocOS        catalog + GocDesk on EPT; GocKrnl / GocSys hop k32 / nix
    ▼
wasitime                         WASMLoader + Go++ wazero interpreter + WASMSafeSpace
    inspect / run / call         ported ~/WASMLoader on examples/wazeropkg, not a vendor dump
```

Product gate: `-DWASIGO_GOCVM=1` (`src/wasigocvm_config.hpp`). Stock
`__wasip2__` is not the identity.

## The three things we own

### 1. Sysroot — `toolchain/`

`toolchain/bootstrap.sh` / `bootstrap.ps1` builds **our** sysroot into
`toolchain/sysroot` (or `WASIGO_TOOLCHAIN`). Stamp:
`toolchain/wasigocvm-toolchain.json` (`name: wasigocvm`,
`product_define: WASIGO_GOCVM`, exceptions, threads).

This is a C/C++ sysroot contract: headers, `eh/` libc++, compiler-rt,
emulated `mmap` / `getpid`, sockets. Drivers probe `wasm32-wasigocvm`
then `wasm32-wasip2` **layouts under our prefix**. A clang binary may
still be named `wasm32-wasip2-clang++` until `bin/wasm32-wasigocvm-clang++`
is the only wrapper — that is a compiler *filename*, not the ABI.
[toolchain/README.md](../toolchain/README.md).

### 2. Libc — POSIX in the module

The guest talks **libc**, not a WIT world (`wasi:cli` / `wasi:sockets`).

| Layer | Where |
| --- | --- |
| C library | sysroot wasi-libc (`unistd`, sockets, `stat`, `uname`, `getenv`) |
| mmap | `-D_WASI_EMULATED_MMAN -lwasi-emulated-mman` — linear memory, the cage |
| getpid | occupancy process table (`gocvm::proc_self`, first pid 1000). Header still needs `_WASI_EMULATED_GETPID`; we do not link libwasi-emulated-getpid |
| libc++ | sysroot `eh/` — exceptions, RTTI, standard WASM EH |
| Go++ libc gate | `src/wasigocvm_libc.hpp` — `gocvm.Call("syscall"\|"win32"\|"linux"\|"android")` stays in-guest |
| Exec | `src/wasigocvm_exec.hpp` — child in EPT/TPT/CHPT, work via WASMWin32 `wasi_call` |
| Net | `src/wasigocvm_net.hpp` — `poll()` on sysroot sockets, one cooperative scheduler |

Win32metadata names (`GetCurrentProcessId`, `GetComputerNameW`, …) still
come from in-tree `WASMWin32/` `wasi_host.hpp` (not `kernel32` PE imports). The
Win32 session sits on **CHPT**, current process/thread on **TPT**, and
the catalog (`catalog.cc`) on **EPT**.

WASI as Bytecode Alliance ships it has no `fork`/`exec`. `os.exec` is a
real child in EPT/TPT/CHPT: parent names it through CHPT, stdout through
EPT, trusted parent through TPT. The child's work is in-tree `WASMWin32/`
`wasi_call` (catalog / `WslExec`), not a simulated command table.
`.wasm` is load/call through wasitime / WASMLoader. `os.user` and `kill` are
libc in this module. TLS is OpenSSL in this module (same stack as
~/WASMLime libdatachannel TlsTransport: memory BIOs, `SSL_do_handshake`,
SNI) — not a hop to `gocvm_host` / Schannel. Native vcpkg OpenSSL DLLs
are the wrong ABI; the driver links `toolchain/openssl-wasm` (wasm
`libssl.a` built from that same 3.6.3 tree).

WASMSafeSpace is the cage. There is no companion host and no native
leftover ABI. gocvm always has Win32, Nix, and Droid catalogs.

### 3. wasitime — the runtime

`wasitime.bat` / `wasitime.sh` is **our** engine CLI (wasmtime-shaped
commands). The compile/run slot Cranelift fills in Wasmtime is
`examples/wazeropkg` here — interpreter, not ISA codegen.
Full write-up: [wasitime.md](wasitime.md).

```
wasitime inspect <file.wasm>
wasitime run     <file.wasm>
wasitime call    <file.wasm> <export> [i32...]
wasitime <file.wasm>                 same as run
wasitime example add|hello|...       wasmbin examples (ported ~/WASMLoader)
```

`inspect` / `run` / `call` are the ported **~/WASMLoader** packages
(`examples/wasmbinpkg`, `examples/wasmloaderpkg`) on the Go++ wazero
interpreter (`examples/wazeropkg`): WASMSafeSpace cage + CPT/EPT/TPT,
WASMv8Bindings-shaped CHPT. The CLI host is `wasitime.exe` (`goclang++`);
`wasitime.wasm` is the same program as a wasigocvm module. Not a vendor
dump of tetratelabs/wazero, not w2g/Wasmtime.

## Versus the Bytecode Alliance stack

They have a host engine (Wasmtime) and a capability ABI (WASI previews /
Component Model). You bring the language. Preview 1 is `fd_write` and no
sockets. Preview 2 is a WIT world. `wasi-sdk` triples are their product
identity (`wasm32-wasip1`, `wasip2`, …).

We have a **language machine**: Go++ → C++ → our sysroot/libc → our
runtime. Becoming a Component Model guest so `__wasip2__` would be the ABI
is the thing we hard-forked *away* from.

`docs/wasip2.md` is retired as a product story. `legacy.bat` remains for
**stock wasip1 noeh** goldens only.

## Build / run

```
compile.bat examples\httppkg\main.go -o httppkg.wasm
wasitime httppkg.wasm
```

Unix: `wasigocvm.sh`, `wasitime.sh`.

ctest: `hello_wasigocvm`, `netpkg_wasigocvm`, `httppkg_wasigocvm`.

## 2026-09-09: bootstrap notes

Host Clang >=22 for exceptions (`WASI_SDK_EXCEPTIONS=DUAL` → `eh/`+`noeh/`).
`ln -sfn` from WSL onto `/mnt/*` is a dead reparse point on Windows — copy
the sysroot. Threads in the *wasi-sdk CMake* gate still want Clang >=23;
wasm `--shared-memory` is not our path (WASMSafeSpace cage is).
