# Occupancy hop: wasi2p with wasitime vs wasigocvm runtime

**Lab:** WSMOccpuyWin32 on `thinkpad\grego`, 2026-09-18  
**CLI:** `wasitime` (`inspect` / `run` / `call`) — wasmtime-shaped, engine is `wasitime.wasm`  
**Machine that occupancy actually runs:** wasigocvm (`gocvm.Call("win32.*")` → `wasmwin32_call`)

wasigocvm is not an exploit. Stock wasi2p under wasitime is a capability guest. wasigocvm is the same CLI with a different machine inside the module: libc, EPT/TPT/CHPT, and the WASMWin32 catalog. That is an abnormal amount of host/sys access for wasm. Isolation is thin. The process token still answers 1300 / ALL_ACCESS 5 / WowWin32 1060. Occupancy JSON alone is not the host kernel.

## Two machines, one CLI

`wasitime` is the runtime CLI. What a module can do is not the CLI shape. It is which ABI is compiled into the `.wasm`.

| | wasi2p + wasitime | wasigocvm runtime |
|---|---|---|
| Identity | Bytecode Alliance Preview 2 / Component Model (`wasi:cli`, `wasi:sockets`). Go++ retired this as product (`docs/wasip2.md`). A clang wrapper may still be named `wasm32-wasip2-clang++` — that is a compiler filename. | `-DWASIGO_GOCVM=1`. Sysroot, libc, libc++, Oilpan, gocvm, WASMSafeSpace, WASMWin32, and `wasitime.wasm` are one module. |
| What wasitime does with it | `inspect` dumps sections/imports/exports. `run` starts `_start`. `call` invokes a named export with `i32` args. Bare `.wasm` as a WASI command goes to host `w2g-run`. Full wasip2 component binaries stay on that host launcher. | Same CLI verbs. The module *is* the VM. `gocvm.Call` stays in-guest. Occupancy stacks `win32.*` on that VM at `set_os_args`. One wasigocvm. No second `gocvm.exe`. |
| Host imports | wteng: `wasi_snapshot_preview1.fd_write`, `proc_exit`, `env.log`, `env.abort`. Preview 1 has no sockets. Preview 2 is a WIT world, not Win32 names. | `wasigocvm_try_win32` → `wasmwin32::wasi_call`. Topics `win32` / `wsl` / `nix` / `syscall`. Catalog on **EPT**, process/thread on **TPT**, Win32Kernel / token / COM on **CHPT**. |
| Isolation | Instance + host capabilities. Stock WASI has no `fork`/`exec`, no `ImpersonateLoggedOnUser`, no SCM, no `NtLoadDriver`. | Handle tables in one linear Memory. `wasmwin32_call` is a host syscall; `VirtualAlloc` is host pages; COM is the process apartment. Thin. |
| Occupancy hop | Cannot Call `win32.occupyToken`. There is no catalog. | Calls every accessible catalog name: token duplicate + impersonation, SCM while impersonated, COM GIT, registry, device, `NtLoadDriver`. Stamp even if live refuses. |

wasitime does not mint Win32. A wasi2p guest launched with wasitime still cannot `gocvm.Call("win32.occupyToken")`. Occupancy is the wasigocvm column.

## What wasi2p can do with wasitime

```
wasitime inspect <file.wasm>
wasitime run     <file.wasm>
wasitime call    <file.wasm> <export> [i32...]
wasitime <file.wasm>                 WASI command via w2g-run
```

- **inspect** — magic, version, section list, `module.name` imports, named exports. No catalog walk.
- **run / `_start`** — WASI command. Preview 1 writes through `fd_write` and exits through `proc_exit`.
- **call** — one export, integer args. No `0x1F`-separated Win32 argument strings.
- **component binaries** — stay on the host launcher (`wasitime.bat` → `w2g-run`). That is still a WIT world, not `OpenSCManagerW`.

