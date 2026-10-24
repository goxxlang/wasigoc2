#include "gocos/wasi.hpp"

#include "gocos/files.hpp"
#include "gocos/klock.hpp"
#include "gocos/jitrt.hpp"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <thread>

#include "src/instance.h"
#include "src/runtime/exn.h"
#include "src/runtime/imports.h"
#include "src/runtime/vfs.h"

#if defined(WASMGOCOS_HAS_NET)
#include "wns/sock.h"
#endif

namespace wasmgocos {

// --- The image ---------------------------------------------------------

namespace {

bool Leb(const uint8_t* p, size_t n, size_t* pos, uint32_t* out) {
  uint32_t v = 0;
  for (int shift = 0; shift < 35; shift += 7) {
    if (*pos >= n) return false;
    uint8_t b = p[(*pos)++];
    v |= static_cast<uint32_t>(b & 0x7f) << shift;
    if ((b & 0x80) == 0) {
      *out = v;
      return true;
    }
  }
  return false;
}

// A core module's function-import count and the name of its wasi:cli/run
// export. False when the bytes are not a well-framed core module.
bool ScanCore(const uint8_t* p, size_t n, uint32_t* imports, std::string* run) {
  static const uint8_t kCore[8] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  if (n < 8 || std::memcmp(p, kCore, 8) != 0) return false;
  size_t pos = 8;
  *imports = 0;
  run->clear();
  while (pos < n) {
    uint8_t id = p[pos++];
    uint32_t size = 0;
    if (!Leb(p, n, &pos, &size) || size > n - pos) return false;
    size_t end = pos + size;
    if (id == 2 || id == 7) {
      size_t q = pos;
      uint32_t count = 0;
      if (!Leb(p, end, &q, &count)) return false;
      for (uint32_t i = 0; i < count; ++i) {
        std::string names[2];
        for (int k = 0; k < (id == 2 ? 2 : 1); ++k) {
          uint32_t len = 0;
          if (!Leb(p, end, &q, &len) || len > end - q) return false;
          names[k].assign(reinterpret_cast<const char*>(p + q), len);
          q += len;
        }
        if (q >= end) return false;
        uint8_t kind = p[q++];
        uint32_t index = 0;
        if (id == 2) {
          if (kind != 0) return false;  // only function imports
          if (!Leb(p, end, &q, &index)) return false;
          ++*imports;
        } else {
          if (!Leb(p, end, &q, &index)) return false;
          if (kind == 0 && names[0].rfind("wasi:cli/run@", 0) == 0 && names[0].size() > 4 &&
              names[0].compare(names[0].size() - 4, 4, "#run") == 0)
            *run = names[0];
        }
      }
    }
    pos = end;
  }
  return pos == n;
}

}  // namespace

bool wasi_image(const uint8_t* data, size_t n, WasiImage* out, std::string* why) {
  auto fail = [&](const char* what) {
    if (why != nullptr) *why = what;
    return false;
  };
  if (n < 8 || std::memcmp(data, "\0asm", 4) != 0) return fail("not wasm");
  uint32_t imports = 0;
  std::string run;
  if (data[4] == 0x01 && data[5] == 0 && data[6] == 0 && data[7] == 0) {
    if (!ScanCore(data, n, &imports, &run)) return fail("core module");
    if (run.empty()) return fail("no wasi:cli/run export");
    out->core.assign(data, data + n);
    out->run = run;
    out->imports = imports;
    out->component = false;
    return true;
  }
  // A component: layer 1. Its core modules are sections with id 1; the
  // guest is the one that exports run (the others are canonical-ABI
  // shims).
  if (!(data[6] == 0x01 && data[7] == 0x00)) return fail("wasm version");
  size_t pos = 8;
  while (pos < n) {
    uint8_t id = data[pos++];
    uint32_t size = 0;
    if (!Leb(data, n, &pos, &size) || size > n - pos) return fail("component");
    if (id == 1 && ScanCore(data + pos, size, &imports, &run) && !run.empty()) {
      out->core.assign(data + pos, data + pos + size);
      out->run = run;
      out->imports = imports;
      out->component = true;
      return true;
    }
    pos += size;
  }
  return fail("no core module exports wasi:cli/run");
}

// --- The host ----------------------------------------------------------

namespace {

enum Op {
  kUnsupported,
  kDrop,
  kGetEnvironment,
  kGetArguments,
  kExit,
  kExitWithCode,
  kPollableBlock,
  kPoll,
  kInputRead,
  kSubscribeReady,
  kCheckWrite,
  kWrite,
  kBlockingFlush,
  kWriteAndFlush,
  kGetStdin,
  kGetStdout,
  kGetStderr,
  kGetTerminal,
  kMonoNow,
  kMonoResolution,
  kSubscribeInstant,
  kSubscribeDuration,
  kWallNow,
  kWallResolution,
  kRandomBytes,
  kInstanceNetwork,
  kCreateSocket,
  kGoclibc,
  kTcp,  // wasi:sockets/tcp and create-tcp-socket (Sockets)
  kFs,   // wasi:filesystem (Fs)
};

// goclibc, by import name.
enum GoOp {
  kGoNone,
  kGoOpen,
  kGoClose,
  kGoRead,
  kGoWrite,
  kGoFlush,
  kGoSeek,
  kGoStat,
  kGoFstat,
  kGoStat2,  // the 24-byte record: stat's, then the modification time
  kGoFstat2,
  kGoMkdir,
  kGoUnlink,
  kGoRmdir,
  kGoRename,
  kGoAccess,
  kGoGetcwd,
  kGoChdir,
  kGoReaddir,
  kGoDup,
};

GoOp GoOpOf(const std::string& name) {
  static const std::map<std::string, GoOp> ops = {
      {"open", kGoOpen},     {"close", kGoClose},   {"read", kGoRead},     {"write", kGoWrite},
      {"flush", kGoFlush},   {"seek", kGoSeek},     {"stat", kGoStat},     {"fstat", kGoFstat},
      {"stat2", kGoStat2},   {"fstat2", kGoFstat2},
      {"mkdir", kGoMkdir},   {"unlink", kGoUnlink}, {"rmdir", kGoRmdir},   {"rename", kGoRename},
      {"access", kGoAccess}, {"getcwd", kGoGetcwd}, {"chdir", kGoChdir},   {"readdir", kGoReaddir},
      {"dup", kGoDup},
  };
  auto it = ops.find(name);
  return it == ops.end() ? kGoNone : it->second;
}

// wasi-libc's errno values and the goclibc open flags (~/go++/libc).
constexpr int32_t kEacces = 2, kEbadf = 8, kEexist = 20, kEinval = 28, kEio = 29, kEisdir = 31,
                  kEnoent = 44, kEnotdir = 54, kEnotempty = 55, kErange = 68;
constexpr uint32_t kGoWronly = 1, kGoRdwr = 2, kGoCreat = 4, kGoTrunc = 8, kGoAppend = 16;
constexpr uint32_t kModeDir = 0x4000 | 0755, kModeFile = 0x8000 | 0644;

enum Kind {
  kFree,
  kInput,
  kOutput,
  kPollable,
  kNetwork,
  kOther,
  kTcpSocket,
  kSockIn,
  kSockOut,
  kUdpSocket,
  kDgramIn,
  kDgramOut,
  kDescriptor,
  kFileIn,
  kFileOut,
  kDirStream,
};

// What a socket pollable waits for.
enum Watch { kWatchNone, kWatchRead, kWatchWrite };

// Where each descriptor method's error code sits in its result: after the
// discriminant, at the alignment of the ok payload.
const std::map<std::string, uint32_t>& DescriptorErrors() {
  static const std::map<std::string, uint32_t> table = {
      {"[method]descriptor.read-via-stream", 4},
      {"[method]descriptor.write-via-stream", 4},
      {"[method]descriptor.append-via-stream", 4},
      {"[method]descriptor.get-flags", 1},
      {"[method]descriptor.get-type", 1},
      {"[method]descriptor.set-size", 1},
      {"[method]descriptor.set-times", 1},
      {"[method]descriptor.sync", 1},
      {"[method]descriptor.sync-data", 1},
      {"[method]descriptor.read-directory", 4},
      {"[method]descriptor.stat", 8},
      {"[method]descriptor.stat-at", 8},
      {"[method]descriptor.set-times-at", 1},
      {"[method]descriptor.link-at", 1},
      {"[method]descriptor.open-at", 4},
      {"[method]descriptor.readlink-at", 4},
      {"[method]descriptor.remove-directory-at", 1},
      {"[method]descriptor.rename-at", 1},
      {"[method]descriptor.symlink-at", 1},
      {"[method]descriptor.unlink-file-at", 1},
      {"[method]descriptor.create-directory-at", 1},
      {"[method]descriptor.metadata-hash", 8},
      {"[method]descriptor.metadata-hash-at", 8},
      {"[method]directory-entry-stream.read-directory-entry", 4},
  };
  return table;
}

// The interface name without its version: "wasi:io/streams@0.2.12" is
// "wasi:io/streams".
std::string Unversioned(const std::string& module) {
  size_t at = module.find('@');
  return at == std::string::npos ? module : module.substr(0, at);
}

uint64_t NowNs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

constexpr uint8_t kSockNotSupported = 2;  // wasi:sockets error-code
constexpr uint32_t kWritePermit = 1u << 20;

}  // namespace

struct WasiHost::Thunk {
  WasiHost* host;
  Op op;
  uint32_t nparams;
  uint32_t err_at;  // kFs: where a method's error code sits
  std::string name;
};

struct WasiHost::GFile {
  std::string walked;
  std::string data;
  uint64_t pos = 0;
  bool readable = true;
  bool writable = false;
  bool append = false;
  bool device = false;
  bool dirty = false;
};

// One TCP socket, shared by its socket resource and its two streams.
struct Sock {
#if defined(WASMGOCOS_HAS_NET)
  enum State { kFresh, kBound, kListening, kConnected };
  State state = kFresh;
  bool v6 = false;
  wns::Endpoint local{};
  wns::Endpoint remote{};
  wns::TcpListener listener;
  wns::TcpConn conn;
  bool connect_started = false;
  uint8_t connect_error = 0;
  bool eof = false;
  // UDP: the datagram socket, and the remote a stream was opened with.
  bool udp = false;
  wns::UdpSocket dgram;
  bool dgram_connected = false;
  wns::Endpoint dgram_remote{};

