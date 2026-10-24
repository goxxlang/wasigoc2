# WASMGocOS

**GocOS is the wasigocvm edge kernel.** The runtime is `gocvm.wasm`.
The guest binding is `gocvm.Call("gocos", …)`. FreeLoader and the NT
kernel are `~/WASMReact`: `gocvm.Call("gocos", "React")` is
`wasmreact_call`. `gocvm.Call("gocos", "Boot")`
brings the OS up.

This is not an emulator and not a drop of Wine / ntoskrnl / WSL:


| Layer        | What                                             | Hop                   |
| ------------ | ------------------------------------------------ | --------------------- |
| **GocKrnl**  | kernel — session, process, thread, file, vmem    | `k32` (`~/WASMWin32`) |
| **GocSys**   | subsystem — loader, window, draw, registry, path | `k32`                 |
| **GocDesk**  | GPU desktop (swapchain, FlushUI)                 | `k32` HWND            |
| **GocNix**   | WSL occupancy                                    | `nix` (`~/WASMNix`)   |
| **GocShell** | console occupancy                                | `k32` CreateProcessW  |


Every call goes through the **gocvm hypervision** layer. That layer
attaches TurboSpace's vCPU (one WHv partition, a 2MB VM window at GPA
`0x200000`). **k32** and **nix** stay the syscall catalogs on the hop.
`GocKrnl` pages in that window are guest physical addresses. GocKrnl
does not malloc.

The kernel JIT runtime (`jitrt`) instantiates every wasm file on one
process runtime, so they share its cage and its EPT / TPT / CHPT. The
vCPU is that runtime's EPT entry. An import compiled for the guest is
one OUT — the catalog `Call(api, arg)`. `JitPid` is that exit: the
guest's only instruction is `call win32.GetCurrentProcessId`. `JitShare`
loads two modules and reports the cage, the engine CHPT, and each
memory's GIA. `gocos.Call` reads its topic and payload out of the
caller's cage memory and writes the reply back there. Those bytes are
a UNIL store, the module's default data segment: the container's
payload is the memory image and its index names the regions. `JitUnil`
writes `topic`, runs the call, and reads `reply` by name. `JitRun`
loads a wasm file onto that runtime and runs its `_start`. The memory
scheduling queue names a cage region twice on its indirect pointer
table: the GIA is the vCPU linear-memory cache key, and the raw
address is the full pointer. The guest executes from the mapped
cache, not from the raw pointer. `JitFile` plants a
file onto that queue. cppgc's job and foreground task runner admit onto
the same queue; `JitMss` posts one of each and reads them back. A heap
ordinal is the FNV-1a 31-bit name of a mojo call. `Heap` is that call:
`file.Heap.Write`, `file.Heap.Read`, and `file.Heap.Exec` plant and run
the bytes the way ~/WASMMsgProxy/examples/heap does. `ProcFs` is those
ordinals. MSS classifies each address: a 32-bit cage offset is a GIA,
and anything that does not fit is the raw pointer. `ProcTable` is the
process table on that classification. A process pointer is raw, and
reading it back through the GIA tag misses. `Service` seats `core.pid`,
`core.proc`, and `core.mem` on that procfs. A service runs only when
the indirect pointer table resolves its record as a raw pointer.
`Vfs` is the origin-private filesystem on that namespace: Plan 9 walk
and bind, with `/proc` bound to procfs. File bytes are read through
the raw IPT handle. `Bio` is the kernel's memory BIO on WASMNetStack. `Net` is the bridge
onto that stack's C interface: a resolver handle and a kernel handle,
`put` for an A record and `query` for the address. `serve`, `open`,
`send`, and `recv` are a proxy channel on that same link.
`send` is what the go++ sysroot OpenSSL BIO reads out of its write
BIO, and `recv` is what it writes into its read BIO. A core service
name is answered on that pipe; other bytes come back unchanged.
`Bbx` is the Lovelace busybox applet table seated on procfs. The
ordinals are the bbx ordinals. `run` is the argv multiplexer, `trap`
is the ordinal, and `/proc/<applet>` runs that applet. File applets
walk the origin filesystem. `spawn`, `sh`, and `CreateProcess` call
`CreateProcessW`, seat the child's process id, and write its output
on the session console. `Fd` is that session: descriptors 0, 1, and 2
are `#c/cons`, `pipe` is a read end and a write end on that table, and
the working directory starts at `/`. `#e` is the environment device on
that table: `#e` reads the names, and `#e/<name>` is that variable.
`#u` is the UniLoader device: `#u` reads container names, `#u/<name>`
is that process, and `#u/<name>/<region>` is the region. A write to
`#u/<name>/ctl` is `call` or `eval`, and the reply stays in `gocvm.Call`.
`react` is the kernel: `RegSet` and `RegQuery` are the hive under
`HKCU\Software\WASMReact`. `#p` is the
process device: `#p` reads the process table, `#p/self` is
this process id, and `#p/<pid>` is that process's image. A UniLoader
container's row is the full process. `CON` is the
DOS name of `#c/cons`, and `NUL` discards bytes. `bind` mounts one of
those devices onto a path. `uname` is the nix hop. `#t` is the local clock, and `date` reads it.
A `;` runs the next command. `&&` runs the next only when the status
is 0, and `||` runs it when the status is not. `Rc` is the last
command that ran. `$NAME`, `${NAME}`, and `%NAME%` expand from the
session environment. `$?` and `%ERRORLEVEL%` are that status. `ctr` loads, opens, reads, and calls
a UniLoader container from the session shell. `ctr open` reads a `.unil`
packed outside the kernel, gzip or plain. `ctr save` writes a live
container back out as a gzip `.unil` a later boot opens. `ctr run` runs a
wasigocvm guest as the container's service: the kernel is its WASI 0.2
host, and its output is `/ctr/<name>/stdout`. A trap in the guest ends
the guest, not the kernel: the reply says `fault=trap: <cause>`. A
guest's files are the kernel's, walked the way the session walks them,
through go++'s `goclibc` or through `wasi:filesystem` (a preopen of `/`).
A guest at `/bin/<name>.wasm` is a shell command: `echo hi | <name>`
feeds its stdin, its stdout goes down the pipeline, and its exit status
is `$?`. A pipeline with a guest in it streams: its stages run at once
(`include/gocos/stream.hpp`), and a broken pipe ends the writer. With a
trailing `&` the line is a session job that `jobs`, `wait`, and `kill`
reach.
`ctr run ... &` runs a guest in the background as a session job, under the
kernel lock (`include/gocos/klock.hpp`). A guest's TCP is the kernel's
network stack (~/WASMNetStack). `Ctr` is a UniLoader
container: a `.unil` service in a cage object named the way wazgoc names
memory (CHPT, TPT, EPT). The same bytes are an IPT object, and the
origin filesystem stores that object at `/ctr/<name>`, and each region
is a file under that path. A write of the same length stores into that
object. Every container
embeds `gocvm.Call`. Its JavaScript engine is UniLoader's `quickjs`:
quickjs-ng through the v8bindings tables. Its WASM runtime is wazgoc
on the shared cage, so `call` runs the embedded wasigocvm module.
`eval` runs source in that
engine, and `call` dispatches `gocos.Call` with
the container's own object as the memory. It is seated on the process
table as a GIA and is not run on the vCPU.
`Env` is the session environment.
It starts as `PATH=/proc`, and busybox `env` reports how many names
are set. `Sh` is the session shell on that table: applets, `cd`,
`pwd`, and `export`. `|` moves bytes through an `Fd` pipe. `>`, `>>`,
and `<` open descriptors on the origin filesystem. A trailing `&`
seats a background job. `Jobs` lists those rows, and `wait` reaps one:
exit code, output, and a write on the session console. `kill` terminates
a running job. `Rc` is the exit status of the last `Sh` command. `Svc` is rc.d:
`init` runs `/etc/rc.d` in order, a daemon lives at `/etc/daemon` and
stays up until `stop`, and a scheduled service lives at `/etc/sched`
and runs every N ticks. `#s` reads that table. `bind` mounts a device or an origin path, so
`bind /ctr/<name> /box` walks that container through `/box`.

