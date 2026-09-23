// In-guest libc dispatch for gocvm.Call. No companion host: syscall,
// os.user, win32, linux/wsl/nix, android/binder/kvm, gocos,
// gocdesk, chrome, kill, and exec stay in this module. TLS is OpenSSL in
// wasigocvm_tls.hpp, not Schannel / gocvm_host.
#pragma once

#ifndef _WASI_EMULATED_GETPID
#define _WASI_EMULATED_GETPID 1
#endif

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "wasigocvm_aspace.hpp"

#define WASIGO_ASPACE_WANT_TYPES 1
#include "wasigocvm_aspace.hpp"

#if defined(__wasi__)
// Weak: occupancy TUs (WSMOccpuyWin32 bridge/install) also include
// runtime.hpp. One definition of the occupancy pid table is enough.
extern "C" __attribute__((weak)) pid_t getpid(void) {
  return static_cast<pid_t>(gocvm::proc_self());
}
extern "C" __attribute__((weak)) pid_t getppid(void) {
  return static_cast<pid_t>(gocvm::proc_ppid());
}
extern "C" __attribute__((weak)) long gocvm_gettid(void) {
  return static_cast<long>(gocvm::proc_tid());
}
#endif

#define WASMWIN32_WASI_HOST_NO_POSIX_HEADERS 1
#include "win32/wasi_host.hpp"
#include "win32/catalog.h"
#include "nix/posix_host.hpp"
#include "nix/catalog.h"
#include "droid/bionic_host.hpp"
#include "droid/catalog.h"
#include "gocos/host.hpp"
#include "gocos/catalog.h"
#if defined(WASIGO_HAS_WASMCHROME) && WASIGO_HAS_WASMCHROME
#include "chrome/host.hpp"
#include "chrome/catalog.h"
#endif

