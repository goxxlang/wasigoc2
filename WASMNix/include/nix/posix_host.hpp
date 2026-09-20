#ifndef WASMNIX_INCLUDE_NIX_POSIX_HOST_HPP_
#define WASMNIX_INCLUDE_NIX_POSIX_HOST_HPP_

// libc backend for wasmnix_call. Same Linux man-pages names as
// host_linux.cc, mapped onto POSIX that WASI actually ships (getpid,
// getenv, getcwd, gethostname, uname, stat, clocks, sleep) plus
// std::thread children for exec-shaped names. host_linux.cc adds
// fork/exec and the nix / wslpath binaries. Nix and unknown commands
// stay honest errors on WASI.

#ifndef _WASI_EMULATED_GETPID
#define _WASI_EMULATED_GETPID 1
#endif

#include "nix/dispatch.h"
#include "nix/catalog.h"
#include "nix/kernel_host.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#if defined(__wasi__)
extern "C" pid_t getppid(void);
#endif

#if !defined(__wasi__)
#include <sys/wait.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#endif

#if defined(__wasi__)
extern "C" char** __wasilibc_get_environ(void);
#else
extern "C" char** environ;
#endif

namespace wasmnix {

inline bool eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline std::string err(const char* what) {
  return std::string("error: ") + what + ": errno " + std::to_string(errno);
}

inline std::string err_msg(const char* msg) {
  return std::string("error: ") + msg;
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

inline std::vector<std::string> split_all(const char* a) {
  std::vector<std::string> v;
  std::string cur;
  if (!a) {
    v.push_back("");
    return v;
  }
  for (const char* p = a;; ++p) {
    if (*p == '\x1f' || *p == 0) {
      v.push_back(cur);
      if (*p == 0) break;
      cur.clear();
    } else {
      cur.push_back(*p);
    }
  }
  return v;
}

inline long posix_uid() {
#if defined(__wasi__)
  return 0;
#else
  return static_cast<long>(getuid());
#endif
}
inline long posix_euid() {
#if defined(__wasi__)
  return 0;
#else
  return static_cast<long>(geteuid());
#endif
}
inline long posix_gid() {
#if defined(__wasi__)
  return 0;
#else
  return static_cast<long>(getgid());
#endif
}
inline long posix_egid() {
#if defined(__wasi__)
  return 0;
#else
  return static_cast<long>(getegid());
#endif
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

inline bool release_is_wsl(const char* rel) {
  if (!rel) return false;
  return std::strstr(rel, "microsoft") != nullptr ||
         std::strstr(rel, "Microsoft") != nullptr ||
         std::strstr(rel, "WSL") != nullptr;
}

inline bool is_wsl() {
  struct utsname u {};
  if (uname(&u) != 0) return false;
  if (release_is_wsl(u.release) || release_is_wsl(u.version)) return true;
  if (std::getenv("WSL_DISTRO_NAME") && std::getenv("WSL_DISTRO_NAME")[0]) return true;
  if (std::getenv("WSL_INTEROP") && std::getenv("WSL_INTEROP")[0]) return true;
  return false;
}

inline std::string cwd() {
  char buf[4096];
  if (!getcwd(buf, sizeof(buf))) return err("getcwd");
  return std::string(buf);
}

inline void sleep_ms(unsigned long ms) {
  if (ms >= 1000ul) {
    sleep(static_cast<unsigned>(ms / 1000ul));
    ms %= 1000ul;
  }
  if (ms) usleep(static_cast<unsigned>(ms * 1000ul));
}

inline std::string env_strings() {
#if defined(__wasi__)
  char** env = __wasilibc_get_environ();
#else
  char** env = ::environ;
#endif
  std::string out;
  if (env) {
    for (char** e = env; *e; ++e) {
      if (!out.empty()) out.push_back('\x1f');
      out += *e;
    }
  }
  return out;
}

inline std::string tick_ns() {
  struct timespec ts {};
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return err("clock_gettime");
  std::uint64_t ns = static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull +
                     static_cast<std::uint64_t>(ts.tv_nsec);
  return std::to_string(ns);
}

inline std::string iso_time(bool local) {
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

inline std::string capture_cmd(const char* cmd) {
#if defined(__wasi__)
  (void)cmd;
  return err_msg("libc has no exec (WASI)");
#else
  if (!cmd || !cmd[0]) return err_msg("empty command");
  FILE* p = popen(cmd, "r");
  if (!p) return err("popen");
  std::string out;
  char buf[4096];
  while (fgets(buf, sizeof(buf), p)) out += buf;
  int rc = pclose(p);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  if (rc != 0 && out.empty()) {
    return err_msg((std::string("command failed rc=") + std::to_string(rc)).c_str());
  }
  if (rc != 0) return std::string("error: ") + out;
  return out;
#endif
}

inline std::string which_bin(const char* name) {
#if defined(__wasi__)
  (void)name;
  return {};
#else
  std::string cmd = std::string("command -v ") + name + " 2>/dev/null";
  std::string p = capture_cmd(cmd.c_str());
  if (p.rfind("error:", 0) == 0) return {};
  return p;
#endif
}

inline std::string wsl_exec_guest(const char* cmd) {
  while (cmd && *cmd == ' ') ++cmd;
  if (!cmd || !cmd[0] || eq(cmd, "uname") || std::strncmp(cmd, "uname ", 6) == 0) {
    return fmt_uname();
  }
  if (eq(cmd, "pwd")) return cwd();
  if (eq(cmd, "hostname")) return hostname();
  if (eq(cmd, "true") || eq(cmd, ":")) return "ok";
  if (std::strncmp(cmd, "echo ", 5) == 0) return cmd + 5;
  if (eq(cmd, "id")) {
    return std::string("uid=") + std::to_string(posix_uid()) +
           " gid=" + std::to_string(posix_gid()) +
           " pid=" + std::to_string(static_cast<long>(getpid()));
  }
#if defined(__wasi__)
  return err_msg("libc has no exec (WASI)");
#else
  return capture_cmd(cmd);
#endif
}

inline std::string file_mode(const char* path) {
  struct ::stat st {};
  if (::stat(path, &st) != 0) return err("stat");
  std::string s = std::to_string(static_cast<long long>(st.st_size));
  s += "\x1f";
  s += std::to_string(static_cast<unsigned>(st.st_mode));
  s += "\x1f";
  s += std::to_string(static_cast<long long>(st.st_mtime));
  s += "\x1f";
  if (S_ISDIR(st.st_mode))
    s += "dir";
  else if (S_ISREG(st.st_mode))
    s += "file";
  else
    s += "other";
  return s;
}

inline std::string copy_file(const char* src, const char* dst) {
  if (!src || !src[0] || !dst || !dst[0]) return err_msg("copy: missing path");
  FILE* in = std::fopen(src, "rb");
  if (!in) return err("fopen");
  FILE* out = std::fopen(dst, "wb");
  if (!out) {
    std::fclose(in);
    return err("fopen");
  }
  char buf[4096];
  size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) {
    if (std::fwrite(buf, 1, n, out) != n) {
      std::fclose(in);
      std::fclose(out);
      return err("fwrite");
    }
  }
  std::fclose(in);
  std::fclose(out);
  return "ok";
}

enum { kFdMax = 128 };

struct NixFd {
  FILE* f = nullptr;
  DIR* d = nullptr;
  std::string path;
  int used = 0;
};

inline NixFd* fd_tab() {
  static NixFd t[kFdMax];
  return t;
}

inline int alloc_file(FILE* f, const std::string& path) {
  NixFd* t = fd_tab();
  for (int i = 3; i < kFdMax; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].f = f;
      t[i].d = nullptr;
      t[i].path = path;
      return i;
    }
  }
  return -1;
}

inline NixFd* fd_at(int h) {
  if (h < 3 || h >= kFdMax || !fd_tab()[h].used) return nullptr;
  return &fd_tab()[h];
}

inline bool close_fd(int h) {
  NixFd* p = fd_at(h);
  if (!p) return false;
  if (p->f) std::fclose(p->f);
  if (p->d) closedir(p->d);
  p->f = nullptr;
  p->d = nullptr;
  p->used = 0;
  p->path.clear();
  return true;
}

inline std::string posix_call(const char* api, const char* args);

inline std::string posix_call(const char* api, const char* args) {
  if (!api || !api[0]) return err_msg("linux needs an API name");
  const char* a = args ? args : "";

  if (eq(api, "getpid")) return std::to_string(static_cast<long>(getpid()));
  if (eq(api, "getppid")) {
#if defined(_WIN32) && !defined(__wasi__)
    return "0";
#else
    return std::to_string(static_cast<long>(getppid()));
#endif
  }
  if (eq(api, "getuid") || eq(api, "geteuid"))
    return std::to_string(eq(api, "geteuid") ? posix_euid() : posix_uid());
  if (eq(api, "getgid") || eq(api, "getegid"))
    return std::to_string(eq(api, "getegid") ? posix_egid() : posix_gid());
  if (eq(api, "gettid")) {
#if defined(__linux__) && !defined(__wasi__)
    return std::to_string(static_cast<long>(syscall(SYS_gettid)));
#else
    return std::to_string(static_cast<long>(getpid()));
#endif
  }
  if (eq(api, "getpgid") || eq(api, "getpgrp")) {
#if defined(__wasi__)
    return std::to_string(static_cast<long>(getpid()));
#else
    pid_t p = eq(api, "getpgrp") ? getpgrp()
                                 : getpgid(static_cast<pid_t>(std::strtol(a, nullptr, 10)));
    if (p < 0) return err(api);
    return std::to_string(static_cast<long>(p));
#endif
  }
  if (eq(api, "getsid")) {
#if defined(__wasi__)
    return std::to_string(static_cast<long>(getpid()));
#else
    pid_t p = getsid(static_cast<pid_t>(std::strtol(a, nullptr, 10)));
    if (p < 0) return err("getsid");
    return std::to_string(static_cast<long>(p));
#endif
  }
  if (eq(api, "uname")) return fmt_uname();
  if (eq(api, "gethostname")) return hostname();
  if (eq(api, "sethostname")) {
#if defined(__wasi__)
    return err_msg("sethostname: not in WASI libc");
#else
    if (sethostname(a, std::strlen(a)) != 0) return err("sethostname");
    return "ok";
#endif
  }
  if (eq(api, "getcwd") || eq(api, "get_current_dir_name") || eq(api, "getwd")) return cwd();
  if (eq(api, "chdir")) {
    if (!a[0]) return err_msg("chdir: empty path");
    if (chdir(a) != 0) return err("chdir");
    return "ok";
  }
  if (eq(api, "fchdir")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h >= 0 && h <= 2) {
      // stdin/out/err are not directories
      return err_msg("fchdir: not a directory");
    }
#if defined(__wasi__)
    return err_msg("fchdir: not in WASI libc");
#else
    if (fchdir(h) != 0) return err("fchdir");
    return "ok";
#endif
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
    if (eq(api, "putenv") && val.empty()) {
      // putenv("NAME=value") in one field
      if (putenv(const_cast<char*>(a)) != 0) return err("putenv");
      return "ok";
    }
    if (setenv(name.c_str(), val.c_str(), 1) != 0) return err("setenv");
    return "ok";
  }
  if (eq(api, "unsetenv")) {
    if (!a[0]) return err_msg("unsetenv: empty name");
    if (unsetenv(a) != 0) return err("unsetenv");
    return "ok";
  }
  if (eq(api, "clearenv")) {
#if defined(__wasi__) || defined(__APPLE__)
    return err_msg("clearenv: not available");
#else
    if (clearenv() != 0) return err("clearenv");
    return "ok";
#endif
  }
  if (eq(api, "environ") || eq(api, "envz_get")) return env_strings();