  wns::SockHandle Handle() const {
    if (udp) return dgram.handle();
    if (state == kListening) return listener.handle();
    return state == kConnected ? conn.handle() : wns::kBadSock;
  }
#endif
};

struct WasiHost::Res {
  int kind = kFree;
  uint64_t deadline = 0;  // a pollable: ready when the monotonic clock reaches it
  std::shared_ptr<Sock> sock;  // a socket, a socket stream, or what a pollable watches
  int watch = kWatchNone;
  // wasi:filesystem: a descriptor's guest path, and for a file its buffer,
  // shared with the descriptor's streams (the goclibc model: read once,
  // written back on flush, drop, or the end of the run).
  std::shared_ptr<GFile> file;
  std::string path;
  bool dir = false;
  bool append = false;
  uint8_t flags = 0;  // descriptor-flags: read 1, write 2
  uint64_t offset = 0;
  std::vector<std::pair<std::string, bool>> entries;
  size_t next = 0;
};

WasiHost::WasiHost(std::vector<std::string> args, std::vector<std::pair<std::string, std::string>> env)
    : args_(std::move(args)), env_(std::move(env)) {
  stdin_ = NewRes(kInput, 0);
  stdout_ = NewRes(kOutput, 0);
  stderr_ = NewRes(kOutput, 0);
  std::string cwd = jit_fd("cwd");
  if (!cwd.empty() && cwd[0] == '/') cwd_ = cwd;
}

WasiHost::~WasiHost() = default;

wasmturbo::HostImport WasiHost::Bind(const std::string& module, const std::string& name,
                                     const wasmturbo::FunctionSig& sig) {
  std::string m = Unversioned(module);
  Op op = kUnsupported;
  uint32_t err_at = 0;
  if (name.rfind("[resource-drop]", 0) == 0) op = kDrop;
  else if (m == "wasi:cli/environment" && name == "get-environment") op = kGetEnvironment;
  else if (m == "wasi:cli/environment" && name == "get-arguments") op = kGetArguments;
  else if (m == "wasi:cli/exit" && name == "exit") op = kExit;
  else if (m == "wasi:cli/exit" && name == "exit-with-code") op = kExitWithCode;
  else if (m == "wasi:io/poll" && name == "[method]pollable.block") op = kPollableBlock;
  else if (m == "wasi:io/poll" && name == "poll") op = kPoll;
  else if (m == "wasi:io/streams" && (name == "[method]input-stream.read" ||
                                      name == "[method]input-stream.blocking-read"))
    op = kInputRead;
  else if (m == "wasi:io/streams" && (name == "[method]input-stream.subscribe" ||
                                      name == "[method]output-stream.subscribe"))
    op = kSubscribeReady;
  else if (m == "wasi:io/streams" && name == "[method]output-stream.check-write") op = kCheckWrite;
  else if (m == "wasi:io/streams" && name == "[method]output-stream.write") op = kWrite;
  else if (m == "wasi:io/streams" && name == "[method]output-stream.blocking-write-and-flush")
    op = kWriteAndFlush;
  else if (m == "wasi:io/streams" && (name == "[method]output-stream.blocking-flush" ||
                                      name == "[method]output-stream.flush"))
    op = kBlockingFlush;
  else if (m == "wasi:cli/stdin" && name == "get-stdin") op = kGetStdin;
  else if (m == "wasi:cli/stdout" && name == "get-stdout") op = kGetStdout;
  else if (m == "wasi:cli/stderr" && name == "get-stderr") op = kGetStderr;
  else if (m.rfind("wasi:cli/terminal-", 0) == 0 && name.rfind("get-terminal-", 0) == 0) op = kGetTerminal;
  else if (m == "wasi:clocks/monotonic-clock" && name == "now") op = kMonoNow;
  else if (m == "wasi:clocks/monotonic-clock" && name == "resolution") op = kMonoResolution;
  else if (m == "wasi:clocks/monotonic-clock" && name == "subscribe-instant") op = kSubscribeInstant;
  else if (m == "wasi:clocks/monotonic-clock" && name == "subscribe-duration") op = kSubscribeDuration;
  else if (m == "wasi:clocks/wall-clock" && name == "now") op = kWallNow;
  else if (m == "wasi:clocks/wall-clock" && name == "resolution") op = kWallResolution;
  else if (m == "wasi:filesystem/preopens" && name == "get-directories") op = kFs;
  else if (m == "wasi:filesystem/types" && name.rfind("[method]", 0) == 0) {
    op = kFs;
    if (DescriptorErrors().count(name) != 0) err_at = DescriptorErrors().at(name);
  } else if (m == "wasi:random/random" && name == "get-random-bytes") op = kRandomBytes;
  else if (m == "wasi:sockets/instance-network" && name == "instance-network") op = kInstanceNetwork;
#if defined(WASMGOCOS_HAS_NET)
  else if ((m == "wasi:sockets/tcp-create-socket" && name == "create-tcp-socket") ||
           (m == "wasi:sockets/tcp" && name.rfind("[method]tcp-socket.", 0) == 0) ||
           (m == "wasi:sockets/udp-create-socket" && name == "create-udp-socket") ||
           (m == "wasi:sockets/udp" && name.rfind("[method]", 0) == 0))
    op = kTcp;
#endif
  else if ((m == "wasi:sockets/tcp-create-socket" && name == "create-tcp-socket") ||
           (m == "wasi:sockets/udp-create-socket" && name == "create-udp-socket"))
    op = kCreateSocket;
  else if (module == "goclibc") op = kGoclibc;
  thunks_.push_back(std::make_unique<Thunk>(
      Thunk{this, op, static_cast<uint32_t>(sig.params.size()), err_at, module + " " + name}));
  return wasmturbo::HostImport{
      [](void* ctx, wasmturbo::Instance* inst, const uint64_t* a) -> uint64_t {
        const Thunk* t = static_cast<const Thunk*>(ctx);
        return t->host->Dispatch(*t, inst, a);
      },
      thunks_.back().get()};
}

bool WasiHost::Str(wasmturbo::Instance* inst, uint32_t ptr, uint32_t len, std::string* out) {
  uint8_t* at = Mem(inst, ptr, len);
  if (at == nullptr) {
    Fault("wasi: pointer outside guest memory");
    return false;
  }
  out->assign(reinterpret_cast<const char*>(at), len);
  return true;
}

std::string WasiHost::Absolute(const std::string& raw) const {
  std::string path = raw;
  if (dos_) {
    for (char& c : path)
      if (c == '\\') c = '/';
    char d = path.size() >= 2 && path[1] == ':' ? path[0] : 0;
    if (d >= 'A' && d <= 'Z') d = static_cast<char>(d - 'A' + 'a');
    if (d >= 'a' && d <= 'z' && (path.size() == 2 || path[2] == '/'))
      path = std::string("/mnt/") + d + path.substr(2);
  }
  if (!path.empty() && (path[0] == '/' || path[0] == '#')) return path;
  return cwd_ == "/" ? "/" + path : cwd_ + "/" + path;
}

std::string WasiHost::Walk(const std::string& path) const {
  std::string walked = files::Walk(Absolute(path));
  if (!dos_ || walked.rfind("/mnt/", 0) != 0) return walked;
  // A drive matches names without case, as drvfs does: each part that is
  // not there as spelled is the entry whose name differs only in case.
  std::string at = walked.substr(0, 6);
  size_t i = 7;
  while (i <= walked.size() && i > 6) {
    size_t end = walked.find('/', i);
    if (end == std::string::npos) end = walked.size();
    std::string part = walked.substr(i, end - i);
    std::string next = at + "/" + part;
    if (!part.empty() && files::StatOf(next).kind == files::Kind::kNone) {
      std::vector<std::pair<std::string, bool>> entries;
      if (files::List(at, &entries)) {
        for (const auto& e : entries) {
          if (e.first.size() != part.size()) continue;
          bool same = true;
          for (size_t k = 0; k < part.size() && same; ++k)
            same = std::tolower(static_cast<unsigned char>(e.first[k])) ==
                   std::tolower(static_cast<unsigned char>(part[k]));
          if (same) {
            next = at + "/" + e.first;
            break;
          }
        }
      }
    }
    if (!part.empty()) at = next;
    i = end + 1;
  }
  return at;
}

int32_t WasiHost::Sync(GFile& f) {
  if (!f.dirty || f.device) return 0;
  if (!files::Write(f.walked, f.data)) return -kEio;
  f.dirty = false;
  return 0;
}

void WasiHost::FlushAll() {
  for (auto& kv : gfd_) Sync(*kv.second);
  for (Res& r : res_) {
    if (r.file) Sync(*r.file);
  }
}

int32_t WasiHost::GoclibcLocal(const char* name, const uint64_t* a) {
  Thunk t{this, kGoclibc, 0, 0, std::string("goclibc ") + (name ? name : "")};
  return Goclibc(t, nullptr, a);
}

int32_t WasiHost::Goclibc(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a) {
  auto i32 = [&](uint32_t k) { return static_cast<uint32_t>(a[k]); };
  GoOp op = GoOpOf(t.name.substr(t.name.find(' ') + 1));
  auto handle = [&](uint32_t k) -> GFile* {
    auto it = gfd_.find(static_cast<int32_t>(i32(k)));
    return it == gfd_.end() ? nullptr : it->second.get();
  };
  std::string path;
  switch (op) {
    case kGoOpen: {
      if (!Str(inst, i32(0), i32(1), &path)) return 0;
      uint32_t flags = i32(2);
      auto f = std::make_shared<GFile>();
      f->walked = Walk(path);
      f->writable = (flags & 3) != 0 || (flags & (kGoCreat | kGoTrunc | kGoAppend)) != 0;
      f->readable = (flags & 3) != kGoWronly;
      f->append = (flags & kGoAppend) != 0;
      files::Stat st = files::StatOf(f->walked);
      if (st.kind == files::Kind::kDir) return -kEisdir;
      if (files::IsDevice(f->walked)) {
        f->device = true;
        if (f->writable && !files::DeviceWritable(f->walked)) return -kEacces;
        if (st.kind == files::Kind::kNone && !f->writable) return -kEnoent;
        if (f->readable && st.kind != files::Kind::kNone) files::Read(f->walked, &f->data);
      } else if (st.kind == files::Kind::kNone) {
        if ((flags & kGoCreat) == 0) return -kEnoent;
        f->dirty = true;
      } else if ((flags & kGoTrunc) != 0) {
        f->dirty = true;
      } else if (!files::Read(f->walked, &f->data)) {
        return -kEio;
      }
      int32_t fd = next_gfd_++;
      gfd_[fd] = f;
      return fd;
    }
    case kGoClose: {
      auto it = gfd_.find(static_cast<int32_t>(i32(0)));
      if (it == gfd_.end()) return -kEbadf;
      int32_t rc = Sync(*it->second);
      gfd_.erase(it);
      return rc;
    }
    case kGoRead: {
      GFile* f = handle(0);
      if (f == nullptr || !f->readable) return -kEbadf;
      uint64_t n = i32(2);
      uint64_t left = f->pos < f->data.size() ? f->data.size() - f->pos : 0;
      if (n > left) n = left;
      if (n > 0 && !Put(inst, i32(1), f->data.data() + f->pos, n)) return 0;
      f->pos += n;
      return static_cast<int32_t>(n);
    }
    case kGoWrite: {
      GFile* f = handle(0);
      if (f == nullptr || !f->writable) return -kEbadf;
      std::string bytes;
      if (!Str(inst, i32(1), i32(2), &bytes)) return 0;
      if (f->device) return files::Write(f->walked, bytes) ? static_cast<int32_t>(bytes.size()) : -kEio;
      if (f->append) f->pos = f->data.size();
      if (f->pos + bytes.size() > f->data.size()) f->data.resize(f->pos + bytes.size());
      if (!bytes.empty()) f->data.replace(f->pos, bytes.size(), bytes);
      f->pos += bytes.size();
      f->dirty = true;
      return static_cast<int32_t>(bytes.size());
    }
    case kGoFlush: {
      GFile* f = handle(0);
      return f == nullptr ? -kEbadf : Sync(*f);
    }
    case kGoSeek: {
      GFile* f = handle(0);
      if (f == nullptr) return -kEbadf;
      int64_t off = static_cast<int32_t>(i32(1));
      uint32_t whence = i32(2);
      int64_t base = whence == 0 ? 0 : whence == 1 ? static_cast<int64_t>(f->pos)
                                     : whence == 2 ? static_cast<int64_t>(f->data.size())
                                                   : -1;
      if (base < 0 || base + off < 0 || base + off > INT32_MAX) return -kEinval;
      f->pos = static_cast<uint64_t>(base + off);
      return static_cast<int32_t>(f->pos);
    }
    case kGoStat:
    case kGoFstat:
    case kGoStat2:
    case kGoFstat2: {
      uint32_t mode = kModeFile;
      uint64_t size = 0;
      int64_t mtime = 0;
      uint32_t out = 0;
      // Written and not yet written back: changed now.
      auto now_ns = [] {
        return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count());
      };
      if (op == kGoFstat || op == kGoFstat2) {
        GFile* f = handle(0);
        if (f == nullptr) return -kEbadf;
        size = f->data.size();
        mtime = f->dirty ? now_ns() : files::StatOf(f->walked).mtime;
        out = i32(1);
      } else {
        if (!Str(inst, i32(0), i32(1), &path)) return 0;
        std::string walked = Walk(path);
        files::Stat st = files::StatOf(walked);
        if (st.kind == files::Kind::kNone) return -kEnoent;
        if (st.kind == files::Kind::kDir) mode = kModeDir;
        size = st.size;
        mtime = st.mtime;
        // A file this guest has open and not written back yet.
        for (const auto& kv : gfd_) {
          if (kv.second->walked == walked && kv.second->dirty) {
            size = kv.second->data.size();
            mtime = now_ns();
          }
        }
        out = i32(2);
      }
      uint8_t raw[24] = {};
      std::memcpy(raw, &mode, 4);
      std::memcpy(raw + 8, &size, 8);
      std::memcpy(raw + 16, &mtime, 8);
      Put(inst, out, raw, op == kGoStat2 || op == kGoFstat2 ? 24 : 16);
      return 0;
    }
    case kGoMkdir: {
      if (!Str(inst, i32(0), i32(1), &path)) return 0;
      std::string walked = Walk(path);
      if (files::StatOf(walked).kind != files::Kind::kNone) return -kEexist;
      return files::MakeDir(walked) ? 0 : -kEnoent;
    }
    case kGoUnlink:
    case kGoRmdir: {
      if (!Str(inst, i32(0), i32(1), &path)) return 0;
      std::string walked = Walk(path);
      files::Stat st = files::StatOf(walked);
      if (st.kind == files::Kind::kNone) return -kEnoent;
      if (st.kind == files::Kind::kDevice) return -kEacces;
      if (op == kGoUnlink) {
        if (st.kind == files::Kind::kDir) return -kEisdir;
        return files::Remove(walked, false) ? 0 : -kEio;
      }
      if (st.kind != files::Kind::kDir) return -kEnotdir;
      std::vector<std::pair<std::string, bool>> entries;
      if (files::List(walked, &entries) && !entries.empty()) return -kEnotempty;
      return files::Remove(walked, true) ? 0 : -kEio;
    }
    case kGoRename: {
      std::string to;
      if (!Str(inst, i32(0), i32(1), &path) || !Str(inst, i32(2), i32(3), &to)) return 0;
      std::string from = Walk(path);
      std::string dest = Walk(to);
      files::Stat st = files::StatOf(from);
      if (st.kind == files::Kind::kNone) return -kEnoent;
      if (st.kind == files::Kind::kFile && files::StatOf(dest).kind == files::Kind::kDir) return -kEisdir;
      return files::Rename(from, dest) ? 0 : -kEinval;
    }
    case kGoAccess:
      if (!Str(inst, i32(0), i32(1), &path)) return 0;
      return files::StatOf(Walk(path)).kind == files::Kind::kNone ? -kEnoent : 0;
    case kGoGetcwd: {
      uint32_t cap = i32(1);
      if (cwd_.size() + 1 > cap) return -kErange;
      if (!Put(inst, i32(0), cwd_.c_str(), cwd_.size() + 1)) return 0;
      return static_cast<int32_t>(cwd_.size());
    }
    case kGoChdir: {
      if (!Str(inst, i32(0), i32(1), &path)) return 0;
      std::string abs = Absolute(path);
      std::string walked = Walk(path);
      files::Kind kind = files::StatOf(walked).kind;
      if (kind == files::Kind::kNone) return -kEnoent;
      if (kind != files::Kind::kDir) return -kEnotdir;
      // A drive keeps the names as they are stored.
      cwd_ = abs[0] == '#' ? abs : dos_ ? walked : wasmturbo::Vfs::Canon(abs);
      return 0;
    }
    case kGoReaddir: {
      if (!Str(inst, i32(0), i32(1), &path)) return 0;
      std::string walked = Walk(path);
      files::Stat st = files::StatOf(walked);
      if (st.kind == files::Kind::kNone) return -kEnoent;
      std::vector<std::pair<std::string, bool>> entries;
      if (st.kind != files::Kind::kDir || !files::List(walked, &entries)) return -kEnotdir;
      std::string blob;
      for (const auto& e : entries) {
        std::string one = std::string(1, e.second ? 'd' : 'f') + e.first + '\0';
        if (blob.size() + one.size() > i32(3)) break;
        blob += one;
      }
      if (!blob.empty() && !Put(inst, i32(2), blob.data(), blob.size())) return 0;
      return static_cast<int32_t>(blob.size());
    }
    case kGoDup: {
      auto it = gfd_.find(static_cast<int32_t>(i32(0)));
      if (it == gfd_.end()) return -kEbadf;
      int32_t fd = next_gfd_++;
      gfd_[fd] = it->second;  // shares the offset, as dup does
      return fd;
    }
    case kGoNone:
      break;
  }
  Fault("wasi: " + t.name + " is not supported");
  return 0;
}