```
gocos.Boot()           →  gocvm.Call("gocos", "Boot")
                          GocKrnl session (k32 GetCurrentProcessId, RtlGetVersion)
                          GocSys  Init    (k32 LoadLibraryW)
                          GocDesk present (k32 CreateWindowExW)
                          GocNix  uname   (nix)
gocos.Version()        →  GocKrnl → k32 RtlGetVersion
gocos.CreateProcess    →  GocKrnl → k32 CreateProcessW
gocos.Alloc            →  GocKrnl → k32 VirtualAlloc (EPT vmem)
gocos.CreateWindow     →  GocSys  → k32 CreateWindowExW
gocos.Present()        →  GocDesk
gocos.Launch(cmd)      →  GocNix  → nix WslExec
```

## Layout

```
include/gocos/dispatch.h   wasmgocos_call — one entry
include/gocos/catalog.h    GocKrnl.* / GocSys.* / GocDesk.* / GocNix.* / GocShell.*
include/gocos/hv.hpp       gocvm hypervision (k32 / nix)
include/gocos/jitrt.hpp    kernel JIT runtime (TurboSpace on the vCPU)
include/gocos/mem.hpp      gocvm vmem (no malloc)
include/gocos/gockrnl.hpp  kernel
include/gocos/gocsys.hpp   subsystem
include/gocos/gocdesk.hpp  GPU desktop
include/gocos/gocshell.hpp occupancy console
include/gocos/host.hpp     boot + dispatch (compiled into wasigocvm)
include/gocos/unil.hpp     .unil bundles packed outside the kernel (gzip, checks)
include/gocos/wasi.hpp     WASI 0.2 host for wasigocvm guests (`ctr run`)
src/catalog.cc             the table
src/wasi.cc                guest images and the canonical-ABI host
examples/service/main.go   the guest `guest_test` runs
src/host_wasi.cc           wasm32
src/host_win.cc            native Windows
src/host_linux.cc          native Linux/WSL
```

## Build (native)

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## wasigocvm

`~/go++/stdlib/gocos` is the guest OS (`import "gocos"`).
`wasigocvm.bat` compiles `host.hpp` into the wasm module
(`-I ~/WASMGocOS/include`) and links `src/catalog.cc` onto EPT.
`gocvm.wasm` is the machine.

```
wasigocvm.bat examples\gocospkg\main.go -o examples\gocospkg\gocospkg.wasm
wasitime examples\gocospkg\gocospkg.wasm
```

The roadmap is [ROADMAP.md](ROADMAP.md).

## License

New code is BSD-3-Clause. WASMWin32c, WASMNix, and GameEngine remain their upstream trees; this repo does not vendor them. GocKrnl / GocSys are this kernel, not a copy of ntoskrnl or Wine.