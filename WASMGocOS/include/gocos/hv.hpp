#ifndef WASMGOCOS_INCLUDE_GOCOS_HV_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_HV_HPP_

// gocvm hypervision. The only hop GocKrnl / GocSys / GocDesk / GocNix
// may take. The machine is TurboSpace's vCPU: one WHv partition, the VM
// window at GPA kVmGpaBase. k32 (~/WASMWin32) and nix (~/WASMNix) stay
// the syscall catalogs on that hop. No libc, no kernel32 import, no
// wineserver, no wsl.exe.
//
// Native and wasm are the same hop. k32 is a vCPU exit
// (wasmturbo_vcpu_syscall). The partition behind that symbol is the
// process's WinHvPlatform partition; in a wasigocvm module the
// WinHvPlatform DLL catalog is that partition.

inline bool eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline std::string err_msg(const char* msg) {
  return std::string("error: ") + msg;
}

inline void split1f(const char* a, std::string* l, std::string* r) {
  const char* p = a ? std::strchr(a, '\x1f') : nullptr;
  if (!p) {
    *l = a ? a : "";
    r->clear();
    return;
  }
  l->assign(a, p);
  *r = p + 1;
}

inline std::string join1f(const char* a, const char* b) {
  std::string s = a ? a : "";
  s += '\x1f';
  s += b ? b : "";
  return s;
}

inline void copy_field(char* dst, size_t cap, const char* src) {
  if (!dst || cap == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  size_t n = std::strlen(src);
  if (n + 1 > cap) n = cap - 1;
  std::memcpy(dst, src, n);
  dst[n] = 0;
}

inline void copy_field(char* dst, size_t cap, const std::string& src) {
  if (src.rfind("error:", 0) == 0) {
    if (dst && cap) dst[0] = 0;
    return;
  }
  copy_field(dst, cap, src.c_str());
}

extern "C" int32_t wasmturbo_vcpu_attach(char* out, unsigned cap);
extern "C" int32_t wasmturbo_vcpu_map(uint32_t bytes, char* out, unsigned cap);
extern "C" int32_t wasmturbo_vcpu_unmap(const char* gpa);
extern "C" int32_t wasmturbo_vcpu_syscall(const char* api, const char* args, char* out,
                                          unsigned cap);
extern "C" uint32_t wasmturbo_vcpu_k32_exits(void);

namespace hv {

inline bool vcpu_attached() {
  static int state = 0;
  if (state == 0) {
    char buf[160];
    buf[0] = 0;
    state = wasmturbo_vcpu_attach(buf, sizeof(buf)) == 0 ? 1 : -1;
  }
  return state == 1;
}

inline bool has_k32() {
#if defined(WASMGOCOS_HAS_WIN32)
  return true;
#else
  return false;
#endif
}

inline bool has_nix() {
#if defined(WASMGOCOS_HAS_NIX)
  return true;
#else
  return false;
#endif
}

inline const char* name() {
  if (vcpu_attached()) {
    if (has_k32() && has_nix()) return "vcpu,k32,nix";
    if (has_k32()) return "vcpu,k32";
    if (has_nix()) return "vcpu,nix";
    return "vcpu";
  }
  if (has_k32() && has_nix()) return "k32,nix";
  if (has_k32()) return "k32";
  if (has_nix()) return "nix";
  return "unbound";
}

inline std::string k32(const char* api, const char* args) {
  if (!vcpu_attached()) return err_msg(api ? api : "vcpu");
  // Heap, not a stack array: the wasigocvm stack is one 64KB page, and
  // this reply is the same size on every build.
  std::string buf(65536, '\0');
  int rc = wasmturbo_vcpu_syscall(api, args ? args : "", buf.data(),
                                  static_cast<unsigned>(buf.size()));
  if (rc != 0) return err_msg(buf[0] ? buf.c_str() : api);
  return std::string(buf.c_str());
}

inline std::string nix(const char* api, const char* args) {
#if defined(WASMGOCOS_NIX_WSL)
  return wasmnix::wsl_call(api, args ? args : "");
#elif defined(WASMGOCOS_HAS_NIX)
  return wasmnix::posix_call(api, args ? args : "");
#else
  (void)api;
  (void)args;
  return err_msg("gocvm: nix hop required");
#endif
}

}  // namespace hv

#endif  // WASMGOCOS_INCLUDE_GOCOS_HV_HPP_