namespace gocvm {

inline std::string wasigocvm_username() {
  if (const char* u = std::getenv("USER")) {
    if (u[0]) return u;
  }
  if (const char* u = std::getenv("USERNAME")) {
    if (u[0]) return u;
  }
  if (const char* u = std::getenv("LOGNAME")) {
    if (u[0]) return u;
  }
  return "wasigocvm";
}

inline std::string wasigocvm_homedir() {
  if (const char* h = std::getenv("HOME")) {
    if (h[0]) return h;
  }
  if (const char* h = std::getenv("USERPROFILE")) {
    if (h[0]) return h;
  }
  char buf[4096];
  if (getcwd(buf, sizeof(buf))) return buf;
  return "/";
}

inline std::string wasigocvm_uid() {
#if defined(__wasi__)
  return "0";
#else
  return std::to_string(static_cast<long>(getuid()));
#endif
}

// os/user reply: "<uid>\x1f<username>\x1f<name>\x1f<homedir>"
inline std::string wasigocvm_fmt_user() {
  std::string name = wasigocvm_username();
  return wasigocvm_uid() + "\x1f" + name + "\x1f" + name + "\x1f" + wasigocvm_homedir();
}

inline bool wasigocvm_try_user(const std::string& payload, std::string* reply) {
  std::string uid = wasigocvm_uid();
  std::string name = wasigocvm_username();
  auto starts = [&](const char* pfx) {
    std::size_t n = std::strlen(pfx);
    return payload.size() >= n && payload.compare(0, n, pfx) == 0;
  };
  if (payload.empty() || payload == "current") {
    *reply = wasigocvm_fmt_user();
    return true;
  }
  if (starts("lookup ")) {
    std::string want = payload.substr(7);
    if (want == name || (want == "root" && uid == "0")) {
      *reply = wasigocvm_fmt_user();
    } else {
      *reply = std::string("error: os.user: unknown user ") + want;
    }
    return true;
  }
  if (starts("lookupid ")) {
    std::string want = payload.substr(9);
    if (want == uid) {
      *reply = wasigocvm_fmt_user();
    } else {
      *reply = std::string("error: os.user: unknown uid ") + want;
    }
    return true;
  }
  *reply = "error: unknown os.user op";
  return true;
}

inline bool wasigocvm_try_syscall(const std::string& payload, std::string* reply) {
  auto starts = [&](const char* pfx) {
    std::size_t n = std::strlen(pfx);
    return payload.size() >= n && payload.compare(0, n, pfx) == 0;
  };
  if (payload == "getpid") {
    *reply = std::to_string(proc_self());
    return true;
  }
  if (payload == "getppid") {
    *reply = std::to_string(proc_ppid());
    return true;
  }
  if (payload == "getcwd" || payload == "getwd") {
    char buf[4096];
    if (!getcwd(buf, sizeof(buf))) {
      *reply = std::string("error: getcwd: errno ") + std::to_string(errno);
    } else {
      *reply = buf;
    }
    return true;
  }
  if (payload == "environ") {
#if defined(__wasi__)
    char** env = __wasilibc_get_environ();
#else
    extern char** environ;
    char** env = environ;
#endif
    std::string out;
    if (env) {
      for (char** e = env; *e; ++e) {
        if (!out.empty()) out.push_back('\x1f');
        out += *e;
      }
    }
    *reply = std::move(out);
    return true;
  }
  if (starts("getenv ")) {
    const char* key = payload.c_str() + 7;
    const char* v = std::getenv(key);
    if (v)
      *reply = std::string("1|") + v;
    else
      *reply = "0|";
    return true;
  }
  if (starts("chdir ")) {
    const char* dir = payload.c_str() + 6;
    if (chdir(dir) != 0) {
      *reply = std::string("error: chdir: errno ") + std::to_string(errno);
    } else {
      *reply = "ok";
    }
    return true;
  }
  if (starts("kill ")) {
    // One address space: only this pid exists. Do not hop to a host
    // TerminateProcess. sig 0 is an existence check; other signals on
    // self succeed without aborting the module (the cage is the process).
    const char* rest = payload.c_str() + 5;
    char* end = nullptr;
    long pid = std::strtol(rest, &end, 10);
    long sig = 0;
    if (end && *end == ' ') sig = std::strtol(end + 1, nullptr, 10);
    long me = static_cast<long>(proc_self());
    if (pid != me && pid != 0) {
      *reply = "error: kill: ESRCH (single-address-space guest)";
      return true;
    }
    (void)sig;
    *reply = "ok";
    return true;
  }
  return false;
}

#if defined(WASIGO_HAS_WASMWIN32) && WASIGO_HAS_WASMV8
// WASMWin32 on the second address space (do not rewrite wasi_host.hpp):
//   CHPT — Win32Kernel session + token/SID/PEB/TEB/HWND/GDI/COM in the cage
//   TPT  — current process / thread (trusted)
//   EPT  — ~/WASMWin32 catalog, vmem, sock, CNG, loaded-module, crypt32, WinHttp, WHP, heap roots
struct Win32Kernel final : public cppgc::GarbageCollected<Win32Kernel> {
  v8::internal::TrustedPointerHandle process_h =
      v8::internal::kNullTrustedPointerHandle;
  v8::internal::TrustedPointerHandle thread_h =
      v8::internal::kNullTrustedPointerHandle;
  v8::internal::ExternalPointerHandle catalog_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle vmem_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle sock_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle cng_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle mod_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle cert_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle http_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle whp_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle heap_h =
      v8::internal::kNullExternalPointerHandle;
  v8::CppHeapPointerHandle token_h = v8::kNullCppHeapPointerHandle;
  v8::CppHeapPointerHandle sid_h = v8::kNullCppHeapPointerHandle;
  v8::CppHeapPointerHandle peb_h = v8::kNullCppHeapPointerHandle;
  v8::CppHeapPointerHandle teb_h = v8::kNullCppHeapPointerHandle;
  v8::CppHeapPointerHandle wnd_h = v8::kNullCppHeapPointerHandle;
  v8::CppHeapPointerHandle gdi_h = v8::kNullCppHeapPointerHandle;
  v8::CppHeapPointerHandle com_h = v8::kNullCppHeapPointerHandle;
  void Trace(cppgc::Visitor*) const {}
};

struct Win32Token {
  const char* kind;
  int pid = 0;
  int tid = 0;
};

struct Win32Sec final : public cppgc::GarbageCollected<Win32Sec> {
  const char* kind;
  explicit Win32Sec(const char* k) : kind(k) {}
  void Trace(cppgc::Visitor*) const {}
};

struct Win32VmemRoot {
  const char* kind = "vmem";
};

struct Win32SockRoot {
  const char* kind = "sock";
};

struct Win32CngRoot {
  const char* kind = "cng";
};

struct Win32ModRoot {
  const char* kind = "mod";
};

struct Win32CertRoot {
  const char* kind = "cert";
};

struct Win32HttpRoot {
  const char* kind = "http";
};

struct Win32WhpRoot {
  const char* kind = "whp";
};

struct Win32HeapRoot {
  const char* kind = "heap";
};

inline Win32Kernel* win32_kernel() {
  static cppgc::Persistent<Win32Kernel> root;
  static cppgc::Persistent<Win32Sec> token_root;
  static cppgc::Persistent<Win32Sec> sid_root;
  static cppgc::Persistent<Win32Sec> peb_root;
  static cppgc::Persistent<Win32Sec> teb_root;
  static cppgc::Persistent<Win32Sec> wnd_root;
  static cppgc::Persistent<Win32Sec> gdi_root;
  static cppgc::Persistent<Win32Sec> com_root;
  static v8::CppHeapPointerHandle chpt = v8::kNullCppHeapPointerHandle;
  static Win32Token process{"process"};
  static Win32Token thread{"thread"};
  process.pid = proc_self();
  thread.tid = proc_tid();
  thread.pid = proc_self();
  static Win32VmemRoot vmem;
  static Win32SockRoot sock;
  static Win32CngRoot cng;
  static Win32ModRoot mod;
  static Win32CertRoot cert;
  static Win32HttpRoot http;
  static Win32WhpRoot whp;
  static Win32HeapRoot heap;
  auto& a = aspace();
  if (chpt != v8::kNullCppHeapPointerHandle) {
    return static_cast<Win32Kernel*>(a.chpt.Get(chpt, v8::kAnyCppHeapPointer));
  }
  Win32Kernel* k = cppgc::MakeGarbageCollected<Win32Kernel>(
      aspace_heap().GetAllocationHandle());
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!k || !aspace_cage().Contains(k)) return nullptr;
#endif
  root = k;
  chpt = a.chpt.AllocateAndInitializeEntry(k, kWin32KernelTag);
  k->process_h = a.tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(&process),
      v8::internal::kGenericTrustedObjectTag);
  k->thread_h = a.tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(&thread),
      v8::internal::kGenericTrustedObjectTag);
  Win32Sec* tok = cppgc::MakeGarbageCollected<Win32Sec>(
      aspace_heap().GetAllocationHandle(), "token");
  Win32Sec* sid = cppgc::MakeGarbageCollected<Win32Sec>(
      aspace_heap().GetAllocationHandle(), "sid");
  Win32Sec* peb = cppgc::MakeGarbageCollected<Win32Sec>(
      aspace_heap().GetAllocationHandle(), "peb");
  Win32Sec* teb = cppgc::MakeGarbageCollected<Win32Sec>(
      aspace_heap().GetAllocationHandle(), "teb");
  Win32Sec* wnd = cppgc::MakeGarbageCollected<Win32Sec>(
      aspace_heap().GetAllocationHandle(), "wnd");
  Win32Sec* gdi = cppgc::MakeGarbageCollected<Win32Sec>(
      aspace_heap().GetAllocationHandle(), "gdi");
  Win32Sec* com = cppgc::MakeGarbageCollected<Win32Sec>(
      aspace_heap().GetAllocationHandle(), "com");
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!tok || !aspace_cage().Contains(tok) || !sid || !aspace_cage().Contains(sid) ||
      !peb || !aspace_cage().Contains(peb) || !teb || !aspace_cage().Contains(teb) ||
      !wnd || !aspace_cage().Contains(wnd) || !gdi || !aspace_cage().Contains(gdi) ||
      !com || !aspace_cage().Contains(com))
    return nullptr;