uint64_t WasiHost::Sockets(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a) {
#if defined(WASMGOCOS_HAS_NET)
  auto i32 = [&](uint32_t k) { return static_cast<uint32_t>(a[k]); };
  uint32_t retptr = t.nparams > 0 ? i32(t.nparams - 1) : 0;
  // wasi:sockets error-code
  constexpr uint8_t kInvalidArgument = 3, kNotInProgress = 7, kWouldBlock = 8, kInvalidState = 9,
                    kAddressInUse = 12, kConnectionRefused = 14;
  auto ok = [&]() -> uint64_t {
    Put8(inst, retptr, 0);
    return 0;
  };
  auto fail = [&](uint32_t at, uint8_t code) -> uint64_t {
    Put8(inst, retptr, 1);
    Put8(inst, retptr + at, code);
    return 0;
  };
  // An ip-socket-address from its flattened variant: case, then port, then
  // the address (ipv4: four octets; ipv6: flow info, eight words, scope).
  auto endpoint = [&](uint32_t k) {
    wns::Endpoint e;
    e.v6 = i32(k) != 0;
    e.port = static_cast<uint16_t>(i32(k + 1));
    if (!e.v6) {
      for (int i = 0; i < 4; i++) e.v4.o[i] = static_cast<uint8_t>(i32(k + 2 + i));
    } else {
      for (int i = 0; i < 8; i++) {
        uint32_t word = i32(k + 3 + i);
        e.ipv6.o[2 * i] = static_cast<uint8_t>(word >> 8);
        e.ipv6.o[2 * i + 1] = static_cast<uint8_t>(word);
      }
    }
    return e;
  };
  // result<ip-socket-address, error-code>
  auto put_address = [&](const wns::Endpoint& e) -> uint64_t {
    Put8(inst, retptr, 0);
    Put8(inst, retptr + 4, e.v6 ? 1 : 0);
    uint16_t port = e.port;
    Put(inst, retptr + 8, &port, 2);
    if (!e.v6) {
      Put(inst, retptr + 10, e.v4.o, 4);
    } else {
      Put32(inst, retptr + 12, 0);
      for (int i = 0; i < 8; i++) {
        uint16_t word = static_cast<uint16_t>(e.ipv6.o[2 * i] << 8 | e.ipv6.o[2 * i + 1]);
        Put(inst, retptr + 16 + 2 * i, &word, 2);
      }
      Put32(inst, retptr + 32, 0);
    }
    return 0;
  };
  auto streams = [&](const std::shared_ptr<Sock>& s, uint32_t at) {
    uint32_t in = NewRes(kSockIn, 0);
    res_[in - 1].sock = s;
    uint32_t out = NewRes(kSockOut, 0);
    res_[out - 1].sock = s;
    Put32(inst, retptr + at, in);
    Put32(inst, retptr + at + 4, out);
  };

  std::string name = t.name.substr(t.name.find(' ') + 1);
  if (name == "create-tcp-socket") {
    if (i32(0) > 1) return fail(4, kInvalidArgument);
    uint32_t h = NewRes(kTcpSocket, 0);
    res_[h - 1].sock = std::make_shared<Sock>();
    res_[h - 1].sock->v6 = i32(0) == 1;
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, h);
    return 0;
  }
  if (name == "create-udp-socket") {
    if (i32(0) > 1) return fail(4, kInvalidArgument);
    uint32_t h = NewRes(kUdpSocket, 0);
    res_[h - 1].sock = std::make_shared<Sock>();
    res_[h - 1].sock->udp = true;
    res_[h - 1].sock->v6 = i32(0) == 1;
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, h);
    return 0;
  }
  if (name.rfind("[method]tcp-socket.", 0) != 0) return Udp(t, inst, a);
  Res* r = Get(i32(0));
  if (r == nullptr || r->kind != kTcpSocket || !r->sock) return Fault("wasi: " + t.name + ": no such socket");
  std::shared_ptr<Sock> sock = r->sock;
  Sock& s = *sock;
  std::string method = name.substr(std::string("[method]tcp-socket.").size());

  if (method == "start-bind") {
    if (s.state != Sock::kFresh) return fail(1, kInvalidState);
    s.local = endpoint(2);
    s.state = Sock::kBound;
    return ok();
  }
  if (method == "finish-bind") return s.state == Sock::kBound ? ok() : fail(1, kNotInProgress);
  if (method == "start-listen") {
    if (s.state != Sock::kBound) return fail(1, kInvalidState);
    if (!s.listener.Listen(s.local)) return fail(1, kAddressInUse);
    s.local = s.listener.local();
    s.state = Sock::kListening;
    return ok();
  }
  if (method == "finish-listen") return s.state == Sock::kListening ? ok() : fail(1, kNotInProgress);
  if (method == "accept") {
    if (s.state != Sock::kListening) return fail(4, kInvalidState);
    wns::Endpoint peer;
    wns::SockHandle h = s.listener.Accept(&peer);
    if (h == wns::kBadSock) return fail(4, kWouldBlock);
    auto conn = std::make_shared<Sock>();
    conn->v6 = s.v6;
    conn->conn.Adopt(h, peer);
    conn->remote = peer;
    conn->local = conn->conn.local();
    conn->state = Sock::kConnected;
    uint32_t sh = NewRes(kTcpSocket, 0);
    res_[sh - 1].sock = conn;
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, sh);
    streams(conn, 8);
    return 0;
  }
  if (method == "start-connect") {
    if (s.state == Sock::kListening || s.state == Sock::kConnected || s.connect_started)
      return fail(1, kInvalidState);
    wns::Endpoint to = endpoint(2);
    bool connected = false;
    // A connect blocks; others take their turns meanwhile.
    kernel_unlocked([&] { connected = sock->conn.Connect(to); });
    s.connect_started = true;
    s.connect_error = connected ? 0 : kConnectionRefused;
    if (connected) {
      s.remote = to;
      s.local = s.conn.local();
    }
    return ok();
  }
  if (method == "finish-connect") {
    if (!s.connect_started) return fail(4, kNotInProgress);
    s.connect_started = false;
    if (s.connect_error != 0) return fail(4, s.connect_error);
    s.state = Sock::kConnected;
    Put8(inst, retptr, 0);
    streams(sock, 4);
    return 0;
  }
  if (method == "local-address") {
    if (s.state == Sock::kFresh) return fail(4, kInvalidState);
    return put_address(s.local);
  }
  if (method == "remote-address") {
    if (s.state != Sock::kConnected) return fail(4, kInvalidState);
    return put_address(s.remote);
  }
  if (method == "is-listening") return s.state == Sock::kListening ? 1 : 0;
  if (method == "subscribe") {
    uint32_t p = NewRes(kPollable, 0);
    res_[p - 1].sock = sock;
    res_[p - 1].watch = s.state == Sock::kListening ? kWatchRead : kWatchNone;
    return p;
  }
  if (method == "shutdown") {
    if (s.state != Sock::kConnected) return fail(1, kInvalidState);
    if (i32(1) != 0) s.conn.ShutdownWrite();  // send or both
    return ok();
  }
  // Options: every setter succeeds; a getter answers what the kernel uses.
  if (method.rfind("set-", 0) == 0) return ok();
  if (method == "keep-alive-enabled") {
    Put8(inst, retptr, 0);
    Put8(inst, retptr + 1, 0);
    return 0;
  }
  if (method == "hop-limit") {
    Put8(inst, retptr, 0);
    Put8(inst, retptr + 1, 64);
    return 0;
  }
  if (method == "keep-alive-count") {
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, 9);
    return 0;
  }
  if (method == "keep-alive-idle-time" || method == "keep-alive-interval" ||
      method == "receive-buffer-size" || method == "send-buffer-size") {
    Put8(inst, retptr, 0);
    Put64(inst, retptr + 8, method.find("buffer") != std::string::npos ? 65536 : 7200000000000ull);
    return 0;
  }
  return Fault("wasi: " + t.name + " is not supported");