  if (eq(api, "sleep")) {
    unsigned sec = static_cast<unsigned>(std::strtoul(a, nullptr, 10));
    sleep(sec);
    return "ok";
  }
  if (eq(api, "usleep")) {
    usleep(static_cast<unsigned>(std::strtoul(a, nullptr, 10)));
    return "ok";
  }
  if (eq(api, "nanosleep")) {
    unsigned long ms = std::strtoul(a, nullptr, 10);
    sleep_ms(ms);
    return "ok";
  }
  if (eq(api, "clock_gettime") || eq(api, "clock_getres")) {
    clockid_t clk = CLOCK_REALTIME;
    if (eq(a, "monotonic") || eq(a, "1")) clk = CLOCK_MONOTONIC;
    struct timespec ts {};
    if (eq(api, "clock_getres")) {
      if (clock_getres(clk, &ts) != 0) return err(api);
    } else {
      if (clock_gettime(clk, &ts) != 0) return err(api);
    }
    return std::to_string(static_cast<long long>(ts.tv_sec)) + "\x1f" +
           std::to_string(static_cast<long>(ts.tv_nsec));
  }
  if (eq(api, "time")) {
    return std::to_string(static_cast<long long>(time(nullptr)));
  }
  if (eq(api, "gettimeofday")) return iso_time(false);
  if (eq(api, "clock")) return tick_ns();
  if (eq(api, "timespec_get")) return iso_time(true);
  if (eq(api, "localtime") || eq(api, "localtime_r") || eq(api, "ctime") ||
      eq(api, "ctime_r"))
    return iso_time(true);
  if (eq(api, "gmtime") || eq(api, "gmtime_r") || eq(api, "asctime") ||
      eq(api, "asctime_r"))
    return iso_time(false);