#endif
  k->token_h = a.chpt.AllocateAndInitializeEntry(tok, kWin32TokenTag);
  k->sid_h = a.chpt.AllocateAndInitializeEntry(sid, kWin32SidTag);
  k->peb_h = a.chpt.AllocateAndInitializeEntry(peb, kWin32PebTag);
  k->teb_h = a.chpt.AllocateAndInitializeEntry(teb, kWin32TebTag);
  k->wnd_h = a.chpt.AllocateAndInitializeEntry(wnd, kWin32WndTag);
  k->gdi_h = a.chpt.AllocateAndInitializeEntry(gdi, kWin32GdiTag);
  k->com_h = a.chpt.AllocateAndInitializeEntry(com, kWin32ComTag);
  token_root = tok;
  sid_root = sid;
  peb_root = peb;
  teb_root = teb;
  wnd_root = wnd;
  gdi_root = gdi;
  com_root = com;
  k->vmem_h = a.ept.AllocateAndInitializeEntry(&vmem, kWin32VmemTag);
  k->sock_h = a.ept.AllocateAndInitializeEntry(&sock, kWin32SockTag);
  k->cng_h = a.ept.AllocateAndInitializeEntry(&cng, kWin32CngTag);
  k->mod_h = a.ept.AllocateAndInitializeEntry(&mod, kWin32ModTag);
  k->cert_h = a.ept.AllocateAndInitializeEntry(&cert, kWin32CertTag);
  k->http_h = a.ept.AllocateAndInitializeEntry(&http, kWin32HttpTag);
  k->whp_h = a.ept.AllocateAndInitializeEntry(&whp, kWin32WhpTag);
  k->heap_h = a.ept.AllocateAndInitializeEntry(&heap, kWin32HeapTag);
#if defined(WASIGO_HAS_WASMWIN32_CATALOG)
  int n = 0;
  const WasmWin32Api* cat = wasmwin32_catalog(&n);
  k->catalog_h = a.ept.AllocateAndInitializeEntry(
      const_cast<WasmWin32Api*>(cat), kWin32CatalogTag);
