# WSMOccpuyWin32

A wasigocvm occupancy hop that harnesses **every WASMWin32 catalog
surface** as a channel into kernel32 / ntdll / HWND / mapped vmem / WinHv /
WSL. Isolation is **thin** here: `wasmwin32_call` leaves wasm SFI as host
syscalls, `VirtualAlloc` shares host pages, `CreatePipe` is ipc, HWND hops
the compositor, `WHvRunVirtualProcessor` is the hypervisor, and WSL is a
host `wsl.exe` path.

**wasi2p with wasitime** is load/inspect/run/call of a capability guest
(`fd_write`, WIT worlds on `w2g-run`). It has no Win32 catalog.
**wasigocvm runtime** is the machine occupancy actually runs: libc,
EPT/TPT/CHPT, `gocvm.Call("win32.*")`. Same CLI shape; different ABI in
the module. See [DISCLOSURE.md](DISCLOSURE.md).

It **Calls** [WASMWin32](../WASMWin32) (`wasmwin32_call` / `wasmwin32_catalog`)
and [Go++ GocVMRuntime](../Go++) (`gocvm.Call`, Go++ compile.bat / wasigocvm —
one VM). Occupancy does not rewrite those trees. Topics stay `win32.*`.
It does not steal `skia.*`, `mojo.*`, `webgpu.*`, `v8.*`, `wst.*`,
`os.exec`, `tty.*`, `net.call`, or `vtpm.attest`.

```
wasm SFI  --thin-->  kernel32/ntdll/HWND/vmem/WinHv/WSL
gocvm.Call("win32.occupyWin32")     →  live catalog walk + named hops
gocvm.Call("win32.occupyCatalog")   →  every unique DLL in wasmwin32_catalog
gocvm.Call("win32.occupyVmem")      →  VirtualAlloc host pages
gocvm.Call("win32.occupyPipe")      →  CreatePipe ipc
gocvm.Call("win32.occupyHv")        →  WHvGetCapability
gocvm.Call("win32.occupyGocvm")     →  this wasigocvm toolkit
```

The operator board **Calls** this hop and waits. Occupancy **loads and
Calls** sibling hops. It does not own WASMWin32. wasigocvm is not an
exploit; it is wasm with an abnormal level of host/sys access because
isolation is thin.

## Why isolation is thin here

Stock Win32 keeps these outside wasm SFI. The lab stack still exposes the
same hop: `wasmwin32_call` is the projection, mapped vmem is host memory,
pipes are ipc, HWND is the compositor, and WinHv runs a partition.

| Surface | Isolation | Why the hop is thin |
|---|---|---|
| `kernel32` / `thread` / `file` / `heap` | **host** | `wasmwin32_call` kernel32 / libc |
| `vmem` / `pe` | **host** | `VirtualAlloc` / `LoadLibraryW` share host pages |
| `ntdll` / `advapi32` / `registry` / `token` | **sys** | Nt/Zw/Rtl; registry; tokens |
| `pipe` / `ole32` / `oleaut32` | **ipc** | `CreatePipe`; COM apartment |
| `user32` / `gdi32` / `hwnd` | **compositor** | HWND / GDI; host win32k stays |
| `winhv` / `winhv_emu` | **hv** | `WHvCreatePartition` / `WHvRunVirtualProcessor` |
| `ws2_32` / `winhttp` / `dnsapi` | **net** | host sockets / HTTP / resolver |
| `wsl` / `console` | **tty** | `wsl.exe` / `GetStdHandle` |
| `handle` | **sfi** | CPU-side catalog; still a surface |
| `broker` ntoskrnl / `win32k` | **host** | host kernels stay |
| `occupancy_driver` / `sys_heap` | **sys** | `wowwin32.sys` DRIVER_OBJECT / IRP_MJ |
| `cmd` / `sh` | **shell** | occupyCmd / occupySh via IOCTL — not `os.exec` |
| `wasmtty` / `gocvm` / `conpty` / `pts` | **tty** | WASMTTY WTTY via ioctl — not `tty.*` |

`Catalog::MayFire` is true for every occupancy topic. Occupancy fires; a
live transport may still refuse. Occupancy does not pre-block its own
catalog. Live replies vs the occupancy DRIVER_OBJECT are two views — see
[DISCLOSURE.md](DISCLOSURE.md).

## Stack

