#ifndef WASMGOCOS_INCLUDE_GOCOS_HV_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_HV_HPP_

// gocvm hypervision. The only hops GocKrnl / GocSys / GocDesk / GocNix
// may take: k32 (~/WASMWin32 wasi_call) and nix (~/WASMNix posix_call)
// for WSL. No libc, no kernel32 import, no wineserver.

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

namespace hv {

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
  if (has_k32() && has_nix()) return "k32,nix";
  if (has_k32()) return "k32";
  if (has_nix()) return "nix";
  return "unbound";
}

inline std::string k32(const char* api, const char* args) {
#if defined(WASMGOCOS_K32_NATIVE)
  char buf[65536];
  buf[0] = 0;
  // wasmwin32_call's return code is the only success/failure signal —
  // an empty buf on success (e.g. GetProcessOutput with nothing new
  // to read yet) is not an error, and must not be reported as one.
  int rc = wasmwin32_call(api, args ? args : "", buf, sizeof(buf));
  if (rc != 0) return err_msg(buf[0] ? buf : api);
  return std::string(buf);
#elif defined(WASMGOCOS_HAS_WIN32)
  return wasmwin32::wasi_call(api, args ? args : "");
#else
  (void)api;
  (void)args;
  return err_msg("gocvm: k32 hop required");
#endif
}

inline std::string nix(const char* api, const char* args) {
#if defined(WASMGOCOS_HAS_NIX)
  return wasmnix::posix_call(api, args ? args : "");
#else
  (void)api;
  (void)args;
  return err_msg("gocvm: nix hop required");
#endif
}

}  // namespace hv

#endif  // WASMGOCOS_INCLUDE_GOCOS_HV_HPP_
