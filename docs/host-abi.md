# Host ABI: WASMWin32 / WASMNix / WASMDroid

**The runtime VM’s host ABI is three named catalogs, always on in
gocvm — not WASI, not WIT, not a native/wasm split.** Guest Go++ talks
`gocvm.Call(topic, payload)`. The machine requires EPT / TPT / CHPT to
be bound, then dispatches to in-tree `WASMWin32/`, `WASMNix/`, and
`WASMDroid/` **in this repo**. There is one in-module path:
libc backends (`wasi_host.hpp` / `posix_host.hpp` / `bionic_host.hpp`).
`host_win.cc` / `host_linux.cc` are not the ABI.

Tables and tags: [architecture.md](architecture.md). Bytecode:
[wasm2-fork.md](wasm2-fork.md). Product: [wasigocvm.md](wasigocvm.md).

## Capability is a catalog name

A capability is a row in a name table (`catalog.cc`), not a WIT
import and not a `kernel32` PE thunk compiled into wasm.

```
Go++  import "win32" / "linux" / "android"
        │
        ▼
gocvm.Call(topic, api + "\x1f" + arg)
        │
        ▼
wasigocvm_libc.hpp     win32_tables_ok / nix_tables_ok / droid_tables_ok
        │              no catalog handle → error, no call
        ▼
wasi_call / posix_call / bionic_call     headers compiled into the module
        │
        ▼
sysroot libc  (getpid, uname, getenv, …)  or in-module hop
              (CreateProcessW thread, Binder, KVM, WslExec)
```

`*_tables_ok()` is the gate: CHPT session, TPT process/thread, EPT
catalog (and extra EPT roots). A handle that fails `Get` with the
matching tag is not that object.

wasitime leftover traps (stdio, args, exit, clocks) are **not** this
ABI. Those exist so `printf` works on the interpreter host. Win32 /
POSIX / Bionic names stay in the guest module.

## Three projections

| Artifact | Guest package | `gocvm.Call` topic | Entry | Catalog |
| --- | --- | --- | --- | --- |
| `WASMWin32/` | `stdlib/win32` | `win32` | `wasmwin32::wasi_call` | win32metadata `Windows.Win32.*` + WSL |
| `WASMNix/` | `stdlib/linux` | `linux`, `wsl`, `nix` | `wasmnix::posix_call` | Linux man-pages / POSIX / WSL / Nix |
| `WASMDroid/` | `stdlib/android` | `android`, `binder`, `kvm` | `wasmdroid::bionic_call` | Bionic + Binder + Android kernel / KVM |

Same shape on every hop: **one dispatch**, **one catalog**, **one libc
backend**. gocvm always has all three; a bare `gocvm.Call("GetCurrentProcessId", "")`
walks Win32, then Nix, then Droid until a catalog row hits.

| File (per tree) | Role |
| --- | --- |
| `include/*/…_host.hpp` | Map catalog names to sysroot POSIX / in-module tables |
| `src/catalog.cc` | The capability list on EPT |

Do not rewrite those trees. Do not `DllImport` kernel32. Missing
`WASMWin32/` / `WASMNix/` / `WASMDroid/` in this repo is a build error.

## Bind on the tables

Boot (`src/wasigocvm_aspace.hpp`) allocates one cage, one cppgc heap,
one `{ept, tpt, chpt}`. Each projection gets a GarbageCollected kernel
in the cage:

```
          CHPT (session / kernel in cage)
            │
Win32Kernel / NixKernel / DroidKernel
            ├─ TPT  process, thread
            └─ EPT  catalog + extra roots
```

| Projection | CHPT | TPT | EPT |
| --- | --- | --- | --- |
| Win32 | `Win32Kernel` + token/SID/PEB/TEB/HWND/GDI/COM | process, thread | catalog, vmem, sock, CNG, modules, cert, WinHttp, WHP, heap |
| Nix | `NixKernel` + session | process, thread | catalog |
| Droid | `DroidKernel` + session | process, thread | catalog, Binder/ashmem/ion |

`CreateProcessW` / `os/exec` is the same hop: cppgc `ExecChild`
on CHPT, parent on TPT, stdout on EPT, work through `wasi_call`
(catalog basename or `WslExec`). Not `cmd.exe`, not a BusyBox table
inside `exec.hpp`.

## Guest Go++

```go
import "win32"
id, err := win32.GetCurrentProcessId()

import "linux"
pid, err := linux.Getpid()

import "android"
pid, err := android.Getpid()
```

Payload is `api` or `api + "\x1f" + arg`. Topics `wsl` / `nix` /
`binder` / `kvm` are aliases onto the Nix or Droid catalog (`WslList`,
`NixRun`, `AServiceManager_getService`, `KVM_CREATE_VM`).

Unknown names return honest `error:` strings. Missing Nix / adb /
unmapped PE is a real error, not a stub success.

## What runs

- Query APIs libc already has (`getpid`, `uname`, `getenv`, `stat`,
  clocks) run in `wasi_host.hpp` / `posix_host.hpp` / `bionic_host.hpp`.
- Process create is a `std::thread` child named on TPT.
- LoadLibrary maps catalog DLLs, `.wasm`, and PE via `WASMPELoader/`.
  MainDLL/TLS is `WHvRunVirtualProcessor` (table-named VP, no PE-to-wasm
  JIT).
- Sockets, TLS, mmap stay sysroot / OpenSSL in this module.

There is no second native ABI.

## Versus a WASI host ABI

| | WASI p1 / p2 / WIT | This VM |
| --- | --- | --- |
| Import | `fd_write`, `wasi:cli/run`, sockets world | none of those as the machine |
| Capability | preview rights / WIT interfaces | catalog row + `tables_ok()` |
| Windows | not present | `WASMWin32/` names |
| Linux | preview POSIX subset | `WASMNix/` man-pages names |
| Android | not present | `WASMDroid/` Bionic / Binder / KVM |
| Exec | no `fork`/`exec` | EPT/TPT/CHPT child + `wasi_call` |

`docs/wasip2.md` is retired as a product story.

## Source map

| Path | What |
| --- | --- |
| `src/wasigocvm_libc.hpp` | `wasigocvm_try_win32` / `_nix` / `_droid`; kernels; `*_tables_ok` |
| `src/wasigocvm_exec.hpp` | exec child; work via `wasi_call` |
| `src/wasigocvm_aspace.hpp` | cage + EPT/TPT/CHPT boot |
| `stdlib/win32` | Go++ projection |
| `stdlib/linux` | Go++ projection |
| `stdlib/android` | Go++ projection |
| `WASMWin32/` | catalog + `wasi_host.hpp` |
| `WASMNix/` | catalog + `posix_host.hpp` |
| `WASMDroid/` | catalog + `bionic_host.hpp` |
