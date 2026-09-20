# WASMNix

A **Linux / WSL / Nix projection** for wasigocvm — not a dump of
`unistd.h`, and not a rewrite of glibc. Names, libraries, and
fd/RAII shape come from the public catalogs:

- Linux man-pages `syscalls(2)` / POSIX.1: https://man7.org/linux/man-pages/man2/syscalls.2.html
- WSL Linux-side hop: https://github.com/microsoft/WSL (`wslpath`, `wslinfo`, `WSLENV`, DrvFs, interop)
- Nix CLI: https://nixos.org/manual/nix/stable/command-ref/
- Hypervisor uapi (the Linux analog of WinHvPlatform / WinHvEmulation):
  `include/uapi/linux/kvm.h`, `vfio.h`, `vhost.h`, `mshv.h`

This is the Linux counterpart of `~/WASMwin32`. That tree projects
`Windows.Win32.*` plus `wsl.exe` / Nix-inside-WSL. This tree projects
Linux names on the WSL/Linux hop; Nix is the `nix` binary **on that
hop**, not a second Win32 stack.

wasm32 cannot `syscall()` into a Linux kernel. Query APIs that libc
already has (`getpid`, `getenv`, `getcwd`, `gethostname`, `uname`,
`stat`, clocks, sleep) run **inside the wasm module** via
`posix_host.hpp` — same `wasmnix_call` names, no companion host
process. `fork` / `posix_spawn` is a `std::thread` child on wasm and
real `popen` / exec on native Linux. `dlopen` loads named modules this
hop implements. `KVM_CREATE_VM` / `KVM_RUN` / `KvmEmulateIo` is the
in-module hypervisor hop (`kvm_host.hpp`) — GPA map, vCPU registers,
HLT/IN/OUT decode, IO/MMIO emulate. The mapper does not JIT x86.

```
wasigocvm guest (wasm32)
linux.getpid()   →  gocvm.Call("linux", ...)
                         →  wasmnix::posix_call()
                         →  getpid / getenv / uname / stat
linux.WslPath()  →  wslpath (WSL) or /mnt/c mapping
linux.NixVersion →  nix --version  (honest error if absent)
```

Native Linux/WSL tests hit libc and the nix / wslpath binaries with no
wasm in between. Native Windows trampolines the same names through
`wsl.exe -e` so the Linux catalog still runs on WSL.

## Layout

```
include/nix/dispatch.h     wasmnix_call — one entry, man-pages names
include/nix/catalog.h      Linux.POSIX.* + Linux.Syscall.* + WSL + Nix
include/nix/posix_host.hpp libc backend (compiled into wasigocvm guests)
include/nix/kvm_host.hpp   KVM / VFIO / vhost / MSHV hop (WinHv analog)
include/nix/kernel_host.hpp vmlinux EXPORT_SYMBOL hop (ntoskrnl analog)
src/catalog.cc             the table (tools/gen_catalog.py)
src/host_linux.cc          real libc / nix / wslpath on Linux and WSL
src/host_wsl.cc            SearchPath(wsl.exe), trampoline Linux names
src/host_wasi.cc           wasmnix_call wrapping posix_host.hpp
tests/linux_test.cc        pid, uname, cwd, mkdir, malloc, catalog
tests/wsl_test.cc          WslIsWsl / WslPath / WslInfo / WslExec (skip if absent)
tests/nix_test.cc          NixVersion (skip if absent)
tests/kvm_test.cc          CREATE_VM / RUN(hlt) / IO emulate / MSHV / VFIO / vhost
tests/kernel_test.cc       kmalloc / kallsyms / kvm_read_guest / irq / bpf / tun
examples/hello/            catalog dump + pid + wsl/nix probe
```

First hop plus the unistd / stat / stdlib / time slice that libc or
`std::thread` can back (`getenv`, `mkdir`, `clock_gettime`,
`posix_spawn`, …). Broader coverage still comes from man-pages and
the Nix command-ref — same `wasmnix_call` names, more rows in the
catalog. The hypervisor hop is complete against `kvm.h` ioctls,
`KVM_EXIT_*`, guest_memfd / nested / TDX exits, VFIO, vhost, and
MSHV (WSL2 / Azure Linux analog of WinHvPlatform). The kernel hop
(`Linux.Kernel.*`) is the ntoskrnl analog: slab (`kmalloc`), mm
(`ioremap`, `copy_to_user`), irq, sched/kthread, workqueue, module/
kallsyms, `kvm_host` internals (`kvm_read_guest`, `gfn_to_hva`),
PCI/DMA/IOMMU, plus bpf / netlink / tun / ptrace / prctl / io_uring
uapi.

## Build (native Windows, trampoline into WSL)

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
build/nix_example.exe
```

## Build (inside WSL / Linux)

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## wasigocvm

`posix_host.hpp` is compiled into the wasm module (`-I ~/WASMnix/include`).
Query APIs and `WslList(uname)` need no companion host. Nix still fails
on WASI (no exec).

## License

New code is BSD-3-Clause. Linux / WSL / Nix remain their upstream APIs;
this repo does not vendor kernel headers or the Nix sources.