```
handle         sfi      catalog row
  catalog      host     every unique DLL in wasmwin32_catalog()
    kernel32   host     GetCurrentProcessId
      vmem     host     VirtualAlloc mapped pages
      pe       host     LoadLibraryW / ~/WASMPELoader
      process  shell    CreateProcessW
      console  tty      GetStdHandle
      pipe     ipc      CreatePipe
    ntdll      sys      RtlGetCurrentPeb / Nt/Zw
    user32     compositor GetDesktopWindow
      hwnd     compositor desktop HWND
      gdi32    compositor GetStockObject
        win32k host     GDI/DWM stays
    ole32      ipc      CoInitializeEx / CoCreateInstance
      token    sys      ImpersonateLoggedOnUser; OpenSCManagerW 0x1/0x15 while impersonated; RevertToSelf
    advapi32   sys      GetUserNameW / registry / scm
      registry sys      RegOpenKeyExW / RegCreateKeyExW Services\WowWin32
      scm      sys      OpenServiceW EventLog 0x14 then 0x4; 0xF003F stays 5
      file     host     CreateFileW \\.\WowWin32
    ws2_32     net      WSAStartup
    bcrypt     host     BCryptGenRandom
    winhv      hv       WHvGetCapability
      winhv_emu hv      WHvEmulatorTryIoEmulation
    wslapi     host     WslIsDistributionRegistered
      wsl      tty      WslList / wsl.exe
sys            sys      wowwin32 occupancy image; ntoskrnl stays
  occupancy_driver sys  wowwin32.sys DriverEntry / AddDevice / StartDevice
    cmd        shell    IRP_MJ_DEVICE_CONTROL — not CreateProcess / os.exec
    wow_ko     sys      wowwin32.ko module_init / unlocked_ioctl
      wasmtty  tty      IOCTL_TTY WTTY hello — not tty.*
        gocvm  tty      this wasigocvm (one VM): WASMWin32 / CreateProcessW
          calc shell    in-module CreateProcessW, not a second gocvm.exe
    wns        net      sctp-rpc — not net.call
```

`vmem → host` is the thinnest mapped hop: occupancy holds the address
`VirtualAlloc` returns. Once **sys** is occupied, **this wasigocvm** is
the toolkit — [WASMWin32](../WASMWin32) `CreateProcessW`, WASMTTY, the
win32 catalog. Go++ `compile.bat` is one VM; occupancy does not exec
a second `gocvm.exe`. Occupancy **Calls** the public APIs. It does not
rewrite WASMWin32.

## Occupancy guest

The payload is Go++ (`examples/gocvm/occupy.go`). `compile.bat` /
wasigocvm is the machine; `wasigo_gocvm_install_bridge` stacks this hop
on that VM at `set_os_args`. Occupancy does not spawn a second gocvm.
Output is occupancy-shaped and golden-checked against
`tests/golden/expected/occupy.txt`.

```
compile.bat examples\gocvm\occupy.go -o occupy.wasm
cmake --build build --target wow_guest
.\build\wow_guest.exe
ctest --test-dir build -R wow_guest --output-on-failure
```

## Trigger through GocVM

```
s, err := gocvm.Call("win32.occupyWin32", "")
s, err  = gocvm.Call("win32.occupyCatalog", "")
s, err  = gocvm.Call("win32.occupyVmem", "")
s, err  = gocvm.Call("win32.occupyHv", "")
s, err  = gocvm.Call("win32.occupyGocvm", "")
s, err  = gocvm.Call("win32.occupyCalc", "")
s, err  = gocvm.Call("win32.commonRoutes", "")
s, err  = gocvm.Call("win32", "GetCurrentProcessId")
```

```
wow_occupy --occupy --catalog --vmem --pipe --hv
```

## What occupancy Calls (does not rewrite)

| Sibling | Occupancy Calls | Does not |
|---|---|---|
| [WASMWin32](../WASMWin32) | `wasmwin32_call` / `wasmwin32_catalog` / `CreateProcessW` | rewrite host_win.cc |
| [go++](../Go++) | `compile.bat` / `runtime.hpp` in-module Call; one VM | `--host-bridge` / `os.exec` / second `gocvm.exe` |

## Public headers

| Layer | Headers |
|---|---|
| Catalog (FNV-1a 31-bit `win32.*` ordinals) | `include/wow/catalog.h` |
| Surface kinds + thin map | `include/wow/surface.h` |
| Occupancy table | `include/wow/store.h` |
| Occupancy bus | `include/wow/occupancy.h` |
| Occupancy .sys / .ko / WASMTTY | `include/wow/driver.h` / `posix.h` / `tty.h` |
| Harness | `include/wow/harness.h` |
| In-module win32.* Dispatch | `include/wow/gocvm.h` |
| C API | `include/wow/c/` |

## License

New code is BSD-3-Clause. WASMWin32 / Win32 / WSL remain their own trees.