#else
  (void)inst;
  (void)a;
  return Fault("wasi: " + t.name + " is not supported");
#endif
}

#if defined(WASMGOCOS_HAS_NET)
namespace {

// An ip-socket-address in guest memory at `at`: the variant's case, then
// its payload four bytes on (port, then the ipv4 octets or the ipv6 flow
// info, words, and scope).
wns::Endpoint EndpointAt(const uint8_t* at) {
  wns::Endpoint e;
  e.v6 = at[0] != 0;
  std::memcpy(&e.port, at + 4, 2);
  if (!e.v6) {
    std::memcpy(e.v4.o, at + 6, 4);
  } else {
    for (int i = 0; i < 8; i++) {
      uint16_t word;
      std::memcpy(&word, at + 12 + 2 * i, 2);
      e.ipv6.o[2 * i] = static_cast<uint8_t>(word >> 8);
      e.ipv6.o[2 * i + 1] = static_cast<uint8_t>(word);
    }
  }
  return e;
}

void PutEndpoint(uint8_t* at, const wns::Endpoint& e) {
  std::memset(at, 0, 32);
  at[0] = e.v6 ? 1 : 0;
  std::memcpy(at + 4, &e.port, 2);
  if (!e.v6) {
    std::memcpy(at + 6, e.v4.o, 4);
  } else {
    for (int i = 0; i < 8; i++) {
      uint16_t word = static_cast<uint16_t>(e.ipv6.o[2 * i] << 8 | e.ipv6.o[2 * i + 1]);
      std::memcpy(at + 12 + 2 * i, &word, 2);
    }
  }
}

}  // namespace
#endif