#endif
  if (chpt == v8::kNullCppHeapPointerHandle) return nullptr;
  if (!a.tpt.Get(k->process_h, v8::internal::kAllIndirectPointerTags)) return nullptr;
  if (!a.tpt.Get(k->thread_h, v8::internal::kAllIndirectPointerTags)) return nullptr;
  if (!a.chpt.Get(k->token_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.chpt.Get(k->sid_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.chpt.Get(k->peb_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.chpt.Get(k->teb_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.chpt.Get(k->wnd_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.chpt.Get(k->gdi_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.chpt.Get(k->com_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.ept.Get(k->vmem_h, v8::internal::kAnyExternalPointer)) return nullptr;
  if (!a.ept.Get(k->sock_h, v8::internal::kAnyExternalPointer)) return nullptr;
  if (!a.ept.Get(k->cng_h, v8::internal::kAnyExternalPointer)) return nullptr;
  if (!a.ept.Get(k->mod_h, v8::internal::kAnyExternalPointer)) return nullptr;
  if (!a.ept.Get(k->cert_h, v8::internal::kAnyExternalPointer)) return nullptr;
  if (!a.ept.Get(k->http_h, v8::internal::kAnyExternalPointer)) return nullptr;
  if (!a.ept.Get(k->whp_h, v8::internal::kAnyExternalPointer)) return nullptr;
  if (!a.ept.Get(k->heap_h, v8::internal::kAnyExternalPointer)) return nullptr;
#if defined(WASIGO_HAS_WASMWIN32_CATALOG)
  if (!a.ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer)) return nullptr;
#endif
  return k;
}

inline bool win32_tables_ok() {
  Win32Kernel* k = win32_kernel();
  if (!k) return false;
  auto& a = aspace();
  if (!a.tpt.Get(k->process_h, v8::internal::kAllIndirectPointerTags)) return false;
  if (!a.tpt.Get(k->thread_h, v8::internal::kAllIndirectPointerTags)) return false;
  if (!a.chpt.Get(k->token_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.chpt.Get(k->sid_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.chpt.Get(k->peb_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.chpt.Get(k->teb_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.chpt.Get(k->wnd_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.chpt.Get(k->gdi_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.chpt.Get(k->com_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.ept.Get(k->vmem_h, v8::internal::kAnyExternalPointer)) return false;
  if (!a.ept.Get(k->sock_h, v8::internal::kAnyExternalPointer)) return false;
  if (!a.ept.Get(k->cng_h, v8::internal::kAnyExternalPointer)) return false;
  if (!a.ept.Get(k->mod_h, v8::internal::kAnyExternalPointer)) return false;
  if (!a.ept.Get(k->cert_h, v8::internal::kAnyExternalPointer)) return false;
  if (!a.ept.Get(k->http_h, v8::internal::kAnyExternalPointer)) return false;
  if (!a.ept.Get(k->whp_h, v8::internal::kAnyExternalPointer)) return false;
  if (!a.ept.Get(k->heap_h, v8::internal::kAnyExternalPointer)) return false;
#if defined(WASIGO_HAS_WASMWIN32_CATALOG)
  if (!a.ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer)) return false;
#endif
  return true;
}
#endif

#if defined(WASIGO_HAS_WASMNIX) && WASIGO_HAS_WASMV8
// WASMNix on the second address space (do not rewrite posix_host.hpp):
//   CHPT — NixKernel session
//   TPT  — current process / thread (trusted)
//   EPT  — ~/WASMNix catalog
struct NixKernel final : public cppgc::GarbageCollected<NixKernel> {
  v8::internal::TrustedPointerHandle process_h =
      v8::internal::kNullTrustedPointerHandle;
  v8::internal::TrustedPointerHandle thread_h =
      v8::internal::kNullTrustedPointerHandle;
  v8::internal::ExternalPointerHandle catalog_h =
      v8::internal::kNullExternalPointerHandle;
  v8::CppHeapPointerHandle session_h = v8::kNullCppHeapPointerHandle;
  void Trace(cppgc::Visitor*) const {}
};

struct NixToken {
  const char* kind;
};

struct NixSession final : public cppgc::GarbageCollected<NixSession> {
  const char* kind;
  explicit NixSession(const char* k) : kind(k) {}
  void Trace(cppgc::Visitor*) const {}
};

inline NixKernel* nix_kernel() {
  static cppgc::Persistent<NixKernel> root;
  static cppgc::Persistent<NixSession> session_root;
  static v8::CppHeapPointerHandle chpt = v8::kNullCppHeapPointerHandle;
  static NixToken process{"process"};
  static NixToken thread{"thread"};
  auto& a = aspace();
  if (chpt != v8::kNullCppHeapPointerHandle) {
    return static_cast<NixKernel*>(a.chpt.Get(chpt, v8::kAnyCppHeapPointer));
  }
  NixKernel* k = cppgc::MakeGarbageCollected<NixKernel>(
      aspace_heap().GetAllocationHandle());
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!k || !aspace_cage().Contains(k)) return nullptr;
#endif
  root = k;
  chpt = a.chpt.AllocateAndInitializeEntry(k, kNixSessionTag);
  k->process_h = a.tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(&process),
      v8::internal::kGenericTrustedObjectTag);
  k->thread_h = a.tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(&thread),
      v8::internal::kGenericTrustedObjectTag);
  NixSession* sess = cppgc::MakeGarbageCollected<NixSession>(
      aspace_heap().GetAllocationHandle(), "session");
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!sess || !aspace_cage().Contains(sess)) return nullptr;
#endif
  k->session_h = a.chpt.AllocateAndInitializeEntry(sess, kNixSessionTag);
  session_root = sess;
#if defined(WASIGO_HAS_WASMNIX_CATALOG)
  int n = 0;
  const WasmNixApi* cat = wasmnix_catalog(&n);
  k->catalog_h = a.ept.AllocateAndInitializeEntry(
      const_cast<WasmNixApi*>(cat), kNixCatalogTag);
#endif
  if (chpt == v8::kNullCppHeapPointerHandle) return nullptr;
  if (!a.tpt.Get(k->process_h, v8::internal::kAllIndirectPointerTags)) return nullptr;
  if (!a.tpt.Get(k->thread_h, v8::internal::kAllIndirectPointerTags)) return nullptr;
  if (!a.chpt.Get(k->session_h, v8::kAnyCppHeapPointer)) return nullptr;
#if defined(WASIGO_HAS_WASMNIX_CATALOG)
  if (!a.ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer)) return nullptr;
#endif
  return k;
}

inline bool nix_tables_ok() {
  NixKernel* k = nix_kernel();
  if (!k) return false;
  auto& a = aspace();
  if (!a.tpt.Get(k->process_h, v8::internal::kAllIndirectPointerTags)) return false;
  if (!a.tpt.Get(k->thread_h, v8::internal::kAllIndirectPointerTags)) return false;
  if (!a.chpt.Get(k->session_h, v8::kAnyCppHeapPointer)) return false;
#if defined(WASIGO_HAS_WASMNIX_CATALOG)
  if (!a.ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer)) return false;
#endif
  return true;
}
#endif

#if defined(WASIGO_HAS_WASMDROID) && WASIGO_HAS_WASMV8
// WASMDroid on the second address space (do not rewrite bionic_host.hpp):
//   CHPT — DroidKernel session
//   TPT  — current process / thread (trusted)
//   EPT  — ~/WASMDroid catalog + Binder root
struct DroidKernel final : public cppgc::GarbageCollected<DroidKernel> {
  v8::internal::TrustedPointerHandle process_h =
      v8::internal::kNullTrustedPointerHandle;
  v8::internal::TrustedPointerHandle thread_h =
      v8::internal::kNullTrustedPointerHandle;
  v8::internal::ExternalPointerHandle catalog_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle binder_h =
      v8::internal::kNullExternalPointerHandle;
  v8::CppHeapPointerHandle session_h = v8::kNullCppHeapPointerHandle;
  void Trace(cppgc::Visitor*) const {}
};

struct DroidToken {
  const char* kind;
};

struct DroidSession final : public cppgc::GarbageCollected<DroidSession> {
  const char* kind;
  explicit DroidSession(const char* k) : kind(k) {}
  void Trace(cppgc::Visitor*) const {}
};

struct DroidBinderRoot {
  const char* kind = "binder";
};

inline DroidKernel* droid_kernel() {
  static cppgc::Persistent<DroidKernel> root;
  static cppgc::Persistent<DroidSession> session_root;
  static v8::CppHeapPointerHandle chpt = v8::kNullCppHeapPointerHandle;
  static DroidToken process{"process"};
  static DroidToken thread{"thread"};
  static DroidBinderRoot binder;
  auto& a = aspace();
  if (chpt != v8::kNullCppHeapPointerHandle) {
    return static_cast<DroidKernel*>(a.chpt.Get(chpt, v8::kAnyCppHeapPointer));
  }
  DroidKernel* k = cppgc::MakeGarbageCollected<DroidKernel>(
      aspace_heap().GetAllocationHandle());
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!k || !aspace_cage().Contains(k)) return nullptr;
#endif
  root = k;
  chpt = a.chpt.AllocateAndInitializeEntry(k, kDroidSessionTag);
  k->process_h = a.tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(&process),
      v8::internal::kGenericTrustedObjectTag);
  k->thread_h = a.tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(&thread),
      v8::internal::kGenericTrustedObjectTag);
  DroidSession* sess = cppgc::MakeGarbageCollected<DroidSession>(
      aspace_heap().GetAllocationHandle(), "session");
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!sess || !aspace_cage().Contains(sess)) return nullptr;
#endif
  k->session_h = a.chpt.AllocateAndInitializeEntry(sess, kDroidSessionTag);
  session_root = sess;
  k->binder_h = a.ept.AllocateAndInitializeEntry(&binder, kDroidBinderTag);
#if defined(WASIGO_HAS_WASMDROID_CATALOG)
  int n = 0;
  const WasmDroidApi* cat = wasmdroid_catalog(&n);
  k->catalog_h = a.ept.AllocateAndInitializeEntry(
      const_cast<WasmDroidApi*>(cat), kDroidCatalogTag);
#endif
  if (chpt == v8::kNullCppHeapPointerHandle) return nullptr;
  if (!a.tpt.Get(k->process_h, v8::internal::kAllIndirectPointerTags)) return nullptr;
  if (!a.tpt.Get(k->thread_h, v8::internal::kAllIndirectPointerTags)) return nullptr;
  if (!a.chpt.Get(k->session_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.ept.Get(k->binder_h, v8::internal::kAnyExternalPointer)) return nullptr;
#if defined(WASIGO_HAS_WASMDROID_CATALOG)
  if (!a.ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer)) return nullptr;
#endif
  return k;
}

inline bool droid_tables_ok() {
  DroidKernel* k = droid_kernel();
  if (!k) return false;
  auto& a = aspace();
  if (!a.tpt.Get(k->process_h, v8::internal::kAllIndirectPointerTags)) return false;
  if (!a.tpt.Get(k->thread_h, v8::internal::kAllIndirectPointerTags)) return false;
  if (!a.chpt.Get(k->session_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.ept.Get(k->binder_h, v8::internal::kAnyExternalPointer)) return false;
#if defined(WASIGO_HAS_WASMDROID_CATALOG)
  if (!a.ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer)) return false;
#endif
  return true;
}
#endif

#if defined(WASIGO_HAS_WASMGOCOS) && WASIGO_HAS_WASMV8
// WASMGocOS on the second address space. GocKrnl / GocSys hop k32 and
// nix through gocvm hypervision. GocDesk is gocvm.Call("gocos"|"gocdesk").
//   CHPT — GocOSKernel session
//   TPT  — process / thread
//   EPT  — catalog + GocDesk HWND root + vmem
struct GocOSKernel final : public cppgc::GarbageCollected<GocOSKernel> {
  v8::internal::TrustedPointerHandle process_h =
      v8::internal::kNullTrustedPointerHandle;
  v8::internal::TrustedPointerHandle thread_h =
      v8::internal::kNullTrustedPointerHandle;
  v8::internal::ExternalPointerHandle catalog_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle desktop_h =
      v8::internal::kNullExternalPointerHandle;
  v8::internal::ExternalPointerHandle vmem_h =
      v8::internal::kNullExternalPointerHandle;
  v8::CppHeapPointerHandle session_h = v8::kNullCppHeapPointerHandle;
  void Trace(cppgc::Visitor*) const {}
};

struct GocOSToken {
  const char* kind;
};

struct GocOSSession final : public cppgc::GarbageCollected<GocOSSession> {
  const char* kind;
  explicit GocOSSession(const char* k) : kind(k) {}
  void Trace(cppgc::Visitor*) const {}
};

struct GocOSDesktopRoot {
  const char* kind = "gocdesk";
};

struct GocOSVmemRoot {
  const char* kind = "vmem";
};

inline GocOSKernel* gocos_kernel() {
  static cppgc::Persistent<GocOSKernel> root;
  static cppgc::Persistent<GocOSSession> session_root;
  static v8::CppHeapPointerHandle chpt = v8::kNullCppHeapPointerHandle;
  static GocOSToken process{"process"};
  static GocOSToken thread{"thread"};
  static GocOSDesktopRoot desktop;
  static GocOSVmemRoot vmem;
  auto& a = aspace();
  if (chpt != v8::kNullCppHeapPointerHandle) {
    return static_cast<GocOSKernel*>(a.chpt.Get(chpt, v8::kAnyCppHeapPointer));
  }
  GocOSKernel* k = cppgc::MakeGarbageCollected<GocOSKernel>(
      aspace_heap().GetAllocationHandle());
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!k || !aspace_cage().Contains(k)) return nullptr;
#endif
  root = k;
  chpt = a.chpt.AllocateAndInitializeEntry(k, kGocOSSessionTag);
  k->process_h = a.tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(&process),
      v8::internal::kGenericTrustedObjectTag);
  k->thread_h = a.tpt.AllocateAndInitializeEntry(
      reinterpret_cast<v8::Address>(&thread),
      v8::internal::kGenericTrustedObjectTag);
  GocOSSession* sess = cppgc::MakeGarbageCollected<GocOSSession>(
      aspace_heap().GetAllocationHandle(), "session");
