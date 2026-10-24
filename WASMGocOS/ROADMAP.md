# Roadmap

GocOS is a wasigocvm edge kernel: Plan 9 names, a nix session, and a DOS
console, on one shared cage. Small guests run on the vCPU. A UniLoader
container is where a larger service lives.

`jit_test` on `build-vcpu` is the check that a step landed.

## Landed

**Bootloader and kernel.** `~/WASMReact` is FreeLoader and ntoskrnl.
`React` is `wasmreact_call`. The session reads that kernel on `#k`:
processes, threads, the object directory, and the memory map. A named
event is created, signaled, and queried through that same call. GocOS
stays the hypervision hop underneath it: vCPU, k32, and nix.

**Hypervision.** `gocvm.Call("gocos", …)` is the guest binding. k32
(`~/WASMWin32`) and nix (`~/WASMNix`) stay the syscall catalogs. Boot,
version, and `CreateProcessW` go through that hop. The vCPU is one WHv
partition: a 2MB window at GPA `0x200000`. Catalog imports and `gocos.Call`
are its shape. A full wasigocvm module does not go on that map.

**One runtime.** Every module shares the process cage and its CHPT, TPT,
and EPT, the naming `~/go++/examples/wazgoc` uses. A cage offset that fits
in 32 bits is a GIA. Anything else is raw. The vCPU is that runtime's EPT
entry, and the GIA is its cache key.

**Indirect pointer table.** The memory-scheduling queue names a region on
both tags. Heap ordinals, procfs, the process table, and `core.pid` /
`core.proc` / `core.mem` are seated there. A service runs only when the
raw tag resolves.

**Origin filesystem.** Plan 9 walk and bind. `/proc` is procfs. A file is
an IPT object. A write of the same length stores into that object. `/dev`
is not pre-bound.

**Namespace.** Session `bind` mounts a device or an origin path. Binding
`/ctr/<name>` onto another path walks that container's files. The read
is the same IPT object.

**Session.** Descriptors 0, 1, and 2 are `#c/cons`. `#e` is the
environment, `#p` the process table, `#t` the local clock, `#n/nul` the
discard device. `CON` and `NUL` are the DOS names. `bind` mounts a device
onto a path. Pipes, jobs, `wait`, and `kill` are on the same table. The
shell has `;`, `&&`, `||`, `$NAME`, `%NAME%`, `$?`, and `%ERRORLEVEL%`.
`Rc` is the last status. Busybox applets sit on procfs. `Bio` is the
kernel memory BIO on WASMNetStack. `Net` is the bridge through that
stack's C interface: resolver and kernel handles, DNS put and query,
and a proxy channel the kernel opens and the resolver receives.

**Containers.** `Ctr` loads a `.unil` image into a `tyMemory` object on
the shared cage and seats it on the process table as a GIA. The same
bytes are an IPT object at `/ctr/<name>`, and each region is a file under
that path. `#p` shows the full container process. `#u` lists names, the
process line, and a region. A write to `#u/<name>/ctl` is `call` or
`eval`, and the reply stays in that container's `gocvm.Call` region. A
different-length write of a region grows that cage object, and both
IPT tags move with the bytes.
The shell command is `ctr`.

Each container carries three runtimes, and none of them is the vCPU:

- `gocvm.Call`, because the guest is go++. `call` enters the embedded
  wasigocvm module.
- `engine=quickjs` / `js=quickjs-ng`, UniLoader's engine, through the
  v8bindings tables. `eval` runs source there.
- `rt=wazgoc`. The image is instantiated on the shared runtime.
  `wasmturbo::Instance` is that module. `call` runs its `gocos.Call`
  export against the container's own memory.

**Bundles from outside.** `ctr open <path> [name]` reads a `.unil` another
process packed: ~/WASMUniLoader's gzip form or the plain one. The kernel
inflates the payload itself (RFC 1951/1952) and checks the header, the
index, every region CRC, and the gzip trailer before the engine parses
it, so a damaged file is a reply and not an abort. The bundle's regions
are the container's. The kernel adds the `gocvm.Call` frame, `result`,
and `name` when the bundle does not carry them. Every `*.js` region runs
in index order in one quickjs context, and the last value is `result`.

**Null guard.** GIA 0 is null. The process runtime keeps cage offset 0
for itself, so the first container of a process is a GIA like the rest.