uint64_t WasiHost::Udp(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a) {
#if defined(WASMGOCOS_HAS_NET)
  auto i32 = [&](uint32_t k) { return static_cast<uint32_t>(a[k]); };
  uint32_t retptr = t.nparams > 0 ? i32(t.nparams - 1) : 0;
  constexpr uint8_t kInvalidArgument = 3, kNotInProgress = 7, kInvalidState = 9, kAddressInUse = 12;
  auto ok = [&]() -> uint64_t {
    Put8(inst, retptr, 0);
    return 0;
  };
  auto fail = [&](uint32_t at, uint8_t code) -> uint64_t {
    Put8(inst, retptr, 1);
    Put8(inst, retptr + at, code);
    return 0;
  };
  // The flattened ip-socket-address at argument k: case, port, address.
  auto endpoint = [&](uint32_t k) {
    uint8_t raw[32] = {};
    raw[0] = static_cast<uint8_t>(i32(k));
    uint16_t port = static_cast<uint16_t>(i32(k + 1));
    std::memcpy(raw + 4, &port, 2);
    if (raw[0] == 0) {
      for (int i = 0; i < 4; i++) raw[6 + i] = static_cast<uint8_t>(i32(k + 2 + i));
    } else {
      for (int i = 0; i < 8; i++) {
        uint16_t word = static_cast<uint16_t>(i32(k + 3 + i));
        std::memcpy(raw + 12 + 2 * i, &word, 2);
      }
    }
    return EndpointAt(raw);
  };
  auto put_address = [&](const wns::Endpoint& e) -> uint64_t {
    uint8_t* at = Mem(inst, retptr, 40);
    if (at == nullptr) return Fault("wasi: pointer outside guest memory");
    at[0] = 0;
    PutEndpoint(at + 4, e);
    return 0;
  };

  std::string name = t.name.substr(t.name.find(' ') + 1);
  Res* r = Get(i32(0));
  if (r == nullptr || !r->sock || !r->sock->udp) return Fault("wasi: " + t.name + ": no such socket");
  std::shared_ptr<Sock> sock = r->sock;
  Sock& s = *sock;

  if (name == "[method]udp-socket.start-bind") {
    if (s.state != Sock::kFresh) return fail(1, kInvalidState);
    if (!s.dgram.Bind(endpoint(2))) return fail(1, kAddressInUse);
    s.local = s.dgram.local();
    s.state = Sock::kBound;
    return ok();
  }
  if (name == "[method]udp-socket.finish-bind") return s.state == Sock::kBound ? ok() : fail(1, kNotInProgress);
  if (name == "[method]udp-socket.stream") {
    // An unbound socket gets an ephemeral port.
    if (s.state == Sock::kFresh) {
      if (!s.dgram.Open(s.v6)) return fail(4, kAddressInUse);
      s.local = s.dgram.local();
      s.state = Sock::kBound;
    }
    s.dgram_connected = i32(1) != 0;
    if (s.dgram_connected) s.dgram_remote = endpoint(2);
    uint32_t in = NewRes(kDgramIn, 0);
    res_[in - 1].sock = sock;
    uint32_t out = NewRes(kDgramOut, 0);
    res_[out - 1].sock = sock;
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, in);
    Put32(inst, retptr + 8, out);
    return 0;
  }
  if (name == "[method]udp-socket.local-address") {
    if (s.state == Sock::kFresh) return fail(4, kInvalidState);
    return put_address(s.local);
  }
  if (name == "[method]udp-socket.remote-address") {
    if (!s.dgram_connected) return fail(4, kInvalidState);
    return put_address(s.dgram_remote);
  }
  if (name == "[method]udp-socket.subscribe" || name == "[method]outgoing-datagram-stream.subscribe")
    return NewRes(kPollable, 0);
  if (name == "[method]incoming-datagram-stream.subscribe") {
    uint32_t p = NewRes(kPollable, 0);
    res_[p - 1].sock = sock;
    res_[p - 1].watch = kWatchRead;
    return p;
  }
  if (name == "[method]udp-socket.unicast-hop-limit") {
    Put8(inst, retptr, 0);
    Put8(inst, retptr + 1, 64);
    return 0;
  }
  if (name == "[method]udp-socket.receive-buffer-size" || name == "[method]udp-socket.send-buffer-size") {
    Put8(inst, retptr, 0);
    Put64(inst, retptr + 8, 65536);
    return 0;
  }
  if (name.rfind("[method]udp-socket.set-", 0) == 0) return ok();
  if (name == "[method]incoming-datagram-stream.receive") {
    // Up to `max` datagrams that are here now; none is an empty list.
    uint64_t max = a[1] < 16 ? a[1] : 16;
    std::vector<std::pair<std::string, wns::Endpoint>> got;
    std::string buf(65536, '\0');
    while (got.size() < max) {
      size_t n = 0;
      wns::Endpoint from;
      wns::IoStatus st = s.dgram.RecvFrom(reinterpret_cast<uint8_t*>(buf.data()), buf.size(), &n, &from);
      if (st != wns::IoStatus::kOk) break;
      if (s.dgram_connected && from != s.dgram_remote) continue;
      got.emplace_back(buf.substr(0, n), from);
    }
    uint32_t list = Alloc(inst, 4, static_cast<uint32_t>(got.size() * 40));
    for (size_t k = 0; k < got.size() && fault_.empty(); k++) {
      uint32_t data = AllocBytes(inst, got[k].first);
      uint8_t* at = Mem(inst, list + 40 * k, 40);
      if (at == nullptr) return Fault("wasi: pointer outside guest memory");
      std::memcpy(at, &data, 4);
      uint32_t len = static_cast<uint32_t>(got[k].first.size());
      std::memcpy(at + 4, &len, 4);
      PutEndpoint(at + 8, got[k].second);
    }
    if (!fault_.empty()) return 0;
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, list);
    Put32(inst, retptr + 8, static_cast<uint32_t>(got.size()));
    return 0;
  }
  if (name == "[method]outgoing-datagram-stream.check-send") {
    Put8(inst, retptr, 0);
    Put64(inst, retptr + 8, 64);
    return 0;
  }
  if (name == "[method]outgoing-datagram-stream.send") {
    uint32_t list = i32(1), count = i32(2);
    uint8_t* items = Mem(inst, list, uint64_t{count} * 44);
    if (items == nullptr) return Fault("wasi: pointer outside guest memory");
    uint64_t sent = 0;
    for (uint32_t k = 0; k < count; k++) {
      const uint8_t* item = items + 44 * k;
      uint32_t data = 0, len = 0;
      std::memcpy(&data, item, 4);
      std::memcpy(&len, item + 4, 4);
      uint8_t* bytes = Mem(inst, data, len);
      if (bytes == nullptr) return Fault("wasi: pointer outside guest memory");
      wns::Endpoint to;
      if (item[8] != 0) to = EndpointAt(item + 12);
      else if (s.dgram_connected) to = s.dgram_remote;
      else return fail(8, kInvalidArgument);
      if (s.dgram.SendTo(std::span<const uint8_t>(bytes, len), to) != wns::IoStatus::kOk) break;
      ++sent;
    }
    Put8(inst, retptr, 0);
    Put64(inst, retptr + 8, sent);
    return 0;
  }
  return Fault("wasi: " + t.name + " is not supported");
#else
  (void)inst;
  (void)a;
  return Fault("wasi: " + t.name + " is not supported");
#endif
}

