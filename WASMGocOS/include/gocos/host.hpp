#ifndef WASMGOCOS_INCLUDE_GOCOS_HOST_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_HOST_HPP_

// Edge kernel. The guest binding is gocvm.Call("gocos", …). What the
// guest is delivered is the Plan 9 namespace (fd, env, cons, jobs, sh),
// and that namespace runs only across WASMNetStack.
//
//   GocKrnl  — kernel
//   GocSys   — subsystem
//   GocNix   — WSL occupancy
//   Plan 9   — session namespace, on the net stack
//
// Every call is gocvm hypervision: k32 (~/WASMWin32) or nix (~/WASMNix).
// Memory is the gocvm model (EPT vmem / VirtualAlloc / mmap).

#ifndef _WASI_EMULATED_GETPID
#define _WASI_EMULATED_GETPID 1
#endif

#include "gocos/dispatch.h"
#include "gocos/catalog.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(__wasm__) || defined(__wasi__)
#  if defined(__has_include)
#    if __has_include("win32/wasi_host.hpp")
#      ifndef WASMWIN32_WASI_HOST_NO_POSIX_HEADERS
#        define WASMWIN32_WASI_HOST_NO_POSIX_HEADERS 1
#      endif
#      include "win32/wasi_host.hpp"
#      define WASMGOCOS_HAS_WIN32 1
#    endif
#    if __has_include("nix/wsl.hpp")
#      include "nix/wsl.hpp"
#      define WASMGOCOS_HAS_NIX 1
#      define WASMGOCOS_NIX_WSL 1
#    elif __has_include("nix/posix_host.hpp")
#      include "nix/posix_host.hpp"
#      define WASMGOCOS_HAS_NIX 1
#    endif
#  endif
#elif defined(_WIN32)
#  if defined(__has_include) && __has_include("win32/dispatch.h")
#    include "win32/dispatch.h"
#    define WASMGOCOS_HAS_WIN32 1
#    define WASMGOCOS_K32_NATIVE 1
#  endif
#endif