  if (eq(api, "sysconf")) {
    long v = sysconf(_SC_PAGESIZE);
    if (eq(a, "nprocessors") || eq(a, "_SC_NPROCESSORS_ONLN"))
      v = sysconf(_SC_NPROCESSORS_ONLN);
    else if (eq(a, "pagesize") || eq(a, "_SC_PAGESIZE") || !a[0])
      v = sysconf(_SC_PAGESIZE);
    else if (eq(a, "clk_tck") || eq(a, "_SC_CLK_TCK"))
      v = sysconf(_SC_CLK_TCK);
    if (v < 0) return err("sysconf");
    return std::to_string(v);
  }
  if (eq(api, "getpagesize") || eq(api, "get_phys_pages")) {
    long v = sysconf(_SC_PAGESIZE);
    return std::to_string(v < 0 ? 4096 : v);
  }
  if (eq(api, "get_nprocs") || eq(api, "get_nprocs_conf") || eq(api, "getcpu")) {
    long v = sysconf(_SC_NPROCESSORS_ONLN);
    return std::to_string(v < 1 ? 1 : v);
  }
  if (eq(api, "sysinfo")) {
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    long page = sysconf(_SC_PAGESIZE);
    return std::to_string(cpus < 1 ? 1 : cpus) + "\x1f" +
           std::to_string(page < 0 ? 4096 : page);
  }
  if (eq(api, "gnu_get_libc_version") || eq(api, "gnu_get_libc_release")) {
#ifdef __GLIBC__
    return capture_cmd("ldd --version 2>/dev/null | head -1");
#else
    return "wasigocvm";
#endif
  }