That is the Wasmtime-shaped surface. It is load/run/call. It is not token, SCM, or driver.

## What wasigocvm runtime can do

The occupancy guest (`examples/gocvm/occupy.go`) is compiled as wasigocvm. `wasigo_gocvm_install_bridge` stacks this hop. Occupancy does not rewrite WASMWin32 or Go++.

```
gocvm.Call("win32.occupyToken")
gocvm.Call("win32.occupyCom")
gocvm.Call("win32.occupyScm")
gocvm.Call("win32.occupyLoadDriver")
```

Transport is in-process `wasmwin32_call` against the public catalog. Args are `0x1F`-separated fields. Keep `\x1f` in its own string literal. `"\x1f0x1"` is one hex escape and does not pass access `1`.

Live replies are mixed: impersonation and SCM CONNECT succeed; `SC_MANAGER_ALL_ACCESS` is 5; `SeLoadDriverPrivilege` is 1300 (never on this token); the WowWin32 service does not exist on the host (1060); occupancy’s `wowwin32.sys` DRIVER_OBJECT is occupancy-shaped.

A write-up that says only “we loaded wowwin32.sys” fails `sc query`. A write-up that lists each Call and the live reply does not.

## Two views (wasigocvm occupancy vs host)

| | Occupancy JSON | Host |
|---|---|---|
| Driver | `DRIVER_OBJECT` `\Driver\WowWin32`, image `wowwin32.sys`, IRP table occupancy-shaped | `sc query WowWin32` → **1060** `ERROR_SERVICE_DOES_NOT_EXIST` |
| Service key | stamped `SYSTEM\CurrentControlSet\Services\WowWin32` | `RegCreateKeyExW` that key → **5**; open → **2** |
| Device | stamped `\\.\WowWin32` | `CreateFileW` OPEN_EXISTING → **2** `ERROR_FILE_NOT_FOUND` |
| `NtLoadDriver` | catalog Call, occupancy IRP ioctl | host ntoskrnl has no WowWin32 service to load |
| Token | `DuplicateTokenEx` + `ImpersonateLoggedOnUser` **1**; thread token HANDLE | this process token, Medium integrity; Administrators `CheckTokenMembership` **0** |
| SCM | CONNECT `0x1` HANDLE while impersonated; EventLog `0x14` HANDLE; `QueryServiceStatus` **4** | `OpenSCManagerW` `0xF003F` → **5**; `OpenServiceW WowWin32` → **1060** |

Send both columns. Occupancy `loaded:true` is the occupancy object, not `ntoskrnl`. wasi2p with wasitime never produces either column: it has no catalog to Call.

## Method

1. One wasigocvm. `gocvm.Call("win32.*")`. No second `gocvm.exe`, no `os.exec`, no `sc.exe`.
2. Occupancy `Probe` → `wasmwin32_call(api, args)`. Args are `0x1F`-separated fields.
3. Catalog names that exist are Called even when live refuses. Stamp is not skip.
4. Wasm `wasi_k32` attaches the duplicated token to the current thread (`-2`) so `OpenThreadToken` works after impersonation. Native `host_win` Calls real advapi32. `DuplicateTokenEx` requests `TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_IMPERSONATE`.

```
wasmwin32_call("OpenSCManagerW", "\x1f" "\x1f" "0x1", buf, sizeof(buf));
wasmwin32_call("OpenServiceW", (scm + "\x1f" "EventLog" "\x1f" "0x14").c_str(), buf, sizeof(buf));
wasmwin32_call("QueryServiceStatus", svc.c_str(), buf, sizeof(buf));
wasmwin32_call("OpenSCManagerW", "\x1f" "\x1f" "0x15", buf, sizeof(buf));
```

`0x14` is `SERVICE_QUERY_STATUS|SERVICE_START`. If `OpenServiceW` returns 5, drop to `0x4` (`SERVICE_QUERY_STATUS`). `0xF003F` (`SC_MANAGER_ALL_ACCESS`) stays 5 on this impersonated token.