#if defined(WASIGO_HAS_WASMSAFESPACE)
  if (!sess || !aspace_cage().Contains(sess)) return nullptr;
#endif
  k->session_h = a.chpt.AllocateAndInitializeEntry(sess, kGocOSSessionTag);
  session_root = sess;
  k->desktop_h = a.ept.AllocateAndInitializeEntry(&desktop, kGocOSDesktopTag);
  k->vmem_h = a.ept.AllocateAndInitializeEntry(&vmem, kGocOSVmemTag);
#if defined(WASIGO_HAS_WASMGOCOS_CATALOG)
  int n = 0;
  const WasmGocOSApi* cat = wasmgocos_catalog(&n);
  k->catalog_h = a.ept.AllocateAndInitializeEntry(
      const_cast<WasmGocOSApi*>(cat), kGocOSCatalogTag);
#endif
  if (chpt == v8::kNullCppHeapPointerHandle) return nullptr;
  if (!a.tpt.Get(k->process_h, v8::internal::kAllIndirectPointerTags)) return nullptr;
  if (!a.tpt.Get(k->thread_h, v8::internal::kAllIndirectPointerTags)) return nullptr;
  if (!a.chpt.Get(k->session_h, v8::kAnyCppHeapPointer)) return nullptr;
  if (!a.ept.Get(k->desktop_h, v8::internal::kAnyExternalPointer)) return nullptr;
  if (!a.ept.Get(k->vmem_h, v8::internal::kAnyExternalPointer)) return nullptr;
