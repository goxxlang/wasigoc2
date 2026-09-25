#ifndef WASMDROID_INCLUDE_DROID_BIONIC_HOST_HPP_
#define WASMDROID_INCLUDE_DROID_BIONIC_HOST_HPP_

// Bionic-shaped libc backend for wasmdroid_call. Names follow AOSP
// bionic (getpid, __system_property_get, android_get_device_api_level)
// plus POSIX that Bionic ships. Binder / KVM / kernel hops stay
// in-module. adb is an honest extra on the host when present.

#ifndef _WASI_EMULATED_GETPID
#define _WASI_EMULATED_GETPID 1
#endif

#include "droid/dispatch.h"
#include "droid/catalog.h"
#include "droid/binder_host.hpp"
#include "droid/kernel_host.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#if defined(_WIN32) && !defined(__wasi__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#define droid_mkdir(p, m) _mkdir(p)
#define droid_getcwd _getcwd
#define droid_chdir _chdir
#define droid_rmdir _rmdir
#define droid_unlink _unlink
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#define droid_mkdir(p, m) mkdir(p, m)
#define droid_getcwd getcwd
#define droid_chdir chdir
#define droid_rmdir rmdir
#define droid_unlink unlink
#endif
#if defined(__wasi__)
extern "C" pid_t getppid(void);
extern "C" long gocvm_gettid(void);
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#endif