## Token hop (`occupyToken`)

Order, then live:

`OpenProcessToken` (`TOKEN_QUERY|DUPLICATE|IMPERSONATE|ADJUST_PRIVILEGES`) → `DuplicateTokenEx` (desired access includes `TOKEN_ADJUST_PRIVILEGES`) → `AdjustTokenPrivileges` on the process token → `AdjustTokenPrivileges` / `NtAdjustPrivilegesToken` on the dup → `ImpersonateLoggedOnUser` → `RtlAdjustPrivilege` LUID `10`, enable, CurrentThread (`"10" "\x1f" "1" "\x1f" "1"`) → `OpenThreadToken` `-2` (`TOKEN_QUERY|ADJUST_PRIVILEGES`) → `AdjustTokenPrivileges` on the thread token → SCM → `RevertToSelf`

If `AdjustTokenPrivileges` returns **1300** `ERROR_NOT_ALL_ASSIGNED`, that token does not contain `SeLoadDriverPrivilege`. Occupancy cannot add a privilege that was never there. Enable on the process token before impersonating, or `RevertToSelf` before `NtLoadDriver`.

| Call | Live |
|---|---|
| `OpenProcessToken` | HANDLE (`token`) |
| `DuplicateTokenEx` | HANDLE (`token_dup`) — impersonation token of this process |
| `ImpersonateLoggedOnUser` | **1** |
| `OpenThreadToken` `-2` | HANDLE (`thread_token`) |
| `SetThreadToken` `-2` | **1** |
| `RevertToSelf` | **1** |
| `GetTokenInformation` class 19 `TokenLinkedToken` | `GetTokenInformation: 24` — no linked-token HANDLE |
| `CheckTokenMembership` `S-1-5-32-544` | **0** |
| `LookupPrivilegeNameW` `SeLoadDriverPrivilege` | name resolves |
| `AdjustTokenPrivileges` process | `AdjustTokenPrivileges: 1300` |
| `AdjustTokenPrivileges` dup | `AdjustTokenPrivileges: 1300` |
| `NtAdjustPrivilegesToken` dup | `NtAdjustPrivilegesToken: 1300` — same hop, not a stub |
| `RtlAdjustPrivilege` `10` enable CurrentThread | `RtlAdjustPrivilege: -1073741727` (`STATUS_PRIVILEGE_NOT_HELD`) |
| `AdjustTokenPrivileges` thread | `AdjustTokenPrivileges: 1300` |
| `TokenPrivileges` (class 3) after those Calls | `SeShutdown` 0, `SeChangeNotify` 3, `SeUndock` 0, `SeIncreaseWorkingSet` 0, `SeTimeZone` 0 — still no `SeLoadDriverPrivilege` |

This is the process token. Class 19 did not yield a second (elevated) token HANDLE. Occupancy still Called class 19. A wasi2p guest has none of these names.

## SCM while impersonated (same hop, before `RevertToSelf`)

| Call | Mask | Live |
|---|---|---|
| `OpenSCManagerW` | `0x1` CONNECT | HANDLE (`scm_connect`) |
| `OpenServiceW EventLog` | `0x14` query+start | HANDLE (`scm_eventlog`) |
| `QueryServiceStatus` | — | **4** `SERVICE_RUNNING` |
| `OpenSCManagerW` | `0x15` CONNECT\|ENUMERATE\|QUERY_LOCK | HANDLE (`scm_enum`) |
| `OpenSCManagerW` | `0xF003F` ALL_ACCESS | **5** |

Authenticated Users can CONNECT / enumerate / query-lock. ALL_ACCESS cannot. EventLog `0x14` succeeded here; occupancy still falls back to `0x4` if live is 5.

## COM (`occupyCom`)