#if defined(WASIGO_HAS_WASMGOCOS_CATALOG)
  if (!a.ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer)) return nullptr;
#endif
  return k;
}

inline bool gocos_tables_ok() {
  GocOSKernel* k = gocos_kernel();
  if (!k) return false;
  auto& a = aspace();
  if (!a.tpt.Get(k->process_h, v8::internal::kAllIndirectPointerTags)) return false;
  if (!a.tpt.Get(k->thread_h, v8::internal::kAllIndirectPointerTags)) return false;
  if (!a.chpt.Get(k->session_h, v8::kAnyCppHeapPointer)) return false;
  if (!a.ept.Get(k->desktop_h, v8::internal::kAnyExternalPointer)) return false;
  if (!a.ept.Get(k->vmem_h, v8::internal::kAnyExternalPointer)) return false;
#if defined(WASIGO_HAS_WASMGOCOS_CATALOG)
  if (!a.ept.Get(k->catalog_h, v8::internal::kAnyExternalPointer)) return false;
#endif
  return true;
}
#endif

inline void wasigocvm_split1f(const std::string& s, std::string* a, std::string* b) {
  auto i = s.find('\x1f');
  if (i == std::string::npos) {
    *a = s;
    *b = "";
    return;
  }
  *a = s.substr(0, i);
  *b = s.substr(i + 1);
}

