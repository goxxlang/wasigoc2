# WASMDroid

An **Android / Bionic / Binder projection** for wasigocvm — not a dump
of `android.jar`, and not a rewrite of ART. Names, libraries, and
handle/RAII shape come from the public AOSP catalogs:

- Bionic libc: https://android.googlesource.com/platform/bionic
- Binder uapi: `include/uapi/linux/android/binder.h` / libbinder
- Android kernel extras: ashmem, ion, wakelock, lmk, binder
- Hypervisor hop (WinHv analog on Android): `kvm.h`, pKVM, Gunyah, AVF
  (Android Virtualization Framework)

This is the Android counterpart of `~/WASMwin32` and `~/WASMnix`.
Win32 projects `Windows.Win32.*`. Nix projects Linux man-pages on the
WSL hop. This tree projects **Bionic + Binder + Android kernel** on
the Android hop. WSL is not Android — the Windows host stays
in-module (optional `adb` is an honest extra when present).

wasm32 cannot `ioctl(/dev/binder)` or `ioctl(/dev/kvm)`. Query APIs
Bionic already has (`getpid`, `getenv`, `getcwd`, `uname`, `stat`,
clocks) run **inside the wasm module** via `bionic_host.hpp`. Binder
transactions, `KVM_CREATE_VM` / `KVM_RUN` / `KvmEmulateIo`, and
`kmalloc` stay in-module. The mapper does not JIT x86.

```
wasigocvm guest (wasm32)
android.getpid()              →  gocvm.Call("android", ...)
binder.AServiceManager_get()  →  in-module Binder hop
kvm.KVM_RUN                   →  GPA / vCPU decode (HLT, IN/OUT)
kernel.kmalloc                →  slab hop (ntoskrnl analog)
```

## Layout

```
include/droid/dispatch.h      wasmdroid_call — one entry
include/droid/catalog.h       Android.Bionic.* + Binder + Kernel + pKVM/AVF
include/droid/bionic_host.hpp Bionic-shaped libc backend
include/droid/binder_host.hpp Binder / ashmem / ion / properties / log
include/droid/kvm_host.hpp    KVM / pKVM / Gunyah / AVF hop
include/droid/kernel_host.hpp vmlinux EXPORT_SYMBOL hop
src/catalog.cc                the table (tools/gen_catalog.py)
src/host_linux.cc             native Linux
src/host_win.cc               native Windows (in-module + optional adb)
src/host_wasi.cc              wasm32
tests/bionic_test.cc          pid, uname, properties, catalog
tests/binder_test.cc          ServiceManager, Parcel, ashmem
tests/kvm_test.cc             CREATE_VM / RUN(hlt) / pKVM / Gunyah / AVF
tests/kernel_test.cc          kmalloc / kallsyms / kvm_read_guest
tests/adb_test.cc             AdbDevices (skip if adb absent)
examples/hello/               catalog dump + pid + binder + kvm
```

The hypervisor hop is complete against `kvm.h` ioctls, `KVM_EXIT_*`,
guest_memfd / nested / TDX exits, VFIO, vhost, MSHV, plus Android
pKVM / Gunyah / AVF. The kernel hop (`Android.Kernel.*`) is the
ntoskrnl analog: slab (`kmalloc`), mm (`ioremap`, `copy_to_user`),
irq, sched/kthread, workqueue, module/kallsyms, `kvm_host` internals
(`kvm_read_guest`, `gfn_to_hva`), PCI/DMA/IOMMU, binder/ashmem/lmk,
plus bpf / netlink / tun / ptrace / prctl / io_uring uapi.

## Build

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Native Linux (including WSL) uses Bionic-shaped libc. Native Windows
is in-module. `AdbDevices` is an honest extra (`adb` on PATH).

## License

New code is BSD-3-Clause. Android / Binder / KVM remain their upstream
APIs; this repo does not vendor AOSP trees or kernel headers.
