#include "wow/surface.h"

#include <cstring>

namespace wow {
namespace {

struct KindRow {
  SurfaceKind kind;
  const char* name;
  Isolation isolation;
  const char* why;
};

constexpr KindRow kKinds[] = {
    {SurfaceKind::kHandle, "handle", Isolation::kSfi,
     "Win32 HANDLE catalog; still a surface"},
    {SurfaceKind::kKernel32, "kernel32", Isolation::kHost,
     "wasmwin32_call kernel32; native kernel32 / wasm libc leave SFI"},
    {SurfaceKind::kNtdll, "ntdll", Isolation::kSys,
     "Nt/Zw/Rtl/Ldr; syscall hop past kernel32"},
    {SurfaceKind::kUser32, "user32", Isolation::kCompositor,
     "HWND / desktop window; compositor hop like present"},
    {SurfaceKind::kGdi32, "gdi32", Isolation::kCompositor,
     "GDI objects; host GDI/DWM stays"},
    {SurfaceKind::kOle32, "ole32", Isolation::kIpc,
     "CoCreateGuid / COM apartment; COM ipc"},
    {SurfaceKind::kOleaut32, "oleaut32", Isolation::kIpc,
     "VARIANT / BSTR; OLE automation ipc"},
    {SurfaceKind::kAdvapi32, "advapi32", Isolation::kSys,
     "registry / token; LSA and SAM stay"},
    {SurfaceKind::kWs2, "ws2_32", Isolation::kNet,
     "WSAStartup / sockets; host UDP/TCP leave SFI"},
    {SurfaceKind::kBcrypt, "bcrypt", Isolation::kHost,
     "BCrypt CNG; host crypto pages"},
    {SurfaceKind::kNcrypt, "ncrypt", Isolation::kHost,
     "NCrypt key storage; host KSP"},
    {SurfaceKind::kCrypt32, "crypt32", Isolation::kHost,
     "CertOpenStore; host cert store"},
    {SurfaceKind::kShell32, "shell32", Isolation::kHost,
     "SHGetFolderPath; host shell namespace"},
    {SurfaceKind::kWinhttp, "winhttp", Isolation::kNet,
     "WinHttpOpen; host HTTP stack"},
    {SurfaceKind::kIphlpapi, "iphlpapi", Isolation::kNet,
     "GetAdaptersAddresses; host IP helper"},
    {SurfaceKind::kVersion, "version", Isolation::kHost,
     "GetFileVersionInfo; host PE version"},
    {SurfaceKind::kComctl32, "comctl32", Isolation::kCompositor,
     "common controls; HWND child of user32"},
    {SurfaceKind::kWininet, "wininet", Isolation::kNet,
     "InternetOpen; host WinINet"},
    {SurfaceKind::kDnsapi, "dnsapi", Isolation::kNet,
     "DnsQuery; host resolver"},
    {SurfaceKind::kSecur32, "secur32", Isolation::kSys,
     "AcquireCredentialsHandle; SSPI"},
    {SurfaceKind::kDbghelp, "dbghelp", Isolation::kHost,
     "StackWalk64; host debug help"},
    {SurfaceKind::kSetupapi, "setupapi", Isolation::kSys,
     "SetupDiGetClassDevs; device node"},
    {SurfaceKind::kNetapi32, "netapi32", Isolation::kNet,
     "NetUserGetInfo; host netapi"},
    {SurfaceKind::kPdh, "pdh", Isolation::kHost,
     "PdhOpenQuery; host performance data"},
    {SurfaceKind::kWevtapi, "wevtapi", Isolation::kSys,
     "EvtQuery; event log"},
    {SurfaceKind::kComdlg32, "comdlg32", Isolation::kCompositor,
     "GetOpenFileName; common dialog HWND"},
    {SurfaceKind::kWinHv, "winhv", Isolation::kHv,
     "WHvCreatePartition / WHvRunVirtualProcessor; hypervisor"},
    {SurfaceKind::kWinHvEmu, "winhv_emu", Isolation::kHv,
     "WHvEmulatorTryIoEmulation / TryMmioEmulation"},
    {SurfaceKind::kWslapi, "wslapi", Isolation::kHost,
     "WslIsDistributionRegistered; wslapi.dll"},
    {SurfaceKind::kWsl, "wsl", Isolation::kTty,
     "WslList / WslExec; wsl.exe host path"},
    {SurfaceKind::kNix, "nix", Isolation::kHost,
     "NixVersion / NixRun inside WSL"},
    {SurfaceKind::kProcess, "process", Isolation::kShell,
     "CreateProcessW; shell via this wasigocvm"},
    {SurfaceKind::kThread, "thread", Isolation::kHost,
     "CreateThread / TlsAlloc; host threads"},
    {SurfaceKind::kFile, "file", Isolation::kHost,
     "CreateFileW / ReadFile; host filesystem"},
    {SurfaceKind::kHeap, "heap", Isolation::kHost,
     "HeapAlloc / GetProcessHeap; host heap"},
    {SurfaceKind::kVmem, "vmem", Isolation::kHost,
     "VirtualAlloc shares host pages; wasm SFI does not hold them"},
    {SurfaceKind::kPipe, "pipe", Isolation::kIpc,
     "CreatePipe; anonymous ipc handles"},
    {SurfaceKind::kConsole, "console", Isolation::kTty,
     "GetStdHandle / WriteConsoleW; console tty"},
    {SurfaceKind::kRegistry, "registry", Isolation::kSys,
     "RegOpenKeyExW; configuration manager"},
    {SurfaceKind::kToken, "token", Isolation::kSys,
     "OpenProcessToken; access token"},
    {SurfaceKind::kPe, "pe", Isolation::kHost,
     "LoadLibraryW maps catalog DLLs / PE via ~/WASMPELoader"},
    {SurfaceKind::kHwnd, "hwnd", Isolation::kCompositor,
     "GetDesktopWindow; HWND compositor hop"},
    {SurfaceKind::kNtoskrnl, "ntoskrnl", Isolation::kHost,
     "host ntoskrnl.exe stays; occupancy does not replace it"},
    {SurfaceKind::kSysHeap, "sys_heap", Isolation::kSys,
     "kernel occupancy heap behind wowwin32.sys IRP"},
    {SurfaceKind::kOccupancyDriver, "occupancy_driver", Isolation::kSys,
     "wowwin32.sys DRIVER_OBJECT / DEVICE_OBJECT / IRP_MJ"},
    {SurfaceKind::kWin32k, "win32k", Isolation::kHost,
     "host win32k.sys GDI/DWM stays; HWND already hops this kernel"},
    {SurfaceKind::kCmd, "cmd", Isolation::kShell,
     "occupyCmd via IOCTL not CreateProcess; not os.exec"},
    {SurfaceKind::kSh, "sh", Isolation::kShell,
     "occupySh via wowwin32.ko ioctl; not os.exec"},
    {SurfaceKind::kWowKo, "wow_ko", Isolation::kSys,
     "wowwin32.ko module_init / unlocked_ioctl"},
    {SurfaceKind::kWasmtty, "wasmtty", Isolation::kTty,
     "WASMTTY WTTY hello via ioctl; occupancy does not steal tty.*"},
    {SurfaceKind::kGocvm, "gocvm", Isolation::kTty,
     "wasigocvm toolkit after sys: in-module WASMWin32 / CreateProcessW; not a second gocvm.exe"},
    {SurfaceKind::kConPty, "conpty", Isolation::kTty,
     "ConPTY occupancy channel under WASMTTY"},
    {SurfaceKind::kPts, "pts", Isolation::kTty, "linux pts under wowwin32.ko"},
    {SurfaceKind::kSctp, "sctp", Isolation::kNet,
     "sctp occupancy; not net.dial / net.call"},
    {SurfaceKind::kWns, "wns", Isolation::kNet,
     "sctp-rpc occupancy hop; occupancy does not steal net.call"},
};

bool EqI(std::string_view a, const char* b) {
  size_t n = std::strlen(b);
  if (a.size() != n) {
    return false;
  }
  for (size_t i = 0; i < n; ++i) {
    char c = a[i];
    char d = b[i];
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
    if (d >= 'A' && d <= 'Z') {
      d = static_cast<char>(d - 'A' + 'a');
    }
    if (c != d) {
      return false;
    }
  }
  return true;
}

}  // namespace

const char* KindToString(SurfaceKind k) {
  for (const auto& r : kKinds) {
    if (r.kind == k) {
      return r.name;
    }
  }
  return "handle";
}

SurfaceKind KindFromString(std::string_view s) {
  for (const auto& r : kKinds) {
    if (s == r.name) {
      return r.kind;
    }
  }
  return KindFromDll(s);
}

SurfaceKind KindFromDll(std::string_view dll) {
  if (EqI(dll, "kernel32") || EqI(dll, "kernel32.dll")) {
    return SurfaceKind::kKernel32;
  }
  if (EqI(dll, "ntdll") || EqI(dll, "ntdll.dll")) {
    return SurfaceKind::kNtdll;
  }
  if (EqI(dll, "user32") || EqI(dll, "user32.dll")) {
    return SurfaceKind::kUser32;
  }
  if (EqI(dll, "gdi32") || EqI(dll, "gdi32.dll")) {
    return SurfaceKind::kGdi32;
  }
  if (EqI(dll, "ole32") || EqI(dll, "ole32.dll")) {
    return SurfaceKind::kOle32;
  }
  if (EqI(dll, "oleaut32") || EqI(dll, "oleaut32.dll")) {
    return SurfaceKind::kOleaut32;
  }
  if (EqI(dll, "advapi32") || EqI(dll, "advapi32.dll")) {
    return SurfaceKind::kAdvapi32;
  }
  if (EqI(dll, "ws2_32") || EqI(dll, "ws2_32.dll")) {
    return SurfaceKind::kWs2;
  }
  if (EqI(dll, "bcrypt") || EqI(dll, "bcrypt.dll")) {
    return SurfaceKind::kBcrypt;
  }
  if (EqI(dll, "ncrypt") || EqI(dll, "ncrypt.dll")) {
    return SurfaceKind::kNcrypt;
  }
  if (EqI(dll, "crypt32") || EqI(dll, "crypt32.dll")) {
    return SurfaceKind::kCrypt32;
  }
  if (EqI(dll, "shell32") || EqI(dll, "shell32.dll")) {
    return SurfaceKind::kShell32;
  }
  if (EqI(dll, "winhttp") || EqI(dll, "winhttp.dll")) {
    return SurfaceKind::kWinhttp;
  }
  if (EqI(dll, "iphlpapi") || EqI(dll, "iphlpapi.dll")) {
    return SurfaceKind::kIphlpapi;
  }
  if (EqI(dll, "version") || EqI(dll, "version.dll")) {
    return SurfaceKind::kVersion;
  }
  if (EqI(dll, "comctl32") || EqI(dll, "comctl32.dll")) {
    return SurfaceKind::kComctl32;
  }
  if (EqI(dll, "wininet") || EqI(dll, "wininet.dll")) {
    return SurfaceKind::kWininet;
  }
  if (EqI(dll, "dnsapi") || EqI(dll, "dnsapi.dll")) {
    return SurfaceKind::kDnsapi;
  }
  if (EqI(dll, "secur32") || EqI(dll, "secur32.dll")) {
    return SurfaceKind::kSecur32;
  }
  if (EqI(dll, "dbghelp") || EqI(dll, "dbghelp.dll")) {
    return SurfaceKind::kDbghelp;
  }
  if (EqI(dll, "setupapi") || EqI(dll, "setupapi.dll")) {
    return SurfaceKind::kSetupapi;
  }
  if (EqI(dll, "netapi32") || EqI(dll, "netapi32.dll")) {
    return SurfaceKind::kNetapi32;
  }
  if (EqI(dll, "pdh") || EqI(dll, "pdh.dll")) {
    return SurfaceKind::kPdh;
  }
  if (EqI(dll, "wevtapi") || EqI(dll, "wevtapi.dll")) {
    return SurfaceKind::kWevtapi;
  }
  if (EqI(dll, "comdlg32") || EqI(dll, "comdlg32.dll")) {
    return SurfaceKind::kComdlg32;
  }
  if (EqI(dll, "WinHvPlatform") || EqI(dll, "winhvplatform.dll")) {
    return SurfaceKind::kWinHv;
  }
  if (EqI(dll, "WinHvEmulation") || EqI(dll, "winhvemulation.dll")) {
    return SurfaceKind::kWinHvEmu;
  }
  if (EqI(dll, "wslapi") || EqI(dll, "wslapi.dll")) {
    return SurfaceKind::kWslapi;
  }
  if (EqI(dll, "wsl.exe") || EqI(dll, "wsl")) {
    return SurfaceKind::kWsl;
  }
  if (EqI(dll, "nix")) {
    return SurfaceKind::kNix;
  }
  if (EqI(dll, "cmd.exe")) {
    return SurfaceKind::kCmd;
  }
  if (EqI(dll, "/bin/sh")) {
    return SurfaceKind::kSh;
  }
  if (EqI(dll, "wowwin32") || EqI(dll, "wowwin32.sys")) {
    return SurfaceKind::kOccupancyDriver;
  }
  if (EqI(dll, "wowwin32.ko")) {
    return SurfaceKind::kWowKo;
  }
  if (EqI(dll, "wasigocvm") || EqI(dll, "gocvm")) {
    return SurfaceKind::kGocvm;
  }
  return SurfaceKind::kHandle;
}

const char* IsolationToString(Isolation i) {
  switch (i) {
    case Isolation::kSfi:
      return "sfi";
    case Isolation::kHost:
      return "host";
    case Isolation::kSys:
      return "sys";
    case Isolation::kIpc:
      return "ipc";
    case Isolation::kCompositor:
      return "compositor";
    case Isolation::kHv:
      return "hv";
    case Isolation::kShell:
      return "shell";
    case Isolation::kTty:
      return "tty";
    case Isolation::kNet:
      return "net";
  }
  return "sfi";
}

Isolation IsolationOf(SurfaceKind k) {
  for (const auto& r : kKinds) {
    if (r.kind == k) {
      return r.isolation;
    }
  }
  return Isolation::kSfi;
}

Isolation IsolationOfDll(std::string_view dll) {
  return IsolationOf(KindFromDll(dll));
}

bool IsThin(SurfaceKind k) { return IsolationOf(k) != Isolation::kSfi; }

std::vector<ThinHop> AllThinHops() {
  std::vector<ThinHop> out;
  for (const auto& r : kKinds) {
    ThinHop h;
    h.kind = r.kind;
    h.isolation = r.isolation;
    h.method = r.name;
    h.why = r.why;
    h.thin = r.isolation != Isolation::kSfi;
    out.push_back(h);
  }
  return out;
}

std::string JsonEscape(std::string_view s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') {
      o += '\\';
    }
    o += c;
  }
  o += '"';
  return o;
}

std::string ThinMapJson() {
  auto hops = AllThinHops();
  std::string o = "{\"thin\":true,\"surfaces\":[";
  bool first = true;
  for (const auto& h : hops) {
    if (!first) {
      o += ',';
    }
    first = false;
    o += "{\"kind\":";
    o += JsonEscape(KindToString(h.kind));
    o += ",\"isolation\":";
    o += JsonEscape(IsolationToString(h.isolation));
    o += ",\"thin\":";
    o += h.thin ? "true" : "false";
    o += ",\"why\":";
    o += JsonEscape(h.why);
    o += '}';
  }
  o += "]}";
  return o;
}

}  // namespace wow
