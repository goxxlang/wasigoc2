# Wasigoc2
<img width="1408" height="768" alt="1788049219961" src="https://github.com/user-attachments/assets/3f1444b7-70d6-49a2-be85-b0c7db589ed2" />

**Go++ is the language. `wasigoc` is the compiler. wasigocvm is the machine. Wasigoc2 is this release of that full stack.**

* **GitHub:** [goxxlang/wasigoc](https://github.com/goxxlang/wasigoc)
* **License:** BSD-3-Clause (`LICENSE`)

Go++ is Go *syntax* with a C++/WASM runtime: Oilpan GC, cooperative goroutines, type_key interning, no data races. `wasigoc` emits C++. **wasigocvm** is a complete machine, and the full stack is wasm — sysroot, libc, libc++, OpenSSL, WASMSafeSpace cage, WASMv8bindings cppgc, in-tree WASMWin32 / WASMNix / WASMDroid catalogs, and `wasitime` itself (`wasitime.wasm`) are one module. It is not the Bytecode Alliance VM, not a Wasmtime flag pack, and not a companion `gocvm_host`.

wasm32 has **one linear memory**. The second address space is three tag-checked pointer tables (EPT / TPT / CHPT), not a second `Memory` and not `--shared-memory`. Architecture: [docs/architecture.md](docs/architecture.md). Bytecode: [docs/wasm2-fork.md](docs/wasm2-fork.md) (WASM 2 hard fork). Host ABI: [docs/host-abi.md](docs/host-abi.md) (in-tree `WASMWin32/` / `WASMNix/` / `WASMDroid/` catalogs). Engine: [docs/wasitime.md](docs/wasitime.md). Product: [docs/wasigocvm.md](docs/wasigocvm.md).

```
Go++ source
    │
    ▼
wasigoc                          frontend (this repo)
    │
    ▼
compile.bat / wasigocvm.bat      -DWASIGO_GOCVM=1
    │                            our sysroot, full libc++ EH/RTTI
    ▼
.wasigocvm.wasm                  Oilpan · libc · OpenSSL · EPT/TPT/CHPT
    │                            WASMWin32 · WASMNix · WASMDroid
    │                            WASMSafeSpace · WASMv8bindings
    ▼
wasitime                         our runtime (wasmtime-shaped CLI)
                                 wazgoc interpreter, not Cranelift
```

`legacy.bat` remains for **stock wasip1 noeh** goldens only. That is not the product.

This is not `gc` and not a Go runtime port. The design is a **Rosetta**: keep the shape of the Go++ source, spell each construct as the C++ feature that is actually strong on WASM. See [docs/language.md](docs/language.md). Handwritten Go++ is `.go`; TypeScript converted by [WASMBruja](../WASMBruja)'s `brujac` is `.goxx`. `wasigoc` accepts both.

---

## What's new in Wasigoc2

Wasigoc2 is the machine, not a compiler bump. The first `wasigoc` shipped a Go++ frontend and stock wasip1 noeh goldens. This tree adds the in-module VM, the interpreter, and the three host catalogs.

| Area | In this tree |
| --- | --- |
| **Machine** | `src/wasigocvm_aspace.hpp` (cage + EPT/TPT/CHPT), `wasigocvm_exec.hpp` (table-named child), `wasigocvm_libc.hpp` (in-guest syscall / catalogs), `wasigocvm_net.hpp` (poll sockets), `wasigocvm_tls.hpp` (OpenSSL memory BIO), `wasigocvm_config.hpp` (`-DWASIGO_GOCVM=1`) |
| **Sysroot** | `toolchain/` — our libc / eh libc++ / mman / getpid; OpenSSL 3 wasm in `toolchain/openssl-wasm` |
| **Host ABI** | In-tree `WASMWin32/`, `WASMNix/`, `WASMDroid/` catalogs. Guest packages `stdlib/win32`, `stdlib/linux`, `stdlib/android`. Missing those three dirs is a build error. |
| **Cage / CHPT** | In-tree `WASMSafeSpace/` (EPT/TPT) and `WASMv8bindings/` (cppgc + CHPT). Missing either is a build error. |
| **Edge kernel** | In-tree `WASMGocOS/` + `stdlib/gocos` — GocKrnl / GocSys hop k32 and nix through `gocvm.Call("gocos")` |
| **Engine** | `wasitime` — wasmtime-shaped `inspect` / `run` / `call` / `link`. Compile/run slot is `examples/wazgoc` (Op stream in the cage), not Cranelift / LLVM / wazevo. |
| **Bytecode** | Hard fork of WASM 2: keep SIMD, bulk memory, reftypes, tail call, atomics, `try_table`. Cut Component Model, WIT, WASM GC (`0xfb` → `gc is oilpan`). |
| **Load/call** | `examples/wasmbinpkg` (decode / peel-to-core), `examples/wasmloaderpkg` (load / run / call / link), `examples/safespacepkg` (cage / GIA / CPT/EPT/TPT), `examples/v8bindpkg` (CHPT) |
| **Test** | `os/exec` child on EPT/TPT/CHPT (`examples/forkexec`); `syscall.Getpid` in-module (`examples/getpid`); cmd through GocOS (`examples/cmdterm`) |

Those sibling trees are now in this repo, loaded as-is (not rewritten): `WASMSafeSpace/`, `WASMv8bindings/`, `WASMGocOS/`, `WASMPELoader/`, `WASMLoader/`, `WASMLime/` (OpenSSL TlsTransport shape), `WASMJsLoader/`.

---

## Quickstart

```bash
compile.bat examples\hello\hello.go -o hello.wasm
wasitime hello.wasm
wasitime inspect hello.wasm
wasitime call examples\wasitime\add.wasm add 2 3
```

Unix: `wasigocvm.sh`, `wasitime.sh`. Needs `toolchain/` ([toolchain/README.md](toolchain/README.md)).

```go
package main

import "fmt"

func main() {
	fmt.Println("hello, wasi")
}
```

Host names are in-module catalogs, not WIT:

```go
import "win32"
id, err := win32.GetCurrentProcessId()

import "linux"
pid, err := linux.Getpid()

import "android"
api, err := android.DeviceApiLevel()
```

`os/exec`, `net`, `crypto/tls`, `syscall`, `os/user`, `win32`, `linux`, `android`, and `gocos` are libc **inside that module** — table-named exec, OpenSSL memory-BIO TLS, WASMWin32 / WASMNix / WASMDroid on EPT/TPT/CHPT. There is no host hop.

Native leftover: `goclang++.bat` emits a host `.exe` and links `gocvm_sandbox` in-process (same cage, not a companion). Stock wasip1 (`legacy.bat`) has no machine and returns a clear "not supported" where WASI cannot go. See [docs/build.md](docs/build.md).

---

## Ecosystem & names

| Name | Description |
| --- | --- |
| **Wasigoc2** | This release: compiler + wasigocvm machine + wasitime engine + in-tree catalogs |
| **Go++** | The language |
| **`wasigoc`** | The compiler. Emits C++ including `int main()`. |
| **wasigocvm** | The machine: `toolchain/` sysroot, in-guest libc, EPT/TPT/CHPT, `compile.bat`. [docs/architecture.md](docs/architecture.md) |
| **wasitime** | Runtime CLI (`inspect` / `run` / `call` / `link`). Host is `wasitime.exe`; same program as a module is `wasitime.wasm`. [docs/wasitime.md](docs/wasitime.md) |
| **GocVM** | `gocvm.Call(topic, payload)` inside the module. No companion host. |
| **`goclang++`** | Native `.exe` of the same frontend. Links **gocvm_sandbox** in-process. |
| **gocvm_sandbox** | WASMSafeSpace cage on the native leftover path. Not a hop from wasm. |

---

## Machine (wasigocvm)

Product gate: `-DWASIGO_GOCVM=1`. A clang binary may still be named `wasm32-wasip2-clang++` — that is a compiler *filename*, not the ABI.

**Second address space** is three tables, not a second `Memory`:

| Table | Artifact | Names |
| --- | --- | --- |
| **EPT** | WASMSafeSpace `ExternalPointerTable` | exec stdout, Win32/Nix/Droid catalogs, vmem / sock / CNG / modules |
| **TPT** | WASMSafeSpace `TrustedPointerTable` | exec parent, process/thread tokens |
| **CHPT** | WASMv8bindings `CppHeapPointerTable` | cppgc `ExecChild`, Win32Kernel, NixKernel, DroidKernel, sessions |

Isolation is the handle. `table.Get` with the matching tag is the object; a handle that does not check out is not that object.

**Host ABI** is three in-tree catalogs, always on in gocvm — not WASI, not WIT:

| Tree | Guest package | `gocvm.Call` topic | Entry |
| --- | --- | --- | --- |
| `WASMWin32/` | `win32` | `win32` | `wasmwin32::wasi_call` |
| `WASMNix/` | `linux` | `linux`, `wsl`, `nix` | `wasmnix::posix_call` |
| `WASMDroid/` | `android` | `android`, `binder`, `kvm` | `wasmdroid::bionic_call` |

Capability is a `catalog.cc` row plus `*_tables_ok()` (CHPT session, TPT process/thread, EPT catalog). `CreateProcessW` / `os/exec` is a `std::thread` child named on those tables; work is `wasi_call`, not a BusyBox table and not a host `cmd.exe`. PE maps through `WASMPELoader/`; MainDLL/TLS is `WHvRunVirtualProcessor`.

**TLS** is OpenSSL 3 wasm (`toolchain/openssl-wasm`), memory BIOs, same shape as WASMLime `TlsTransport`. Not Schannel.

**GocOS** is the edge kernel (`import "gocos"`). Binding is `gocvm.Call("gocos")` on gocvm.wasm. GocKrnl / GocSys hop **k32** (`WASMWin32/`) and **nix** (`WASMNix/`). Not an emulator and not Wine.

---

## Engine (wasitime)

`wasitime` sits in the same *place* Cranelift fills in Wasmtime: CLI in, execute a module. It is an interpreter of this WASM 2 fork, not ISA codegen.

```
wasitime inspect <file.wasm>
wasitime run     <file.wasm> [--] [argv...]
wasitime call    <file.wasm> <export> [args...]
wasitime link    name=file.wasm ... [--call] module.export [args...]
wasitime example <add|double|hello|plugin>
wasitime <file.wasm>                 same as run
```

| Package | Job |
| --- | --- |
| `examples/wasitime` | CLI |
| `examples/wasmloaderpkg` | load / run / call / link |
| `examples/wasmbinpkg` | decode; peel outer wrapper → nested core |
| `examples/wazgoc` | compile wasm → `Op[]` in the cage; interpret |
| `examples/safespacepkg` | cage, GIA, CPT/EPT/TPT |
| `examples/v8bindpkg` | CHPT |

Not a vendor dump of tetratelabs/wazero. Not w2g. Not Wasmtime. Outer Component Model wrappers are discarded; only the nested **core** module is instantiated. `inspect` prints core magic `00 61 73 6d 01 00 00 00`.

---

## Language & library

* **Frontend:** Recursive-descent Go frontend with automatic semicolon insertion.
* **Type identity:** Interned `go/types` (pointer equality) and matching C++ codegen — methods on defined types (`type Duration int64`), generic named types (`type Set[T any] struct`), named array/slice types, anonymous `interface{ M() }`, range-over-func.
* **Modules & scope:** Packages as C++ namespaces; `go.mod` `replace` directives and `internal/` access rules.
* **Standard library:** Builtins `fmt`, `errors`, `os`, `reflect`, `gocvm` plus compiled packages under `stdlib/` (public `go list std` minus `internal/`/`vendor/` and target-impossible APIs). Extensions: `win32`, `linux`, `android`, `gocos`, `unil`, `guac`, `ogchan`, `liveview`, `websocket`. On wasigocvm, `net`, `os/exec` (`LookPath` included), `os/user`, `syscall`, `crypto/tls`, and the catalog packages are in-module. [docs/stdlib.md](docs/stdlib.md).
* **Concurrency:** Cooperative `go`/`chan`/`select` on C++20 coroutines. `Chan` matches Go's concurrent-use guarantee; `Map` panics on detected concurrent writes.
* **GC:** Oilpan (`cppgc`) — `GarbageCollected<T>`, `Member<T>`, `Persistent<T>`, stop-the-world mark-sweep. CHPT names cppgc objects in the cage. Not a Go collector clone.
* **TLS:** OpenSSL 3 wasm (`toolchain/openssl-wasm`), memory BIOs. Not Schannel.
* **Codecs & hashes:** `flate`/`gzip`/`zlib`/`bzip2`/`lzw`, `PNG`/`JPEG`/`GIF`, SHA-2/3, AES-128, P-256, Ed25519, with documented bounds.

Language surface: [docs/language.md](docs/language.md). Compiler-bug diary: [docs/design-log.md](docs/design-log.md).

---

## Examples

Product-facing programs under `examples/` (stdlib goldens stay one-package-per-dir as before):

| Example | What it is |
| --- | --- |
| `hello` | `fmt.Println` through wasigocvm + wasitime |
| `netpkg` / `httppkg` | in-module sockets / HTTP/1.0 |
| `getpid` | `syscall.Getpid` via in-guest libc |
| `forkexec` | `os/exec` child on EPT/TPT/CHPT (`echo` / `true` / `uname`) |
| `win32` | WASMWin32 catalog (`GetCurrentProcessId`, `WslList`, …) |
| `linuxpkg` | WASMNix `getpid` / `uname` |
| `droidpkg` | WASMDroid Bionic / API level |
| `gocospkg` | GocOS boot + `RtlGetVersion` |
| `cmdterm` | `cmd.exe` through GocOS + k32 |
| `wasitime` | engine CLI |
| `wasmloader` | load / call / inspect through wazgoc |
| `safespace` | cage + EPT/TPT/CPT port |
| `wteng` | wasmtime-shaped embedding smoke (product load is wasmloaderpkg) |
| `electron` | Electron-shaped guest packed as a `unil` bundle |
| `ogchan` | Open Graph document channel |
| `liveviewpkg` | LiveView language: HTML+CSS+JS-events+templates as one `live` document |
| `websocketpkg` | gorilla/websocket port (RFC 6455 frames, Upgrader, Dialer) |

---

## Build instructions

CMake 3.16+, C++20 (MSVC or clang). `wasigoc` is a **host** binary. The machine is `compile.bat` + `toolchain/`. `WASMWin32/`, `WASMNix/`, and `WASMDroid/` must be present.

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

* **MSVC note:** pass `-C Debug` to `ctest`.
* **wasigocvm goldens:** `hello_wasigocvm`, `netpkg_wasigocvm`, `httppkg_wasigocvm`, `getpid_wasigocvm`, `linuxpkg_wasigocvm`, `droidpkg_wasigocvm`, `gocospkg_wasigocvm`, `ogchan_wasigocvm`, `electron_wasigocvm` (full libc++, `-DWASIGO_GOCVM=1`).
* **Stock wasip1 goldens** (`*_golden`) still use wasi-sdk noeh via `legacy.bat`.

Details: [docs/build.md](docs/build.md), [toolchain/README.md](toolchain/README.md).

---

## Directory layout

```text
src/                 wasigoc frontend + runtime.hpp + wasigocvm_*.hpp
stdlib/              Go++ standard library (ordinary .go)
  win32/ linux/      guest projections of the in-tree catalogs
  android/ gocos/
  ogchan/ unil/ guac/
toolchain/           our sysroot, OpenSSL wasm, wasigocvm stamp
WASMWin32/           in-tree catalog + wasi_host.hpp (required)
WASMNix/             in-tree catalog + posix_host.hpp (required)
WASMDroid/           in-tree catalog + bionic_host.hpp (required)
examples/
  wasitime/          engine CLI
  wazgoc/         WASM 2 interpreter (Cranelift-slot)
  wasmbinpkg/        decode / peel-to-core
  wasmloaderpkg/     load / run / call / link
  safespacepkg/      cage + CPT/EPT/TPT
  v8bindpkg/         CHPT
  forkexec/ win32/   exec goldens
  linuxpkg/ droidpkg/ gocospkg/ cmdterm/
compile.bat          default: wasigocvm machine
wasigocvm.bat / .sh  same machine as compile.bat
legacy.bat           stock wasip1 noeh leftover
wasitime.bat / .sh   our runtime CLI
goclang++.bat        native leftover .exe
docs/                architecture, wasm2-fork, wasitime, host-abi, …
tests/               smoketest + wasm golden harness
```

---

## Documentation index

| Document | Contents |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | wasigocvm: one Memory, EPT/TPT/CHPT, in-module libc |
| [docs/wasigocvm.md](docs/wasigocvm.md) | product: sysroot, libc, wasitime — vs Bytecode Alliance |
| [docs/wasm2-fork.md](docs/wasm2-fork.md) | WASM 2 hard fork: opcodes kept, Oilpan/cage instead of WASM GC |
| [docs/wasitime.md](docs/wasitime.md) | engine: wasmtime-shaped CLI, wazgoc interpreter |
| [docs/host-abi.md](docs/host-abi.md) | in-tree WASMWin32 / WASMNix / WASMDroid catalogs |
| [docs/language.md](docs/language.md) | Go++ syntax, Rosetta, modules |
| [docs/stdlib.md](docs/stdlib.md) | builtins, wasigocvm in-guest packages, n/a |
| [docs/build.md](docs/build.md) | CMake, ctest, wasigocvm vs stock wasip1 |
| [docs/wasip2.md](docs/wasip2.md) | retired — wasip2 is not the product |
| [docs/design-log.md](docs/design-log.md) | tracker + compiler-bug diary |
| [CONTRIBUTING.md](CONTRIBUTING.md) | names, goldens, style |
| [SECURITY.md](SECURITY.md) | what this is not; gocvm_sandbox ABAC |