inline bool wasigocvm_try_win32(const std::string& topic, const std::string& payload,
                                std::string* reply) {
  if (topic != "win32") return false;
#if WASIGO_HAS_WASMV8
  if (!win32_tables_ok()) {
    *reply = "error: win32: EPT/TPT/CHPT bind failed";
    return true;
  }
#endif
  std::string api, rest;
  wasigocvm_split1f(payload, &api, &rest);
  *reply = wasmwin32::wasi_call(api.c_str(), rest.c_str());
  return true;
}

inline bool wasigocvm_try_nix(const std::string& topic, const std::string& payload,
                              std::string* reply) {
  if (topic != "linux" && topic != "wsl" && topic != "nix") return false;
#if WASIGO_HAS_WASMV8
  if (!nix_tables_ok()) {
    *reply = "error: linux: EPT/TPT/CHPT bind failed";
    return true;
  }
#endif
  if (topic == "linux") {
    std::string api, rest;
    wasigocvm_split1f(payload, &api, &rest);
    *reply = wasmnix::posix_call(api.c_str(), rest.c_str());
    return true;
  }
  if (topic == "wsl") {
    std::string op, rest;
    wasigocvm_split1f(payload, &op, &rest);
    if (op == "list" || op.empty()) {
      *reply = wasmnix::posix_call("WslList", "");
    } else if (op == "exec") {
      *reply = wasmnix::posix_call("WslExec", rest.c_str());
    } else if (op == "registered") {
      *reply = wasmnix::posix_call("WslIsDistributionRegistered", rest.c_str());
    } else {
      *reply = wasmnix::posix_call("WslExec", payload.c_str());
    }
    return true;
  }
  if (payload.empty() || payload == "version") {
    *reply = wasmnix::posix_call("NixVersion", "");
  } else {
    std::string op, rest;
    wasigocvm_split1f(payload, &op, &rest);
    if (op == "run")
      *reply = wasmnix::posix_call("NixRun", rest.c_str());
    else
      *reply = wasmnix::posix_call("NixRun", payload.c_str());
  }
  return true;
}