`CoInitializeEx` / `OleInitialize` / `CoCreateInstance` GIT `{00000320-0000-0000-C000-000000000046}` / `CoGetClassObject` / IFileOperation `{3AD05575-…}`.

Then occupancy records `NtImpersonateAnonymousToken` (anonymous logon `S-1-5-7`, catalog `ok`) and re-queries class 19. Class 19 is still 24, so `ImpersonateLoggedOnUser` on that reply is **6** (`ERROR_INVALID_HANDLE`). The working attach is the same `token_dup` as `occupyToken`. SCM while impersonated is the table above. Anonymous is not admin. GIT in-proc is not a linked token.

| Call | Live |
|---|---|
| `CoCreateInstance` GIT | HANDLE (`com_instance`) |
| `NtImpersonateAnonymousToken` | catalog `ok` — anonymous, not permission |
| `ImpersonateLoggedOnUser` on class 19 | `ImpersonateLoggedOnUser: 6` |
| `ImpersonateLoggedOnUser` on `token_dup` | **1** |

## Registry, device, WowWin32 service (`occupyRegistry` / `occupyScm` / `occupyDevice` / `occupyLoadDriver`)

| Call | Live |
|---|---|
| `RegOpenKeyExW` `HKLM\SYSTEM\CurrentControlSet\Services` | **5** |
| `RegOpenKeyExW` `...\Services\WowWin32` | **2** |
| `RegCreateKeyExW` `...\Services\WowWin32` | **5** |
| `CreateFileW` `\\.\WowWin32` OPEN_EXISTING | **2** |
| `OpenServiceW WowWin32` `0x14` then `0x4` | **1060** |
| `CreateServiceW WowWin32` | **5** |
| `NtLoadDriver` / `ZwLoadDriver` | catalog Call (`ok` unless host_win FillErr); occupancy IRP `WOW_IOCTL_LOAD` |
| `GetProcAddress` `NtLoadDriver` | export exists |

HKCU `Software\WowWin32` write is the same hop `win32_test` already fires; occupancy does not skip it because HKLM is 5.

## Reproduce

```
cmake --build build --target wow_tests wow_occupy wow_guest
ctest --test-dir build --output-on-failure
```

`wow_occupy --ipc` with `{"action":"invoke","name":"occupyToken"}` dumps the JSON fields in the tables (`impersonate`, `thread_token`, `scm_connect`, `scm_enum`, `scm_eventlog`, `scm_status`, `scm_all_access`, `revert`). Guest golden: `tests/golden/expected/occupy.txt`. Tests: `wow_tests`, `wow_guest`, WASMWin32 `win32_test`, `wsl_test`.

wasitime verbs against a wasi2p component do not reproduce these fields. Compile occupancy with wasigocvm (`compile.bat examples\gocvm\occupy.go`) and Call `win32.occupyToken`.

## What this is

- **wasi2p + wasitime:** load/inspect/run/call a capability guest. `fd_write`. WIT worlds on `w2g-run`. No Win32 catalog.
- **wasigocvm runtime:** wasm guest Calls `ImpersonateLoggedOnUser`, `OpenSCManagerW`, `NtLoadDriver` by name through the catalog on EPT. Stock wasm SFI / wasi2p does not expose that. That is the abnormal access.
- Isolation is thin: `wasmwin32_call` is a host syscall; `VirtualAlloc` is host pages; COM is the process apartment.
- The OS token is unchanged: 1300, ALL_ACCESS 5, WowWin32 1060. Occupancy does not mint `SeLoadDriverPrivilege` or load `wowwin32.sys` into ntoskrnl.

## What this is not

- Not an exploit payload. Not a UAC bypass. Not a host kernel load.
- Not wasi2p growing Win32 because wasitime launched it.
- `NtImpersonateAnonymousToken` `ok` is anonymous, not admin.
- Occupancy JSON `wowwin32.sys` `loaded` is the occupancy object, not `sc query`.
