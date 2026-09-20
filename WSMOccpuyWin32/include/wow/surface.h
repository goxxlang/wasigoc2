#ifndef WOW_SURFACE_H_
#define WOW_SURFACE_H_

#include "wow/c/types.h"

#include <string>
#include <string_view>
#include <vector>

namespace wow {

// Every WASMWin32 catalog DLL / namespace is an occupancy surface.
// Isolation is thin wherever the hop leaves wasm SFI: kernel32 host,
// ntdll sys, HWND compositor, pipes ipc, WinHv, WSL, sockets.
enum class SurfaceKind {
  kHandle = WOW_SURFACE_HANDLE,
  kKernel32 = WOW_SURFACE_KERNEL32,
  kNtdll = WOW_SURFACE_NTDLL,
  kUser32 = WOW_SURFACE_USER32,
  kGdi32 = WOW_SURFACE_GDI32,
  kOle32 = WOW_SURFACE_OLE32,
  kOleaut32 = WOW_SURFACE_OLEAUT32,
  kAdvapi32 = WOW_SURFACE_ADVAPI32,
  kWs2 = WOW_SURFACE_WS2,
  kBcrypt = WOW_SURFACE_BCRYPT,
  kNcrypt = WOW_SURFACE_NCRYPT,
  kCrypt32 = WOW_SURFACE_CRYPT32,
  kShell32 = WOW_SURFACE_SHELL32,
  kWinhttp = WOW_SURFACE_WINHTTP,
  kIphlpapi = WOW_SURFACE_IPHLPAPI,
  kVersion = WOW_SURFACE_VERSION,
  kComctl32 = WOW_SURFACE_COMCTL32,
  kWininet = WOW_SURFACE_WININET,
  kDnsapi = WOW_SURFACE_DNSAPI,
  kSecur32 = WOW_SURFACE_SECUR32,
  kDbghelp = WOW_SURFACE_DBGHELP,
  kSetupapi = WOW_SURFACE_SETUPAPI,
  kNetapi32 = WOW_SURFACE_NETAPI32,
  kPdh = WOW_SURFACE_PDH,
  kWevtapi = WOW_SURFACE_WEVTAPI,
  kComdlg32 = WOW_SURFACE_COMDLG32,
  kWinHv = WOW_SURFACE_WINHV,
  kWinHvEmu = WOW_SURFACE_WINHV_EMU,
  kWslapi = WOW_SURFACE_WSLAPI,
  kWsl = WOW_SURFACE_WSL,
  kNix = WOW_SURFACE_NIX,
  kProcess = WOW_SURFACE_PROCESS,
  kThread = WOW_SURFACE_THREAD,
  kFile = WOW_SURFACE_FILE,
  kHeap = WOW_SURFACE_HEAP,
  kVmem = WOW_SURFACE_VMEM,
  kPipe = WOW_SURFACE_PIPE,
  kConsole = WOW_SURFACE_CONSOLE,
  kRegistry = WOW_SURFACE_REGISTRY,
  kToken = WOW_SURFACE_TOKEN,
  kPe = WOW_SURFACE_PE,
  kHwnd = WOW_SURFACE_HWND,
  kNtoskrnl = WOW_SURFACE_NTOSKRNL,
  kSysHeap = WOW_SURFACE_SYS_HEAP,
  kOccupancyDriver = WOW_SURFACE_OCCUPANCY_DRIVER,
  kWin32k = WOW_SURFACE_WIN32K,
  kCmd = WOW_SURFACE_CMD,
  kSh = WOW_SURFACE_SH,
  kWowKo = WOW_SURFACE_WOW_KO,
  kWasmtty = WOW_SURFACE_WASMTTY,
  kGocvm = WOW_SURFACE_GOCVM,
  kConPty = WOW_SURFACE_CONPTY,
  kPts = WOW_SURFACE_PTS,
  kSctp = WOW_SURFACE_SCTP,
  kWns = WOW_SURFACE_WNS,
};

enum class Isolation {
  kSfi = WOW_ISOLATION_SFI,
  kHost = WOW_ISOLATION_HOST,
  kSys = WOW_ISOLATION_SYS,
  kIpc = WOW_ISOLATION_IPC,
  kCompositor = WOW_ISOLATION_COMPOSITOR,
  kHv = WOW_ISOLATION_HV,
  kShell = WOW_ISOLATION_SHELL,
  kTty = WOW_ISOLATION_TTY,
  kNet = WOW_ISOLATION_NET,
};

struct ThinHop {
  SurfaceKind kind = SurfaceKind::kKernel32;
  Isolation isolation = Isolation::kHost;
  const char* method = "";
  const char* why = "";
  bool thin = true;
};

const char* KindToString(SurfaceKind k);
SurfaceKind KindFromString(std::string_view s);
SurfaceKind KindFromDll(std::string_view dll);
const char* IsolationToString(Isolation i);
Isolation IsolationOf(SurfaceKind k);
Isolation IsolationOfDll(std::string_view dll);
bool IsThin(SurfaceKind k);

std::string JsonEscape(std::string_view s);
std::vector<ThinHop> AllThinHops();
std::string ThinMapJson();

}  // namespace wow

#endif  // WOW_SURFACE_H_