inline bool wasigocvm_try_droid(const std::string& topic, const std::string& payload,
                                std::string* reply) {
  if (topic != "android" && topic != "droid" && topic != "binder" &&
      topic != "kvm")
    return false;
#if WASIGO_HAS_WASMV8
  if (!droid_tables_ok()) {
    *reply = "error: android: EPT/TPT/CHPT bind failed";
    return true;
  }
#endif
  std::string api, rest;
  wasigocvm_split1f(payload, &api, &rest);
  if (topic == "binder" && (api.empty() || api == "get")) {
    *reply = wasmdroid::bionic_call("AServiceManager_getService", rest.c_str());
    return true;
  }
  if (topic == "kvm" && (api.empty() || api == "create")) {
    *reply = wasmdroid::bionic_call("KVM_CREATE_VM", rest.c_str());
    return true;
  }
  *reply = wasmdroid::bionic_call(api.c_str(), rest.c_str());
  return true;
}

inline bool wasigocvm_try_desktopengine(const std::string& topic,
                                        const std::string& payload,
                                        std::string* reply) {
#if defined(WASIGO_HAS_WASMGOCOS)
  if (topic != "desktopengine" && topic != "desktop" && topic != "gocdesk")
    return false;
#if WASIGO_HAS_WASMV8
  if (!gocos_tables_ok()) {
    *reply = "error: gocdesk: EPT/TPT/CHPT bind failed";
    return true;
  }
#endif
  std::string api, rest;
  wasigocvm_split1f(payload, &api, &rest);
  *reply = wasmgocos::desktopengine_call(api.c_str(), rest.c_str());
  return true;
#else
  (void)topic;
  (void)payload;
  (void)reply;
  return false;
#endif
}

inline bool wasigocvm_try_gocos(const std::string& topic, const std::string& payload,
                                std::string* reply) {
#if defined(WASIGO_HAS_WASMGOCOS)
  if (topic != "gocos" && topic != "gockrnl" && topic != "gocsys") return false;
#if WASIGO_HAS_WASMV8
  if (!gocos_tables_ok()) {
    *reply = "error: gocos: EPT/TPT/CHPT bind failed";
    return true;
  }
#endif
  std::string api, rest;
  wasigocvm_split1f(payload, &api, &rest);
  *reply = wasmgocos::gocos_call(api.c_str(), rest.c_str());
  return true;
#else
  (void)topic;
  (void)payload;
  (void)reply;
  return false;
#endif
}

inline bool wasigocvm_catalog_ok_reply(const std::string& r) {
  if (r.empty()) return false;
  if (r.size() >= 6 && r.compare(0, 6, "error:") == 0) return false;
  return true;
}

inline bool wasigocvm_try_any_catalog(const std::string& api, const std::string& arg,
                                     std::string* reply) {
#if WASIGO_HAS_WASMV8
  if (win32_tables_ok()) {
    *reply = wasmwin32::wasi_call(api.c_str(), arg.c_str());
    if (wasigocvm_catalog_ok_reply(*reply)) return true;
  }
  if (nix_tables_ok()) {
    *reply = wasmnix::posix_call(api.c_str(), arg.c_str());
    if (wasigocvm_catalog_ok_reply(*reply)) return true;
  }
  if (droid_tables_ok()) {
    *reply = wasmdroid::bionic_call(api.c_str(), arg.c_str());
    if (wasigocvm_catalog_ok_reply(*reply)) return true;
  }
#else
  *reply = wasmwin32::wasi_call(api.c_str(), arg.c_str());
  if (wasigocvm_catalog_ok_reply(*reply)) return true;
  *reply = wasmnix::posix_call(api.c_str(), arg.c_str());
  if (wasigocvm_catalog_ok_reply(*reply)) return true;
  *reply = wasmdroid::bionic_call(api.c_str(), arg.c_str());
  if (wasigocvm_catalog_ok_reply(*reply)) return true;
#endif
  return false;
}

inline bool wasigocvm_try_chrome(const std::string& topic, const std::string& payload,
                                 std::string* reply) {
#if defined(WASIGO_HAS_WASMCHROME) && WASIGO_HAS_WASMCHROME
  if (topic != "chrome") return false;
  std::string api, rest;
  wasigocvm_split1f(payload, &api, &rest);
  *reply = wasmchrome::chrome_call(api.c_str(), rest.c_str());
  return true;
#else
  (void)topic;
  (void)payload;
  (void)reply;
  return false;
#endif
}

inline bool wasigocvm_try_libc(const std::string& topic, const std::string& payload,
                               std::string* reply) {
  if (topic == "syscall") return wasigocvm_try_syscall(payload, reply);
  if (topic == "os.user") return wasigocvm_try_user(payload, reply);
  if (wasigocvm_try_win32(topic, payload, reply)) return true;
  if (wasigocvm_try_nix(topic, payload, reply)) return true;
  if (wasigocvm_try_droid(topic, payload, reply)) return true;
  if (wasigocvm_try_desktopengine(topic, payload, reply)) return true;
  if (wasigocvm_try_gocos(topic, payload, reply)) return true;
  if (wasigocvm_try_chrome(topic, payload, reply)) return true;
  return wasigocvm_try_any_catalog(topic, payload, reply);
}

}  // namespace gocvm