  if (eq(api, "mkdir") || eq(api, "mkdirat")) {
    std::string path = a;
    if (eq(api, "mkdirat")) {
      std::string dirfd, rest;
      split1f(a, &dirfd, &rest);
      path = rest.empty() ? dirfd : rest;
    }
    if (path.empty()) return err_msg("mkdir: empty path");
    if (mkdir(path.c_str(), 0777) != 0) return err("mkdir");
    return "ok";
  }
  if (eq(api, "rmdir")) {
    if (!a[0]) return err_msg("rmdir: empty path");
    if (rmdir(a) != 0) return err("rmdir");
    return "ok";
  }
  if (eq(api, "unlink") || eq(api, "unlinkat") || eq(api, "remove")) {
    std::string path = a;
    if (eq(api, "unlinkat")) {
      std::string dirfd, rest;
      split1f(a, &dirfd, &rest);
      path = rest.empty() ? dirfd : rest;
    }
    if (path.empty()) return err_msg("unlink: empty path");
    if (unlink(path.c_str()) != 0) return err("unlink");
    return "ok";
  }
  if (eq(api, "rename") || eq(api, "renameat") || eq(api, "renameat2")) {
    std::string src, dst;
    split1f(a, &src, &dst);
    if (src.empty() || dst.empty()) return err_msg("rename: missing path");
    if (rename(src.c_str(), dst.c_str()) != 0) return err("rename");
    return "ok";
  }
  if (eq(api, "link") || eq(api, "linkat")) {
    std::string src, dst;
    split1f(a, &src, &dst);
    if (src.empty() || dst.empty()) return err_msg("link: missing path");
    if (link(src.c_str(), dst.c_str()) != 0) return err("link");
    return "ok";
  }
  if (eq(api, "symlink") || eq(api, "symlinkat")) {
    std::string tgt, path;
    split1f(a, &tgt, &path);
    if (tgt.empty() || path.empty()) return err_msg("symlink: missing path");
    if (symlink(tgt.c_str(), path.c_str()) != 0) return err("symlink");
    return "ok";
  }
  if (eq(api, "readlink") || eq(api, "readlinkat")) {
    if (!a[0]) return err_msg("readlink: empty path");
    char buf[4096];
    ssize_t n = readlink(a, buf, sizeof(buf) - 1);
    if (n < 0) return err("readlink");
    buf[n] = 0;
    return buf;
  }
  if (eq(api, "access") || eq(api, "euidaccess") || eq(api, "faccessat") ||
      eq(api, "faccessat2")) {
    std::string path, mode;
    split1f(a, &path, &mode);
    if (path.empty()) path = a;
    int m = F_OK;
    if (mode.find('r') != std::string::npos || mode == "4") m |= R_OK;
    if (mode.find('w') != std::string::npos || mode == "2") m |= W_OK;
    if (mode.find('x') != std::string::npos || mode == "1") m |= X_OK;
    if (access(path.c_str(), m) != 0) return err("access");
    return "ok";
  }
  if (eq(api, "chmod") || eq(api, "fchmod") || eq(api, "fchmodat") ||
      eq(api, "fchmodat2")) {
    std::string path, mode;
    split1f(a, &path, &mode);
    if (path.empty()) return err_msg("chmod: empty path");
    unsigned m = static_cast<unsigned>(std::strtoul(mode.empty() ? "0644" : mode.c_str(),
                                                    nullptr, 8));
    if (chmod(path.c_str(), static_cast<mode_t>(m)) != 0) return err("chmod");
    return "ok";
  }
  if (eq(api, "umask")) {
#if defined(__wasi__)
    return err_msg("umask: not in WASI libc");
#else
    mode_t m = static_cast<mode_t>(std::strtoul(a[0] ? a : "022", nullptr, 8));
    mode_t old = umask(m);
    umask(old);  // restore; report previous
    if (a[0]) umask(m);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%03o", static_cast<unsigned>(a[0] ? m : old));
    return buf;
#endif
  }
  if (eq(api, "stat") || eq(api, "lstat") || eq(api, "fstat") || eq(api, "statx") ||
      eq(api, "fstatat") || eq(api, "newfstatat") || eq(api, "stat64") ||
      eq(api, "lstat64") || eq(api, "fstat64") || eq(api, "statfs") ||
      eq(api, "fstatfs") || eq(api, "statvfs") || eq(api, "fstatvfs")) {
    std::string path = a;
    if (eq(api, "fstat") || eq(api, "fstat64") || eq(api, "fstatfs") ||
        eq(api, "fstatvfs")) {
      int h = static_cast<int>(std::strtol(a, nullptr, 10));
      if (NixFd* f = fd_at(h)) path = f->path;
    }
    if (path.empty()) return err_msg("stat: empty path");
    return file_mode(path.c_str());
  }
  if (eq(api, "realpath") || eq(api, "canonicalize_file_name")) {
    if (!a[0]) return err_msg("realpath: empty path");
#if defined(__wasi__)
    if (a[0] == '/') return a;
    std::string d = cwd();
    if (d.rfind("error:", 0) == 0) return d;
    if (!d.empty() && d.back() != '/') d += '/';
    d += a;
    return d;
#else
    char buf[4096];
    if (!realpath(a, buf)) return err("realpath");
    return buf;
#endif
  }

