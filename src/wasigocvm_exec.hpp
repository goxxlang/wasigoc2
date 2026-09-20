// In-guest fork+exec. The child is a real process in the second address
// space (EPT / TPT / CHPT), not a BusyBox stand-in. Work runs through
// ~/WASMWin32 (catalog on EPT, process/thread on TPT, session on CHPT).
// A .wasm payload is load/call via wasitime / WASMLoader, not rewritten
// here. Included from wasigocvm_net.hpp (after wasigocvm_libc.hpp).
#pragma once

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "wasigocvm_aspace.hpp"

#define WASIGO_ASPACE_WANT_TYPES 1
#include "wasigocvm_aspace.hpp"

namespace gocvm {

inline std::string exec_basename(const std::string& p) {
  auto s = p.find_last_of("/\\");
  std::string b = s == std::string::npos ? p : p.substr(s + 1);
  if (b.size() > 4) {
    std::string tail = b.substr(b.size() - 4);
    if (tail == ".exe" || tail == ".EXE") b.resize(b.size() - 4);
  }
  return b;
}

inline std::vector<std::string> exec_split_argv(const std::string& payload) {
  std::vector<std::string> argv;
  std::string cur;
  for (char ch : payload) {
    if (ch == '\x1f') {
      argv.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(ch);
    }
  }
  argv.push_back(cur);
  while (!argv.empty() && argv.back().empty()) argv.pop_back();
  return argv;
}

inline bool exec_is_wasm_path(const std::string& path) {
  if (path.size() >= 5) {
    std::string t = path.substr(path.size() - 5);
    if (t == ".wasm" || t == ".WASM") return true;
  }
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  char mag[4] = {};
  size_t n = std::fread(mag, 1, 4, f);
  std::fclose(f);
  return n == 4 && mag[0] == '\0' && mag[1] == 'a' && mag[2] == 's' && mag[3] == 'm';
}

inline std::string exec_join_argv(const std::vector<std::string>& argv) {
  std::string s;
  for (size_t i = 0; i < argv.size(); ++i) {
    if (i) s.push_back(' ');
    s += argv[i];
  }
  return s;
}

// Child work is WASMWin32 on EPT/TPT/CHPT — same wasi_call the win32
// topic uses, not a second command table in this file.
inline int exec_run(const std::vector<std::string>& argv, std::string* out) {
  out->clear();
  if (argv.empty() || argv[0].empty()) {
    *out = "error: exec: empty argv\n";
    return 127;
  }
  if (exec_is_wasm_path(argv[0])) {
    *out = "error: exec wasm via wasitime / WASMLoader (load/call, not rewrite)\n";
    return 127;
  }
#if defined(WASIGO_HAS_WASMWIN32)
#if WASIGO_HAS_WASMV8
  if (!win32_tables_ok()) {
    *out = "error: exec: child not in EPT/TPT/CHPT";
    return 127;
  }
#endif
  const std::string cmd = exec_basename(argv[0]);
  std::string api;
#if defined(WASIGO_HAS_WASMWIN32_CATALOG) && WASIGO_HAS_WASMV8
  if (Win32Kernel* k = win32_kernel()) {
    int n = 0;
    const auto* cat = static_cast<const WasmWin32Api*>(
        aspace().ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer));
    wasmwin32_catalog(&n);
    if (cat) {
      for (int i = 0; i < n; ++i) {
        if (cat[i].name && cmd == cat[i].name) {
          api = cat[i].name;
          break;
        }
      }
    }
  }
#endif
  std::string reply;
  if (!api.empty()) {
    std::string rest;
    for (size_t i = 1; i < argv.size(); ++i) {
      if (i > 1) rest.push_back('\x1f');
      rest += argv[i];
    }
    reply = wasmwin32::wasi_call(api.c_str(), rest.c_str());
  } else {
    reply = wasmwin32::wasi_call("CreateProcessW", exec_join_argv(argv).c_str());
  }
  if (reply.rfind("error:", 0) == 0) {
    *out = reply;
    return 1;
  }
  *out = reply;
  return 0;
#else
  *out = "error: exec: WASMWin32 required (EPT/TPT/CHPT child)";
  return 127;
#endif
}

inline bool exec_lookpath(const std::string& file, std::string* reply) {
  const std::string b = exec_basename(file);
#if defined(WASIGO_HAS_WASMWIN32_CATALOG) && WASIGO_HAS_WASMV8
  if (win32_tables_ok()) {
    if (Win32Kernel* k = win32_kernel()) {
      int n = 0;
      const auto* cat = static_cast<const WasmWin32Api*>(
          aspace().ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer));
      wasmwin32_catalog(&n);
      if (cat) {
        for (int i = 0; i < n; ++i) {
          if (cat[i].name && b == cat[i].name) {
            *reply = b;
            return true;
          }
        }
      }
    }
  }
