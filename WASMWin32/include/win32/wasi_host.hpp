#ifndef WASMWIN32_INCLUDE_WIN32_WASI_HOST_HPP_
#define WASMWIN32_INCLUDE_WIN32_WASI_HOST_HPP_

// libc backend for wasmwin32_call. Same win32metadata names as host_win.cc,
// mapped onto POSIX that WASI actually ships (getpid, getenv, getcwd,
// gethostname, uname, stat, clocks, sleep) plus std::thread CreateProcess.
// host_win.cc stays native-only (not compiled into wasm). LoadLibrary is
// the same hop as the rest of kernel32: named modules this machine has,
// and files it can map, including PE via the in-module mapper
// (`~/WASMPELoader` via pe_map.hpp).
// WslList / WslExec names (uname, echo, true) are the guest itself. Nix
// and unknown commands stay honest errors.

#ifndef _WASI_EMULATED_GETPID
#define _WASI_EMULATED_GETPID 1
#endif
#if defined(__wasi__)
extern "C" long gocvm_gettid(void);
#endif

#include "win32/dispatch.h"
#include "win32/catalog.h"
#include "win32/pe_map.hpp"

#ifndef WASMWIN32_WASI_HOST_NO_POSIX_HEADERS
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#else
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#endif

#if defined(__wasi__)
extern "C" char** __wasilibc_get_environ(void);
#endif

#ifndef FILE_ATTRIBUTE_READONLY
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#endif