uint64_t WasiHost::Fs(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a) {
  auto i32 = [&](uint32_t k) { return static_cast<uint32_t>(a[k]); };
  uint32_t retptr = t.nparams > 0 ? i32(t.nparams - 1) : 0;
  // wasi:filesystem error-code
  constexpr uint8_t kBadDescriptor = 3, kExist = 7, kInvalid = 12, kIo = 13, kIsDirectory = 14,
                    kNoEntry = 20, kNotDirectory = 24, kNotEmpty = 25, kUnsupported = 27;
  // descriptor-type
  constexpr uint8_t kCharDevice = 2, kDirectory = 3, kRegularFile = 6;
  std::string name = t.name.substr(t.name.find(' ') + 1);

  if (name == "get-directories") {
    // One preopen: `/`, readable, writable, and the whole namespace under it.
    uint32_t h = NewRes(kDescriptor, 0);
    res_[h - 1].path = "/";
    res_[h - 1].dir = true;
    res_[h - 1].flags = 1 | 2 | 32;
    uint32_t text = AllocBytes(inst, "/");
    uint32_t list = Alloc(inst, 4, 12);
    if (!fault_.empty()) return 0;
    Put32(inst, list, h);
    Put32(inst, list + 4, text);
    Put32(inst, list + 8, 1);
    Put32(inst, retptr, list);
    Put32(inst, retptr + 4, 1);
    return 0;
  }

  // An error in a result whose ok payload sits `at` bytes in.
  auto fail = [&](uint32_t at, uint8_t code) -> uint64_t {
    Put8(inst, retptr, 1);
    Put8(inst, retptr + at, code);
    return 0;
  };
  uint32_t err_at = t.err_at != 0 ? t.err_at : 1;
  Res* self = Get(i32(0));
  if (name == "[method]directory-entry-stream.read-directory-entry") {
    if (self == nullptr || self->kind != kDirStream) return fail(4, kBadDescriptor);
    Put8(inst, retptr, 0);
    if (self->next >= self->entries.size()) {
      Put8(inst, retptr + 4, 0);  // none: the end
      return 0;
    }
    const auto& e = self->entries[self->next++];
    uint32_t text = AllocBytes(inst, e.first);
    if (!fault_.empty()) return 0;
    Put8(inst, retptr + 4, 1);
    Put8(inst, retptr + 8, e.second ? kDirectory : kRegularFile);
    Put32(inst, retptr + 12, text);
    Put32(inst, retptr + 16, static_cast<uint32_t>(e.first.size()));
    return 0;
  }
  if (self == nullptr || self->kind != kDescriptor) return fail(err_at, kBadDescriptor);
  Res& d = *self;

  // A path relative to a directory descriptor, as the kernel walks it.
  auto at_path = [&](uint32_t ptr, uint32_t len, std::string* walked) -> bool {
    std::string rel;
    if (!Str(inst, ptr, len, &rel)) return false;
    std::string abs = !rel.empty() && rel[0] == '/' ? rel : (d.path == "/" ? "/" : d.path + "/") + rel;
    abs = wasmturbo::Vfs::Canon(abs);
    *walked = files::Walk(abs);
    return true;
  };
  // result<descriptor-stat, error-code>: the record starts at 8. The
  // origin filesystem keeps one time, when the bytes or entries last
  // changed: data-modification (option<datetime> at 56) and status-change
  // (at 80). Access (at 32) is none.
  auto put_stat = [&](uint8_t type, uint64_t size, int64_t mtime) -> uint64_t {
    uint8_t* at = Mem(inst, retptr, 104);
    if (at == nullptr) return Fault("wasi: pointer outside guest memory");
    std::memset(at, 0, 104);
    at[8] = type;
    uint64_t links = 1;
    std::memcpy(at + 16, &links, 8);
    std::memcpy(at + 24, &size, 8);
    if (mtime > 0) {
      uint64_t seconds = static_cast<uint64_t>(mtime / 1000000000);
      uint32_t nanos = static_cast<uint32_t>(mtime % 1000000000);
      for (uint32_t off : {56u, 80u}) {
        at[off] = 1;
        std::memcpy(at + off + 8, &seconds, 8);
        std::memcpy(at + off + 16, &nanos, 4);
      }
    }
    return 0;
  };
  // A file this guest has written and not yet written back changed now.
  auto now_ns = [] {
    return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count());
  };
  auto type_of = [&](const files::Stat& st) {
    return st.kind == files::Kind::kDir ? kDirectory : st.kind == files::Kind::kDevice ? kCharDevice : kRegularFile;
  };
  auto stream = [&](int kind, uint64_t offset, bool append) -> uint64_t {
    if (d.dir) return fail(4, kIsDirectory);
    uint32_t h = NewRes(kind, 0);
    res_[h - 1].file = d.file;
    res_[h - 1].offset = offset;
    res_[h - 1].append = append;
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, h);
    return 0;
  };

  // new-timestamp for data-modification, flattened from a[k]: no-change
  // (0), now (1), or a timestamp (2: seconds, nanoseconds). The origin
  // filesystem keeps no access time, so that one is ignored.
  auto set_time = [&](const std::string& walked, uint32_t k) -> uint64_t {
    uint32_t disc = i32(k);
    if (disc == 0) {
      Put8(inst, retptr, 0);
      return 0;
    }
    int64_t mtime = disc == 1 ? now_ns() : static_cast<int64_t>(a[k + 1]) * 1000000000 + i32(k + 2);
    if (!files::SetTime(walked, mtime)) return fail(1, kNoEntry);
    Put8(inst, retptr, 0);
    return 0;
  };

  std::string method = name.substr(std::string("[method]descriptor.").size());
  if (method == "read-via-stream") return stream(kFileIn, a[1], false);
  if (method == "write-via-stream") return stream(kFileOut, a[1], false);
  if (method == "append-via-stream") return stream(kFileOut, 0, true);
  if (method == "get-flags") {
    Put8(inst, retptr, 0);
    Put8(inst, retptr + 1, d.flags);
    return 0;
  }
  if (method == "get-type") {
    Put8(inst, retptr, 0);
    Put8(inst, retptr + 1, d.dir ? kDirectory : kRegularFile);
    return 0;
  }
  if (method == "set-size") {
    if (d.dir) return fail(1, kIsDirectory);
    d.file->data.resize(static_cast<size_t>(a[1]));
    d.file->dirty = true;
    Put8(inst, retptr, 0);
    return 0;
  }
  if (method == "set-times") {
    // (self, atime: 3 words, mtime: 3 words, retptr). An open file is
    // written back first, so its write does not stamp over the time.
    if (d.file && Sync(*d.file) != 0) return fail(1, kIo);
    return set_time(d.dir ? files::Walk(d.path) : d.file->walked, 4);
  }
  if (method == "sync" || method == "sync-data") {
    if (d.file && Sync(*d.file) != 0) return fail(1, kIo);
    Put8(inst, retptr, 0);
    return 0;
  }
  if (method == "stat") {
    if (d.dir) return put_stat(kDirectory, 0, files::StatOf(files::Walk(d.path)).mtime);
    int64_t mtime = d.file->dirty ? now_ns() : files::StatOf(d.file->walked).mtime;
    return put_stat(kRegularFile, d.file->data.size(), mtime);
  }
  if (method == "metadata-hash" || method == "metadata-hash-at") {
    std::string walked = d.dir ? files::Walk(d.path) : d.file->walked;
    if (method == "metadata-hash-at" && !at_path(i32(2), i32(3), &walked)) return 0;
    files::Stat st = files::StatOf(walked);
    if (st.kind == files::Kind::kNone) return fail(8, kNoEntry);
    uint64_t lower = 1469598103934665603ull;
    for (char c : walked) lower = (lower ^ static_cast<uint8_t>(c)) * 1099511628211ull;
    Put8(inst, retptr, 0);
    Put64(inst, retptr + 8, lower);
    Put64(inst, retptr + 16, st.size);
    return 0;
  }
  if (!d.dir) return fail(err_at, kNotDirectory);  // every -at method needs a directory

  std::string walked;
  if (method == "stat-at") {
    if (!at_path(i32(2), i32(3), &walked)) return 0;
    files::Stat st = files::StatOf(walked);
    if (st.kind == files::Kind::kNone) return fail(8, kNoEntry);
    uint64_t size = st.size;
    int64_t mtime = st.mtime;
    for (const Res& r : res_) {  // a file this guest has open and not written back
      if (r.kind == kDescriptor && r.file && r.file->walked == walked && r.file->dirty) {
        size = r.file->data.size();
        mtime = now_ns();
      }
    }
    return put_stat(type_of(st), size, mtime);
  }
  if (method == "set-times-at") {
    // (self, path-flags, path, atime: 3 words, mtime: 3 words, retptr)
    if (!at_path(i32(2), i32(3), &walked)) return 0;
    for (Res& r : res_) {
      if (r.kind == kDescriptor && r.file && r.file->walked == walked && Sync(*r.file) != 0) return fail(1, kIo);
    }
    return set_time(walked, 7);
  }
  if (method == "open-at") {
    if (!at_path(i32(2), i32(3), &walked)) return 0;
    uint32_t oflags = i32(4);
    uint8_t dflags = static_cast<uint8_t>(i32(5));
    bool create = (oflags & 1) != 0, directory = (oflags & 2) != 0, exclusive = (oflags & 4) != 0,
         truncate = (oflags & 8) != 0;
    files::Stat st = files::StatOf(walked);
    if (st.kind != files::Kind::kNone && create && exclusive) return fail(4, kExist);
    if (st.kind == files::Kind::kNone) {
      if (!create || directory) return fail(4, kNoEntry);
      if (!files::Write(walked, "")) return fail(4, kNoEntry);
      st.kind = files::Kind::kFile;
    }
    if (directory && st.kind != files::Kind::kDir) return fail(4, kNotDirectory);
    uint32_t h = NewRes(kDescriptor, 0);
    Res& nd = res_[h - 1];
    std::string rel;
    Str(inst, i32(2), i32(3), &rel);
    nd.path = wasmturbo::Vfs::Canon(!rel.empty() && rel[0] == '/' ? rel : (d.path == "/" ? "/" : d.path + "/") + rel);
    nd.flags = dflags;
    if (st.kind == files::Kind::kDir) {
      if ((dflags & 2) != 0) {
        nd.kind = kFree;
        return fail(4, kIsDirectory);
      }
      nd.dir = true;
    } else {
      nd.file = std::make_shared<GFile>();
      nd.file->walked = walked;
      nd.file->device = st.kind == files::Kind::kDevice;
      if (truncate) {
        nd.file->dirty = true;
      } else if (!files::Read(walked, &nd.file->data)) {
        nd.kind = kFree;
        return fail(4, kIo);
      }
    }
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, h);
    return 0;
  }
  if (method == "read-directory") {
    std::string at = files::Walk(d.path);
    uint32_t h = NewRes(kDirStream, 0);
    if (!files::List(at, &res_[h - 1].entries)) {
      res_[h - 1].kind = kFree;
      return fail(4, kNotDirectory);
    }
    Put8(inst, retptr, 0);
    Put32(inst, retptr + 4, h);
    return 0;
  }
  if (method == "create-directory-at") {
    if (!at_path(i32(1), i32(2), &walked)) return 0;
    if (files::StatOf(walked).kind != files::Kind::kNone) return fail(1, kExist);
    if (!files::MakeDir(walked)) return fail(1, kNoEntry);
    Put8(inst, retptr, 0);
    return 0;
  }
  if (method == "remove-directory-at" || method == "unlink-file-at") {
    if (!at_path(i32(1), i32(2), &walked)) return 0;
    files::Stat st = files::StatOf(walked);
    if (st.kind == files::Kind::kNone) return fail(1, kNoEntry);
    bool rmdir = method == "remove-directory-at";
    if (rmdir && st.kind != files::Kind::kDir) return fail(1, kNotDirectory);
    if (!rmdir && st.kind == files::Kind::kDir) return fail(1, kIsDirectory);
    if (rmdir) {
      std::vector<std::pair<std::string, bool>> entries;
      if (files::List(walked, &entries) && !entries.empty()) return fail(1, kNotEmpty);
    }
    if (!files::Remove(walked, rmdir)) return fail(1, kIo);
    Put8(inst, retptr, 0);
    return 0;
  }
  if (method == "rename-at") {
    if (!at_path(i32(1), i32(2), &walked)) return 0;
    Res* to_dir = Get(i32(3));
    if (to_dir == nullptr || to_dir->kind != kDescriptor || !to_dir->dir) return fail(1, kBadDescriptor);
    std::string rel;
    if (!Str(inst, i32(4), i32(5), &rel)) return 0;
    std::string to = files::Walk(wasmturbo::Vfs::Canon(
        !rel.empty() && rel[0] == '/' ? rel : (to_dir->path == "/" ? "/" : to_dir->path + "/") + rel));
    files::Stat st = files::StatOf(walked);
    if (st.kind == files::Kind::kNone) return fail(1, kNoEntry);
    if (st.kind == files::Kind::kFile && files::StatOf(to).kind == files::Kind::kDir) return fail(1, kIsDirectory);
    if (!files::Rename(walked, to)) return fail(1, kInvalid);
    Put8(inst, retptr, 0);
    return 0;
  }
  // link-at, symlink-at, readlink-at: the origin filesystem has no links.
  return fail(err_at, kUnsupported);
}

uint64_t WasiHost::Fault(const std::string& why) {
  if (fault_.empty()) fault_ = why;
  wasmturbo_exn_terminate();
  return 0;
}