**Persistence.** `ctr save <name> <path>` writes the live container back
out as ~/WASMUniLoader's gzip `.unil`. The bytes come through the IPT the
way `read` sees them, so a region that grew is saved at its new size, and
every region keeps its name, mime, kind, and flags. The kernel deflates
the payload itself (LZ77, fixed Huffman). The file is written beside the
path and renamed over it, so a torn save leaves the last image. The next
boot loads it with `ctr open`. `unil_save` and `unil_reboot` are that
check across two processes.

**Guests.** `ctr run <name> <guest.wasm> [args]` runs a program built
with `~/go++/wasigocvm.bat` as the container's service. The build is a
WASI 0.2 command: a core module of about 9MB for a hello world, 11,800
functions, and 117 imports (`wasi:*` and go++'s `goclibc`). An older
build wraps it in a component, and the kernel finds the core module
inside. The kernel is the host. It instantiates the core module on the
shared runtime with its own canonical-ABI implementation bound to every
import (`include/gocos/wasi.hpp`), and runs `wasi:cli/run` off the vCPU.
No component shim is instantiated. Arguments are the container name and
the rest of the line. The environment is the session's. Stdout and stderr
are files under `/ctr/<name>`, and stdout is written on `#c/cons`. The
exit code is `Rc`. The guest's linear memory is the container's cage
object, seated on the process table as a GIA. Stdin is closed. There are
no preopens, so a filesystem call is `bad-descriptor`, a socket is
`not-supported`, and `goclibc` is `-ENOSYS`. Any other import, or a
pointer outside guest memory, ends the guest with a `fault=` in the reply.

What that took underneath:

- ~/WASMReact compiles into a wasigocvm guest again: `aspace.cc` builds
  against go++'s older `v8-platform.h`.
- `wasigocvm.bat` and `wasigocvm.sh` build setjmp/longjmp
  (`-mllvm -wasm-enable-sjlj -lsetjmp`). Clang lowers each `setjmp` onto a
  `try_table` that catches libsetjmp's `__c_longjmp` tag, and each
  `longjmp` onto a `throw` of it, so WASMReact's bugcheck landing pad is
  setjmp in the guest the same as natively.
- WASMTurboSpace runs wasm exceptions (`try_table`, `throw`, `throw_ref`,
  `exnref`) with a pending flag and landing pads, not native unwinding.
  It takes embedder imports of any value type, and it terminates a guest
  through the same flag.
- A full cage refuses `memory.grow` instead of aborting. The process cage
  is 512MB of demand-zero memory.

`guest_test` runs `examples/service` (built from `main.go` by the CMake
build). The guest boots its own ReactOS and bugchecks it: `KeBugCheckEx`
longjmps to the landing pad `ReactOSCall` set with setjmp, in the guest.
WASMTurboSpace's `sjlj_test` runs the same lowering from a C fixture:
a jump across frames, `longjmp(env, 0)`, a jump through an inner setjmp,
and a setjmp taken 1,000 times.

On MinGW every binary links the GCC runtime statically. Linked
dynamically, a test run from a shell whose PATH finds Git for Windows'
older `libstdc++-6.dll` first ran against the wrong C++ runtime.

**Traps end the guest.** A trap in a guest (out-of-bounds memory,
`unreachable`, divide by zero, a bad `call_indirect`, a table index) no
longer aborts the kernel. The engine records the trap's spec message and
terminates the guest through the pending flag, the same way `exit` does,
and `ctr run` replies `exit=1 fault=trap: <cause>`. Output the guest
wrote before the trap stays under `/ctr/<name>`, and the next command
runs. A go++ runtime panic is this path: it aborts, and wasi-libc's
`abort` is `unreachable`. Unbounded recursion is a trap too: every
function entry compares the stack pointer with a limit the entry sets
from the running thread's stack, and replies `fault=trap: call stack
exhausted`.

**Guest files.** A guest's files are the kernel's. go++'s file calls
(`os.Open`, `ReadFile`, `WriteFile`, `Stat`, `ReadDir`, `Mkdir`,
`Remove`, `Rename`) reach `goclibc`, and the kernel walks each path the
way the session does, from the guest's own working directory: binds,
union mounts, DOS names, and devices (`include/gocos/files.hpp`). Origin
files are whole-file and binary-safe. A guest's file is read when it is
opened and written back when it is flushed or closed, so writing a file
line by line makes one object, not one per write. `#e`, `#p`, `#s`, `#u`,
`#k`, and `#t` read as snapshots; `#c/cons` and `#e/<name>` take writes.
What a guest writes, the session reads, and a session bind is in the
guest's namespace.

**The WASI filesystem.** `wasi:filesystem` has one preopen, `/`, on the
same files: a C program built by go++'s `wasm32-wasip2-clang` uses libc's
`open`, `read`, `write`, `lseek`, `fstat`, `ftruncate`, `mkdir`,
`opendir`, `rename`, `unlink`, and `rmdir` on the kernel's origin
filesystem, and gets `ENOENT`, `EEXIST`, and `ENOTEMPTY` where POSIX says.
A descriptor's streams share its buffer, which is written back on flush,
drop, or the end of the run, like `goclibc`. There are no links
(`unsupported`) and no timestamps. `tests/data/fsguest.c` is the test.

**Background guests and daemons.** `ctr run <name> <guest> [args] &`
runs the guest on its own thread and is a session job: `jobs`, `wait`,
and `kill` reach it. The kernel runs one thread at a time under the
kernel lock (`include/gocos/klock.hpp`), in FIFO turns, and each thread
keeps its own WASM execution state, saved and loaded at every handoff. A
guest hands the lock back at yield points -- every function entry and
loop iteration, while another thread waits -- and around blocking host
calls, so a guest spinning in a loop with no calls still leaves the shell
responsive. `kill` ends the guest at its next yield point (`exit=137
fault=killed`). `svc daemon <name> ctr run ... &` makes it a daemon, and
`svc stop` kills it.

**Guest TCP.** A guest's `wasi:sockets` TCP is the kernel's network
stack: ~/WASMNetStack's `wns` sockets, real TCP on the host. go++'s
`net.Listen`, `Accept`, `Dial`, `Read`, and `Write` work, so a guest is a
network service a host program reaches, a guest reaches a host service,
and two guests talk. `wasi:io/poll` waits on socket readiness with the
kernel lock given up, and a connect does too. UDP is on `wns::UdpSocket`: go++'s `ListenPacket`,
`DialPacket`, `ReadFrom`, and `WriteTo` work the same three ways.
That took one fix in go++'s net bridge (`src/wasigocvm_net.hpp`): it
registered a `ListenPacket` socket as not listening, so `WriteTo` called
`send()` with no address, which fails on any host.
`GOCOS_WASI_TRACE=1` prints every import a guest calls and its result.

**Guests stored in the OS.** `ctr install <host-path> /bin/<name>.wasm`
puts a guest into the origin filesystem, and `ctr run` or `ctr start`
takes a path starting with `/` or `#` as the kernel's own, walked as a
guest walks it (binds included). Anything else is a host path.

**Commit on use.** The 512MB process cage is reserved address space;
the cage commits pages in 64KB steps as its allocator hands them out, so
commit charge follows use (about 80MB after a guest run, not 580MB).

**Guests are commands.** A shell command `<name>` that is
`/bin/<name>.wasm` in the kernel's files, or a `.wasm` path there, runs
that guest: its stdin is the pipe or `<` file the stage was given, its
stdout goes down the pipeline or to `>`, and its exit status is `$?`, so
`&&` and `||` follow it. `echo abc | upper | upper > /out.txt` is two
guests. A command is not a container: nothing is seated, its stderr
and any fault go to the console, and its memory goes back to the cage
when it ends.

**Pipelines stream.** A pipeline with a guest in it runs every stage at
once, each on its own thread under the kernel lock, joined by streams
(`include/gocos/stream.hpp`): 64KB each, the writer waiting while one is
full and the reader while it is empty, with the lock given up. A guest's
stdin and stdout are its streams, so it reads as the stage before it
writes. When a reader ends, its writer's next write is a broken pipe and
ends that guest with status 141, as SIGPIPE does, so `yes | head 3` ends.
A stage that is not a guest reads its input to the end, then runs.
The status is the last stage's. A pipeline with no guest runs one stage
at a time, as before.

**Guest commands in the background.** A command line with a guest in it
and a trailing `&` (`service yes | upper > /out.txt &`) is a session job:
the whole line runs on its own thread under the kernel lock, `jobs` lists
it by its text, `wait` gives its status and output, and its output goes to
the console when it ends. `kill` ends every stage: the job's kill flag is
each stage's, a guest ends at its next yield point, and a stage waiting
on a stream stops waiting. A killed job's status is 137.

**File times.** Every origin file and directory keeps when it last
changed, in nanoseconds (`wasmturbo::Vfs::Stat`'s `mtime`): a write
stamps the file, the same-length store in place too, and creating,
removing, or renaming an entry stamps its directory. `wasi:filesystem`'s
`stat` reports it as the data-modification and status-change times, so a
C guest's `st_mtim` is real; a file the guest has written and not yet
written back reads as changed now. `/`, `/proc`, and devices have none.
`set-times` and `set-times-at` set it (C's `utimensat` and `futimens`,
with `UTIME_NOW` and `UTIME_OMIT`), and the shell's `touch <path>...`
makes a file that is not there and sets one that is to now.

**go++: file times.** `goclibc` has `stat2` and `fstat2` beside `stat`
and `fstat`: a 24-byte record, the 16 bytes before and then the time.
go++'s libc calls the new ones; a guest built before calls the old ones
and still gets 16 bytes. go++'s `os.FileInfo` has `ModTime()`, a
`time.Time`, so `fi.ModTime().Unix()` works in a guest.

**Instances give back what they took.** Destroying a
`wasmturbo::Instance` frees its memory, globals, and table in the cage
(`Runtime::ReleaseCageObject`) and its names on CPT, CHPT, TPT, and
EPT. A 512MB cage runs a guest command any number of times; before this,
the fifth 16MB guest in a 64MB cage failed. A container keeps its
instance, so what the IPT names still lives for the process, and the
process runtime is never destroyed.

**go++: string literals.** The generator wrote a control byte as `\x1f`,
which in C++ goes on reading hex digits (`"\x1fbeta"` did not compile),
and a Go string with a NUL decayed to a `const char*` cut there. Control
bytes are three-digit octal now, and a literal holding a NUL is
`std::string("...", n)`. The guest's workarounds are gone
(`examples/service/main.go` writes `"alpha\x1fbeef1\x00gamma"`).

**Init.** `/etc/rc.d/<name>` is an init script. `svc init` reads that
directory and runs every script in order, including ones seated with
`svc rc`. A daemon is `/etc/daemon/<name>` and stays up until `stop`.
A scheduled service is `/etc/sched/<name>`, the file `<every> <body>`,
and runs every N ticks. `#s` reads the service table, and `#s/<name>`
is one row.

**Session jobs.** Every child the session launches is in one k32 job
object that kills on close. No child outlives the kernel process.

**Interrupting a foreground guest.** A foreground command (no trailing
`&`) has no job for `kill` to reach, since it runs on the caller's own
thread inside `wasmgocos_call`. `ForegroundGuard`
(`include/gocos/klock.hpp`, `src/klock.cc`) installs that call's kill
flag globally, not thread-local, for the outermost call only -- a
guest's own `gocos.Call` reentering the host keeps running under the
outer call's flag, the same as `KernelLock`'s own reentrancy. Ctrl-C on
the console (`SetConsoleCtrlHandler` in `src/host_win.cc`) calls
`kernel_interrupt_foreground`, which raises that flag and preempts, the
same as `kill` for a job; the guest ends at its next yield point with
`exit=137 fault=killed`, the same reply shape. `guest_test` runs a
foreground `spin` guest on its own thread and interrupts it the same
way.

## In force

These are the shape of the machine, not a backlog.

- go++ stays the compiler and the guest. The kernel does not edit it;
  what the toolchain builds (setjmp) is changed in go++ itself.
- The vCPU map stays 2MB. Larger services are containers.
- WASMPELoader does not sit on the vCPU.
- A container address is a GIA. A host `CreateProcessW` row stays raw.
- Objects the IPT names live for the process. They are not freed.
- The Windows console banner is `RtlGetVersion`. It does not grow a
  GocOS word to satisfy a test.
- `uname` on this Windows build is `Windows_NT`, the computer name, and
  `RtlGetVersion`. The nix hop is not linked into the native kernel.

## Next

1. **go++: `runtime_smoketest`.** It expects a `no gocvm machine
   registered` message that nothing in go++ prints any more; the other
   159 go++ tests pass.