namespace wasmwin32 {

inline bool eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline std::string err(const char* what) {
  return std::string("error: ") + what + ": errno " + std::to_string(errno);
}

inline std::string err_msg(const char* msg) {
  return std::string("error: ") + msg;
}

inline std::string hostname() {
  char buf[256];
  if (gethostname(buf, sizeof(buf)) == 0 && buf[0]) return std::string(buf);
  struct utsname u {};
  if (uname(&u) == 0 && u.nodename[0]) return std::string(u.nodename);
  return "wasigocvm";
}

inline std::string fmt_uname() {
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
}

inline std::string tick_ms() {
  struct timespec ts {};
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return err("clock_gettime");
  std::uint64_t ms = static_cast<std::uint64_t>(ts.tv_sec) * 1000ull +
                     static_cast<std::uint64_t>(ts.tv_nsec) / 1000000ull;
  return std::to_string(ms);
}

inline std::string qpc_ns() {
  struct timespec ts {};
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return err("clock_gettime");
  std::uint64_t ns = static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull +
                     static_cast<std::uint64_t>(ts.tv_nsec);
  return std::to_string(ns);
}

inline std::string cwd() {
  char buf[4096];
  if (!getcwd(buf, sizeof(buf))) return err("getcwd");
  return std::string(buf);
}

// Process environment owned by this module. WASMGocOS reads it through
// the k32 hop (GetEnvironmentVariableW / GetEnvironmentStringsW). The
// block is a Windows environment. A value already in the process is kept.
inline std::map<std::string, std::string>& module_env() {
  static std::map<std::string, std::string> m;
  return m;
}

inline void ensure_windows_env() {
  static int once = 0;
  if (once) return;
  once = 1;
  struct Row {
    const char* k;
    const char* v;
  };
  static const Row rows[] = {
      {"ALLUSERSPROFILE", "C:\\ProgramData"},
      {"APPDATA", "C:\\Users\\wasigocvm\\AppData\\Roaming"},
      {"CommonProgramFiles", "C:\\Program Files\\Common Files"},
      {"CommonProgramFiles(x86)", "C:\\Program Files (x86)\\Common Files"},
      {"COMPUTERNAME", "WASIGO"},
      {"ComSpec", "C:\\Windows\\System32\\cmd.exe"},
      {"HOME", "C:\\Users\\wasigocvm"},
      {"HOMEDRIVE", "C:"},
      {"HOMEPATH", "\\Users\\wasigocvm"},
      {"LOCALAPPDATA", "C:\\Users\\wasigocvm\\AppData\\Local"},
      {"LOGNAME", "wasigocvm"},
      {"NUMBER_OF_PROCESSORS", "1"},
      {"OS", "Windows_NT"},
      {"PATH", "C:\\Windows\\System32;C:\\Windows"},
      {"PATHEXT", ".COM;.EXE;.BAT;.CMD;.VBS;.JS;.WS;.MSC"},
      {"PROCESSOR_ARCHITECTURE", "AMD64"},
      {"ProgramData", "C:\\ProgramData"},
      {"ProgramFiles", "C:\\Program Files"},
      {"ProgramFiles(x86)", "C:\\Program Files (x86)"},
      {"PUBLIC", "C:\\Users\\Public"},
      {"SystemDrive", "C:"},
      {"SystemRoot", "C:\\Windows"},
      {"TEMP", "C:\\Users\\wasigocvm\\AppData\\Local\\Temp"},
      {"TMP", "C:\\Users\\wasigocvm\\AppData\\Local\\Temp"},
      {"TMPDIR", "C:\\Users\\wasigocvm\\AppData\\Local\\Temp"},
      {"USER", "wasigocvm"},
      {"USERDOMAIN", "WASIGO"},
      {"USERNAME", "wasigocvm"},
      {"USERPROFILE", "C:\\Users\\wasigocvm"},
      {"WINDIR", "C:\\Windows"},
      {"windir", "C:\\Windows"},
  };
  for (const Row& r : rows) {
    if (module_env().count(r.k)) continue;
    const char* cur = std::getenv(r.k);
    if (cur && cur[0])
      module_env()[r.k] = cur;
    else {
      module_env()[r.k] = r.v;
      setenv(r.k, r.v, 0);
    }
  }
#if defined(__wasi__)
  char** env = __wasilibc_get_environ();
#else
  extern char** environ;
  char** env = environ;
#endif
  if (env) {
    for (char** e = env; *e; ++e) {
      std::string row = *e;
      size_t eqp = row.find('=');
      if (eqp == std::string::npos || eqp == 0) continue;
      std::string k = row.substr(0, eqp);
      if (!module_env().count(k)) module_env()[k] = row.substr(eqp + 1);
    }
  }
}

inline const char* env_get(const char* key) {
  ensure_windows_env();
  if (!key || !key[0]) return nullptr;
  auto it = module_env().find(key);
  if (it == module_env().end() || it->second.empty()) return nullptr;
  return it->second.c_str();
}

inline void env_set(const char* key, const char* val) {
  ensure_windows_env();
  if (!key || !key[0]) return;
  if (!val || !val[0]) {
    module_env().erase(key);
    unsetenv(key);
    return;
  }
  module_env()[key] = val;
  setenv(key, val, 1);
}

inline std::string windir() {
  if (const char* w = env_get("WINDIR")) return w;
  if (const char* w = env_get("SystemRoot")) return w;
  if (const char* w = env_get("WASIGO_WINDIR")) return w;
  return "C:\\Windows";
}

inline unsigned file_attrs(const char* path) {
  struct ::stat st {};
  if (::stat(path, &st) != 0) return 0xffffffffu;
  unsigned a = 0;
  if (S_ISDIR(st.st_mode))
    a |= FILE_ATTRIBUTE_DIRECTORY;
  else
    a |= FILE_ATTRIBUTE_NORMAL;
  if ((st.st_mode & 0222) == 0) a |= FILE_ATTRIBUTE_READONLY;
  if (S_ISREG(st.st_mode)) a |= FILE_ATTRIBUTE_ARCHIVE;
  return a;
}

inline void sleep_ms(unsigned long ms) {
  if (ms >= 1000ul) {
    sleep(static_cast<unsigned>(ms / 1000ul));
    ms %= 1000ul;
  }
  if (ms) usleep(static_cast<unsigned>(ms * 1000ul));
}

inline void split1f(const char* a, std::string* left, std::string* right) {
  const char* p = a ? std::strchr(a, '\x1f') : nullptr;
  if (!p) {
    *left = a ? a : "";
    *right = "";
    return;
  }
  *left = std::string(a, p);
  *right = p + 1;
}

inline std::string join1f(const std::string& a, const std::string& b) {
  return a + "\x1f" + b;
}

inline std::string env_strings() {
  ensure_windows_env();
  std::string out;
  for (const auto& kv : module_env()) {
    if (kv.second.empty()) continue;
    if (!out.empty()) out.push_back('\x1f');
    out += kv.first;
    out.push_back('=');
    out += kv.second;
  }
  return out;
}

inline std::string expand_env(const char* in) {
  std::string s = in ? in : "";
  std::string out;
  for (size_t i = 0; i < s.size();) {
    if (s[i] != '%') {
      out.push_back(s[i++]);
      continue;
    }
    size_t j = s.find('%', i + 1);
    if (j == std::string::npos) {
      out.append(s.substr(i));
      break;
    }
    std::string key = s.substr(i + 1, j - i - 1);
    const char* v = key.empty() ? nullptr : env_get(key.c_str());
    if (v)
      out += v;
    else
      out.append(s.substr(i, j - i + 1));
    i = j + 1;
  }
  return out;
}

inline std::string full_path(const char* path) {
  if (!path || !path[0]) return err_msg("GetFullPathNameW: empty path");
  std::string p = path;
  if (p[0] == '/' || (p.size() > 1 && p[1] == ':')) return p;
  std::string d = cwd();
  if (d.rfind("error:", 0) == 0) return d;
  if (!d.empty() && d.back() != '/') d += '/';
  d += p;
  return d;
}

inline std::string copy_file(const char* src, const char* dst) {
  if (!src || !src[0] || !dst || !dst[0]) return err_msg("CopyFileW: missing path");
  FILE* in = std::fopen(src, "rb");
  if (!in) return err("CopyFileW");
  FILE* out = std::fopen(dst, "wb");
  if (!out) {
    std::fclose(in);
    return err("CopyFileW");
  }
  char buf[4096];
  size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) {
    if (std::fwrite(buf, 1, n, out) != n) {
      std::fclose(in);
      std::fclose(out);
      return err("CopyFileW");
    }
  }
  std::fclose(in);
  std::fclose(out);
  return "ok";
}