  if (eq(api, "open") || eq(api, "openat") || eq(api, "openat2") || eq(api, "creat") ||
      eq(api, "fopen") || eq(api, "fdopen") || eq(api, "freopen")) {
    std::string path, rest, flags;
    split1f(a, &path, &rest);
    split1f(rest.c_str(), &flags, &rest);
    if (path.empty()) return err_msg("open: empty path");
    const char* mode = "rb+";
    if (flags.find('w') != std::string::npos || flags.find("CREAT") != std::string::npos ||
        eq(api, "creat"))
      mode = "wb+";
    else if (flags.find('a') != std::string::npos)
      mode = "ab+";
    else if (flags == "r" || flags == "rb")
      mode = "rb";
    else if (flags == "w" || flags == "wb")
      mode = "wb+";
    FILE* f = std::fopen(path.c_str(), mode);
    if (!f && (mode[0] == 'w' || mode[0] == 'a')) f = std::fopen(path.c_str(), "wb+");
    if (!f) f = std::fopen(path.c_str(), "rb");
    if (!f) return err("open");
    int h = alloc_file(f, path);
    if (h < 0) {
      std::fclose(f);
      return err_msg("open: fd table full");
    }
    return std::to_string(h);
  }
  if (eq(api, "close") || eq(api, "fclose") || eq(api, "close_range")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (h <= 2) return "ok";
    if (!close_fd(h)) return err_msg("close: bad fd");
    return "ok";
  }
  if (eq(api, "read") || eq(api, "fread") || eq(api, "pread") || eq(api, "pread64")) {
    std::string hs, nstr;
    split1f(a, &hs, &nstr);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    unsigned n = static_cast<unsigned>(std::strtoul(nstr.empty() ? "4096" : nstr.c_str(),
                                                    nullptr, 10));
    if (n > 65536) n = 65536;
    if (h == 0) {
      std::string s(n, '\0');
      size_t got = std::fread(s.data(), 1, n, stdin);
      s.resize(got);
      return s;
    }
    NixFd* f = fd_at(h);
    if (!f || !f->f) return err_msg("read: bad fd");
    std::string s(n, '\0');
    size_t got = std::fread(s.data(), 1, n, f->f);
    s.resize(got);
    return s;
  }
  if (eq(api, "write") || eq(api, "fwrite") || eq(api, "pwrite") || eq(api, "pwrite64")) {
    std::string hs, data;
    split1f(a, &hs, &data);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    if (h == 1) {
      std::fputs(data.c_str(), stdout);
      return std::to_string(data.size());
    }
    if (h == 2) {
      std::fputs(data.c_str(), stderr);
      return std::to_string(data.size());
    }
    NixFd* f = fd_at(h);
    if (!f || !f->f) return err_msg("write: bad fd");
    size_t n = std::fwrite(data.data(), 1, data.size(), f->f);
    return std::to_string(n);
  }
  if (eq(api, "lseek") || eq(api, "lseek64") || eq(api, "fseek") || eq(api, "fseeko")) {
    std::string hs, rest, off, whence;
    split1f(a, &hs, &rest);
    split1f(rest.c_str(), &off, &whence);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    NixFd* f = fd_at(h);
    if (!f || !f->f) return err_msg("lseek: bad fd");
    int w = SEEK_SET;
    if (whence == "1" || whence == "SEEK_CUR") w = SEEK_CUR;
    if (whence == "2" || whence == "SEEK_END") w = SEEK_END;
    if (std::fseek(f->f, std::strtol(off.c_str(), nullptr, 10), w) != 0) return err("lseek");
    return std::to_string(std::ftell(f->f));
  }
  if (eq(api, "fsync") || eq(api, "fdatasync") || eq(api, "fflush")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (NixFd* f = fd_at(h)) {
      if (f->f) std::fflush(f->f);
      return "ok";
    }
    return err_msg("fsync: bad fd");
  }
  if (eq(api, "dup") || eq(api, "dup2") || eq(api, "dup3") || eq(api, "fileno")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    return std::to_string(h);
  }
  if (eq(api, "pipe") || eq(api, "pipe2")) {
#if defined(__wasi__)
    return err_msg("pipe: not in WASI libc");
#else
    int p[2];
    if (pipe(p) != 0) return err("pipe");
    FILE* r = fdopen(p[0], "rb");
    FILE* w = fdopen(p[1], "wb");
    if (!r || !w) return err("fdopen");
    int rh = alloc_file(r, "pipe:r");
    int wh = alloc_file(w, "pipe:w");
    if (rh < 0 || wh < 0) return err_msg("pipe: fd table full");
    return std::to_string(rh) + "\x1f" + std::to_string(wh);
#endif
  }
  if (eq(api, "opendir") || eq(api, "fdopendir")) {
    if (!a[0]) return err_msg("opendir: empty path");
    DIR* d = opendir(a);
    if (!d) return err("opendir");
    NixFd* t = fd_tab();
    for (int i = 3; i < kFdMax; ++i) {
      if (!t[i].used) {
        t[i].used = 2;
        t[i].d = d;
        t[i].path = a;
        return std::to_string(i);
      }
    }
    closedir(d);
    return err_msg("opendir: fd table full");
  }
  if (eq(api, "readdir") || eq(api, "readdir_r") || eq(api, "readdir64") ||
      eq(api, "getdents") || eq(api, "getdents64")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    NixFd* f = fd_at(h);
    if (!f || !f->d) return err_msg("readdir: bad dir");
    errno = 0;
    struct dirent* e = readdir(f->d);
    if (!e) return errno ? err("readdir") : "";
    return e->d_name;
  }
  if (eq(api, "closedir")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    if (!close_fd(h)) return err_msg("closedir: bad fd");
    return "ok";
  }