uint8_t* WasiHost::Mem(wasmturbo::Instance* inst, uint64_t ptr, uint64_t len) {
  // No instance: GoclibcLocal, whose pointers are this module's own.
  if (inst == nullptr) return ptr == 0 && len != 0 ? nullptr : reinterpret_cast<uint8_t*>(ptr);
  const wasmturbo::CageObject& mem = inst->memory_object();
  if (mem.base == 0 || ptr > mem.size_bytes || len > mem.size_bytes - ptr) return nullptr;
  return reinterpret_cast<uint8_t*>(mem.base) + ptr;
}

bool WasiHost::Put(wasmturbo::Instance* inst, uint32_t ptr, const void* bytes, size_t n) {
  uint8_t* at = Mem(inst, ptr, n);
  if (at == nullptr) {
    Fault("wasi: pointer outside guest memory");
    return false;
  }
  std::memcpy(at, bytes, n);
  return true;
}

uint32_t WasiHost::Alloc(wasmturbo::Instance* inst, uint32_t align, uint32_t size) {
  if (size == 0) return align;  // an empty list's pointer only has to be aligned
  int32_t ptr = inst->CallI32("cabi_realloc", {0, 0, static_cast<int32_t>(align), static_cast<int32_t>(size)});
  if (wasmturbo::ExnTerminating() || wasmturbo::ExnPending()) return 0;
  if (ptr == 0 || Mem(inst, static_cast<uint32_t>(ptr), size) == nullptr) {
    Fault("wasi: cabi_realloc");
    return 0;
  }
  return static_cast<uint32_t>(ptr);
}

uint32_t WasiHost::AllocBytes(wasmturbo::Instance* inst, const std::string& s) {
  uint32_t ptr = Alloc(inst, 1, static_cast<uint32_t>(s.size()));
  if (!fault_.empty()) return 0;
  if (!s.empty()) Put(inst, ptr, s.data(), s.size());
  return ptr;
}

uint32_t WasiHost::NewRes(int kind, uint64_t deadline) {
  for (size_t i = 0; i < res_.size(); ++i) {
    if (res_[i].kind == kFree) {
      res_[i] = Res();
      res_[i].kind = kind;
      res_[i].deadline = deadline;
      return static_cast<uint32_t>(i + 1);
    }
  }
  res_.emplace_back();
  res_.back().kind = kind;
  res_.back().deadline = deadline;
  return static_cast<uint32_t>(res_.size());
}

WasiHost::Res* WasiHost::Get(uint32_t handle) {
  if (handle == 0 || handle > res_.size() || res_[handle - 1].kind == kFree) return nullptr;
  return &res_[handle - 1];
}

bool WasiHost::Ready(const Res& r) const {
  if (r.kind != kPollable) return true;
#if defined(WASMGOCOS_HAS_NET)
  if (r.watch != kWatchNone && r.sock) {
    const Sock& s = *r.sock;
    wns::SockHandle h = s.Handle();
    if (h == wns::kBadSock || s.eof) return true;
    if (r.watch == kWatchWrite && (s.udp || s.conn.pending() == 0)) return true;
    wns::Poller poller;
    poller.Watch(h, r.watch == kWatchRead, r.watch == kWatchWrite);
    poller.Wait(0);
    return r.watch == kWatchRead ? poller.Readable(h) : poller.Writable(h);
  }
#endif
  return NowNs() >= r.deadline;
}

void WasiHost::Wait(const std::vector<uint32_t>& handles) {
  uint64_t soonest = UINT64_MAX;
  bool sockets = false;
  for (uint32_t h : handles) {
    Res* r = Get(h);
    if (r == nullptr || Ready(*r)) return;
    if (r->watch != kWatchNone) sockets = true;
    else if (r->deadline < soonest) soonest = r->deadline;
  }
#if defined(WASMGOCOS_HAS_NET)
  if (sockets) {
    // Watch every socket, up to the soonest deadline and never more than
    // 50ms, with the kernel lock given up; the caller checks again.
    wns::Poller poller;
    for (uint32_t h : handles) {
      Res* r = Get(h);
      if (r == nullptr || r->watch == kWatchNone || !r->sock) continue;
      wns::SockHandle sh = r->sock->Handle();
      if (sh != wns::kBadSock) poller.Watch(sh, r->watch == kWatchRead, r->watch == kWatchWrite);
    }
    uint64_t now = NowNs();
    uint64_t ms = soonest == UINT64_MAX ? 50 : soonest > now ? (soonest - now) / 1000000 : 0;
    if (ms > 50) ms = 50;
    kernel_unlocked([&] { poller.Wait(static_cast<int>(ms)); });
    return;
  }
#endif
  if (soonest == UINT64_MAX) return;
  uint64_t now = NowNs();
  // A blocking wait gives the kernel lock up (include/gocos/klock.hpp).
  if (soonest > now)
    kernel_unlocked([&] { std::this_thread::sleep_for(std::chrono::nanoseconds(soonest - now)); });
}

uint64_t WasiHost::Dispatch(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a) {
  ++calls_;
  // GOCOS_WASI_TRACE=1: every import call and its result on stderr, with
  // the result byte a retptr call wrote.
  static const bool trace = [] {
    const char* v = std::getenv("GOCOS_WASI_TRACE");
    return v != nullptr && v[0] == '1';
  }();
  if (trace) {
    uint32_t retptr = t.nparams > 0 ? static_cast<uint32_t>(a[t.nparams - 1]) : 0;
    uint64_t rc = DispatchOp(t, inst, a);
    uint8_t* at = t.nparams > 0 ? Mem(inst, retptr, 8) : nullptr;
    std::fprintf(stderr, "wasi %s -> %llu", t.name.c_str(), static_cast<unsigned long long>(rc));
    if (at != nullptr) std::fprintf(stderr, " [ret %u %u]", at[0], at[4]);
    std::fprintf(stderr, "\n");
    return rc;
  }
  return DispatchOp(t, inst, a);
}

