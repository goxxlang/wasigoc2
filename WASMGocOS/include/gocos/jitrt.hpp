#ifndef WASMGOCOS_INCLUDE_GOCOS_JITRT_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_JITRT_HPP_

// Kernel JIT runtime. One process runtime: every wasm file instantiated
// here shares its cage and its EPT / TPT / CHPT. A pointer in that space
// is a GIA. The vCPU is that runtime's EPT entry (the partition), and a
// guest import is one OUT — the catalog Call(api, arg).

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gocos/stream.hpp"

namespace wasmturbo {
class Runtime;
}

namespace wasmgocos {

// The one process runtime. Every module and every kernel service shares
// its cage.
wasmturbo::Runtime& ProcessRuntime();

// The kernel's own process id: the process that owns the cage. Native
// Windows asks kernel32; the wasm build asks WASI, the same getpid the
// k32 catalog's GetCurrentProcessId answers with.
uint32_t KernelPid();

struct JitModule;

// Decode and instantiate `wasm` on the process runtime. A module the
// decoder rejects aborts, the same contract TurboSpace uses. Returns
// null when `wasm` is empty.
JitModule* jit_load(const uint8_t* wasm, size_t n);

// Same, with a `.unil` container as the module's default data segment.
// The container's payload seeds linear memory (creating it when the
// module has no memory section) and its index names the regions.
// A container the parser rejects aborts. A null container is jit_load.
JitModule* jit_load_unil(const uint8_t* wasm, size_t n, const uint8_t* unil, size_t unil_n);
void jit_drop(JitModule* module);

// Read or replace one named UNIL region. The size must match the region.
// False when the module has no such region.
bool jit_unil_read(JitModule* module, const char* name, void* out, size_t cap, size_t* n);
bool jit_unil_write(JitModule* module, const char* name, const void* bytes, size_t n);

// A module whose data is a `.unil` container: regions "topic", "payload",
// and "reply". Writes GetCurrentProcessId into topic, runs gocos.Call,
// and reads the reply back by name. "error: …" on failure.
std::string jit_unil();

// File I/O in the process cage, the same plant/take as
// ~/WASMMsgProxy/examples/heap. Reads `path` (after writing `body` when
// `body` is non-empty), copies the bytes into the cage, and schedules
// them on the memory queue. The queue's IPT names the GIA and the raw
// address under different tags. `jit_file_take` resolves the GIA entry
// and copies the bytes out. The raw entry does not resolve as a GIA.
// The allocation stays for the process.
bool jit_file_plant(const char* path, const void* body, size_t body_n, uint32_t* gia_ipt,
                    uint32_t* gia, uint32_t* len);
bool jit_file_take(uint32_t gia_ipt, void* out, size_t cap, size_t* n);

// `path`, or `path` + 0x1F + body to write the file first. Reply is
// `gia_ipt=<h> raw_ipt=<h> gia=<gia> raw=<addr> len=<n> cage=<base> text=<bytes>`,
// or "error: …".
std::string jit_file(const char* args);

// Schedules "hello from the heap" on the memory queue, checks that the
// GIA entry and the raw entry resolve only under their own tag, then
// posts a cppgc job and a foreground task onto that same queue.
// `cage=<base> gia=<gia> raw=<addr> jobs=<n> tasks=<n> depth=<n> text=<bytes>`,
// or "error: …".
std::string jit_mss();

// Read a wasm file and instantiate it on the process runtime.
JitModule* jit_load_file(const char* path);

// Run `export_name` on the vCPU. The export returns one i32 and takes
// at most two i32 arguments. False when the vCPU is not attached.
// A missing export aborts. The module's cage objects stay where they
// are; this path is for a body whose result comes back in eax.
bool jit_i32(JitModule* module, const char* export_name, int32_t* result,
             const int32_t* args, int nargs);

// Run `export_name` in the cage. Loads and stores resolve the module's
// memory through CHPT. Same argument limit as jit_i32.
bool jit_cage_i32(JitModule* module, const char* export_name, int32_t* result,
                  const int32_t* args, int nargs);

// go++ entry: `_start`, then `run`, then `main`, then an export whose
// name ends in `#run`, then the start section. False when the module
// has none. The one it finds must be `() -> ()`.
bool jit_entry(JitModule* module);

// Load `path` onto the process runtime and run its entry. The reply is
// `entry=<0|1> cage=<base> engine=<chpt> hv=<ept>`, or "error: …".
std::string jit_run(const char* path);

// Names of this module on the shared runtime. `type_key` is function 0.
// `memory_gia` is 0 when the module has no memory. `hypervisor` is the
// vCPU's EPT handle, 0 when the platform has no partition.
struct JitNames {
  uint64_t cage_base = 0;
  uint32_t engine = 0;
  uint32_t hypervisor = 0;
  uint32_t instance_ept = 0;
  uint32_t memory_gia = 0;
  uint32_t memory_chpt = 0;
  uint64_t memory_base = 0;  // the cage object's bytes, 0 when there is no memory
  uint32_t type_key = 0;
};
bool jit_names(JitModule* module, JitNames* out);

// Two modules on the process runtime. Reports the shared cage, engine
// CHPT, and vCPU EPT, plus each memory's GIA. "error: …" when they do
// not share, or when the first module's cage load does not return 42.
std::string jit_share();

// One-function module: call win32.GetCurrentProcessId and return the
// pid as decimal text. "error: …" when the vCPU is not attached.
std::string jit_pid();

// Ordinal call on the memory queue. The ordinal is the FNV-1a 31-bit
// name, the same call as ~/WASMMsgProxy/examples/heap. `args` is the
// name or the decimal ordinal, then 0x1F-separated fields.
//   file.Heap.Write + path + body  →  `write <ord> writeFS <path>`
//   file.Heap.Read  + path         →  `read <ord> readHeap 0x<raw>`
//   file.Heap.Exec                 →  `heapExec <ord> heapExec 0x<raw> <text>`
// A miss is `read miss`, `heapExec miss`, or `unmatched <ord>`.
std::string jit_heap(const char* args);

// Procfs of the heap ordinals. No args lists one node per line:
// `<ord> <name> 0x<raw> <len>`. An ordinal argument returns that node's
// bytes. `empty` when nothing is bound, `read miss` when the ordinal
// does not name a node.
std::string jit_procfs(const char* args);

// Process table. Each row is a heap ordinal whose address MSS classified.
// A process pointer is not a 32-bit cage offset, so the row is the raw
// handle: `<ord> <pid> <image> raw 0x<address> gia_match=0`. No args lists
// the table, after seating the current process. An ordinal argument
// returns that row's image bytes.
std::string jit_proctable(const char* args);

// Core services on procfs. Each one runs only when the IPT resolves its
// record as a raw pointer. No args lists `core.pid`, `core.proc`, and
// `core.mem`. A name or ordinal runs that service. `gia <name>` resolves
// the GIA handle instead and does not run (`ipt miss`).
std::string jit_service(const char* args);

// Origin-private filesystem with a Plan 9 namespace. Files live in the
// cage. `/proc` is bound to procfs. A read copies bytes only after the
// IPT raw tag resolves the file. `gia <path>` does not.
// Verbs, 0x1F-separated: ls, mkdir, write, read, stat, bind, gia.
std::string jit_vfs(const char* args);

// Busybox. The applet table is the Lovelace bbx table: the same names
// and the same ordinals, seated on procfs. `args` is a method and an
// optional 0x1F field: table, trap, run, ps, register, spawn.
// File applets walk the origin filesystem. spawn seats a process row.
std::string jit_bbx(const char* args);

// Session file descriptors. 0, 1, and 2 are the console `#c/cons`.
// A CreateProcess that waits writes the child's output there. Other
// descriptors name files in the origin filesystem. `#e` is the
// environment: `#e` reads the names, and `#e/<name>` is that variable.
// `#u` is the UniLoader device: `#u` reads container names, `#u/<name>`
// is that container's process, and `#u/<name>/<region>` is the region.
// A write to `#u/<name>/ctl` is `call <topic> <payload>` or `eval <source>`.
// The reply stays in that container's `gocvm.Call` region.
// `#p` is the process table: `#p` reads the rows, `#p/self` is this
// process id, and `#p/<pid>` is that process's image. A UniLoader
// container row is the full process: pid, name, GIA, CHPT, TPT, EPT,
// and the embedded call. `CON` is
// `#c/cons` and `NUL` discards writes. `#t` is the local clock.
// `#s` is the service table: `#s` reads every service, and `#s/<name>`
// is that one row. `#k` is the NT kernel: `#k` reads its processes,
// `#k/threads` its threads, `#k/ob` the object directory, `#k/ob/<name>`
// a directory under it, `#k/mem` the loader memory map, `#k/clock` the
// kernel time, `#k/cons` the boot console, and `#k/handles` the kernel
// handle table. `#k/pool` is the pool tag table. `#k/ob/??` is the DOS
// device map. `#k/types` is the object-type table. `#k/pfn` is the page
// frame list. `#k/mem` is the loader memory map. `#k/aspace` is the
// kernel address space. `#k/ob` follows the rest of the path.
// `bind` mounts a device or an origin path onto a path. `-b` and `-a`
// stack a union; a walk searches that directory in order. Longest prefix
// first. A bind of `/ctr/<name>` walks that container's files, the same
// IPT objects.
// `pipe` adds a read end and a write end. The working directory starts
// at `/`. Verbs,
// 0x1F-separated: open, read, write, close, dup, pipe, cwd, chdir.
// No args lists the table.
std::string jit_fd(const char* args);

// Session environment. It starts as `PATH=/proc`. No args lists
// `NAME=value` lines. Verbs, 0x1F-separated: get, set, unset.
std::string jit_env(const char* args);

// Session shell. Applets, `cd`, `pwd`, `export`, and `unset` run
// against the session. `|` moves the left command's output through an
// `Fd` pipe. `>`, `>>`, and `<` are descriptors on the origin filesystem.
// A trailing `&` seats a background job. `;` runs the next command.
// `&&` runs the next command when the status is 0, and `||` runs it
// when the status is not 0. `Rc` is the last command that ran.
// `$NAME`, `${NAME}`, and `%NAME%` expand from the session environment.
// `$?` and `%ERRORLEVEL%` are that status. `ctr` loads, reads, and calls
// a UniLoader container.
std::string jit_sh(const char* line);

// A UniLoader container on the shared cage. The service bytes are a
// `.unil` payload in a tyMemory object named CHPT / TPT / EPT, the same
// naming wazgoc uses. The payload is also an IPT object, GIA tag and raw
// tag, and the origin filesystem seats that same object at `/ctr/<name>`.
// Each region is its own IPT file under that path.
// Every container is go++, so the image embeds `gocvm.Call`. The same
// image has UniLoader's engine: `engine=quickjs`, quickjs-ng through the
// v8bindings tables. Its WASM runtime is wazgoc, on the shared cage, so
// `call` runs the embedded wasigocvm module. `eval` runs a source in that
// engine. `call` writes
// the topic and payload into that object and dispatches `gocos.Call`.
// It is not loaded onto the vCPU. The process table seats that cage
// address, so the row is a GIA. `load` takes a name and
// an optional service body. `read` takes a pid and a region name. `save`
// takes a name or pid and a path, and writes the live object there as a
// gzip `.unil` that `open` loads on a later boot. No
// args lists the containers. `jit_ctr_line` is that container's process
// line, or empty when `pid` is not a container.
// `run` takes a name, the path of a wasigocvm guest (include/gocos/wasi.hpp),
// and its arguments, and runs the guest as the container's service.
// `start` runs it in the background on its own thread and replies with its
// job; `wait`, `kill`, and `state` take the name. A guest path starting with
// `/` or `#` is the kernel's own; `install <host-path> <os-path>` puts a
// host file there.
std::string jit_ctr(const char* args);
// Appends to the session console, `#c/cons`.
void jit_cons(const std::string& text);
// A path the way the session walks it: DOS names, union mounts, and binds
// followed. An origin path (`/...`) or a device (`#e/PATH`, ...).
std::string jit_walk(const std::string& path);
// A device's bytes as a session read sees them, and a session write to a
// device (`#c/cons`, `#e/<name>`, `#n/nul`).
bool jit_dev_read(const std::string& dev, std::string* out);
bool jit_dev_write(const std::string& dev, const std::string& bytes);
// Seats a background guest (`ctr start`) on the session's job table and
// returns its job number: `jobs`, `wait`, and `kill` then reach it.
// `image` is what `jobs` shows; `ctr <name>` when empty.
int jit_job_guest(const std::string& name, const std::string& image = "");
// A shell command line in the background (`upper < in > out &`): `run`
// runs it on its own thread under the kernel lock, with a kill flag
// (include/gocos/klock.hpp), and returns its output and status. The job is
// on the session's job table, so `jobs`, `wait`, and `kill` reach it; its
// output goes to the console when it ends. The reply is `job <n> ...`.
std::string jit_ctr_start_command(const std::string& line, std::function<std::string(int* code)> run);
// A background command's output, once it has ended.
std::string jit_ctr_job_output(const std::string& name);
std::string jit_ctr_line(uint32_t pid);
// A different-length write of a container region. Empty when `path` is
// not one. Otherwise the cage object grows, both IPT tags move with it,
// and the reply is `write <path>`.
std::string jit_ctr_grow(const char* path, const char* body);
// A guest run as a command: `argv[0]` is its name, `io` its stdin and
// stdout, `*out` its stdout when that is not a stream, and `*code` its
// exit status. Nothing is seated, and its memory goes back to the cage
// when it ends. Empty on success, else the refusal (the image is missing
// or not a guest).
struct GuestIo {
  std::string in;                     // stdin, when in_stream is null
  std::shared_ptr<Stream> in_stream;  // include/gocos/stream.hpp
  std::shared_ptr<Stream> out_stream;
};
std::string jit_ctr_exec(const std::string& path, const std::vector<std::string>& argv, const GuestIo& io,
                         std::string* out, int* code);

// rc.d, daemons, and scheduled services. `rc` seats an init script with
// an order at `/etc/rc.d/<name>`. `daemon` stays running until `stop`
// and lives at `/etc/daemon/<name>`. `sched` runs its body every N
// `tick`s after `start`; the file at `/etc/sched/<name>` is
// `<every> <body>`. `start` with no name runs every init script in
// order. A file written into those directories is seated on `list` or
// on `start` of that name.
std::string jit_svc(const char* args);

// FreeLoader and ntoskrnl (~/WASMReact). No args boots. Otherwise the
// first field is the wasmreact_call name and the rest are its arguments.
std::string jit_react(const char* args);

// Load a linked PE/PE32+ DLL onto the JIT and bind its imports to the
// call catalog. This is the no-host path: a VM with a para/k32 hop maps
// DLLs through LoadLibraryW (the host kernel32), but a VM without one
// reaches here, where WASMPELoader maps the image and every import
// (kernel32!X, ntdll!X, ...) is linked to ~/WASMReact's own kernel32
// (kernel32 -> ntdll -> ntoskrnl). An import WASMReact does not serve is
// left unresolved and named in the reply. This links the IAT; executing
// the mapped code is the vCPU hop that already runs PE bodies.
//   args: a file path, or "minimal" / empty for WASMPELoader's built-in
//         import-free DLL.
//   reply: `backend=react base=<addr> size=<n> entry=<rva> machine=0x<h>
//           exports=<n> imports=<n> react=<n> unresolved=<n>`
//          then one `\x1f <dll>!<name>=<lib>:<index>` (or `=unresolved`)
//          per import.
//   "error: …" on failure.
std::string jit_dll(const char* args);

// The ~/WASMReact catalog index that answers `name` as kernel32 / ntdll,
// or -1. jit_dll links each PE import through this.
int jit_react_export_index(const char* name);

// Session jobs. A background CreateProcess is a row here until `wait`
// reaps it: the exit code, the output, and a console write. No args
// lists `id pid image running|exited`. `wait` reaps every open job.
// `wait` and an id reap that one. `kill` and an id terminates that
// process with exit code 1; `wait` then reaps it.
std::string jit_jobs(const char* args);

// Exit status of the last session-shell command. No args reads it.
// `set` and a number replace it.
std::string jit_status(const char* args);

// Bridge onto ~/WASMNetStack's C interface (`wns/c/system.h`). No args
// is `net resolver=<h> kernel=<h>`. `put` seats an A record on the
// resolver. `query` asks from the kernel stack and returns the address.
// `serve` lets the resolver accept a proxy channel. `open` is a TCP
// target from the kernel, `send` writes that channel, and `recv` reads
// what arrived on the resolver.
std::string jit_net(const char* args);

// Memory BIO over WASMNetStack. `send` is the bytes a guest BIO_read
// from its write BIO; the kernel carries them to the core. `recv` is
// what BIO_write into the read BIO consumes. A core service name is
// answered; any other bytes come back unchanged.
std::string jit_bio(const char* args);

// True on the kernel side of a Plan 9 hop. The namespace runs there.
// A call from anywhere else is carried on WASMNetStack.
bool plan9_local();

// One namespace operation across the net stack. `verb` is fd, env, sh,
// jobs, rc, or walk. The reply is that operation's own reply. The link
// is two WASMNetStack stacks; without it the namespace does not run.
std::string plan9_hop(const char* verb, const char* args);

}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_JITRT_HPP_