#endif
#if defined(WASIGO_HAS_WASMWIN32)
  // WslExec surface in wasi_host.hpp (not a copy of those programs here).
  static const char* k[] = {"true", "false", "echo", "pwd", "hostname", "uname",
                            "id", ":", nullptr};
  for (int i = 0; k[i]; ++i) {
    if (b == k[i]) {
      *reply = b;
      return true;
    }
  }
#endif
  *reply = "error: exec: executable file not found in $PATH";
  return false;
}

#if WASIGO_HAS_WASMV8
struct ExecChild final : public cppgc::GarbageCollected<ExecChild> {
  v8::internal::ExternalPointerHandle out_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::TrustedPointerHandle parent_h =
      v8::internal::kNullTrustedPointerHandle;
  size_t out_len = 0;
  size_t rpos = 0;
  int exit_code = -1;
  void Trace(cppgc::Visitor*) const {}
};

inline ExecChild* exec_child_alloc() {
  ExecChild* child = cppgc::MakeGarbageCollected<ExecChild>(
      exec_cppgc_heap().GetAllocationHandle());
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!child || !exec_cage().Contains(child)) return nullptr;
#endif
  return child;
}
#endif

#if defined(WASIGO_GOCVM_HAS_PTHREAD) && WASIGO_GOCVM_HAS_PTHREAD

struct ExecProc {
  std::thread thr;
  std::mutex mu;
#if WASIGO_HAS_WASMV8
  cppgc::Persistent<ExecChild> root;  // GC root only; name is the CHPT handle
  v8::CppHeapPointerHandle chpt = v8::kNullCppHeapPointerHandle;
#endif
  char* cage_out = nullptr;
  size_t out_len = 0;
  size_t rpos = 0;
  int exit_code = -1;
  int pid = 0;
  bool done = false;
  std::thread::id tid{};

  ~ExecProc();
};

#if WASIGO_HAS_WASMV8
inline ExecChild* exec_child_of(ExecProc* p) {
  if (!p) return nullptr;
  // Parent → child is CHPT. Persistent is not an address.
  auto* child = static_cast<ExecChild*>(
      exec_aspace().chpt.Get(p->chpt, v8::kAnyCppHeapPointer));
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (child && !exec_cage().Contains(child)) return nullptr;
#endif
  return child;
}

inline ExecProc* exec_parent_of(ExecChild* child) {
  if (!child) return nullptr;
  v8::Address a = exec_aspace().tpt.Get(
      child->parent_h, v8::internal::kAllIndirectPointerTags);
  return a ? reinterpret_cast<ExecProc*>(a) : nullptr;
}

inline const char* exec_stdout_of(ExecChild* child) {
  if (!child) return nullptr;
  return static_cast<const char*>(
      exec_aspace().ept.Get(child->out_h, v8::internal::kAnyExternalPointer));
}

inline void exec_free_stdout(ExecChild* child) {
  if (!child || child->out_h == v8::internal::kNullExternalPointerHandle) return;
  void* p = exec_aspace().ept.Get(child->out_h, v8::internal::kAnyExternalPointer);
  exec_aspace().ept.FreeEntry(child->out_h);
  child->out_h = v8::internal::kNullExternalPointerHandle;
  delete[] static_cast<char*>(p);
}

inline ExecProc::~ExecProc() {
  if (ExecChild* child = exec_child_of(this)) {
    exec_free_stdout(child);
    if (child->parent_h != v8::internal::kNullTrustedPointerHandle) {
      exec_aspace().tpt.FreeEntry(child->parent_h);
      child->parent_h = v8::internal::kNullTrustedPointerHandle;
    }
  }
  if (chpt != v8::kNullCppHeapPointerHandle) {
    exec_aspace().chpt.FreeEntry(chpt);
    chpt = v8::kNullCppHeapPointerHandle;
  }
  root.Clear();
}

inline bool exec_bind_child(ExecProc* raw, std::string* err) {
  ExecChild* child = exec_child_alloc();
  if (!child) {
    if (err) *err = "error: exec: EPT/TPT/CHPT child alloc failed";
    return false;
  }
  raw->root = child;
  raw->chpt = exec_aspace().chpt.AllocateAndInitializeEntry(child, kExecChildTag);
  child->parent_h = exec_aspace().tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(raw),
      v8::internal::kGenericTrustedObjectTag);
  if (raw->chpt == v8::kNullCppHeapPointerHandle ||
      child->parent_h == v8::internal::kNullTrustedPointerHandle ||
      exec_child_of(raw) != child || exec_parent_of(child) != raw) {
    if (err) *err = "error: exec: EPT/TPT/CHPT bind failed";
    return false;
  }
  return true;
}