  if (eq(api, "malloc") || eq(api, "calloc") || eq(api, "valloc") ||
      eq(api, "pvalloc") || eq(api, "memalign") || eq(api, "aligned_alloc") ||
      eq(api, "posix_memalign") || eq(api, "OPENSSL_malloc")) {
    size_t n = static_cast<size_t>(std::strtoul(a, nullptr, 10));
    if (!n) n = 1;
    void* p = std::malloc(n);
    if (!p) return err_msg("malloc: oom");
    if (eq(api, "calloc")) std::memset(p, 0, n);
    return std::to_string(reinterpret_cast<uintptr_t>(p));
  }
  if (eq(api, "free") || eq(api, "OPENSSL_free")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    if (p) std::free(reinterpret_cast<void*>(p));
    return "ok";
  }
  if (eq(api, "realloc") || eq(api, "reallocarray")) {
    std::string ps, ns;
    split1f(a, &ps, &ns);
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(std::strtoull(ps.c_str(), nullptr, 10)));
    size_t n = static_cast<size_t>(std::strtoul(ns.c_str(), nullptr, 10));
    void* q = std::realloc(p, n ? n : 1);
    if (!q) return err_msg("realloc: oom");
    return std::to_string(reinterpret_cast<uintptr_t>(q));
  }
  if (eq(api, "mmap") || eq(api, "mmap64") || eq(api, "mmap2") || eq(api, "memfd_create")) {
    size_t n = static_cast<size_t>(std::strtoul(a, nullptr, 10));
    if (!n) n = 4096;
    void* p = std::malloc(n);
    if (!p) return err_msg("mmap: oom");
    std::memset(p, 0, n);
    return std::to_string(reinterpret_cast<uintptr_t>(p));
  }
  if (eq(api, "munmap") || eq(api, "brk")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    if (p) std::free(reinterpret_cast<void*>(p));
    return "ok";
  }
  if (eq(api, "mprotect") || eq(api, "msync") || eq(api, "madvise") ||
      eq(api, "mlock") || eq(api, "munlock") || eq(api, "posix_madvise"))
    return "ok";

  if (eq(api, "getlogin") || eq(api, "getlogin_r")) {
    if (const char* u = std::getenv("USER")) {
      if (u[0]) return u;
    }
    if (const char* u = std::getenv("LOGNAME")) {
      if (u[0]) return u;
    }
    if (const char* u = std::getenv("USERNAME")) {
      if (u[0]) return u;
    }
    return "wasigocvm";
  }
  if (eq(api, "isatty")) {
    int h = a[0] ? static_cast<int>(std::strtol(a, nullptr, 10)) : 1;
    return isatty(h) ? "1" : "0";
  }
  if (eq(api, "strerror") || eq(api, "strerror_r") || eq(api, "perror")) {
    int e = a[0] ? static_cast<int>(std::strtol(a, nullptr, 10)) : errno;
    const char* s = std::strerror(e);
    return s ? s : "unknown";
  }
  if (eq(api, "strlen")) return std::to_string(std::strlen(a));
  if (eq(api, "strcmp") || eq(api, "strncmp")) {
    std::string l, r;
    split1f(a, &l, &r);
    return std::to_string(std::strcmp(l.c_str(), r.c_str()));
  }
  if (eq(api, "strcasecmp") || eq(api, "strncasecmp")) {
    std::string l, r;
    split1f(a, &l, &r);
#if defined(_WIN32)
    return std::to_string(_stricmp(l.c_str(), r.c_str()));
#else
    return std::to_string(strcasecmp(l.c_str(), r.c_str()));
#endif
  }
  if (eq(api, "memcpy") || eq(api, "memmove") || eq(api, "strcpy") || eq(api, "strncpy") ||
      eq(api, "strcat") || eq(api, "strdup") || eq(api, "strndup"))
    return a ? a : "";
  if (eq(api, "memset") || eq(api, "bzero") || eq(api, "explicit_bzero")) return "ok";

  if (eq(api, "sched_yield") || eq(api, "pthread_yield") || eq(api, "pthread_self")) {
    std::this_thread::yield();
    return eq(api, "pthread_self") ? "1" : "ok";
  }
  if (eq(api, "pthread_equal")) return "1";
  if (eq(api, "getentropy") || eq(api, "getrandom") || eq(api, "RAND_bytes") ||
      eq(api, "RAND_status")) {
#if defined(__wasi__)
    unsigned v = static_cast<unsigned>(time(nullptr) ^ getpid());
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08x", v);
    return buf;
#else
    unsigned char b[16];
    FILE* ur = std::fopen("/dev/urandom", "rb");
    if (!ur) return err("getrandom");
    size_t got = std::fread(b, 1, sizeof(b), ur);
    std::fclose(ur);
    std::string hex;
    static const char* h = "0123456789abcdef";
    for (size_t i = 0; i < got; ++i) {
      hex.push_back(h[b[i] >> 4]);
      hex.push_back(h[b[i] & 0xf]);
    }
    return hex;
#endif
  }

  if (eq(api, "fork") || eq(api, "vfork") || eq(api, "clone") || eq(api, "clone3") ||
      eq(api, "posix_spawn") || eq(api, "posix_spawnp") || eq(api, "system") ||
      eq(api, "execve") || eq(api, "execv") || eq(api, "execvp") || eq(api, "execl") ||
      eq(api, "execlp")) {
    std::string cmd = a[0] ? a : "true";
    std::string out = wsl_exec_guest(cmd.c_str());
    if (out.rfind("error:", 0) == 0) return out;
    return "0\x1f" + out;  // pid-ish \x1f output
  }
  if (eq(api, "wait") || eq(api, "waitpid") || eq(api, "waitid") || eq(api, "wait4"))
    return "0";
  if (eq(api, "_exit") || eq(api, "exit") || eq(api, "_Exit") || eq(api, "quick_exit"))
    return "ok";
  if (eq(api, "kill") || eq(api, "tkill") || eq(api, "tgkill") || eq(api, "raise"))
    return "ok";
  if (eq(api, "signal") || eq(api, "sigaction") || eq(api, "sigprocmask")) return "ok";

  if (eq(api, "dlopen") || eq(api, "dlmopen")) {
    if (!a[0]) return "1";
    int n = 0;
    const WasmNixApi* c = wasmnix_catalog(&n);
    std::string want = a;
    for (int i = 0; i < n; ++i) {
      if (c[i].lib && (eq(c[i].lib, want.c_str()) ||
                       want.find(c[i].lib) != std::string::npos))
        return "1";
    }
    return err_msg("dlopen: module not found");
  }
  if (eq(api, "dlsym") || eq(api, "dlvsym") || eq(api, "dlinfo") || eq(api, "dladdr")) {
    int n = 0;
    const WasmNixApi* c = wasmnix_catalog(&n);
    std::string hs, name;
    split1f(a, &hs, &name);
    if (name.empty()) name = a;
    for (int i = 0; i < n; ++i)
      if (c[i].name && eq(c[i].name, name.c_str()))
        return std::to_string(1000 + i);
    return err_msg("dlsym: not found");
  }
  if (eq(api, "dlclose") || eq(api, "dlerror")) return eq(api, "dlerror") ? "" : "ok";

  // WSL Linux-side
  if (eq(api, "WslIsWsl") || eq(api, "WslInfoMsWsl")) return is_wsl() ? "1" : "0";
  if (eq(api, "WslDistro") || eq(api, "WSL_DISTRO_NAME")) {
    if (const char* d = std::getenv("WSL_DISTRO_NAME")) {
      if (d[0]) return d;
    }
    struct utsname u {};
    uname(&u);
    return u.nodename[0] ? u.nodename : "wasigocvm";
  }
  if (eq(api, "WslInterop") || eq(api, "WSL_INTEROP") || eq(api, "WslInteropSocket")) {
    if (const char* s = std::getenv("WSL_INTEROP")) {
      if (s[0]) return s;
    }
    return is_wsl() ? "/run/WSL" : err_msg("WSL_INTEROP: not set");
  }
  if (eq(api, "WslInteropEnabled")) {
    return (is_wsl() || std::getenv("WSL_INTEROP")) ? "1" : "0";
  }
  if (eq(api, "WslMountRoot") || eq(api, "WslDrvFs")) {
    if (is_wsl()) {
      struct ::stat st {};
      if (::stat("/mnt/c", &st) == 0) return "/mnt/c";
      if (::stat("/mnt", &st) == 0) return "/mnt";
    }
    return err_msg("DrvFs: not mounted");
  }
  if (eq(api, "WslConf")) {
    struct ::stat st {};
    if (::stat("/etc/wsl.conf", &st) == 0) return "/etc/wsl.conf";
    return err_msg("wsl.conf: not found");
  }
  if (eq(api, "WslgDisplay") || eq(api, "DISPLAY")) {
    if (const char* d = std::getenv("DISPLAY")) {
      if (d[0]) return d;
    }
    return err_msg("DISPLAY: not set");
  }
  if (eq(api, "WslgWayland") || eq(api, "WAYLAND_DISPLAY")) {
    if (const char* d = std::getenv("WAYLAND_DISPLAY")) {
      if (d[0]) return d;
    }
    return err_msg("WAYLAND_DISPLAY: not set");
  }
  if (eq(api, "WslgPulse") || eq(api, "PULSE_SERVER")) {
    if (const char* d = std::getenv("PULSE_SERVER")) {
      if (d[0]) return d;
    }
    return err_msg("PULSE_SERVER: not set");
  }
  if (eq(api, "WslLibPath")) {
    struct ::stat st {};
    if (::stat("/usr/lib/wsl/lib", &st) == 0) return "/usr/lib/wsl/lib";
    return err_msg("wsl lib: not found");
  }
  if (eq(api, "WslInit") || eq(api, "/init")) {
    struct ::stat st {};
    if (::stat("/init", &st) == 0) return "/init";
    return err_msg("/init: not found");
  }
  if (eq(api, "WslEnv") || eq(api, "WSLENV")) {
    if (const char* e = std::getenv("WSLENV")) return e;
    return "";
  }
  if (eq(api, "WslPath") || eq(api, "WslPathUnix") || eq(api, "WslPathWindows") ||
      eq(api, "WslPathMixed") || eq(api, "wslpath")) {
    std::string flags, path;
    split1f(a, &flags, &path);
    if (path.empty()) {
      path = flags;
      flags.clear();
    }
    if (eq(api, "WslPathWindows")) flags = "-w";
    if (eq(api, "WslPathMixed")) flags = "-m";
    if (eq(api, "WslPathUnix")) flags = "-u";
    std::string bin = which_bin("wslpath");
    if (bin.empty()) {
      // Honest fallback: /mnt/c/Users/... <-> C:\Users\...
      if (path.size() >= 3 && ((path[0] >= 'A' && path[0] <= 'Z') ||
                               (path[0] >= 'a' && path[0] <= 'z')) &&
          path[1] == ':') {
        char drive = path[0];
        if (drive >= 'A' && drive <= 'Z') drive = static_cast<char>(drive - 'A' + 'a');
        std::string out = "/mnt/";
        out.push_back(drive);
        for (size_t i = 2; i < path.size(); ++i) out.push_back(path[i] == '\\' ? '/' : path[i]);
        return out;
      }
      if (path.rfind("/mnt/", 0) == 0 && path.size() > 6) {
        char drive = path[5];
        if (drive >= 'a' && drive <= 'z') drive = static_cast<char>(drive - 'a' + 'A');
        std::string out;
        out.push_back(drive);
        out += ":";
        for (size_t i = 6; i < path.size(); ++i) out.push_back(path[i] == '/' ? '\\' : path[i]);
        return out;
      }
      return path.empty() ? cwd() : path;
    }
    std::string cmd = "wslpath";
    if (!flags.empty()) {
      cmd += " ";
      cmd += flags;
    }
    if (!path.empty()) {
      cmd += " ";
      cmd += "'";
      cmd += path;
      cmd += "'";
    }
    return capture_cmd(cmd.c_str());
  }
  if (eq(api, "WslInfo") || eq(api, "WslInfoNetworkingMode") || eq(api, "WslInfoVersion") ||
      eq(api, "WslInfoVmId") || eq(api, "WslInfoNthreads") || eq(api, "wslinfo")) {
    std::string bin = which_bin("wslinfo");
    std::string flag;
    if (eq(api, "WslInfoNetworkingMode")) flag = "--networking-mode";
    else if (eq(api, "WslInfoVersion")) flag = "--wsl-version";
    else if (eq(api, "WslInfoVmId")) flag = "--vm-id";
    else if (eq(api, "WslInfoNthreads")) flag = "--nthreads";
    else if (eq(api, "WslInfo") || eq(api, "wslinfo")) flag = a[0] ? a : "--wsl-version";
    if (bin.empty()) {
      if (flag.find("nthreads") != std::string::npos)
        return posix_call("get_nprocs", "");
      if (flag.find("version") != std::string::npos) {
        struct utsname u {};
        uname(&u);
        return u.release;
      }
      return is_wsl() ? "wsl2" : err_msg("wslinfo: not in WSL");
    }
    std::string cmd = "wslinfo";
    if (!flag.empty()) {
      cmd += " ";
      cmd += flag;
    }
    return capture_cmd(cmd.c_str());
  }
  if (eq(api, "WslVar") || eq(api, "wslvar")) {
    std::string bin = which_bin("wslvar");
    if (bin.empty()) {
      const char* v = a[0] ? std::getenv(a) : nullptr;
      return v ? v : err_msg("wslvar: not found");
    }
    std::string cmd = std::string("wslvar ") + a;
    return capture_cmd(cmd.c_str());
  }
  if (eq(api, "WslList")) {
    if (is_wsl()) {
      struct utsname u {};
      uname(&u);
      std::string s = u.sysname;
      if (const char* d = std::getenv("WSL_DISTRO_NAME")) {
        if (d[0]) return d;
      }
      if (u.release[0]) {
        s += "-";
        s += u.release;
      }
      return s;
    }
    return wsl_exec_guest("uname");
  }
  if (eq(api, "WslExec") || eq(api, "WslLaunchWin32")) return wsl_exec_guest(a);
  if (eq(api, "WslIsDistributionRegistered")) {
    if (!a[0] || is_wsl()) return "1";
    struct utsname u {};
    uname(&u);
    if (eq(a, u.sysname) || eq(a, u.nodename) || eq(a, "wasigocvm")) return "1";
    return "0";
  }
  if (eq(api, "WslVersion") || eq(api, "WslStatus")) {
    if (is_wsl()) {
      struct utsname u {};
      uname(&u);
      return u.release;
    }
    return err_msg("not WSL");
  }

  // Nix CLI — native binary, not via wsl.exe (this hop is already Linux)
  auto nix_cmd = [&](const char* sub) -> std::string {
    std::string bin = which_bin("nix");
    if (bin.empty()) bin = which_bin("nix-build");
    if (bin.empty()) return err_msg("nix is not on PATH");
    std::string cmd = "nix";
    if (sub && sub[0]) {
      cmd += " ";
      cmd += sub;
    }
    if (a[0]) {
      cmd += " ";
      cmd += a;
    }
    return capture_cmd(cmd.c_str());
  };
  if (eq(api, "NixVersion") || eq(api, "nix --version")) {
    std::string bin = which_bin("nix");
    if (bin.empty()) return err_msg("nix is not on PATH");
    return capture_cmd("nix --version");
  }
  if (eq(api, "NixRun") || eq(api, "nix run")) return nix_cmd("run");
  if (eq(api, "NixBuild") || eq(api, "nix build") || eq(api, "nix-build"))
    return nix_cmd("build");
  if (eq(api, "NixDevelop") || eq(api, "nix develop")) return nix_cmd("develop");
  if (eq(api, "NixFlake") || eq(api, "nix flake")) return nix_cmd("flake");
  if (eq(api, "NixEval") || eq(api, "nix eval")) return nix_cmd("eval");
  if (eq(api, "NixSearch") || eq(api, "nix search")) return nix_cmd("search");
  if (eq(api, "NixStore") || eq(api, "nix store") || eq(api, "nix-store"))
    return nix_cmd("store");
  if (eq(api, "NixShell") || eq(api, "nix-shell") || eq(api, "nix env shell"))
    return nix_cmd("shell");
  if (eq(api, "NixProfile") || eq(api, "nix profile")) return nix_cmd("profile");
  if (eq(api, "NixHash") || eq(api, "nix hash") || eq(api, "nix-hash"))
    return nix_cmd("hash");
  if (eq(api, "NixConfig") || eq(api, "nix config") || eq(api, "nix config show"))
    return nix_cmd("config show");
  if (eq(api, "NixDoctor")) {
    std::string bin = which_bin("nix-info");
    if (!bin.empty()) return capture_cmd("nix-info -m");
    return nix_cmd("--version");
  }
  if (eq(api, "NixRepl") || eq(api, "NixDaemon") || eq(api, "NixUpgradeNix") ||
      eq(api, "nix") || eq(api, "nix-env") || eq(api, "nix-channel") ||
      eq(api, "nix-instantiate") || eq(api, "nix-collect-garbage") ||
      eq(api, "nix-prefetch-url") || eq(api, "NixEnv") || eq(api, "NixDerivation") ||
      eq(api, "NixCopy") || eq(api, "NixLog") || eq(api, "NixPathInfo") ||
      eq(api, "NixRegistry") || eq(api, "NixWhyDepends") || eq(api, "NixBundle") ||
      eq(api, "NixKey") || eq(api, "NixNar") || eq(api, "NixPrintDevEnv") ||
      eq(api, "NixFmt") || eq(api, "NixFormatter") || eq(api, "NixEdit") ||
      eq(api, "NixHelp") || eq(api, "NixHelpStores")) {
    if (eq(api, "nix")) return nix_cmd(a);
    std::string bin = which_bin("nix");
    if (bin.empty()) return err_msg("nix is not on PATH");
    std::string cmd = api;
    if (cmd.rfind("Nix", 0) == 0) {
      cmd = "nix ";
      // naive: NixWhyDepends -> why-depends
      std::string rest = api + 3;
      std::string kebab;
      for (size_t i = 0; i < rest.size(); ++i) {
        char c = rest[i];
        if (c >= 'A' && c <= 'Z') {
          if (!kebab.empty()) kebab.push_back('-');
          kebab.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
          kebab.push_back(c);
        }
      }
      cmd += kebab;
    }
    if (a[0]) {
      cmd += " ";
      cmd += a;
    }
    return capture_cmd(cmd.c_str());
  }

  if (eq(api, "sd_booted")) {
    struct ::stat st {};
    if (::stat("/run/systemd/system", &st) == 0) return "1";
    return "0";
  }
  if (eq(api, "systemd-detect-virt")) {
    if (is_wsl()) return "wsl";
    return capture_cmd("systemd-detect-virt 2>/dev/null");
  }
  {
    std::string kv;
    if (try_kernel(api, a, &kv)) return kv;
    if (try_kvm(api, a, &kv)) return kv;
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

}  // namespace wasmnix

#endif  // WASMNIX_INCLUDE_NIX_POSIX_HOST_HPP_