inline std::string system_time(bool local) {
  struct timespec ts {};
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return err("clock_gettime");
  time_t sec = ts.tv_sec;
  struct tm t {};
#if defined(_WIN32)
  if (local)
    localtime_s(&t, &sec);
  else
    gmtime_s(&t, &sec);
#else
  if (local)
    localtime_r(&sec, &t);
  else
    gmtime_r(&sec, &t);
#endif
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
                t.tm_sec, static_cast<int>(ts.tv_nsec / 1000000));
  return std::string(buf);
}

inline std::string filetime_now() {
  struct timespec ts {};
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return err("clock_gettime");
  unsigned long long unix100 =
      static_cast<unsigned long long>(ts.tv_sec) * 10000000ull +
      static_cast<unsigned long long>(ts.tv_nsec) / 100ull;
  return std::to_string(unix100 + 116444736000000000ull);
}

inline std::string attrs_ex(const char* path) {
  struct ::stat st {};
  if (::stat(path, &st) != 0) return err("GetFileAttributesExW");
  unsigned a = file_attrs(path);
  return std::to_string(a) + "\x1f" + std::to_string(static_cast<long long>(st.st_size)) +
         "\x1f" + std::to_string(static_cast<long long>(st.st_mtime));
}

inline const char* wsl_command_line(const char* cmd) {
  if (!cmd) return cmd;
  const char* p = std::strstr(cmd, " /c ");
  if (!p) p = std::strstr(cmd, " /C ");
  if (p) return p + 4;
  p = std::strstr(cmd, " -Command ");
  if (p) return p + 10;
  p = std::strstr(cmd, " -c ");
  if (p) return p + 4;
  return cmd;
}

inline std::string k32_nt_version();

inline std::string wsl_exec(const char* cmd) {
  while (cmd && *cmd == ' ') ++cmd;
  cmd = wsl_command_line(cmd);
  while (cmd && *cmd == ' ') ++cmd;
  if (!cmd || !cmd[0] || eq(cmd, "uname") || std::strncmp(cmd, "uname ", 6) == 0) {
    return fmt_uname();
  }
  if (eq(cmd, "pwd")) return cwd();
  if (eq(cmd, "hostname")) return hostname();
  if (eq(cmd, "true") || eq(cmd, ":")) return "ok";
  if (eq(cmd, "ver")) {
    return std::string("Microsoft Windows [Version ") + k32_nt_version() + "]";
  }
  if (std::strncmp(cmd, "echo ", 5) == 0) return cmd + 5;
  if (std::strncmp(cmd, "Write-Output ", 13) == 0) return cmd + 13;
  if (eq(cmd, "id")) {
    return std::string("uid=0 gid=0 pid=") + std::to_string(static_cast<long>(getpid()));
  }
  return err_msg("exec: not a WslExec line (wasigocvm CreateProcessW runs images)");
}

inline std::string wasi_call(const char* api, const char* args);