inline void exec_commit_child(ExecProc* raw, int code, const std::string& tmp) {
  ExecChild* child = exec_child_of(raw);
  if (!child) return;
  exec_free_stdout(child);
  child->exit_code = code;
  child->rpos = 0;
  child->out_len = tmp.size();
  if (!tmp.empty()) {
    char* buf = new char[tmp.size()];
    std::memcpy(buf, tmp.data(), tmp.size());
    child->out_h = exec_aspace().ept.AllocateAndInitializeEntry(buf, kExecStdoutTag);
  }
  raw->exit_code = code;
  raw->out_len = child->out_len;
}
#else
inline ExecProc::~ExecProc() { delete[] cage_out; }

inline void exec_commit_child(ExecProc* raw, int code, const std::string& tmp) {
  raw->cage_out = nullptr;
  raw->out_len = tmp.size();
  raw->exit_code = code;
  if (tmp.empty()) return;
  raw->cage_out = new char[tmp.size()];
  std::memcpy(raw->cage_out, tmp.data(), tmp.size());
}
#endif

struct ExecTable {
  uint64_t next_handle = 0;
  std::unordered_map<uint64_t, std::unique_ptr<ExecProc>> procs;

  bool spawn(const std::string& argv_joined, uint64_t* handle, std::string* err) {
    auto argv = exec_split_argv(argv_joined);
    auto p = std::make_unique<ExecProc>();
    ExecProc* raw = p.get();
    raw->pid = proc_alloc_id();
    uint64_t h = ++next_handle;
#if WASIGO_HAS_WASMV8
    if (!exec_bind_child(raw, err)) return false;
#endif
    // One linear memory; isolation is EPT/TPT/CHPT. The child still
    // runs on a real std::thread (CreateProcessW is the same hop).
    try {
      raw->thr = std::thread([raw, argv]() {
        std::string tmp;
        int code = exec_run(argv, &tmp);
        std::lock_guard<std::mutex> lk(raw->mu);
        exec_commit_child(raw, code, tmp);
        raw->tid = std::this_thread::get_id();
        raw->done = true;
      });
    } catch (...) {
      *err = "error: std::thread create failed";
      return false;
    }
    procs[h] = std::move(p);
    *handle = h;
    return true;
  }

  ExecProc* get(uint64_t h) {
    auto it = procs.find(h);
    return it == procs.end() ? nullptr : it->second.get();
  }

  void join_done(ExecProc* p) {
    if (!p) return;
    bool done = false;
    {
      std::lock_guard<std::mutex> lk(p->mu);
      done = p->done;
    }
    if (done && p->thr.joinable()) p->thr.join();
  }

  bool done(uint64_t h) {
    ExecProc* p = get(h);
    if (!p) return true;
    std::lock_guard<std::mutex> lk(p->mu);
    return p->done;
  }

  bool readable(uint64_t h) {
    ExecProc* p = get(h);
    if (!p) return true;
    std::lock_guard<std::mutex> lk(p->mu);
#if WASIGO_HAS_WASMV8
    if (ExecChild* c = exec_child_of(p)) {
      return p->done || c->rpos < c->out_len;
    }
#endif
    return p->done || p->rpos < p->out_len;
  }

  std::string take_out(ExecProc* p) {
#if WASIGO_HAS_WASMV8
    ExecChild* c = exec_child_of(p);
    const char* out = exec_stdout_of(c);
    if (!c || !out || c->out_len == 0) return {};
    return std::string(out, c->out_len);
#else
    if (!p || !p->cage_out || p->out_len == 0) return {};
    return std::string(p->cage_out, p->out_len);
#endif
  }

  std::string read_out(ExecProc* p, int maxlen) {
    if (maxlen <= 0 || maxlen > 60000) maxlen = 60000;
#if WASIGO_HAS_WASMV8
    ExecChild* c = exec_child_of(p);
    const char* out = exec_stdout_of(c);
    if (!c || !out) return {};
    if (c->rpos >= c->out_len) return {};
    size_t n = c->out_len - c->rpos;
    if (n > static_cast<size_t>(maxlen)) n = static_cast<size_t>(maxlen);
    std::string chunk(out + c->rpos, n);
    c->rpos += n;
    return chunk;
#else
    if (!p || !p->cage_out) return {};
    if (p->rpos >= p->out_len) return {};
    size_t n = p->out_len - p->rpos;
    if (n > static_cast<size_t>(maxlen)) n = static_cast<size_t>(maxlen);
    std::string chunk(p->cage_out + p->rpos, n);
    p->rpos += n;
    return chunk;
#endif
  }
};

#endif  // WASIGO_GOCVM_HAS_PTHREAD

}  // namespace gocvm