namespace wasmgocos {

#include "gocos/hv.hpp"
#include "gocos/mem.hpp"
#include "gocos/gockrnl.hpp"
#include "gocos/gocsys.hpp"
#include "gocos/gocdesk.hpp"
#include "gocos/gocshell.hpp"

#if defined(WASMGOCOS_HAS_JITRT)
std::string jit_pid();
std::string jit_share();
std::string jit_run(const char* path);
std::string jit_unil();
std::string jit_file(const char* args);
std::string jit_mss();
std::string jit_heap(const char* args);
std::string jit_procfs(const char* args);
std::string jit_proctable(const char* args);
std::string jit_service(const char* args);
std::string jit_vfs(const char* args);
std::string jit_bbx(const char* args);
std::string jit_fd(const char* args);
std::string jit_env(const char* args);
std::string jit_sh(const char* line);
std::string jit_jobs(const char* args);
std::string jit_status(const char* args);
std::string jit_ctr(const char* args);
std::string jit_svc(const char* args);
std::string jit_react(const char* args);
std::string jit_dll(const char* args);
#if defined(WASMGOCOS_HAS_NET)
std::string jit_bio(const char* args);
std::string jit_net(const char* args);
#endif
#endif

bool plan9_local();
std::string plan9_hop(const char* verb, const char* args);

inline std::string gockrnl_boot() {
  GocKrnl& k = gockrnl();
  if (k.booted) return gockrnl_status();
  copy_field(k.hvname, sizeof(k.hvname), hv::name());
  copy_field(k.title, sizeof(k.title), "GocOS");
  copy_field(k.cwd, sizeof(k.cwd), "C:\\");

  // The session is the delivery. It does not come up without the stack.
  std::string ns = plan9_hop("fd", "");
  if (ns.rfind("error:", 0) == 0) return ns;
  copy_field(k.desk, sizeof(k.desk), "plan9");

  if (!hv::has_k32()) {
    copy_field(k.pid, sizeof(k.pid), "unbound");
    copy_field(k.version, sizeof(k.version), "gockrnl");
    copy_field(k.session, sizeof(k.session), "unbound");
    copy_field(k.station, sizeof(k.station), "GocSta0");
    copy_field(k.ntdll, sizeof(k.ntdll), "unbound");
    copy_field(k.k32mod, sizeof(k.k32mod), "unbound");
    copy_field(k.user32, sizeof(k.user32), "unbound");
    copy_field(k.gdi32, sizeof(k.gdi32), "unbound");
    if (hv::has_nix()) {
      std::string u = hv::nix("uname", "");
      if (u.rfind("error:", 0) != 0) copy_field(k.nix, sizeof(k.nix), u);
    }
    k.booted = true;
    return gockrnl_status();
  }

  std::string pid = hv::k32("GetCurrentProcessId", "");
  if (pid.rfind("error:", 0) == 0) return pid;
  copy_field(k.pid, sizeof(k.pid), pid);

  std::string ver = hv::k32("RtlGetVersion", "");
  if (ver.rfind("error:", 0) != 0) copy_field(k.version, sizeof(k.version), ver);

  std::string sta = hv::k32("GetDesktopWindow", "");
  if (sta.rfind("error:", 0) == 0) return sta;
  copy_field(k.station, sizeof(k.station), "GocSta0");
  copy_field(k.session, sizeof(k.session), pid);

  std::string sys = gocsys_init();
  if (sys.rfind("error:", 0) == 0) return sys;

  if (hv::has_nix()) {
    std::string u = hv::nix("uname", "");
    if (u.rfind("error:", 0) != 0) copy_field(k.nix, sizeof(k.nix), u);
  }

  std::string cwd = hv::k32("GetCurrentDirectoryW", "");
  if (cwd.rfind("error:", 0) != 0 && !cwd.empty())
    copy_field(k.cwd, sizeof(k.cwd), cwd);

  k.booted = true;
  return gockrnl_status();
}

inline std::string gockrnl_halt() {
  GocKrnl& k = gockrnl();
  if (k.desk[0] && std::strcmp(k.desk, "unbound") != 0 &&
      std::strcmp(k.desk, "plan9") != 0)
    hv::k32("DestroyWindow", k.desk);
  GocDesk& d = gocdesk();
  d = GocDesk{};
  GocPage* pages = goc_pages();
  for (int i = 0; i < kGocPages; ++i) {
    if (pages[i].used) goc_free(pages[i].addr);
  }
  k = GocKrnl{};
  return "os=gocos\x1fhalted";
}

inline std::string gocnix_call(const char* api, const char* a) {
  if (eq(api, "Launch") || eq(api, "Exec") || eq(api, "WslLaunch") ||
      eq(api, "WslLaunchInteractive") || eq(api, "WslExec")) {
    if (!a || !a[0] || eq(a, "uname") || std::strncmp(a, "uname ", 6) == 0)
      return hv::nix("uname", "");
    return hv::nix("WslExec", a ? a : "");
  }
  if (eq(api, "List") || eq(api, "WslList")) return hv::nix("WslList", "");
  if (eq(api, "Registered") || eq(api, "WslIsDistributionRegistered")) {
    if (!a || !a[0]) a = "gocos";
    return hv::nix("WslIsDistributionRegistered", a);
  }
  if (eq(api, "Path") || eq(api, "WslPath") || eq(api, "WslPathUnix"))
    return hv::nix("WslPath", a ? a : "");
  if (eq(api, "Uname")) return hv::nix("uname", "");
  if (eq(api, "Alloc")) return goc_alloc_nix(a);
  if (eq(api, "Free")) return goc_free_nix(a);
  return hv::nix(api, a ? a : "");
}

inline std::string gocos_call(const char* api, const char* args) {
  if (!api || !api[0]) return err_msg("gocos needs an API name");
  const char* a = args ? args : "";

  if (eq(api, "Boot") || eq(api, "GocOSBoot") || eq(api, "Occupancy") ||
      eq(api, "Init"))
    return gockrnl_boot();
  if (eq(api, "Halt") || eq(api, "GocOSHalt")) return gockrnl_halt();
  if (eq(api, "Status") || eq(api, "Session") || eq(api, "Station")) {
    std::string b = gockrnl_boot();
    if (b.rfind("error:", 0) == 0) return b;
    if (eq(api, "Session")) return gockrnl().session;
    if (eq(api, "Station")) return gockrnl().station;
    return b;
  }
  if (eq(api, "Version") || eq(api, "RtlGetVersion")) {
    std::string b = gockrnl_boot();
    if (b.rfind("error:", 0) == 0) return b;
    return gockrnl_version();
  }
#if defined(WASMGOCOS_HAS_JITRT)
  if (eq(api, "JitPid")) return jit_pid();
  if (eq(api, "JitShare")) return jit_share();
  if (eq(api, "JitRun")) return jit_run(a);
  if (eq(api, "JitUnil")) return jit_unil();
  if (eq(api, "JitFile")) return jit_file(a);
  if (eq(api, "JitMss")) return jit_mss();
  if (eq(api, "Heap")) return jit_heap(a);
  if (eq(api, "ProcFs")) return jit_procfs(a);
  if (eq(api, "ProcTable")) return jit_proctable(a);
  if (eq(api, "Service")) return jit_service(a);
  if (eq(api, "Vfs")) return jit_vfs(a);
  if (eq(api, "Bbx")) return jit_bbx(a);
  if (eq(api, "Fd")) return jit_fd(a);
  if (eq(api, "Env")) return jit_env(a);
  if (eq(api, "Sh")) return jit_sh(a);
  if (eq(api, "Jobs")) return jit_jobs(a);
  if (eq(api, "Rc")) return jit_status(a);
  if (eq(api, "Ctr")) return jit_ctr(a);
  if (eq(api, "Svc")) return jit_svc(a);
  if (eq(api, "React")) return jit_react(a);
  if (eq(api, "JitDll")) return jit_dll(a);
#if defined(WASMGOCOS_HAS_NET)
  if (eq(api, "Bio")) return jit_bio(a);
  if (eq(api, "Net")) return jit_net(a);
#endif
#endif

  if (eq(api, "CreateProcess") || eq(api, "NtCreateProcess") ||
      eq(api, "NtCreateProcessEx") || eq(api, "NtCreateUserProcess"))
    return gockrnl_create_process(a, a, false);
  if (eq(api, "OpenProcess")) return gockrnl_open_process(a);
  if (eq(api, "TerminateProcess") || eq(api, "NtTerminateProcess"))
    return gockrnl_terminate(a);
  if (eq(api, "Close") || eq(api, "NtClose") || eq(api, "CloseHandle"))
    return gockrnl_close(a);
  if (eq(api, "CreateThread") || eq(api, "NtCreateThread") ||
      eq(api, "NtCreateThreadEx"))
    return gockrnl_create_thread(a);
  if (eq(api, "TerminateThread") || eq(api, "NtTerminateThread") ||
      eq(api, "Terminate"))
    return hv::k32("TerminateThread", a);
  if (eq(api, "CreateFile") || eq(api, "OpenFile") || eq(api, "ReadFile") ||
      eq(api, "WriteFile") || eq(api, "NtCreateFile") || eq(api, "NtOpenFile") ||
      eq(api, "NtReadFile") || eq(api, "NtWriteFile"))
    return gockrnl_file(api, a);
  if (eq(api, "Alloc") || eq(api, "VirtualAlloc") ||
      eq(api, "NtAllocateVirtualMemory") || eq(api, "PoolAlloc")) {
    if (!hv::has_k32() && hv::has_nix()) return goc_alloc_nix(a);
    return goc_alloc(a);
  }
  if (eq(api, "Free") || eq(api, "VirtualFree") ||
      eq(api, "NtFreeVirtualMemory") || eq(api, "PoolFree"))
    return goc_free(a);
  if (eq(api, "Protect") || eq(api, "NtProtectVirtualMemory") ||
      eq(api, "VirtualProtect"))
    return goc_protect(a);
  if (eq(api, "Query") || eq(api, "NtQueryVirtualMemory") ||
      eq(api, "VirtualQuery"))
    return goc_query(a);
  if (eq(api, "Map") || eq(api, "NtMapViewOfSection")) return goc_map(a);
  if (eq(api, "Unmap") || eq(api, "NtUnmapViewOfSection")) return goc_unmap(a);
  if (eq(api, "Wait") || eq(api, "NtWaitForSingleObject") ||
      eq(api, "WaitForSingleObject"))
    return gockrnl_wait(a);
  if (eq(api, "Delay") || eq(api, "NtDelayExecution") || eq(api, "Sleep"))
    return gockrnl_delay(a);
  if (eq(api, "Yield") || eq(api, "NtYieldExecution")) return gockrnl_yield();
  if (eq(api, "CreateEvent") || eq(api, "SetEvent") || eq(api, "ResetEvent") ||
      eq(api, "Duplicate") || eq(api, "QueryObject") ||
      eq(api, "NtCreateEvent") || eq(api, "NtSetEvent") ||
      eq(api, "NtResetEvent") || eq(api, "NtDuplicateObject") ||
      eq(api, "NtQueryObject"))
    return gockrnl_event(api, a);
  if (eq(api, "BugCheck") || eq(api, "CreateDevice") || eq(api, "SystemThread") ||
      eq(api, "KeBugCheckEx") || eq(api, "IoCreateDevice") ||
      eq(api, "PsCreateSystemThread") || eq(api, "ExAllocatePool") ||
      eq(api, "ExFreePool"))
    return gockrnl_pool(api, a);

  if (eq(api, "LoadModule") || eq(api, "LoadLibraryW") || eq(api, "LoadLibraryA") ||
      eq(api, "LdrLoadDll"))
    return gocsys_load(a);
  if (eq(api, "GetProc") || eq(api, "GetProcAddress") ||
      eq(api, "LdrGetProcedureAddress"))
    return gocsys_getproc(a);
  if (eq(api, "FreeModule") || eq(api, "FreeLibrary") || eq(api, "LdrUnloadDll"))
    return gocsys_free_mod(a);
  if (eq(api, "GetModule") || eq(api, "GetModuleHandleW") ||
      eq(api, "LdrGetDllHandle"))
    return gocsys_get_mod(a);
  if (eq(api, "BuildId")) return gocsys_build_id();
  if (eq(api, "HostVersion")) return gocsys_host_version();
  if (eq(api, "CreateWindow") || eq(api, "CreateWindowExW") ||
      eq(api, "NtUserCreateWindowEx") || eq(api, "DestroyWindow") ||
      eq(api, "NtUserDestroyWindow") || eq(api, "GetMessage") ||
      eq(api, "NtUserGetMessage") || eq(api, "PeekMessage") ||
      eq(api, "NtUserPeekMessage") || eq(api, "Dispatch") ||
      eq(api, "DispatchMessageW") || eq(api, "Translate") ||
      eq(api, "TranslateMessage") || eq(api, "DefProc") ||
      eq(api, "DefWindowProcW") || eq(api, "RegisterClass") ||
      eq(api, "RegisterClassExW") || eq(api, "ShowWindow") ||
      eq(api, "UpdateWindow") || eq(api, "PostQuit") || eq(api, "PostQuitMessage") ||
      eq(api, "Message") || eq(api, "MessageBoxW"))
    return gocsys_wnd(api, a);
  if (eq(api, "GetDC") || eq(api, "ReleaseDC") || eq(api, "BitBlt") ||
      eq(api, "CreateBrush") || eq(api, "NtGdiCreateSolidBrush") ||
      eq(api, "DeleteObject") || eq(api, "NtGdiDeleteObjectApp") ||
      eq(api, "CreateCompatibleDC") || eq(api, "SelectObject"))
    return gocsys_draw(api, a);
  if (eq(api, "OpenKey") || eq(api, "RegOpenKeyExW") || eq(api, "QueryValue") ||
      eq(api, "RegQueryValueExW") || eq(api, "CloseKey") || eq(api, "RegCloseKey"))
    return gocsys_reg(api, a);
  if (eq(api, "FolderPath") || eq(api, "SHGetFolderPathW") ||
      eq(api, "ShellExec") || eq(api, "ShellExecuteW"))
    return gocsys_shell(api, a);
  if (eq(api, "NtToUnix")) return gocsys_nt_to_unix(a);
  if (eq(api, "UnixToNt")) return gocsys_unix_to_nt(a);

  if (gocdesk_is_api(api)) return gocdesk_call(api, a);

  if (eq(api, "Launch") || eq(api, "Exec") || eq(api, "List") ||
      eq(api, "Registered") || eq(api, "Path") || eq(api, "Uname") ||
      eq(api, "WslLaunch") || eq(api, "WslLaunchInteractive") ||
      eq(api, "WslExec") || eq(api, "WslList") ||
      eq(api, "WslIsDistributionRegistered") || eq(api, "WslPath") ||
      eq(api, "WslPathUnix"))
    return gocnix_call(api, a);

  if (eq(api, "Cmd") || eq(api, "CmdExec") || eq(api, "command.com") ||
      eq(api, "cmd.exe"))
    return gocshell_cmd(a);
  if (eq(api, "Pwsh") || eq(api, "PowerShell") || eq(api, "powershell") ||
      eq(api, "powershell.exe") || eq(api, "pwsh.exe"))
    return gocshell_pwsh(a);
  if (eq(api, "Conhost") || eq(api, "conhost")) {
    std::string b = gockrnl_boot();
    if (b.rfind("error:", 0) == 0) return b;
    return gocshell_conhost();
  }
  if (eq(api, "Prompt") || eq(api, "GetPrompt")) return gocshell_prompt();
  // Interactive console: raw input bytes in, display bytes out. The terminal
  // (echo, line editing, prompt, dispatch) is here in the module. The first
  // call boots and primes the banner + prompt.
  if (eq(api, "Console")) {
    std::string b = gockrnl_boot();
    if (b.rfind("error:", 0) == 0) return b;
    return gocshell_console(a);
  }
  if (eq(api, "Shell")) return gockrnl().shell == 1 ? "pwsh" : "cmd";
  if (eq(api, "CmdExample")) {
    gockrnl().shell = 0;
    gockrnl_create_process("cmd.exe", a[0] ? a : "cmd.exe", false);
    return gocshell_conhost();
  }

  if (hv::has_k32()) {
    std::string r = hv::k32(api, a);
    if (r.rfind("error: unknown api", 0) != 0) return r;
    if (hv::has_nix()) return hv::nix(api, a);
    return r;
  }
  if (hv::has_nix()) return hv::nix(api, a);
  return err_msg("gocvm: k32 hop required");
}

inline int fill_out(char* out, unsigned cap, const std::string& s) {
  if (!out || cap == 0) return s.rfind("error:", 0) == 0 ? -1 : 0;
  unsigned n = static_cast<unsigned>(s.size());
  if (n + 1 > cap) n = cap - 1;
  std::memcpy(out, s.data(), n);
  out[n] = 0;
  return s.rfind("error:", 0) == 0 ? -1 : 0;
}

}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_HOST_HPP_