namespace wasmdroid {

inline bool eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline std::string err(const char* what) {
  return std::string("error: ") + what + ": errno " + std::to_string(errno);
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

inline std::string cwd() {
  char buf[4096];
  if (!droid_getcwd(buf, sizeof(buf))) return err("getcwd");
  return std::string(buf);
}

inline std::string hostname() {
#if defined(_WIN32) && !defined(__wasi__)
  const char* c = std::getenv("COMPUTERNAME");
  return c && c[0] ? c : "wasmdroid";
#else
  char buf[256];
  if (gethostname(buf, sizeof(buf)) == 0 && buf[0]) return buf;
  struct utsname u {};
  if (uname(&u) == 0 && u.nodename[0]) return u.nodename;
  return "wasmdroid";
#endif
}

inline std::string fmt_uname() {
#if defined(_WIN32) && !defined(__wasi__)
  return "Linux wasmdroid 6.1.0-android14-generic #1 SMP x86_64";
#else
  struct utsname u {};
  if (uname(&u) != 0) return err("uname");
  std::string s = u.sysname;
  s += " ";
  s += u.nodename;
  s += " ";
  s += u.release;
  s += " ";
  s += u.version;
  s += " ";
  s += u.machine;
  return s;
#endif
}

inline long droid_pid() {
#if defined(_WIN32) && !defined(__wasi__)
  return static_cast<long>(_getpid());
#else
  return static_cast<long>(getpid());
#endif
}

inline std::string capture_cmd(const char* cmd) {
#if defined(__wasi__)
  (void)cmd;
  return err_msg("no exec on this hop");
#else
  // Host CLI extra (adb). Not a stand-in for Binder / KVM.
  if (!cmd || !cmd[0]) return err_msg("empty command");
#if defined(_WIN32)
  FILE* p = _popen(cmd, "r");
#else
  FILE* p = popen(cmd, "r");
#endif
  if (!p) return err("popen");
  std::string out;
  char buf[4096];
  while (fgets(buf, sizeof(buf), p)) out += buf;
#if defined(_WIN32)
  int rc = _pclose(p);
#else
  int rc = pclose(p);
#endif
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  if (rc != 0 && out.empty())
    return err_msg((std::string("command failed rc=") + std::to_string(rc)).c_str());
  if (rc != 0) return std::string("error: ") + out;
  return out;
#endif
}

inline std::string bionic_call(const char* api, const char* args) {
  if (!api || !api[0]) return err_msg("android needs an API name");
  const char* a = args ? args : "";

  if (eq(api, "getpid"))
    return std::to_string(droid_pid());
  if (eq(api, "pthread_gettid_np") || eq(api, "gettid")) {
#if defined(__wasi__)
    return std::to_string(gocvm_gettid());
#else
    return std::to_string(droid_pid());
#endif
  }
#if defined(__wasi__)
  if (eq(api, "getppid")) return std::to_string(static_cast<long>(getppid()));
  if (eq(api, "getuid") || eq(api, "geteuid") || eq(api, "getgid") || eq(api, "getegid"))
    return "0";
#elif !defined(_WIN32)
  if (eq(api, "getppid")) return std::to_string(static_cast<long>(getppid()));
  if (eq(api, "getuid") || eq(api, "geteuid"))
    return std::to_string(static_cast<long>(eq(api, "geteuid") ? geteuid() : getuid()));
  if (eq(api, "getgid") || eq(api, "getegid"))
    return std::to_string(static_cast<long>(eq(api, "getegid") ? getegid() : getgid()));
#else
  if (eq(api, "getppid") || eq(api, "getuid") || eq(api, "geteuid") || eq(api, "getgid") ||
      eq(api, "getegid"))
    return "0";
#endif
  if (eq(api, "uname")) return fmt_uname();
  if (eq(api, "gethostname")) return hostname();
  if (eq(api, "getcwd") || eq(api, "get_current_dir_name")) return cwd();
  if (eq(api, "chdir")) {
    if (!a[0]) return err_msg("chdir: empty path");
    if (droid_chdir(a) != 0) return err("chdir");
    return "ok";
  }
  if (eq(api, "getenv") || eq(api, "secure_getenv")) {
    if (!a[0]) return err_msg("getenv: empty name");
    const char* v = std::getenv(a);
    if (!v) return err_msg("getenv: not found");
    return v;
  }
  if (eq(api, "setenv") || eq(api, "putenv")) {
    std::string name, val;
    split1f(a, &name, &val);
    if (name.empty()) return err_msg("setenv: empty name");
#if defined(_WIN32) && !defined(__wasi__)
    std::string e = name + "=" + val;
    _putenv(e.c_str());
#else
    if (setenv(name.c_str(), val.c_str(), 1) != 0) return err("setenv");
#endif
    return "ok";
  }
  if (eq(api, "sleep") || eq(api, "usleep") || eq(api, "nanosleep")) {
#if defined(_WIN32) && !defined(__wasi__)
    unsigned long ms = std::strtoul(a, nullptr, 10);
    if (eq(api, "sleep")) ms *= 1000;
    if (eq(api, "usleep")) ms /= 1000;
    Sleep((DWORD)(ms ? ms : 0));
#else
    unsigned long v = std::strtoul(a, nullptr, 10);
    if (eq(api, "sleep"))
      sleep(static_cast<unsigned>(v));
    else
      usleep(static_cast<unsigned>(eq(api, "nanosleep") ? v * 1000 : v));
#endif
    return "ok";
  }
  if (eq(api, "clock_gettime") || eq(api, "time") || eq(api, "clock_getres"))
    return std::to_string(static_cast<long long>(time(nullptr)));
  if (eq(api, "malloc") || eq(api, "calloc") || eq(api, "valloc")) {
    size_t n = static_cast<size_t>(std::strtoul(a, nullptr, 10));
    if (!n) n = 1;
    void* p = std::malloc(n);
    if (!p) return err_msg("malloc: oom");
    if (eq(api, "calloc")) std::memset(p, 0, n);
    return std::to_string(reinterpret_cast<uintptr_t>(p));
  }
  if (eq(api, "free")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    if (p) std::free(reinterpret_cast<void*>(p));
    return "ok";
  }
  if (eq(api, "mkdir")) {
    if (!a[0]) return err_msg("mkdir: empty path");
    if (droid_mkdir(a, 0777) != 0) return err("mkdir");
    return "ok";
  }
  if (eq(api, "rmdir")) {
    if (!a[0]) return err_msg("rmdir: empty path");
    if (droid_rmdir(a) != 0) return err("rmdir");
    return "ok";
  }
  if (eq(api, "unlink") || eq(api, "remove")) {
    if (!a[0]) return err_msg("unlink: empty path");
    if (droid_unlink(a) != 0) return err("unlink");
    return "ok";
  }
  if (eq(api, "access") || eq(api, "stat") || eq(api, "lstat") || eq(api, "fstatat")) {
    std::string path, rest;
    split1f(a, &path, &rest);
    if (path.empty()) path = a;
#if defined(_WIN32) && !defined(__wasi__)
    if (_access(path.c_str(), 0) != 0) return err("access");
    return "ok";
#else
    struct ::stat st {};
    if (::stat(path.c_str(), &st) != 0) return err("stat");
    return std::to_string(static_cast<long long>(st.st_size)) + "\x1f" +
           std::to_string(static_cast<unsigned>(st.st_mode));
#endif
  }
  if (eq(api, "strlen")) return std::to_string(std::strlen(a));
  if (eq(api, "android_get_device_api_level") || eq(api, "android_get_application_target_sdk_version"))
    return "34";
  if (eq(api, "getprogname")) return "wasmdroid";
  if (eq(api, "arc4random") || eq(api, "arc4random_uniform"))
    return std::to_string(static_cast<unsigned>(droid_pid() ^ static_cast<long>(time(nullptr))));
  if (eq(api, "android_set_abort_message")) return "ok";
  if (eq(api, "dlopen") || eq(api, "android_dlopen_ext")) {
    if (!a[0]) return "1";
    int n = 0;
    const WasmDroidApi* c = wasmdroid_catalog(&n);
    for (int i = 0; i < n; ++i)
      if (c[i].lib && (eq(c[i].lib, a) || std::strstr(a, c[i].lib))) return "1";
    return err_msg("dlopen: module not found");
  }
  if (eq(api, "dlsym") || eq(api, "dlclose") || eq(api, "dlerror"))
    return eq(api, "dlerror") ? "" : "ok";
  if (eq(api, "AdbDevices") || eq(api, "adb")) {
    std::string r = capture_cmd(a[0] ? a : "adb devices");
    return r;
  }
  if (eq(api, "AdbShell") || eq(api, "AdbExec")) {
    std::string cmd = std::string("adb shell ") + (a[0] ? a : "echo ok");
    return capture_cmd(cmd.c_str());
  }

  {
    std::string extra;
    if (try_binder(api, a, &extra)) return extra;
    if (try_kernel(api, a, &extra)) return extra;
    if (try_kvm(api, a, &extra)) return extra;
  }
  return std::string("error: unknown api ") + api;
}

inline int fill_out(char* out, unsigned cap, const std::string& s) {
  if (!out || cap == 0) return s.rfind("error:", 0) == 0 ? -1 : 0;
  unsigned n = static_cast<unsigned>(s.size());
  if (n + 1 > cap) n = cap - 1;
  std::memcpy(out, s.data(), n);
  out[n] = 0;
  return s.rfind("error:", 0) == 0 ? -1 : 0;
}

}  // namespace wasmdroid

#endif  // WASMDROID_INCLUDE_DROID_BIONIC_HOST_HPP_