// In-memory device dispatch (winioctl.h CTL_CODE names, not a winmd dump).
// DeviceIoControl in wasi_k32.hpp routes here instead of a host FS trap.
enum : unsigned {
  kIoctlDiskGetDriveGeometry = 0x00070000u,
  kIoctlDiskGetLengthInfo = 0x0007405cu,
  kIoctlStorageCheckVerify = 0x002d4800u,
  kIoctlSerialGetBaudRate = 0x001b0044u,
  kFsctlPipePeek = 0x0011400cu
};

inline std::string wasi_device_ioctl(unsigned code, const std::string& in) {
  (void)in;
  switch (code) {
    case kIoctlDiskGetDriveGeometry:
      return std::string("512") + "\x1f" + "1" + "\x1f" + "255" + "\x1f" + "63";
    case kIoctlDiskGetLengthInfo:
      return "1048576";
    case kIoctlStorageCheckVerify:
      return "ok";
    case kIoctlSerialGetBaudRate:
      return "9600";
    default:
      return {};
  }
}

#include "win32/wasi_k32.hpp"

// Returns the reply string. Prefix "error:" means failure (matches host_win).
inline std::string wasi_call(const char* api, const char* args) {
  if (!api || !api[0]) return err_msg("win32 needs an API name");
  const char* a = args ? args : "";

  if (eq(api, "GetCurrentProcessId")) {
    return std::to_string(static_cast<long>(getpid()));
  }
  if (eq(api, "GetCurrentThreadId")) {
#if defined(__wasi__)
    return std::to_string(gocvm_gettid());
#else
    return "1";
#endif
  }
  if (eq(api, "GetLastError")) {
    return std::to_string(errno);
  }
  if (eq(api, "GetTickCount64")) {
    return tick_ms();
  }
  if (eq(api, "Sleep")) {
    sleep_ms(std::strtoul(a, nullptr, 10));
    return "ok";
  }
  if (eq(api, "GetComputerNameW")) {
    return hostname();
  }
  if (eq(api, "GetEnvironmentVariableW")) {
    if (!a[0]) return err_msg("GetEnvironmentVariableW: empty name");
    const char* v = env_get(a);
    if (!v) return err_msg("GetEnvironmentVariableW: not found");
    return v;
  }
  if (eq(api, "GetWindowsDirectoryW")) {
    return windir();
  }
  if (eq(api, "GetSystemDirectoryW")) {
    if (const char* s = env_get("WASIGO_SYSDIR")) return s;
    std::string w = windir();
    if (!w.empty() && w.back() != '\\' && w.back() != '/') w.push_back('\\');
    return w + "System32";
  }
  if (eq(api, "GetCurrentDirectoryW")) {
    return cwd();
  }
  if (eq(api, "GetModuleFileNameW")) {
    if (const char* p = env_get("_")) {
      if (p[0]) return p;
    }
    std::string d = cwd();
    if (d.rfind("error:", 0) == 0) return d;
    if (!d.empty() && d.back() != '/') d += '/';
    d += "main.wasm";
    return d;
  }
  if (eq(api, "GetFileAttributesW")) {
    if (!a[0]) return err_msg("GetFileAttributesW: empty path");
    unsigned attr = file_attrs(a);
    if (attr == 0xffffffffu) return err("GetFileAttributesW");
    return std::to_string(attr);
  }
  if (eq(api, "QueryPerformanceFrequency")) {
    return "1000000000";
  }
  if (eq(api, "QueryPerformanceCounter")) {
    return qpc_ns();
  }
  if (eq(api, "GetCurrentProcess")) return "-1";
  if (eq(api, "GetCurrentThread")) return "-2";
  if (eq(api, "SwitchToThread")) return "0";
  if (eq(api, "IsDebuggerPresent")) return "0";
  if (eq(api, "GetACP") || eq(api, "GetOEMCP")) return "65001";
  if (eq(api, "GetStdHandle")) {
    if (eq(a, "-10") || eq(a, "stdin")) return "0";
    if (eq(a, "-11") || eq(a, "stdout")) return "1";
    if (eq(a, "-12") || eq(a, "stderr")) return "2";
    return err_msg("GetStdHandle: unknown handle");
  }
  if (eq(api, "OutputDebugStringW")) {
    if (a[0]) std::fputs(a, stderr);
    return "ok";
  }
  if (eq(api, "GetUserNameW")) {
    if (const char* u = env_get("USER")) {
      if (u[0]) return u;
    }
    if (const char* u = env_get("USERNAME")) {
      if (u[0]) return u;
    }
    if (const char* u = env_get("LOGNAME")) {
      if (u[0]) return u;
    }
    return "wasigocvm";
  }
  if (eq(api, "GetSystemInfo")) return "0\x1f" "65536\x1f" "1";
  if (eq(api, "GetSystemTime")) return system_time(false);
  if (eq(api, "GetLocalTime")) return system_time(true);
  if (eq(api, "GetSystemTimeAsFileTime")) return filetime_now();
  if (eq(api, "GetCommandLineW")) {
    if (const char* p = env_get("_")) {
      if (p[0]) return p;
    }
    return "main.wasm";
  }
  if (eq(api, "GetTempPathW")) {
    if (const char* t = env_get("TMPDIR")) {
      if (t[0]) return t;
    }
    if (const char* t = env_get("TEMP")) {
      if (t[0]) return t;
    }
    return "/tmp";
  }
  if (eq(api, "GetEnvironmentStringsW")) return env_strings();
  if (eq(api, "SetEnvironmentVariableW")) {
    std::string name, val;
    split1f(a, &name, &val);
    if (name.empty()) return err_msg("SetEnvironmentVariableW: empty name");
    env_set(name.c_str(), val.c_str());
    return "ok";
  }
  if (eq(api, "SetCurrentDirectoryW")) {
    if (!a[0]) return err_msg("SetCurrentDirectoryW: empty path");
    if (chdir(a) != 0) return err("SetCurrentDirectoryW");
    return "ok";
  }
  if (eq(api, "ExpandEnvironmentStringsW")) return expand_env(a);
  if (eq(api, "GetFullPathNameW")) return full_path(a);
  if (eq(api, "GetFileAttributesExW")) {
    if (!a[0]) return err_msg("GetFileAttributesExW: empty path");
    return attrs_ex(a);
  }
  if (eq(api, "SetFileAttributesW")) {
    std::string path, bits;
    split1f(a, &path, &bits);
    if (path.empty()) return err_msg("SetFileAttributesW: empty path");
    unsigned attr = static_cast<unsigned>(std::strtoul(bits.c_str(), nullptr, 10));
    mode_t mode = (attr & FILE_ATTRIBUTE_READONLY) ? 0444 : 0666;
    if (chmod(path.c_str(), mode) != 0) return err("SetFileAttributesW");
    return "ok";
  }
  if (eq(api, "CreateDirectoryW")) {
    if (!a[0]) return err_msg("CreateDirectoryW: empty path");
    if (mkdir(a, 0777) != 0) return err("CreateDirectoryW");
    return "ok";
  }
  if (eq(api, "RemoveDirectoryW")) {
    if (!a[0]) return err_msg("RemoveDirectoryW: empty path");
    if (rmdir(a) != 0) return err("RemoveDirectoryW");
    return "ok";
  }
  if (eq(api, "DeleteFileW")) {
    if (!a[0]) return err_msg("DeleteFileW: empty path");
    if (unlink(a) != 0) return err("DeleteFileW");
    return "ok";
  }
  if (eq(api, "MoveFileW")) {
    std::string src, dst;
    split1f(a, &src, &dst);
    if (src.empty() || dst.empty()) return err_msg("MoveFileW: missing path");
    if (rename(src.c_str(), dst.c_str()) != 0) return err("MoveFileW");
    return "ok";
  }
  if (eq(api, "CopyFileW")) {
    std::string src, dst;
    split1f(a, &src, &dst);
    return copy_file(src.c_str(), dst.c_str());
  }
  std::string extra;
  if (try_kernel32(api, a, &extra)) return extra;
  if (eq(api, "WslIsDistributionRegistered")) {
    struct utsname u {};
    uname(&u);
    if (!a[0] || eq(a, "wasigocvm") || eq(a, "WASI") || eq(a, "wasi") ||
        eq(a, u.sysname) || eq(a, u.nodename)) {
      return "1";
    }
    return "0";
  }
  if (eq(api, "WslList")) {
    struct utsname u {};
    if (uname(&u) != 0) return err("uname");
    std::string s = u.sysname;
    if (u.release[0]) {
      s += "-";
      s += u.release;
    }
    return s;
  }
  if (eq(api, "WslExec")) {
    return wsl_exec(a);
  }
  if (eq(api, "NixVersion") || eq(api, "NixRun")) {
    return err_msg("nix is not in libc (WASI has no exec)");
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

}  // namespace wasmwin32

#endif  // WASMWIN32_INCLUDE_WIN32_WASI_HOST_HPP_