uint64_t WasiHost::DispatchOp(const Thunk& t, wasmturbo::Instance* inst, const uint64_t* a) {
  auto i32 = [&](uint32_t k) { return static_cast<uint32_t>(a[k]); };
  uint32_t retptr = t.nparams > 0 ? i32(t.nparams - 1) : 0;
  switch (t.op) {
    case kUnsupported:
      return Fault("wasi: " + t.name + " is not supported");
    case kDrop: {
      Res* r = Get(i32(0));
      uint32_t h = i32(0);
      if (r != nullptr && h != stdin_ && h != stdout_ && h != stderr_) {
        if (r->file) Sync(*r->file);
        r->file.reset();
        r->entries.clear();
        r->kind = kFree;
        r->sock.reset();  // the socket closes with its last handle
        r->watch = kWatchNone;
      }
      return 0;
    }
    case kGetEnvironment:
    case kGetArguments: {
      bool env = t.op == kGetEnvironment;
      uint32_t n = static_cast<uint32_t>(env ? env_.size() : args_.size());
      uint32_t stride = env ? 16 : 8;
      uint32_t list = Alloc(inst, 4, n * stride);
      for (uint32_t k = 0; k < n && fault_.empty(); ++k) {
        const std::string& first = env ? env_[k].first : args_[k];
        uint32_t p0 = AllocBytes(inst, first);
        Put32(inst, list + k * stride, p0);
        Put32(inst, list + k * stride + 4, static_cast<uint32_t>(first.size()));
        if (env) {
          uint32_t p1 = AllocBytes(inst, env_[k].second);
          Put32(inst, list + k * stride + 8, p1);
          Put32(inst, list + k * stride + 12, static_cast<uint32_t>(env_[k].second.size()));
        }
      }
      if (!fault_.empty()) return 0;
      Put32(inst, retptr, list);
      Put32(inst, retptr + 4, n);
      return 0;
    }
    case kExit:
    case kExitWithCode:
      exited_ = true;
      exit_code_ = t.op == kExit ? (i32(0) == 0 ? 0 : 1) : static_cast<int>(i32(0) & 0xff);
      wasmturbo_exn_terminate();
      return 0;
    case kPollableBlock: {
      Res* r = Get(i32(0));
      while (r != nullptr && !Ready(*r) && !wasmturbo::ExnTerminating()) {
        Wait({i32(0)});
        r = Get(i32(0));
      }
      return 0;
    }
    case kPoll: {
      uint32_t ptr = i32(0), n = i32(1);
      uint8_t* list = Mem(inst, ptr, uint64_t{n} * 4);
      if (list == nullptr) return Fault("wasi: pointer outside guest memory");
      std::vector<uint32_t> handles(n);
      std::memcpy(handles.data(), list, uint64_t{n} * 4);
      // At least one is ready when poll returns, as the spec asks.
      std::vector<uint32_t> ready;
      for (int pass = 0; ready.empty() && !wasmturbo::ExnTerminating(); ++pass) {
        if (pass > 0) Wait(handles);
        for (uint32_t k = 0; k < n; ++k) {
          Res* r = Get(handles[k]);
          if (r == nullptr || Ready(*r)) ready.push_back(k);
        }
      }
      if (wasmturbo::ExnTerminating()) return 0;
      uint32_t out = Alloc(inst, 4, static_cast<uint32_t>(ready.size() * 4));
      if (!fault_.empty()) return 0;
      if (!ready.empty()) Put(inst, out, ready.data(), ready.size() * 4);
      Put32(inst, retptr, out);
      Put32(inst, retptr + 4, static_cast<uint32_t>(ready.size()));
      return 0;
    }
    case kInputRead: {
#if defined(WASMGOCOS_HAS_NET)
      Res* r = Get(i32(0));
      if (r != nullptr && r->kind == kSockIn && r->sock && !r->sock->eof) {
        Sock& s = *r->sock;
        uint64_t want = a[1] < 65536 ? a[1] : 65536;
        std::string buf(static_cast<size_t>(want), '\0');
        size_t got = 0;
        wns::IoStatus st = want == 0 ? wns::IoStatus::kAgain
                                     : s.conn.Recv(reinterpret_cast<uint8_t*>(buf.data()), buf.size(), &got);
        if (st == wns::IoStatus::kOk || st == wns::IoStatus::kAgain) {
          buf.resize(st == wns::IoStatus::kOk ? got : 0);
          uint32_t ptr = AllocBytes(inst, buf);
          if (!fault_.empty()) return 0;
          Put8(inst, retptr, 0);
          Put32(inst, retptr + 4, ptr);
          Put32(inst, retptr + 8, static_cast<uint32_t>(buf.size()));
          return 0;
        }
        s.eof = true;
      }
#endif
      if (r != nullptr && r->kind == kFileIn && r->file) {
        const std::string& data = r->file->data;
        if (r->offset < data.size()) {
          uint64_t n = data.size() - r->offset;
          if (n > a[1]) n = a[1];
          if (n > 65536) n = 65536;
          uint32_t ptr = AllocBytes(inst, data.substr(static_cast<size_t>(r->offset), static_cast<size_t>(n)));
          if (!fault_.empty()) return 0;
          r->offset += n;
          Put8(inst, retptr, 0);
          Put32(inst, retptr + 4, ptr);
          Put32(inst, retptr + 8, static_cast<uint32_t>(n));
          return 0;
        }
      }
      if (i32(0) == stdin_ && in_stream_) {
        // A pipeline's stream: wait for bytes, or its end.
        std::string got = in_stream_->Read(static_cast<size_t>(a[1] < 65536 ? a[1] : 65536));
        if (!got.empty() || a[1] == 0) {
          uint32_t ptr = AllocBytes(inst, got);
          if (!fault_.empty()) return 0;
          Put8(inst, retptr, 0);
          Put32(inst, retptr + 4, ptr);
          Put32(inst, retptr + 8, static_cast<uint32_t>(got.size()));
          return 0;
        }
      } else if (i32(0) == stdin_ && in_pos_ < in_.size()) {
        uint64_t n = in_.size() - in_pos_;
        if (n > a[1]) n = a[1];
        if (n > 65536) n = 65536;
        uint32_t ptr = AllocBytes(inst, in_.substr(in_pos_, static_cast<size_t>(n)));
        if (!fault_.empty()) return 0;
        in_pos_ += static_cast<size_t>(n);
        Put8(inst, retptr, 0);
        Put32(inst, retptr + 4, ptr);
        Put32(inst, retptr + 8, static_cast<uint32_t>(n));
        return 0;
      }
      // Stdin, and a socket at its end: result err, stream-error closed.
      Put8(inst, retptr, 1);
      Put8(inst, retptr + 4, 1);
      return 0;
    }
    case kSubscribeReady: {
      Res* r = Get(i32(0));
      if (r != nullptr && (r->kind == kSockIn || r->kind == kSockOut)) {
        uint32_t p = NewRes(kPollable, 0);
        res_[p - 1].sock = r->sock;
        res_[p - 1].watch = r->kind == kSockIn ? kWatchRead : kWatchWrite;
        return p;
      }
      return NewRes(kPollable, 0);
    }
    case kCheckWrite: {
      Res* r = Get(i32(0));
#if defined(WASMGOCOS_HAS_NET)
      if (r != nullptr && r->kind == kSockOut && r->sock && r->sock->conn.open()) {
        wns::TcpConn& conn = r->sock->conn;
        if (conn.Flush() == wns::IoStatus::kError) {
          Put8(inst, retptr, 1);
          Put8(inst, retptr + 8, 1);
          return 0;
        }
        size_t pending = conn.pending();
        Put8(inst, retptr, 0);
        Put64(inst, retptr + 8, pending >= kWritePermit ? 0 : kWritePermit - pending);
        return 0;
      }
#endif
      if (r == nullptr || (r->kind != kOutput && r->kind != kFileOut)) {
        Put8(inst, retptr, 1);
        Put8(inst, retptr + 8, 1);  // closed
        return 0;
      }
      Put8(inst, retptr, 0);
      Put64(inst, retptr + 8, kWritePermit);
      return 0;
    }
    case kWrite:
    case kWriteAndFlush: {
      uint32_t h = i32(0), ptr = i32(1), len = i32(2);
      Res* r = Get(h);
#if defined(WASMGOCOS_HAS_NET)
      if (r != nullptr && r->kind == kSockOut && r->sock && r->sock->conn.open()) {
        uint8_t* bytes = Mem(inst, ptr, len);
        if (bytes == nullptr) return Fault("wasi: pointer outside guest memory");
        wns::TcpConn& conn = r->sock->conn;
        conn.Queue(std::span<const uint8_t>(bytes, len));
        wns::IoStatus st = conn.Flush();
        while (t.op == kWriteAndFlush && st == wns::IoStatus::kAgain && fault_.empty()) {
          kernel_unlocked([&] { std::this_thread::sleep_for(std::chrono::milliseconds(1)); });
          if (wasmturbo::ExnTerminating()) return 0;
          st = conn.Flush();
        }
        Put8(inst, retptr, st == wns::IoStatus::kError ? 1 : 0);
        if (st == wns::IoStatus::kError) Put8(inst, retptr + 4, 1);
        return 0;
      }
#endif
      if (r != nullptr && r->kind == kFileOut && r->file) {
        uint8_t* bytes = Mem(inst, ptr, len);
        if (bytes == nullptr) return Fault("wasi: pointer outside guest memory");
        std::string& data = r->file->data;
        if (r->append) r->offset = data.size();
        if (r->offset + len > data.size()) data.resize(static_cast<size_t>(r->offset + len));
        std::memcpy(&data[static_cast<size_t>(r->offset)], bytes, len);
        r->offset += len;
        r->file->dirty = true;
        if (t.op == kWriteAndFlush) Sync(*r->file);
        Put8(inst, retptr, 0);
        return 0;
      }
      if (r == nullptr || r->kind != kOutput) {
        Put8(inst, retptr, 1);
        Put8(inst, retptr + 4, 1);  // closed
        return 0;
      }
      uint8_t* bytes = Mem(inst, ptr, len);
      if (bytes == nullptr) return Fault("wasi: pointer outside guest memory");
      if (h == stdout_ && out_stream_) {
        // Copied out first: the wait gives the lock up, and guest memory
        // is only the guest's while this thread holds it.
        std::string chunk(reinterpret_cast<const char*>(bytes), len);
        if (!out_stream_->Write(chunk)) {
          // A broken pipe: the reader has ended, so this guest ends too.
          exited_ = true;
          exit_code_ = 141;
          wasmturbo_exn_terminate();
          return 0;
        }
        Put8(inst, retptr, 0);
        return 0;
      }
      (h == stderr_ ? err_ : out_).append(reinterpret_cast<const char*>(bytes), len);
      Put8(inst, retptr, 0);
      return 0;
    }
    case kBlockingFlush: {
#if defined(WASMGOCOS_HAS_NET)
      Res* r = Get(i32(0));
      if (r != nullptr && r->kind == kSockOut && r->sock && r->sock->conn.open()) {
        wns::TcpConn& conn = r->sock->conn;
        wns::IoStatus st = conn.Flush();
        while (st == wns::IoStatus::kAgain) {
          kernel_unlocked([&] { std::this_thread::sleep_for(std::chrono::milliseconds(1)); });
          if (wasmturbo::ExnTerminating()) return 0;
          st = conn.Flush();
        }
        Put8(inst, retptr, st == wns::IoStatus::kError ? 1 : 0);
        if (st == wns::IoStatus::kError) Put8(inst, retptr + 4, 1);
        return 0;
      }
#endif
      Res* f = Get(i32(0));
      if (f != nullptr && f->kind == kFileOut && f->file) Sync(*f->file);
      Put8(inst, retptr, 0);
      return 0;
    }
    case kGetStdin:
      return stdin_;
    case kGetStdout:
      return stdout_;
    case kGetStderr:
      return stderr_;
    case kGetTerminal:
      Put8(inst, retptr, 0);  // none: not a terminal
      return 0;
    case kMonoNow:
      return NowNs();
    case kMonoResolution:
      return 1;
    case kSubscribeInstant:
      return NewRes(kPollable, a[0]);
    case kSubscribeDuration:
      return NewRes(kPollable, NowNs() + a[0]);
    case kWallNow: {
      auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
      Put64(inst, retptr, static_cast<uint64_t>(ns / 1000000000));
      Put32(inst, retptr + 8, static_cast<uint32_t>(ns % 1000000000));
      return 0;
    }
    case kWallResolution:
      Put64(inst, retptr, 0);
      Put32(inst, retptr + 8, 100);
      return 0;
    case kRandomBytes: {
      uint64_t n = a[0];
      if (n > (64u << 20)) return Fault("wasi: get-random-bytes too large");
      std::string bytes(static_cast<size_t>(n), '\0');
      std::random_device rd;
      for (char& c : bytes) c = static_cast<char>(rd());
      uint32_t ptr = AllocBytes(inst, bytes);
      if (!fault_.empty()) return 0;
      Put32(inst, retptr, ptr);
      Put32(inst, retptr + 4, static_cast<uint32_t>(n));
      return 0;
    }
    case kInstanceNetwork:
      return NewRes(kNetwork, 0);
    case kCreateSocket:
      Put8(inst, retptr, 1);
      Put8(inst, retptr + 4, kSockNotSupported);
      return 0;
    case kGoclibc:
      return static_cast<uint32_t>(Goclibc(t, inst, a));
    case kTcp:
      return Sockets(t, inst, a);
    case kFs:
      return Fs(t, inst, a);
  }
  return Fault("wasi: " + t.name);
}

}  // namespace wasmgocos
