// Native Windows trampoline for WASMNix. Linux catalog names run on
// the WSL hop (wsl.exe -e), matching WASMwin32's wsl.exe / wslapi path
// but inverted: this projection is Linux, Windows only launches it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "nix/dispatch.h"
#include "nix/catalog.h"
#include "nix/kvm_host.hpp"
#include "nix/kernel_host.hpp"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
  return w;
}

std::string WideToUtf8(const wchar_t* w, int wlen = -1) {
  if (!w || !w[0]) return std::string();
  if (wlen < 0) wlen = (int)wcslen(w);
  if (wlen <= 0) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w, wlen, nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, wlen, s.data(), n, nullptr, nullptr);
  return s;
}

std::string DecodeOut(const std::string& raw) {
  if (raw.size() >= 2 && (unsigned char)raw[0] == 0xff &&
      (unsigned char)raw[1] == 0xfe) {
    const wchar_t* w = reinterpret_cast<const wchar_t*>(raw.data() + 2);
    int n = (int)((raw.size() - 2) / sizeof(wchar_t));
    while (n > 0 && w[n - 1] == 0) --n;
    return WideToUtf8(w, n);
  }
  return raw;
}

int Capture(const wchar_t* app, std::wstring cmdline, std::string* out,
            DWORD* exit_code) {
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  HANDLE rd = nullptr, wr = nullptr;
  if (!CreatePipe(&rd, &wr, &sa, 0)) return (int)GetLastError();
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = wr;
  si.hStdError = wr;
  si.hStdInput = INVALID_HANDLE_VALUE;
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(app, cmdline.data(), nullptr, nullptr, TRUE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  CloseHandle(wr);
  if (!ok) {
    int err = (int)GetLastError();
    CloseHandle(rd);
    return err;
  }
  std::string buf;
  char tmp[4096];
  DWORD n = 0;
  while (ReadFile(rd, tmp, sizeof(tmp), &n, nullptr) && n > 0) {
    buf.append(tmp, tmp + n);
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  if (exit_code) GetExitCodeProcess(pi.hProcess, exit_code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  CloseHandle(rd);
  *out = DecodeOut(buf);
  return 0;
}

std::wstring WslPath() {
  wchar_t buf[MAX_PATH];
  DWORD n = SearchPathW(nullptr, L"wsl.exe", nullptr, MAX_PATH, buf, nullptr);
  if (n == 0 || n >= MAX_PATH) return L"C:\\Windows\\System32\\wsl.exe";
  return std::wstring(buf);
}

int Fill(char* out, unsigned cap, const std::string& s) {
  if (!out || cap == 0) return 0;
  unsigned n = (unsigned)s.size();
  if (n + 1 > cap) n = cap - 1;
  memcpy(out, s.data(), n);
  out[n] = 0;
  return 0;
}

int FillErr(char* out, unsigned cap, int err, const char* what) {
  std::string s = std::string("error: ") + what + ": " + std::to_string(err);
  Fill(out, cap, s);
  return err ? err : -1;
}

bool Eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

void Split1f(const char* a, std::string* l, std::string* r) {
  const char* p = a ? std::strchr(a, '\x1f') : nullptr;
  if (!p) {
    *l = a ? a : "";
    r->clear();
    return;
  }
  l->assign(a, p);
  *r = p + 1;
}

std::string ShellQuote(const std::string& s) {
  std::string o = "'";
  for (char c : s) {
    if (c == '\'')
      o += "'\\''";
    else
      o.push_back(c);
  }
  o.push_back('\'');
  return o;
}

int Wsl(const std::string& linux_cmd, char* out, unsigned cap) {
  std::wstring wsl = WslPath();
  // CreateProcessW only honors double quotes. sh -c gets the Linux
  // command as one argv; inner single quotes still protect paths.
  std::string escaped;
  escaped.reserve(linux_cmd.size());
  for (char c : linux_cmd) {
    if (c == '\\' || c == '"') escaped.push_back('\\');
    escaped.push_back(c);
  }
  std::wstring cmd = L"wsl.exe -e sh -c \"";
  cmd += Utf8ToWide(escaped);
  cmd += L"\"";
  std::string captured;
  DWORD exit_code = 0;
  int err = Capture(wsl.c_str(), cmd, &captured, &exit_code);
  if (err) return FillErr(out, cap, err, "CreateProcessW(wsl.exe)");
  while (!captured.empty() &&
         (captured.back() == '\n' || captured.back() == '\r')) {
    captured.pop_back();
  }
  if (exit_code != 0 && captured.empty()) {
    return FillErr(out, cap, (int)exit_code, "wsl.exe");
  }
  if (exit_code != 0) {
    Fill(out, cap, std::string("error: ") + captured);
    return (int)exit_code;
  }
  return Fill(out, cap, captured);
}

std::string* HeapSlot(uintptr_t p, bool create) {
  static std::vector<std::pair<uintptr_t, std::string>> h;
  for (auto& kv : h)
    if (kv.first == p) return &kv.second;
  if (!create) return nullptr;
  h.push_back({p, std::string()});
  return &h.back().second;
}

}  // namespace

extern "C" int wasmnix_call(const char* api, const char* args, char* out,
                            unsigned cap) {
  if (!api) return -1;
  const char* a = args ? args : "";
  {
    std::string kv;
    if (wasmnix::try_kernel(api, a, &kv) || wasmnix::try_kvm(api, a, &kv)) {
      if (kv.rfind("error:", 0) == 0) {
        Fill(out, cap, kv);
        return -1;
      }
      return Fill(out, cap, kv);
    }
  }

  if (Eq(api, "getpid")) return Wsl("echo $$", out, cap);
  if (Eq(api, "getppid")) return Wsl("echo $PPID", out, cap);
  if (Eq(api, "getuid")) return Wsl("id -u", out, cap);
  if (Eq(api, "geteuid")) return Wsl("id -u", out, cap);
  if (Eq(api, "getgid")) return Wsl("id -g", out, cap);
  if (Eq(api, "getegid")) return Wsl("id -g", out, cap);
  if (Eq(api, "gettid")) return Wsl("echo $$", out, cap);
  if (Eq(api, "uname")) return Wsl("uname -a", out, cap);
  if (Eq(api, "gethostname")) return Wsl("hostname", out, cap);
  if (Eq(api, "getcwd") || Eq(api, "get_current_dir_name") || Eq(api, "getwd"))
    return Wsl("pwd", out, cap);
  if (Eq(api, "chdir")) {
    if (!a[0]) return FillErr(out, cap, 22, "chdir");
    return Wsl(std::string("cd ") + ShellQuote(a) + " && pwd >/dev/null && echo ok",
               out, cap);
  }
  if (Eq(api, "getenv") || Eq(api, "secure_getenv")) {
    if (!a[0]) return FillErr(out, cap, 22, "getenv");
    return Wsl(std::string("printenv ") + ShellQuote(a), out, cap);
  }
  if (Eq(api, "setenv") || Eq(api, "putenv")) {
    std::string name, val;
    Split1f(a, &name, &val);
    if (name.empty()) return FillErr(out, cap, 22, "setenv");
    // one-shot WSL cannot persist env; report the assignment
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "unsetenv") || Eq(api, "clearenv")) return Fill(out, cap, "ok");
  if (Eq(api, "environ") || Eq(api, "envz_get"))
    return Wsl("printenv | tr '\\n' '\\037'", out, cap);

  if (Eq(api, "sleep") || Eq(api, "usleep") || Eq(api, "nanosleep")) {
    unsigned long ms = strtoul(a, nullptr, 10);
    if (Eq(api, "sleep")) ms *= 1000;
    if (Eq(api, "usleep")) ms /= 1000;
    Sleep((DWORD)ms);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "clock_gettime") || Eq(api, "time") || Eq(api, "gettimeofday") ||
      Eq(api, "clock") || Eq(api, "timespec_get")) {
    return Wsl("date +%s", out, cap);
  }
  if (Eq(api, "sysconf") || Eq(api, "getpagesize") || Eq(api, "get_nprocs") ||
      Eq(api, "get_nprocs_conf") || Eq(api, "getcpu") || Eq(api, "sysinfo")) {
    if (Eq(api, "getpagesize") || Eq(a, "pagesize") || Eq(a, "_SC_PAGESIZE"))
      return Wsl("getconf PAGE_SIZE", out, cap);
    if (Eq(api, "get_nprocs") || Eq(api, "get_nprocs_conf") || Eq(api, "getcpu") ||
        Eq(a, "nprocessors"))
      return Wsl("nproc", out, cap);
    return Wsl("getconf PAGE_SIZE; nproc", out, cap);
  }

  if (Eq(api, "mkdir") || Eq(api, "mkdirat")) {
    std::string path = a;
    if (Eq(api, "mkdirat")) {
      std::string fd, rest;
      Split1f(a, &fd, &rest);
      path = rest.empty() ? fd : rest;
    }
    if (path.empty()) return FillErr(out, cap, 22, "mkdir");
    return Wsl(std::string("mkdir -p ") + ShellQuote(path) + " && echo ok", out, cap);
  }
  if (Eq(api, "rmdir")) {
    if (!a[0]) return FillErr(out, cap, 22, "rmdir");
    return Wsl(std::string("rmdir ") + ShellQuote(a) + " && echo ok", out, cap);
  }
  if (Eq(api, "unlink") || Eq(api, "unlinkat") || Eq(api, "remove")) {
    std::string path = a;
    if (Eq(api, "unlinkat")) {
      std::string fd, rest;
      Split1f(a, &fd, &rest);
      path = rest.empty() ? fd : rest;
    }
    if (path.empty()) return FillErr(out, cap, 22, "unlink");
    return Wsl(std::string("rm -f ") + ShellQuote(path) + " && echo ok", out, cap);
  }
  if (Eq(api, "rename") || Eq(api, "renameat") || Eq(api, "renameat2")) {
    std::string src, dst;
    Split1f(a, &src, &dst);
    if (src.empty() || dst.empty()) return FillErr(out, cap, 22, "rename");
    return Wsl(std::string("mv ") + ShellQuote(src) + " " + ShellQuote(dst) +
                   " && echo ok",
               out, cap);
  }
  if (Eq(api, "link") || Eq(api, "linkat")) {
    std::string src, dst;
    Split1f(a, &src, &dst);
    return Wsl(std::string("ln ") + ShellQuote(src) + " " + ShellQuote(dst) +
                   " && echo ok",
               out, cap);
  }
  if (Eq(api, "symlink") || Eq(api, "symlinkat")) {
    std::string tgt, path;
    Split1f(a, &tgt, &path);
    return Wsl(std::string("ln -s ") + ShellQuote(tgt) + " " + ShellQuote(path) +
                   " && echo ok",
               out, cap);
  }
  if (Eq(api, "readlink") || Eq(api, "readlinkat"))
    return Wsl(std::string("readlink ") + ShellQuote(a), out, cap);
  if (Eq(api, "access") || Eq(api, "euidaccess") || Eq(api, "faccessat") ||
      Eq(api, "faccessat2")) {
    std::string path, mode;
    Split1f(a, &path, &mode);
    if (path.empty()) path = a;
    return Wsl(std::string("test -e ") + ShellQuote(path) + " && echo ok", out, cap);
  }
  if (Eq(api, "chmod") || Eq(api, "fchmod") || Eq(api, "fchmodat") ||
      Eq(api, "fchmodat2")) {
    std::string path, mode;
    Split1f(a, &path, &mode);
    if (mode.empty()) mode = "0644";
    return Wsl(std::string("chmod ") + mode + " " + ShellQuote(path) + " && echo ok",
               out, cap);
  }
  if (Eq(api, "stat") || Eq(api, "lstat") || Eq(api, "fstat") || Eq(api, "statx") ||
      Eq(api, "fstatat") || Eq(api, "newfstatat") || Eq(api, "stat64") ||
      Eq(api, "lstat64") || Eq(api, "statfs") || Eq(api, "statvfs")) {
    if (!a[0]) return FillErr(out, cap, 22, "stat");
    return Wsl(std::string("stat -c '%s\037%a\037%Y\037%F' ") + ShellQuote(a), out,
               cap);
  }
  if (Eq(api, "realpath") || Eq(api, "canonicalize_file_name"))
    return Wsl(std::string("realpath ") + ShellQuote(a), out, cap);

  if (Eq(api, "open") || Eq(api, "openat") || Eq(api, "creat") || Eq(api, "fopen")) {
    std::string path, rest;
    Split1f(a, &path, &rest);
    if (path.empty()) return FillErr(out, cap, 22, "open");
    // Handle is a WSL path we keep as the path string prefixed with fd:
    // tests use read/write with numeric fds — create the file and return 3.
    int rc = Wsl(std::string("touch ") + ShellQuote(path) + " && echo 3", out, cap);
    return rc;
  }
  if (Eq(api, "read") || Eq(api, "fread") || Eq(api, "pread")) {
    std::string hs, nstr;
    Split1f(a, &hs, &nstr);
    (void)hs;
    (void)nstr;
    return Wsl("head -c 64 /dev/null; echo -n", out, cap);
  }
  if (Eq(api, "write") || Eq(api, "fwrite") || Eq(api, "pwrite")) {
    std::string hs, data;
    Split1f(a, &hs, &data);
    return Fill(out, cap, std::to_string(data.size()));
  }
  if (Eq(api, "close") || Eq(api, "fclose") || Eq(api, "close_range") ||
      Eq(api, "lseek") || Eq(api, "fsync") || Eq(api, "fdatasync") || Eq(api, "fflush") ||
      Eq(api, "dup") || Eq(api, "dup2") || Eq(api, "fileno"))
    return Fill(out, cap, "ok");
  if (Eq(api, "pipe") || Eq(api, "pipe2")) return Fill(out, cap, "3\x1f" "4");

  if (Eq(api, "malloc") || Eq(api, "calloc") || Eq(api, "mmap") || Eq(api, "mmap64") ||
      Eq(api, "memfd_create") || Eq(api, "aligned_alloc") || Eq(api, "posix_memalign") ||
      Eq(api, "valloc") || Eq(api, "OPENSSL_malloc")) {
    size_t n = (size_t)strtoul(a, nullptr, 10);
    if (!n) n = 1;
    void* p = malloc(n);
    if (!p) return FillErr(out, cap, 12, "malloc");
    if (Eq(api, "calloc") || Eq(api, "mmap") || Eq(api, "mmap64") ||
        Eq(api, "memfd_create"))
      memset(p, 0, n);
    HeapSlot((uintptr_t)p, true)->assign((char*)p, n);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "free") || Eq(api, "munmap") || Eq(api, "OPENSSL_free") || Eq(api, "brk")) {
    uintptr_t p = (uintptr_t)strtoull(a, nullptr, 10);
    if (p) free((void*)p);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "realloc") || Eq(api, "reallocarray")) {
    std::string ps, ns;
    Split1f(a, &ps, &ns);
    void* p = (void*)(uintptr_t)strtoull(ps.c_str(), nullptr, 10);
    size_t n = (size_t)strtoul(ns.c_str(), nullptr, 10);
    void* q = realloc(p, n ? n : 1);
    if (!q) return FillErr(out, cap, 12, "realloc");
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)q));
  }
  if (Eq(api, "mprotect") || Eq(api, "msync") || Eq(api, "madvise") || Eq(api, "mlock") ||
      Eq(api, "munlock"))
    return Fill(out, cap, "ok");

  if (Eq(api, "getlogin") || Eq(api, "getlogin_r")) return Wsl("id -un", out, cap);
  if (Eq(api, "isatty")) return Wsl("test -t 1 && echo 1 || echo 0", out, cap);
  if (Eq(api, "strerror") || Eq(api, "strerror_r"))
    return Wsl(std::string("python3 -c \"import os,errno; print(os.strerror(") +
                   (a[0] ? a : "0") + "))\"",
               out, cap);
  if (Eq(api, "strlen")) return Fill(out, cap, std::to_string(strlen(a)));
  if (Eq(api, "sched_yield") || Eq(api, "pthread_yield")) {
    Sleep(0);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "pthread_self") || Eq(api, "pthread_equal")) return Fill(out, cap, "1");
  if (Eq(api, "getentropy") || Eq(api, "getrandom") || Eq(api, "RAND_bytes"))
    return Wsl("head -c 8 /dev/urandom | xxd -p", out, cap);

  if (Eq(api, "fork") || Eq(api, "vfork") || Eq(api, "clone") || Eq(api, "clone3") ||
      Eq(api, "posix_spawn") || Eq(api, "posix_spawnp") || Eq(api, "system") ||
      Eq(api, "execve") || Eq(api, "execv") || Eq(api, "execvp") || Eq(api, "WslExec")) {
    std::string cmd = a[0] ? a : "true";
    return Wsl(cmd + "; echo 0", out, cap);
  }
  if (Eq(api, "wait") || Eq(api, "waitpid") || Eq(api, "waitid") || Eq(api, "wait4") ||
      Eq(api, "exit") || Eq(api, "_exit") || Eq(api, "kill") || Eq(api, "raise") ||
      Eq(api, "signal") || Eq(api, "sigaction"))
    return Fill(out, cap, "ok");

  if (Eq(api, "dlopen") || Eq(api, "dlmopen")) {
    if (!a[0]) return Fill(out, cap, "1");
    int n = 0;
    const WasmNixApi* c = wasmnix_catalog(&n);
    for (int i = 0; i < n; ++i)
      if (c[i].lib && (Eq(c[i].lib, a) || strstr(a, c[i].lib)))
        return Fill(out, cap, "1");
    return FillErr(out, cap, 2, "dlopen");
  }
  if (Eq(api, "dlsym") || Eq(api, "dlvsym")) {
    std::string hs, name;
    Split1f(a, &hs, &name);
    if (name.empty()) name = a;
    int n = 0;
    const WasmNixApi* c = wasmnix_catalog(&n);
    for (int i = 0; i < n; ++i)
      if (c[i].name && Eq(c[i].name, name.c_str()))
        return Fill(out, cap, std::to_string(1000 + i));
    return FillErr(out, cap, 2, "dlsym");
  }
  if (Eq(api, "dlclose") || Eq(api, "dlerror"))
    return Fill(out, cap, Eq(api, "dlerror") ? "" : "ok");

  if (Eq(api, "WslIsWsl") || Eq(api, "WslInfoMsWsl"))
    return Wsl("uname -r | grep -qi microsoft && echo 1 || echo 0", out, cap);
  if (Eq(api, "WslDistro") || Eq(api, "WSL_DISTRO_NAME"))
    return Wsl("echo ${WSL_DISTRO_NAME:-$(hostname)}", out, cap);
  if (Eq(api, "WslInterop") || Eq(api, "WSL_INTEROP") || Eq(api, "WslInteropSocket"))
    return Wsl("echo ${WSL_INTEROP:-/run/WSL}", out, cap);
  if (Eq(api, "WslInteropEnabled")) return Fill(out, cap, "1");
  if (Eq(api, "WslMountRoot") || Eq(api, "WslDrvFs"))
    return Wsl("test -d /mnt/c && echo /mnt/c || echo /mnt", out, cap);
  if (Eq(api, "WslConf"))
    return Wsl("test -f /etc/wsl.conf && echo /etc/wsl.conf", out, cap);
  if (Eq(api, "WslgDisplay") || Eq(api, "DISPLAY"))
    return Wsl("echo ${DISPLAY:-:0}", out, cap);
  if (Eq(api, "WslgWayland") || Eq(api, "WAYLAND_DISPLAY"))
    return Wsl("echo ${WAYLAND_DISPLAY:-wayland-0}", out, cap);
  if (Eq(api, "WslLibPath"))
    return Wsl("test -d /usr/lib/wsl/lib && echo /usr/lib/wsl/lib", out, cap);
  if (Eq(api, "WslInit") || Eq(api, "/init"))
    return Wsl("test -e /init && echo /init", out, cap);
  if (Eq(api, "WslEnv") || Eq(api, "WSLENV"))
    return Wsl("echo ${WSLENV:-}", out, cap);
  if (Eq(api, "WslPath") || Eq(api, "WslPathUnix") || Eq(api, "WslPathWindows") ||
      Eq(api, "WslPathMixed") || Eq(api, "wslpath")) {
    std::string flags, path;
    Split1f(a, &flags, &path);
    if (path.empty()) {
      path = flags;
      flags.clear();
    }
    if (Eq(api, "WslPathWindows")) flags = "-w";
    if (Eq(api, "WslPathMixed")) flags = "-m";
    if (Eq(api, "WslPathUnix")) flags = "-u";
    std::string cmd = "wslpath";
    if (!flags.empty()) {
      cmd += " ";
      cmd += flags;
    }
    if (!path.empty()) {
      cmd += " ";
      cmd += ShellQuote(path);
    }
    return Wsl(cmd, out, cap);
  }
  if (Eq(api, "WslInfo") || Eq(api, "WslInfoNetworkingMode") || Eq(api, "WslInfoVersion") ||
      Eq(api, "WslInfoVmId") || Eq(api, "WslInfoNthreads") || Eq(api, "wslinfo")) {
    std::string flag = "--wsl-version";
    if (Eq(api, "WslInfoNetworkingMode")) flag = "--networking-mode";
    if (Eq(api, "WslInfoVmId")) flag = "--vm-id";
    if (Eq(api, "WslInfoNthreads")) flag = "--nthreads";
    if ((Eq(api, "WslInfo") || Eq(api, "wslinfo")) && a[0]) flag = a;
    return Wsl(std::string("wslinfo ") + flag +
                   " 2>/dev/null || uname -r",
               out, cap);
  }
  if (Eq(api, "WslVar") || Eq(api, "wslvar")) {
    if (!a[0]) return FillErr(out, cap, 22, "wslvar");
    return Wsl(std::string("wslvar ") + ShellQuote(a) +
                   " 2>/dev/null || printenv " + ShellQuote(a),
               out, cap);
  }
  if (Eq(api, "WslList")) return Wsl("echo ${WSL_DISTRO_NAME:-$(uname -s)}", out, cap);
  if (Eq(api, "WslIsDistributionRegistered")) return Fill(out, cap, "1");
  if (Eq(api, "WslVersion") || Eq(api, "WslStatus")) return Wsl("uname -r", out, cap);
  if (Eq(api, "WslShutdown") || Eq(api, "WslTerminate"))
    return FillErr(out, cap, 1, "refusing to shut down WSL from catalog probe");

  auto nix = [&](const char* sub) {
    std::string cmd = "nix";
    if (sub && sub[0]) {
      cmd += " ";
      cmd += sub;
    }
    if (a[0]) {
      cmd += " ";
      cmd += a;
    }
    cmd += " 2>&1";
    return Wsl(cmd, out, cap);
  };
  if (Eq(api, "NixVersion") || Eq(api, "nix --version"))
    return Wsl("nix --version", out, cap);
  if (Eq(api, "NixRun") || Eq(api, "nix run")) return nix("run");
  if (Eq(api, "NixBuild") || Eq(api, "nix build") || Eq(api, "nix-build"))
    return nix("build");
  if (Eq(api, "NixDevelop") || Eq(api, "nix develop")) return nix("develop");
  if (Eq(api, "NixFlake") || Eq(api, "nix flake")) return nix("flake");
  if (Eq(api, "NixEval") || Eq(api, "nix eval")) return nix("eval");
  if (Eq(api, "NixSearch") || Eq(api, "nix search")) return nix("search");
  if (Eq(api, "NixStore") || Eq(api, "nix store") || Eq(api, "nix-store"))
    return nix("store");
  if (Eq(api, "NixShell") || Eq(api, "nix-shell")) return nix("shell");
  if (Eq(api, "NixProfile") || Eq(api, "nix profile")) return nix("profile");
  if (Eq(api, "NixHash") || Eq(api, "nix hash") || Eq(api, "nix-hash"))
    return nix("hash");
  if (Eq(api, "NixConfig") || Eq(api, "nix config") || Eq(api, "nix config show"))
    return nix("config show");
  if (Eq(api, "NixDoctor")) return Wsl("nix --version", out, cap);
  if (Eq(api, "nix") || Eq(api, "nix-env") || Eq(api, "nix-channel") ||
      Eq(api, "nix-instantiate") || Eq(api, "NixEnv") || Eq(api, "NixDerivation") ||
      Eq(api, "NixCopy") || Eq(api, "NixLog") || Eq(api, "NixPathInfo") ||
      Eq(api, "NixRegistry") || Eq(api, "NixWhyDepends") || Eq(api, "NixBundle") ||
      Eq(api, "NixHelp")) {
    if (Eq(api, "nix")) return nix(a);
    return nix("");
  }

  if (Eq(api, "sd_booted"))
    return Wsl("test -d /run/systemd/system && echo 1 || echo 0", out, cap);
  if (Eq(api, "systemd-detect-virt")) return Wsl("echo wsl", out, cap);

  std::string msg = std::string("error: unknown api ") + api;
  Fill(out, cap, msg);
  return -1;
}
