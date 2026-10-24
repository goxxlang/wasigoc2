#ifndef WASMGOCOS_INCLUDE_GOCOS_WASI_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_WASI_HPP_

// A wasigocvm guest run as a container service. `wasigocvm.bat` writes a
// WASI 0.2 command: a core module that imports `wasi:*` interfaces (and
// go++'s own `goclibc` file layer) lowered through the canonical ABI, and
// exports `wasi:cli/run@0.2.x#run`, `memory`, and `cabi_realloc`. An older
// build wraps that module in a component; wasi_image finds it inside.
//
// The kernel is the host. It instantiates the core module on the shared
// runtime with WasiHost bound to every import, so no component shim is
// instantiated and nothing runs outside the kernel process.

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gocos/stream.hpp"

namespace wasmturbo {
class Instance;
struct FunctionSig;
struct HostImport;
}  // namespace wasmturbo

namespace wasmgocos {

struct WasiImage {
  std::vector<uint8_t> core;  // the core module
  std::string run;            // its wasi:cli/run export
  uint32_t imports = 0;
  bool component = false;     // unwrapped from a component
};

// A core module or a component. False with `why` set when the bytes are
// neither, or when no core module exports a wasi:cli/run.
bool wasi_image(const uint8_t* data, size_t n, WasiImage* out, std::string* why);

// The canonical-ABI side of every import a go++ guest makes:
//   wasi:cli       arguments, environment, exit, stdin/stdout/stderr, and
//                  no terminal
//   wasi:io        streams and pollables; stdin is closed
//   wasi:clocks    monotonic and wall clock
//   wasi:random    random bytes
//   wasi:filesystem  one preopen, `/`: descriptors and their streams on the
//                  same files as goclibc, buffered the same way
//   wasi:sockets   TCP on the kernel's network stack (~/WASMNetStack's
//                  wns sockets): bind, listen, accept, connect, and socket
//                  streams, with poll readiness from the sockets. A wait
//                  gives the kernel lock up. UDP likewise, on wns
//                  datagram sockets: bind, a stream with or without a
//                  remote, and datagram lists in both directions.
//   goclibc        the kernel's files (include/gocos/files.hpp): a path
//                  walks the session's namespace from the guest's own
//                  working directory, which starts at the session's. A
//                  file is read when it is opened and written back when it
//                  is flushed or closed; a device write goes through as it
//                  happens. Errors are -errno in wasi-libc's numbering.
// Any other import, or a pointer outside the guest's memory, ends the
// guest with `fault()` set.
class WasiHost {
 public:
  WasiHost(std::vector<std::string> args, std::vector<std::pair<std::string, std::string>> env);
  ~WasiHost();

  // For wasmturbo::Instance's resolver: binds (module, name) to this host.
  wasmturbo::HostImport Bind(const std::string& module, const std::string& name,
                             const wasmturbo::FunctionSig& sig);

  bool exited() const { return exited_; }
  int exit_code() const { return exit_code_; }
  const std::string& out() const { return out_; }
  const std::string& err() const { return err_; }
  const std::string& fault() const { return fault_; }
  // Calls the guest made, by import name.
  uint32_t calls() const { return calls_; }
  // Writes back every file a guest left open: libc's exit does not flush
  // through goclibc. Call after the run.
  void FlushAll();
  // What the guest reads on stdin, then end of stream. Empty by default:
  // stdin is closed.
  void set_stdin(std::string bytes) { in_ = std::move(bytes); }
  // Stdin read from, and stdout written to, a pipeline's stream
  // (include/gocos/stream.hpp) instead. A write to a stream whose reader
  // is gone ends the guest with status 141.
  void set_stdin_stream(std::shared_ptr<Stream> s) { in_stream_ = std::move(s); }
  void set_stdout_stream(std::shared_ptr<Stream> s) { out_stream_ = std::move(s); }

  // The same goclibc calls for the module that links this kernel (go++'s
  // goclibc built with GOCLIBC_IN_KERNEL): `name` is the import name, and
  // every pointer is an address in this module's own memory.
  int32_t GoclibcLocal(const char* name, const uint64_t* a);
  // DOS paths: `X:\rest` walks as /mnt/x/rest (drvfs), and `\` is `/`.
  void set_dos(bool on) { dos_ = on; }
  void set_cwd(std::string cwd) { cwd_ = std::move(cwd); }

  struct Thunk;

 private:
  struct Res;
  struct GFile;
  uint64_t Dispatch(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a);
  uint64_t DispatchOp(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a);
  int32_t Goclibc(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a);
  // The guest string at [ptr, ptr + len); false (and a fault) when it is
  // outside guest memory.
  bool Str(wasmturbo::Instance* inst, uint32_t ptr, uint32_t len, std::string* out);
  // A guest path as the kernel walks it (files::Walk), from cwd_.
  std::string Walk(const std::string& path) const;
  std::string Absolute(const std::string& path) const;
  int32_t Sync(GFile& f);
  uint8_t* Mem(wasmturbo::Instance* inst, uint64_t ptr, uint64_t len);
  bool Put(wasmturbo::Instance* inst, uint32_t ptr, const void* bytes, size_t n);
  bool Put8(wasmturbo::Instance* inst, uint32_t ptr, uint8_t v) { return Put(inst, ptr, &v, 1); }
  bool Put32(wasmturbo::Instance* inst, uint32_t ptr, uint32_t v) { return Put(inst, ptr, &v, 4); }
  bool Put64(wasmturbo::Instance* inst, uint32_t ptr, uint64_t v) { return Put(inst, ptr, &v, 8); }
  uint32_t Alloc(wasmturbo::Instance* inst, uint32_t align, uint32_t size);
  uint32_t AllocBytes(wasmturbo::Instance* inst, const std::string& s);
  uint64_t Fault(const std::string& why);
  uint32_t NewRes(int kind, uint64_t deadline);
  Res* Get(uint32_t handle);
  bool Ready(const Res& r) const;
  void Wait(const std::vector<uint32_t>& handles);
  uint64_t Sockets(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a);
  uint64_t Udp(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a);
  uint64_t Fs(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a);

  std::vector<std::string> args_;
  std::vector<std::pair<std::string, std::string>> env_;
  std::vector<std::unique_ptr<Thunk>> thunks_;
  std::vector<Res> res_;
  uint32_t stdin_ = 0, stdout_ = 0, stderr_ = 0;
  bool exited_ = false;
  int exit_code_ = 0;
  std::string out_, err_, fault_;
  std::string in_;
  size_t in_pos_ = 0;
  std::shared_ptr<Stream> in_stream_, out_stream_;
  uint32_t calls_ = 0;
  std::string cwd_ = "/";
  bool dos_ = false;
  std::map<int32_t, std::shared_ptr<GFile>> gfd_;
  int32_t next_gfd_ = 3;
};

}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_WASI_HPP_
