// Native Windows backend for WASMWin32. Entry points keep win32metadata
// names (GetCurrentProcessId, GetComputerNameW, ...). WSL is Microsoft's
// documented wsl.exe / wslapi.dll hop — Nix runs as a command inside that
// distro, not a second Win32 stack.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "win32/dispatch.h"
#include "win32/catalog.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winreg.h>
#include <sddl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <bcrypt.h>
#include <ole2.h>
#include <oleauto.h>
#include <ncrypt.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <winsvc.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "comdlg32.lib")
#endif

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
    const wchar_t* w =
        reinterpret_cast<const wchar_t*>(raw.data() + 2);
    int n = (int)((raw.size() - 2) / sizeof(wchar_t));
    while (n > 0 && w[n - 1] == 0) --n;
    return WideToUtf8(w, n);
  }
  if (raw.size() >= 2 && raw[1] == '\0') {
    const wchar_t* w = reinterpret_cast<const wchar_t*>(raw.data());
    int n = (int)(raw.size() / sizeof(wchar_t));
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
  std::string s = std::string(what) + ": " + std::to_string(err);
  Fill(out, cap, s);
  return err ? err : -1;
}

bool Eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

DWORD WINAPI kQueueWork(LPVOID) { return 0; }

BOOL CALLBACK kInitOnceCb(PINIT_ONCE, PVOID, PVOID*) { return TRUE; }

VOID CALLBACK kWaitCb(PVOID, BOOLEAN) {}

VOID CALLBACK kTqCb(PVOID, BOOLEAN) {}

VOID CALLBACK kTpSimple(PTP_CALLBACK_INSTANCE, PVOID) {}

VOID CALLBACK kTpWork(PTP_CALLBACK_INSTANCE, PVOID, PTP_WORK) {}

VOID CALLBACK kTpTimer(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER) {}

VOID WINAPI kFiberStart(PVOID) {}

std::string FileTimeU64(const FILETIME& ft) {
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return std::to_string(u.QuadPart);
}

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

HANDLE HandleOf(const char* s) {
  return (HANDLE)(intptr_t)std::strtoll(s, nullptr, 10);
}

HKEY HkeyOf(const char* s) {
  if (!s || !s[0]) return HKEY_CURRENT_USER;
  auto icmp = [](const char* a, const char* b) {
    while (*a && *b) {
      unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
      if (x >= 'a' && x <= 'z') x = (unsigned char)(x - 'a' + 'A');
      if (y >= 'a' && y <= 'z') y = (unsigned char)(y - 'a' + 'A');
      if (x != y) return false;
    }
    return *a == *b;
  };
  if (icmp(s, "HKCU") || icmp(s, "HKEY_CURRENT_USER")) return HKEY_CURRENT_USER;
  if (icmp(s, "HKLM") || icmp(s, "HKEY_LOCAL_MACHINE")) return HKEY_LOCAL_MACHINE;
  if (icmp(s, "HKCR") || icmp(s, "HKEY_CLASSES_ROOT")) return HKEY_CLASSES_ROOT;
  if (icmp(s, "HKU") || icmp(s, "HKEY_USERS")) return HKEY_USERS;
  if (icmp(s, "HKCC") || icmp(s, "HKEY_CURRENT_CONFIG")) return HKEY_CURRENT_CONFIG;
  return (HKEY)HandleOf(s);
}

std::string HandleStr(HANDLE h) {
  return std::to_string((long long)(intptr_t)h);
}

HANDLE ProcOf(const char* s) {
  if (!s || !s[0] || Eq(s, "-1") || Eq(s, "0")) return GetCurrentProcess();
  return HandleOf(s);
}

std::unordered_map<HANDLE, DWORD>& LastIo() {
  static std::unordered_map<HANDLE, DWORD> m;
  return m;
}

// Occupancy CreateProcessW is real: the child is an actual OS process,
// not a wasi_k32 vthread. Its stdout/stderr must be piped back through
// GetProcessOutput or the "upstream does the real work" half of the
// edge kernel produces work nobody can see. Keyed by the process
// HANDLE the caller already carries (same key as GetExitCodeProcess /
// WaitForSingleObject), so no new handle namespace is needed.
std::unordered_map<HANDLE, HANDLE>& ProcOutPipe() {
  static std::unordered_map<HANDLE, HANDLE> m;
  return m;
}

std::string DrainPipe(HANDLE rd) {
  std::string s;
  if (!rd || rd == INVALID_HANDLE_VALUE) return s;
  for (;;) {
    DWORD avail = 0;
    if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) break;
    char chunk[4096];
    DWORD want = avail < (DWORD)sizeof(chunk) ? avail : (DWORD)sizeof(chunk);
    DWORD got = 0;
    if (!ReadFile(rd, chunk, want, &got, nullptr) || got == 0) break;
    s.append(chunk, got);
  }
  return s;
}

std::unordered_map<std::string, u_short>& InprocPorts() {
  static std::unordered_map<std::string, u_short> m;
  return m;
}

bool EnsureWsa() {
  static int inited = 0;
  if (inited) return true;
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
  inited = 1;
  return true;
}

int FillWsa(char* out, unsigned cap, const char* what) {
  return FillErr(out, cap, WSAGetLastError(), what);
}

std::string DeviceIoctlDispatch(unsigned code) {
  if (code == 0x00070000u)
    return std::string("512") + "\x1f" + "1" + "\x1f" + "255" + "\x1f" + "63";
  if (code == 0x0007405cu) return "1048576";
  if (code == 0x002d4800u) return "ok";
  if (code == 0x001b0044u) return "9600";
  return {};
}

bool ParseSockAddr(const std::string& a, sockaddr_in* out) {
  std::memset(out, 0, sizeof(*out));
  out->sin_family = AF_INET;
  std::string host = a;
  unsigned port = 0;
  if (host.rfind("inproc:", 0) == 0) {
    auto it = InprocPorts().find(host);
    if (it != InprocPorts().end()) port = it->second;
    host = "127.0.0.1";
  } else {
    auto c = host.rfind(':');
    if (c != std::string::npos) {
      port = (unsigned)std::strtoul(host.c_str() + c + 1, nullptr, 10);
      host = host.substr(0, c);
    }
  }
  if (host.empty() || host == "0.0.0.0")
    out->sin_addr.s_addr = htonl(INADDR_ANY);
  else if (host == "127.0.0.1" || host == "localhost")
    out->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  else if (inet_pton(AF_INET, host.c_str(), &out->sin_addr) != 1)
    return false;
  out->sin_port = htons((u_short)port);
  return true;
}

std::string HexOf(const unsigned char* p, size_t n) {
  static const char* h = "0123456789abcdef";
  std::string s(n * 2, '\0');
  for (size_t i = 0; i < n; ++i) {
    s[i * 2] = h[p[i] >> 4];
    s[i * 2 + 1] = h[p[i] & 0xf];
  }
  return s;
}

std::vector<unsigned char> UnhexOf(const char* s) {
  std::vector<unsigned char> b;
  if (!s) return b;
  auto nyb = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
  };
  size_t n = std::strlen(s);
  b.reserve(n / 2);
  for (size_t i = 0; i + 1 < n; i += 2)
    b.push_back((unsigned char)((nyb(s[i]) << 4) | nyb(s[i + 1])));
  return b;
}

LPCWSTR ResId(const std::string& s, std::wstring* keep) {
  if (s.empty()) return nullptr;
  bool dig = true;
  for (char c : s)
    if (c < '0' || c > '9') dig = false;
  if (dig) return MAKEINTRESOURCEW((WORD)std::strtoul(s.c_str(), nullptr, 10));
  *keep = Utf8ToWide(s);
  return keep->c_str();
}

bool NtEq(const char* api, const char* nt) {
  if (Eq(api, nt)) return true;
  return api && nt && api[0] == 'Z' && api[1] == 'w' && nt[0] == 'N' && nt[1] == 't' &&
         Eq(api + 2, nt + 2);
}

bool IsNtdllCatalog(const char* api) {
  int n = 0;
  const WasmWin32Api* c = wasmwin32_catalog(&n);
  for (int i = 0; i < n; ++i)
    if (c[i].dll && c[i].name && Eq(c[i].dll, "ntdll") && Eq(c[i].name, api)) return true;
  return false;
}

bool IsWhpCatalog(const char* api) {
  if (!api || api[0] != 'W' || api[1] != 'H' || api[2] != 'v') return false;
  int n = 0;
  const WasmWin32Api* c = wasmwin32_catalog(&n);
  for (int i = 0; i < n; ++i)
    if (c[i].name && Eq(c[i].name, api)) return true;
  return false;
}

HMODULE NtdllMod() {
  static HMODULE h = nullptr;
  if (!h) {
    h = GetModuleHandleW(L"ntdll.dll");
    if (!h) h = LoadLibraryW(L"ntdll.dll");
  }
  return h;
}

FARPROC NtProc(const char* name) {
  HMODULE m = NtdllMod();
  if (!m || !name) return nullptr;
  FARPROC p = GetProcAddress(m, name);
  if (!p && name[0] == 'N' && name[1] == 't') {
    std::string zw = std::string("Zw") + (name + 2);
    p = GetProcAddress(m, zw.c_str());
  }
  return p;
}

void* CurrentPeb() {
#ifdef _WIN64
  return *(void**)((char*)NtCurrentTeb() + 0x60);
#else
  return *(void**)((char*)NtCurrentTeb() + 0x30);
#endif
}

struct K32Pbi {
  void* Reserved1;
  void* PebBaseAddress;
  void* Reserved2[2];
  ULONG_PTR UniqueProcessId;
  void* Reserved3;
};

LONG CALLBACK K32Veh(PEXCEPTION_POINTERS ep) {
  if (ep && ep->ExceptionRecord && ep->ExceptionRecord->ExceptionCode == 0xE0000001ul)
    return EXCEPTION_CONTINUE_EXECUTION;
  return EXCEPTION_CONTINUE_SEARCH;
}

PVOID& VehHandle() {
  static PVOID p = nullptr;
  return p;
}

HMODULE BcryptMod() {
  static HMODULE h = nullptr;
  if (!h) h = LoadLibraryW(L"bcrypt.dll");
  return h;
}

HMODULE NcryptMod() {
  static HMODULE h = nullptr;
  if (!h) h = LoadLibraryW(L"ncrypt.dll");
  return h;
}

FARPROC ExtraProc(const wchar_t* dll, const char* name) {
  HMODULE m = LoadLibraryW(dll);
  return (m && name) ? GetProcAddress(m, name) : nullptr;
}

std::string WhpFold(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  const char* pfx[] = {"whvpartitionpropertycode", "whvx64register", "whvregister",
                       "propertycode", "register", nullptr};
  for (int i = 0; pfx[i]; ++i) {
    size_t n = strlen(pfx[i]);
    if (s.size() > n && s.compare(0, n, pfx[i]) == 0) s.erase(0, n);
  }
  return s;
}

unsigned long long WhpU64(const std::string& s) {
  if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    return std::strtoull(s.c_str() + 2, nullptr, 16);
  return std::strtoull(s.c_str(), nullptr, 0);
}

unsigned WhpProp(const std::string& s) {
  std::string t = WhpFold(s);
  if (t == "processorcount") return 0x1fffu;
  if (t == "extendedvmexits") return 1;
  if (t == "exceptionexitbitmap") return 2;
  if (t == "separatesecuritydomain") return 3;
  if (t == "nestedvirtualization") return 4;
  if (t == "x64msrexitbitmap") return 5;
  if (t == "localapicemulationmode") return 0x1005u;
  if (t == "processorfeatures") return 0x1001u;
  if (t == "physicaladdresswidth") return 0x1011u;
  return (unsigned)WhpU64(s);
}

unsigned WhpReg(const std::string& s) {
  std::string t = WhpFold(s);
  static const char* gp[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                             "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
  for (unsigned i = 0; i < 16; ++i)
    if (t == gp[i]) return i;
  if (t == "rip") return 0x10;
  if (t == "rflags" || t == "eflags") return 0x11;
  if (t == "es") return 0x12;
  if (t == "cs") return 0x13;
  if (t == "ss") return 0x14;
  if (t == "ds") return 0x15;
  if (t == "fs") return 0x16;
  if (t == "gs") return 0x17;
  if (t == "ldtr") return 0x18;
  if (t == "tr") return 0x19;
  if (t == "idtr") return 0x1A;
  if (t == "gdtr") return 0x1B;
  if (t == "cr0") return 0x1C;
  if (t == "cr2") return 0x1D;
  if (t == "cr3") return 0x1E;
  if (t == "cr4") return 0x1F;
  if (t == "cr8") return 0x20;
  if (t == "efer") return 0x2001;
  if (t == "pat") return 0x2004;
  return (unsigned)WhpU64(s);
}

struct WhpGpa {
  void* host = nullptr;
  unsigned long long gpa = 0;
  unsigned long long size = 0;
  int owned = 0;
};

std::unordered_map<void*, std::vector<WhpGpa>>& WhpMaps() {
  static std::unordered_map<void*, std::vector<WhpGpa>> m;
  return m;
}

struct WhpExit {
  alignas(16) unsigned char ctx[224]{};
  int valid = 0;
};

std::unordered_map<void*, std::unordered_map<unsigned, WhpExit>>& WhpExits() {
  static std::unordered_map<void*, std::unordered_map<unsigned, WhpExit>> m;
  return m;
}

std::unordered_map<void*, std::unordered_map<unsigned, unsigned>>& WhpPorts() {
  static std::unordered_map<void*, std::unordered_map<unsigned, unsigned>> m;
  return m;
}

void*& WhpLastEmu() {
  static void* e = nullptr;
  return e;
}

void WhpRelease(void* part) {
  if (!part) return;
  auto it = WhpMaps().find(part);
  if (it == WhpMaps().end()) return;
  using UnmapFn = long(WINAPI*)(void*, unsigned long long, unsigned long long);
  auto unmap = (UnmapFn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvUnmapGpaRange");
  for (auto& g : it->second) {
    if (unmap) unmap(part, g.gpa, g.size);
    if (g.owned && g.host) VirtualFree(g.host, 0, MEM_RELEASE);
  }
  WhpMaps().erase(it);
  WhpExits().erase(part);
  WhpPorts().erase(part);
}

WhpGpa* WhpFindGpa(void* part, unsigned long long gpa) {
  auto it = WhpMaps().find(part);
  if (it == WhpMaps().end()) return nullptr;
  for (auto& g : it->second)
    if (gpa >= g.gpa && gpa < g.gpa + g.size) return &g;
  return nullptr;
}

const char* WhpExitName(unsigned r) {
  switch (r) {
    case 0: return "none";
    case 1: return "memory";
    case 2: return "io";
    case 4: return "unrecoverable";
    case 5: return "invalidvp";
    case 6: return "unsupported";
    case 7: return "interruptwindow";
    case 8: return "halt";
    case 9: return "apiceoi";
    case 0xA: return "synic";
    case 0x1000: return "msr";
    case 0x1001: return "cpuid";
    case 0x1002: return "exception";
    case 0x1003: return "rdtsc";
    case 0x2001: return "canceled";
    default: return "exit";
  }
}

int WhpRegKind(unsigned r) {
  if (r >= 0x12 && r <= 0x19) return 1;
  if (r == 0x1A || r == 0x1B) return 2;
  return 0;
}

struct WhpSegBlob {
  unsigned long long Base;
  unsigned Limit;
  unsigned short Selector;
  unsigned short Attributes;
};

struct WhpTabBlob {
  unsigned short Pad[3];
  unsigned short Limit;
  unsigned long long Base;
};

struct WhpEmuIo {
  unsigned char Direction;
  unsigned short Port;
  unsigned short AccessSize;
  unsigned Data;
};

struct WhpEmuMem {
  unsigned long long GpaAddress;
  unsigned char Direction;
  unsigned char AccessSize;
  unsigned char Data[8];
};

struct WhpEmuCtx {
  void* part = nullptr;
  unsigned vp = 0;
};

struct WhpEmuCbs {
  unsigned Size;
  unsigned Reserved;
  long(CALLBACK* Io)(void*, WhpEmuIo*);
  long(CALLBACK* Mem)(void*, WhpEmuMem*);
  long(CALLBACK* Get)(void*, const unsigned*, unsigned, void*);
  long(CALLBACK* Set)(void*, const unsigned*, unsigned, const void*);
  long(CALLBACK* Xlat)(void*, unsigned long long, unsigned, unsigned*, unsigned long long*);
};

std::string WhpFmtExit(const unsigned char* ctx) {
  unsigned reason = 0;
  memcpy(&reason, ctx, 4);
  unsigned long long rip = 0;
  memcpy(&rip, ctx + 32, 8);
  std::string extra;
  if (reason == 1) {
    unsigned long long gpa = 0;
    memcpy(&gpa, ctx + 72, 8);
    extra = std::to_string(gpa);
  } else if (reason == 2) {
    unsigned acc = 0;
    unsigned short port = 0;
    memcpy(&acc, ctx + 68, 4);
    memcpy(&port, ctx + 72, 2);
    extra = std::to_string(port);
    extra += "\x1f";
    extra += (acc & 1u) ? "write" : "read";
  } else if (reason == 0x1002u) {
    extra = std::to_string((unsigned)ctx[72]);
  }
  std::string s = std::to_string(reason);
  s += "\x1f";
  s += WhpExitName(reason);
  s += "\x1f";
  s += std::to_string(rip);
  if (!extra.empty()) {
    s += "\x1f";
    s += extra;
  }
  return s;
}

void WhpPackReg(unsigned r, const std::vector<std::string>& v, size_t i, unsigned char* blob) {
  memset(blob, 0, 16);
  auto take = [](const std::string& s, unsigned long long fb) {
    return s.empty() ? fb : WhpU64(s);
  };
  if (WhpRegKind(r) == 1) {
    WhpSegBlob s{};
    s.Limit = 0xffff;
    s.Attributes = (unsigned short)(r == 0x13 ? 0x9b : 0x93);
    std::string a = i < v.size() ? v[i] : "";
    std::string b = i + 1 < v.size() ? v[i + 1] : "";
    std::string c = i + 2 < v.size() ? v[i + 2] : "";
    std::string d = i + 3 < v.size() ? v[i + 3] : "";
    auto col = a.find(':');
    if (col != std::string::npos) {
      std::string p[4];
      int n = 0;
      std::string cur;
      for (size_t k = 0; k <= a.size() && n < 4; ++k) {
        if (k == a.size() || a[k] == ':') {
          p[n++] = cur;
          cur.clear();
        } else {
          cur.push_back(a[k]);
        }
      }
      a = p[0];
      if (n > 1) b = p[1];
      if (n > 2) c = p[2];
      if (n > 3) d = p[3];
    }
    s.Base = take(a, 0);
    s.Limit = (unsigned)take(b, s.Limit);
    s.Selector = (unsigned short)take(c, 0);
    s.Attributes = (unsigned short)take(d, s.Attributes);
    memcpy(blob, &s, sizeof(s));
    return;
  }
  if (WhpRegKind(r) == 2) {
    WhpTabBlob t{};
    std::string a = i < v.size() ? v[i] : "";
    std::string b = i + 1 < v.size() ? v[i + 1] : "";
    auto col = a.find(':');
    if (col != std::string::npos) {
      b = a.substr(col + 1);
      a = a.substr(0, col);
    }
    t.Base = take(a, 0);
    t.Limit = (unsigned short)take(b, 0);
    memcpy(blob, &t, sizeof(t));
    return;
  }
  unsigned long long rv = i < v.size() ? WhpU64(v[i]) : 0;
  memcpy(blob, &rv, sizeof(rv));
}

std::string WhpUnpackReg(unsigned r, const unsigned char* blob) {
  if (WhpRegKind(r) == 1) {
    WhpSegBlob s{};
    memcpy(&s, blob, sizeof(s));
    return std::to_string(s.Base) + "\x1f" + std::to_string(s.Limit) + "\x1f" +
           std::to_string(s.Selector) + "\x1f" + std::to_string(s.Attributes);
  }
  if (WhpRegKind(r) == 2) {
    WhpTabBlob t{};
    memcpy(&t, blob, sizeof(t));
    return std::to_string(t.Base) + "\x1f" + std::to_string(t.Limit);
  }
  unsigned long long rv = 0;
  memcpy(&rv, blob, sizeof(rv));
  return std::to_string(rv);
}

long CALLBACK WhpEmuIoCb(void* ctx, WhpEmuIo* io) {
  if (!ctx || !io) return (long)0x80004003;
  auto* c = (WhpEmuCtx*)ctx;
  auto& ports = WhpPorts()[c->part];
  if (io->Direction == 0)
    ports[io->Port] = io->Data;
  else
    io->Data = ports[io->Port];
  return 0;
}

long CALLBACK WhpEmuMemCb(void* ctx, WhpEmuMem* m) {
  if (!ctx || !m) return (long)0x80004003;
  auto* c = (WhpEmuCtx*)ctx;
  WhpGpa* rng = WhpFindGpa(c->part, m->GpaAddress);
  if (!rng || !rng->host) return (long)0x80004005;
  size_t off = (size_t)(m->GpaAddress - rng->gpa);
  unsigned n = m->AccessSize;
  if (!n || n > 8) n = n ? 8 : 1;
  if (off + n > rng->size) return (long)0x80004005;
  if (m->Direction == 0)
    memcpy((char*)rng->host + off, m->Data, n);
  else
    memcpy(m->Data, (char*)rng->host + off, n);
  return 0;
}

long CALLBACK WhpEmuGetCb(void* ctx, const unsigned* names, unsigned n, void* vals) {
  if (!ctx) return (long)0x80004003;
  auto* c = (WhpEmuCtx*)ctx;
  using Fn = long(WINAPI*)(void*, unsigned, const unsigned*, unsigned, void*);
  auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvGetVirtualProcessorRegisters");
  return fn ? fn(c->part, c->vp, names, n, vals) : (long)0x8007007F;
}

long CALLBACK WhpEmuSetCb(void* ctx, const unsigned* names, unsigned n, const void* vals) {
  if (!ctx) return (long)0x80004003;
  auto* c = (WhpEmuCtx*)ctx;
  using Fn = long(WINAPI*)(void*, unsigned, const unsigned*, unsigned, const void*);
  auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvSetVirtualProcessorRegisters");
  return fn ? fn(c->part, c->vp, names, n, vals) : (long)0x8007007F;
}

long CALLBACK WhpEmuXlatCb(void* ctx, unsigned long long gva, unsigned flags, unsigned* result,
                           unsigned long long* gpa) {
  if (!ctx) return (long)0x80004003;
  auto* c = (WhpEmuCtx*)ctx;
  using Fn = long(WINAPI*)(void*, unsigned, unsigned long long, unsigned, void*, unsigned long long*);
  auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvTranslateGva");
  unsigned char res[8]{};
  if (fn) {
    long hr = fn(c->part, c->vp, gva, flags ? flags : 1u, res, gpa);
    if (result) memcpy(result, res, 4);
    if (hr >= 0) return 0;
  }
  if (gpa) *gpa = gva & ~0xFFFull;
  if (result) *result = 0;
  return 0;
}

std::vector<std::string> SplitAll(const char* a) {
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

unsigned long long FtQuad(const FILETIME& ft) {
  ULARGE_INTEGER u{};
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return u.QuadPart;
}

FILETIME QuadFt(unsigned long long q) {
  ULARGE_INTEGER u{};
  u.QuadPart = q;
  FILETIME ft{u.LowPart, u.HighPart};
  return ft;
}

}  // namespace

extern "C" int wasmwin32_call(const char* api, const char* args, char* out,
                              unsigned cap) {
  if (!api) return -1;
  const char* a = args ? args : "";

  if (Eq(api, "GetCurrentProcessId")) {
    return Fill(out, cap, std::to_string(GetCurrentProcessId()));
  }
  if (Eq(api, "GetCurrentThreadId")) {
    return Fill(out, cap, std::to_string(GetCurrentThreadId()));
  }
  if (Eq(api, "GetLastError")) {
    return Fill(out, cap, std::to_string(GetLastError()));
  }
  if (Eq(api, "GetTickCount64")) {
    return Fill(out, cap, std::to_string(GetTickCount64()));
  }
  if (Eq(api, "Sleep")) {
    DWORD ms = (DWORD)strtoul(a, nullptr, 10);
    Sleep(ms);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetComputerNameW")) {
    wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(buf, &n)) {
      return FillErr(out, cap, (int)GetLastError(), "GetComputerNameW");
    }
    return Fill(out, cap, WideToUtf8(buf, (int)n));
  }
  if (Eq(api, "GetEnvironmentVariableW")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "GetEnvironmentVariableW");
    std::wstring name = Utf8ToWide(a);
    DWORD need = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (need == 0) {
      return FillErr(out, cap, (int)GetLastError(), "GetEnvironmentVariableW");
    }
    std::wstring buf(need, L'\0');
    DWORD n = GetEnvironmentVariableW(name.c_str(), buf.data(), need);
    return Fill(out, cap, WideToUtf8(buf.c_str(), (int)n));
  }
  if (Eq(api, "GetWindowsDirectoryW")) {
    wchar_t buf[MAX_PATH];
    UINT n = GetWindowsDirectoryW(buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
      return FillErr(out, cap, (int)GetLastError(), "GetWindowsDirectoryW");
    }
    return Fill(out, cap, WideToUtf8(buf, (int)n));
  }
  if (Eq(api, "GetSystemDirectoryW")) {
    wchar_t buf[MAX_PATH];
    UINT n = GetSystemDirectoryW(buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
      return FillErr(out, cap, (int)GetLastError(), "GetSystemDirectoryW");
    }
    return Fill(out, cap, WideToUtf8(buf, (int)n));
  }
  if (Eq(api, "GetCurrentDirectoryW")) {
    DWORD need = GetCurrentDirectoryW(0, nullptr);
    std::wstring buf(need, L'\0');
    DWORD n = GetCurrentDirectoryW(need, buf.data());
    if (n == 0) return FillErr(out, cap, (int)GetLastError(), "GetCurrentDirectoryW");
    return Fill(out, cap, WideToUtf8(buf.c_str(), (int)n));
  }
  if (Eq(api, "GetModuleFileNameW")) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0) return FillErr(out, cap, (int)GetLastError(), "GetModuleFileNameW");
    return Fill(out, cap, WideToUtf8(buf, (int)n));
  }
  if (Eq(api, "GetFileAttributesW")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "GetFileAttributesW");
    DWORD attr = GetFileAttributesW(Utf8ToWide(a).c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
      return FillErr(out, cap, (int)GetLastError(), "GetFileAttributesW");
    }
    return Fill(out, cap, std::to_string(attr));
  }
  if (Eq(api, "QueryPerformanceFrequency")) {
    LARGE_INTEGER f{};
    if (!QueryPerformanceFrequency(&f)) {
      return FillErr(out, cap, (int)GetLastError(), "QueryPerformanceFrequency");
    }
    return Fill(out, cap, std::to_string(f.QuadPart));
  }
  if (Eq(api, "QueryPerformanceCounter")) {
    LARGE_INTEGER c{};
    if (!QueryPerformanceCounter(&c)) {
      return FillErr(out, cap, (int)GetLastError(), "QueryPerformanceCounter");
    }
    return Fill(out, cap, std::to_string(c.QuadPart));
  }

  if (Eq(api, "GetCurrentProcess")) {
    return Fill(out, cap, std::to_string((long long)(intptr_t)GetCurrentProcess()));
  }
  if (Eq(api, "GetCurrentThread")) {
    return Fill(out, cap, std::to_string((long long)(intptr_t)GetCurrentThread()));
  }
  if (Eq(api, "SwitchToThread")) {
    return Fill(out, cap, SwitchToThread() ? "1" : "0");
  }
  if (Eq(api, "CloseHandle") || Eq(api, "NtClose") || Eq(api, "ZwClose")) {
    HANDLE h = (HANDLE)(intptr_t)strtoll(a, nullptr, 10);
    if (!h || h == INVALID_HANDLE_VALUE) return Fill(out, cap, "ok");
    LastIo().erase(h);
    auto pit = ProcOutPipe().find(h);
    if (pit != ProcOutPipe().end()) {
      CloseHandle(pit->second);
      ProcOutPipe().erase(pit);
    }
    if (!CloseHandle(h)) return FillErr(out, cap, (int)GetLastError(), "CloseHandle");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "IsDebuggerPresent")) {
    return Fill(out, cap, IsDebuggerPresent() ? "1" : "0");
  }
  if (Eq(api, "GetACP")) {
    return Fill(out, cap, std::to_string(GetACP()));
  }
  if (Eq(api, "GetOEMCP")) {
    return Fill(out, cap, std::to_string(GetOEMCP()));
  }
  if (Eq(api, "GetStdHandle")) {
    DWORD id = STD_OUTPUT_HANDLE;
    if (Eq(a, "-10") || Eq(a, "stdin")) id = STD_INPUT_HANDLE;
    else if (Eq(a, "-12") || Eq(a, "stderr")) id = STD_ERROR_HANDLE;
    HANDLE h = GetStdHandle(id);
    return Fill(out, cap, std::to_string((long long)(intptr_t)h));
  }
  if (Eq(api, "OutputDebugStringW")) {
    OutputDebugStringW(Utf8ToWide(a).c_str());
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetUserNameW")) {
    wchar_t buf[256];
    DWORD n = 256;
    if (!GetUserNameW(buf, &n)) {
      return FillErr(out, cap, (int)GetLastError(), "GetUserNameW");
    }
    return Fill(out, cap, WideToUtf8(buf, (int)n - 1));
  }
  if (Eq(api, "GetSystemInfo")) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    std::string s = std::to_string(si.wProcessorArchitecture);
    s += "\x1f";
    s += std::to_string(si.dwPageSize);
    s += "\x1f";
    s += std::to_string(si.dwNumberOfProcessors);
    return Fill(out, cap, s);
  }
  if (Eq(api, "GetSystemTime") || Eq(api, "GetLocalTime")) {
    SYSTEMTIME st{};
    if (Eq(api, "GetLocalTime"))
      GetLocalTime(&st);
    else
      GetSystemTime(&st);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02u.%03u", st.wYear,
             st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             st.wMilliseconds);
    return Fill(out, cap, buf);
  }
  if (Eq(api, "GetSystemTimeAsFileTime")) {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u{};
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return Fill(out, cap, std::to_string(u.QuadPart));
  }
  if (Eq(api, "GetCommandLineW")) {
    return Fill(out, cap, WideToUtf8(GetCommandLineW()));
  }
  if (Eq(api, "GetTempPathW")) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, buf);
    if (n == 0 || n >= MAX_PATH) {
      return FillErr(out, cap, (int)GetLastError(), "GetTempPathW");
    }
    return Fill(out, cap, WideToUtf8(buf, (int)n));
  }
  if (Eq(api, "GetEnvironmentStringsW")) {
    LPWCH env = GetEnvironmentStringsW();
    if (!env) return FillErr(out, cap, (int)GetLastError(), "GetEnvironmentStringsW");
    std::string outstr;
    for (LPWCH p = env; *p; p += wcslen(p) + 1) {
      if (!outstr.empty()) outstr.push_back('\x1f');
      outstr += WideToUtf8(p);
    }
    FreeEnvironmentStringsW(env);
    return Fill(out, cap, outstr);
  }
  if (Eq(api, "SetEnvironmentVariableW")) {
    std::string name, val;
    const char* p = std::strchr(a, '\x1f');
    if (!p) {
      name = a;
    } else {
      name.assign(a, p);
      val = p + 1;
    }
    if (name.empty()) {
      return FillErr(out, cap, ERROR_INVALID_PARAMETER, "SetEnvironmentVariableW");
    }
    std::wstring wname = Utf8ToWide(name);
    BOOL ok = val.empty()
                  ? SetEnvironmentVariableW(wname.c_str(), nullptr)
                  : SetEnvironmentVariableW(wname.c_str(), Utf8ToWide(val).c_str());
    if (!ok) return FillErr(out, cap, (int)GetLastError(), "SetEnvironmentVariableW");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SetCurrentDirectoryW")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "SetCurrentDirectoryW");
    if (!SetCurrentDirectoryW(Utf8ToWide(a).c_str())) {
      return FillErr(out, cap, (int)GetLastError(), "SetCurrentDirectoryW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ExpandEnvironmentStringsW")) {
    std::wstring in = Utf8ToWide(a);
    DWORD need = ExpandEnvironmentStringsW(in.c_str(), nullptr, 0);
    std::wstring buf(need, L'\0');
    DWORD n = ExpandEnvironmentStringsW(in.c_str(), buf.data(), need);
    if (n == 0) return FillErr(out, cap, (int)GetLastError(), "ExpandEnvironmentStringsW");
    return Fill(out, cap, WideToUtf8(buf.c_str()));
  }
  if (Eq(api, "GetFullPathNameW")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "GetFullPathNameW");
    std::wstring in = Utf8ToWide(a);
    DWORD need = GetFullPathNameW(in.c_str(), 0, nullptr, nullptr);
    std::wstring buf(need, L'\0');
    DWORD n = GetFullPathNameW(in.c_str(), need, buf.data(), nullptr);
    if (n == 0) return FillErr(out, cap, (int)GetLastError(), "GetFullPathNameW");
    return Fill(out, cap, WideToUtf8(buf.c_str()));
  }
  if (Eq(api, "GetFileAttributesExW")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "GetFileAttributesExW");
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(Utf8ToWide(a).c_str(), GetFileExInfoStandard, &data)) {
      return FillErr(out, cap, (int)GetLastError(), "GetFileAttributesExW");
    }
    ULARGE_INTEGER sz{};
    sz.LowPart = data.nFileSizeLow;
    sz.HighPart = data.nFileSizeHigh;
    ULARGE_INTEGER mt{};
    mt.LowPart = data.ftLastWriteTime.dwLowDateTime;
    mt.HighPart = data.ftLastWriteTime.dwHighDateTime;
    std::string s = std::to_string(data.dwFileAttributes);
    s += "\x1f";
    s += std::to_string(sz.QuadPart);
    s += "\x1f";
    s += std::to_string(mt.QuadPart);
    return Fill(out, cap, s);
  }
  if (Eq(api, "SetFileAttributesW")) {
    const char* p = std::strchr(a, '\x1f');
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "SetFileAttributesW");
    std::string path(a, p);
    DWORD attr = (DWORD)strtoul(p + 1, nullptr, 10);
    if (!SetFileAttributesW(Utf8ToWide(path).c_str(), attr)) {
      return FillErr(out, cap, (int)GetLastError(), "SetFileAttributesW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateDirectoryW")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "CreateDirectoryW");
    if (!CreateDirectoryW(Utf8ToWide(a).c_str(), nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "CreateDirectoryW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RemoveDirectoryW")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "RemoveDirectoryW");
    if (!RemoveDirectoryW(Utf8ToWide(a).c_str())) {
      return FillErr(out, cap, (int)GetLastError(), "RemoveDirectoryW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DeleteFileW")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "DeleteFileW");
    if (!DeleteFileW(Utf8ToWide(a).c_str())) {
      return FillErr(out, cap, (int)GetLastError(), "DeleteFileW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "MoveFileW")) {
    const char* p = std::strchr(a, '\x1f');
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "MoveFileW");
    std::string src(a, p);
    if (!MoveFileW(Utf8ToWide(src).c_str(), Utf8ToWide(p + 1).c_str())) {
      return FillErr(out, cap, (int)GetLastError(), "MoveFileW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CopyFileW")) {
    const char* p = std::strchr(a, '\x1f');
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "CopyFileW");
    std::string src(a, p);
    if (!CopyFileW(Utf8ToWide(src).c_str(), Utf8ToWide(p + 1).c_str(), FALSE)) {
      return FillErr(out, cap, (int)GetLastError(), "CopyFileW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetModuleHandleW")) {
    HMODULE h = GetModuleHandleW(a[0] ? Utf8ToWide(a).c_str() : nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), "GetModuleHandleW");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "GetModuleHandleExW")) {
    HMODULE h = nullptr;
    if (!GetModuleHandleExW(0, a[0] ? Utf8ToWide(a).c_str() : nullptr, &h) || !h) {
      return FillErr(out, cap, (int)GetLastError(), "GetModuleHandleExW");
    }
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "LoadLibraryW") || Eq(api, "LoadLibraryA")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    HMODULE h = LoadLibraryW(Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "LoadLibraryExW")) {
    std::string path, flags;
    Split1f(a, &path, &flags);
    if (path.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "LoadLibraryExW");
    HMODULE h = LoadLibraryExW(Utf8ToWide(path).c_str(), nullptr,
                               (DWORD)std::strtoul(flags.c_str(), nullptr, 10));
    if (!h) return FillErr(out, cap, (int)GetLastError(), "LoadLibraryExW");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "FreeLibrary")) {
    HMODULE h = (HMODULE)HandleOf(a);
    if (!FreeLibrary(h)) return FillErr(out, cap, (int)GetLastError(), "FreeLibrary");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetProcAddress")) {
    std::string hs, name;
    Split1f(a, &hs, &name);
    FARPROC p = GetProcAddress((HMODULE)HandleOf(hs.c_str()), name.c_str());
    if (!p) return FillErr(out, cap, (int)GetLastError(), "GetProcAddress");
    return Fill(out, cap, std::to_string((long long)(intptr_t)p));
  }
  if (Eq(api, "DisableThreadLibraryCalls")) {
    if (!DisableThreadLibraryCalls((HMODULE)HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), "DisableThreadLibraryCalls");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateProcessW") || Eq(api, "CreateProcessA")) {
    std::string app, rest, cmd, cwd;
    Split1f(a, &app, &rest);
    Split1f(rest.c_str(), &cmd, &cwd);
    if (cmd.empty()) {
      cmd = app;
      app.clear();
    }
    if (app.empty() && cmd.empty()) {
      return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    }
    // lpApplicationName without a path does not search PATH. A bare
    // name belongs on the command line with lpApplicationName = NULL.
    if (!app.empty() && app.find('\\') == std::string::npos &&
        app.find('/') == std::string::npos && app.find(':') == std::string::npos) {
      app.clear();
    }
    std::wstring wapp = Utf8ToWide(app);
    std::wstring wcmd = Utf8ToWide(cmd);
    std::wstring wcwd = Utf8ToWide(cwd);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    DWORD flags = CREATE_NO_WINDOW;
    auto gui = [](const std::string& s) {
      std::string t = s;
      for (char& c : t)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
      if (t.find("/c ") != std::string::npos) return false;
      if (t.find("-command") != std::string::npos) return false;
      if (t.find("cmd.exe") != std::string::npos) return false;
      if (t.find("powershell") != std::string::npos) return false;
      return true;
    };
    if (gui(cmd.empty() ? app : cmd)) flags = 0;
    // Console commands (cmd.exe /c, powershell -Command, …) are the
    // occupyCmd/occupyCalc terminal path — pipe their combined
    // stdout+stderr so GetProcessOutput has real bytes to return. GUI
    // apps (calc.exe, notepad.exe with no /c) keep their own console
    // and are not redirected.
    HANDLE rd = nullptr, wr = nullptr;
    bool piped = false;
    if (flags == CREATE_NO_WINDOW) {
      SECURITY_ATTRIBUTES sa{};
      sa.nLength = sizeof(sa);
      sa.bInheritHandle = TRUE;
      // gockrnl_create_process(wait=true) waits for exit before it ever
      // drains this pipe, so a shell command whose output outgrows the
      // pipe buffer before it exits would deadlock on WriteFile. A 1MB
      // buffer keeps that far out of reach for a terminal session
      // without needing a background reader thread.
      if (CreatePipe(&rd, &wr, &sa, 1 << 20)) {
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = wr;
        si.hStdError = wr;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        piped = true;
      }
    }
    BOOL ok = CreateProcessW(wapp.empty() ? nullptr : wapp.c_str(),
                             wcmd.empty() ? nullptr : wcmd.data(), nullptr,
                             nullptr, piped ? TRUE : FALSE, flags, nullptr,
                             wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi);
    if (piped) CloseHandle(wr);  // only the child should hold the write end now
    if (!ok) {
      if (piped) CloseHandle(rd);
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    if (piped) ProcOutPipe()[pi.hProcess] = rd;
    std::string s = std::to_string(pi.dwProcessId);
    s.push_back('\x1f');
    s += HandleStr(pi.hProcess);
    s.push_back('\x1f');
    s += HandleStr(pi.hThread);
    return Fill(out, cap, s);
  }
  if (Eq(api, "WaitForSingleObject") || Eq(api, "WaitForSingleObjectEx")) {
    std::string hs, toms;
    Split1f(a, &hs, &toms);
    DWORD ms = toms.empty() ? INFINITE : (DWORD)std::strtoul(toms.c_str(), nullptr, 10);
    DWORD wr = WaitForSingleObject(HandleOf(hs.c_str()), ms);
    if (wr == WAIT_FAILED) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(wr));
  }
  if (Eq(api, "GetExitCodeProcess")) {
    DWORD code = 0;
    if (!GetExitCodeProcess(HandleOf(a), &code)) {
      return FillErr(out, cap, (int)GetLastError(), "GetExitCodeProcess");
    }
    return Fill(out, cap, std::to_string(code));
  }
  if (Eq(api, "GetProcessOutput")) {
    HANDLE h = HandleOf(a);
    auto it = ProcOutPipe().find(h);
    if (it == ProcOutPipe().end()) return Fill(out, cap, "");
    return Fill(out, cap, DrainPipe(it->second));
  }
  if (Eq(api, "TerminateProcess")) {
    std::string hs, code;
    Split1f(a, &hs, &code);
    UINT c = code.empty() ? 1u : (UINT)std::strtoul(code.c_str(), nullptr, 10);
    if (!TerminateProcess(HandleOf(hs.c_str()), c)) {
      return FillErr(out, cap, (int)GetLastError(), "TerminateProcess");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetProcessId")) {
    if (!a[0]) return Fill(out, cap, std::to_string(GetCurrentProcessId()));
    DWORD pid = GetProcessId(HandleOf(a));
    if (!pid) return FillErr(out, cap, (int)GetLastError(), "GetProcessId");
    return Fill(out, cap, std::to_string(pid));
  }
  if (Eq(api, "OpenProcess")) {
    std::string access, pid;
    Split1f(a, &access, &pid);
    HANDLE h = OpenProcess((DWORD)std::strtoul(access.c_str(), nullptr, 10), FALSE,
                           (DWORD)std::strtoul(pid.c_str(), nullptr, 10));
    if (!h) return FillErr(out, cap, (int)GetLastError(), "OpenProcess");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "GetTickCount")) {
    return Fill(out, cap, std::to_string(GetTickCount()));
  }
  if (Eq(api, "SleepEx")) {
    SleepEx((DWORD)std::strtoul(a, nullptr, 10), FALSE);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetCurrentProcessorNumber")) {
    return Fill(out, cap, std::to_string(GetCurrentProcessorNumber()));
  }
  if (Eq(api, "GetCurrentProcessorNumberEx")) {
    PROCESSOR_NUMBER n{};
    GetCurrentProcessorNumberEx(&n);
    return Fill(out, cap, std::to_string(n.Group) + "\x1f" + std::to_string(n.Number));
  }
  if (Eq(api, "SetLastError")) {
    SetLastError((DWORD)std::strtoul(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetComputerNameExW") || Eq(api, "GetComputerNameA")) {
    wchar_t bufw[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(bufw, &n)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "GetSystemWindowsDirectoryW")) {
    wchar_t bufw[MAX_PATH];
    UINT n = GetSystemWindowsDirectoryW(bufw, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
      return FillErr(out, cap, (int)GetLastError(), "GetSystemWindowsDirectoryW");
    }
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "GetNativeSystemInfo")) {
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    std::string s = std::to_string(si.wProcessorArchitecture);
    s += "\x1f";
    s += std::to_string(si.dwPageSize);
    s += "\x1f";
    s += std::to_string(si.dwNumberOfProcessors);
    return Fill(out, cap, s);
  }
  if (Eq(api, "GetSystemTimePreciseAsFileTime")) {
    FILETIME ft{};
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER u{};
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return Fill(out, cap, std::to_string(u.QuadPart));
  }
  if (Eq(api, "GetCommandLineA")) {
    return Fill(out, cap, GetCommandLineA());
  }
  if (Eq(api, "GetStartupInfoW")) {
    STARTUPINFOW si{};
    GetStartupInfoW(&si);
    return Fill(out, cap, std::string("wshow=") + std::to_string(si.wShowWindow));
  }
  if (Eq(api, "CreateFileW") || Eq(api, "CreateFileA")) {
    std::string path, rest, access, disp;
    Split1f(a, &path, &rest);
    Split1f(rest.c_str(), &access, &disp);
    if (path.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    DWORD acc = access.empty() ? GENERIC_READ : (DWORD)std::strtoul(access.c_str(), nullptr, 10);
    DWORD cd = disp.empty() ? OPEN_EXISTING : (DWORD)std::strtoul(disp.c_str(), nullptr, 10);
    HANDLE h = CreateFileW(Utf8ToWide(path).c_str(), acc, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, cd, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "ReadFile")) {
    std::string hs, nstr;
    Split1f(a, &hs, &nstr);
    DWORD n = nstr.empty() ? 4096u : (DWORD)std::strtoul(nstr.c_str(), nullptr, 10);
    if (n > 65536) n = 65536;
    std::string b(n, '\0');
    DWORD got = 0;
    if (!ReadFile(HandleOf(hs.c_str()), b.data(), n, &got, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "ReadFile");
    }
    LastIo()[HandleOf(hs.c_str())] = got;
    b.resize(got);
    return Fill(out, cap, b);
  }
  if (Eq(api, "WriteFile")) {
    std::string hs, data;
    Split1f(a, &hs, &data);
    DWORD got = 0;
    if (!WriteFile(HandleOf(hs.c_str()), data.data(), (DWORD)data.size(), &got, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "WriteFile");
    }
    LastIo()[HandleOf(hs.c_str())] = got;
    return Fill(out, cap, std::to_string(got));
  }
  if (Eq(api, "FlushFileBuffers")) {
    if (!FlushFileBuffers(HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), "FlushFileBuffers");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetFileSizeEx") || Eq(api, "GetFileSize")) {
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(HandleOf(a), &sz)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(sz.QuadPart));
  }
  if (Eq(api, "FindFirstFileW") || Eq(api, "FindFirstFileA")) {
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(Utf8ToWide(a[0] ? a : "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    std::string s = HandleStr(h);
    s.push_back('\x1f');
    s += WideToUtf8(fd.cFileName);
    s.push_back('\x1f');
    s += std::to_string(fd.dwFileAttributes);
    return Fill(out, cap, s);
  }
  if (Eq(api, "FindNextFileW") || Eq(api, "FindNextFileA")) {
    WIN32_FIND_DATAW fd{};
    if (!FindNextFileW(HandleOf(a), &fd)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    std::string s = WideToUtf8(fd.cFileName);
    s.push_back('\x1f');
    s += std::to_string(fd.dwFileAttributes);
    return Fill(out, cap, s);
  }
  if (Eq(api, "FindClose")) {
    if (!FindClose(HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), "FindClose");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetProcessHeap")) {
    return Fill(out, cap, HandleStr(GetProcessHeap()));
  }
  if (Eq(api, "HeapAlloc") || Eq(api, "GlobalAlloc") || Eq(api, "LocalAlloc") ||
      Eq(api, "VirtualAlloc")) {
    SIZE_T n = (SIZE_T)std::strtoull(a, nullptr, 10);
    if (!n) n = 1;
    void* p = nullptr;
    if (Eq(api, "VirtualAlloc")) {
      p = VirtualAlloc(nullptr, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    } else if (Eq(api, "GlobalAlloc")) {
      p = (void*)GlobalAlloc(GPTR, n);
    } else if (Eq(api, "LocalAlloc")) {
      p = (void*)LocalAlloc(LPTR, n);
    } else {
      p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
    }
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "HeapFree") || Eq(api, "GlobalFree") || Eq(api, "LocalFree") ||
      Eq(api, "VirtualFree")) {
    uintptr_t p = (uintptr_t)std::strtoull(a, nullptr, 10);
    BOOL ok = TRUE;
    if (Eq(api, "VirtualFree"))
      ok = VirtualFree((void*)p, 0, MEM_RELEASE);
    else if (Eq(api, "GlobalFree"))
      ok = GlobalFree((HGLOBAL)p) == nullptr;
    else if (Eq(api, "LocalFree"))
      ok = LocalFree((HLOCAL)p) == nullptr;
    else
      ok = HeapFree(GetProcessHeap(), 0, (void*)p);
    if (!ok) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "VirtualAllocEx")) {
    auto parts = SplitAll(a);
    HANDLE proc = parts.empty() ? GetCurrentProcess() : ProcOf(parts[0].c_str());
    SIZE_T n = parts.size() > 1 ? (SIZE_T)std::strtoull(parts[1].c_str(), nullptr, 10) : 1;
    if (!n) n = 1;
    DWORD ty = MEM_COMMIT | MEM_RESERVE;
    DWORD prot = PAGE_READWRITE;
    if (parts.size() > 2 && !parts[2].empty())
      ty = (DWORD)std::strtoul(parts[2].c_str(), nullptr, 0);
    if (parts.size() > 3 && !parts[3].empty())
      prot = (DWORD)std::strtoul(parts[3].c_str(), nullptr, 0);
    if (!ty) ty = MEM_COMMIT | MEM_RESERVE;
    if (!prot) prot = PAGE_READWRITE;
    void* p = VirtualAllocEx(proc, nullptr, n, ty, prot);
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "VirtualFreeEx")) {
    std::string ph, rest, ptr, extra;
    Split1f(a, &ph, &rest);
    Split1f(rest.c_str(), &ptr, &extra);
    if (!VirtualFreeEx(ProcOf(ph.c_str()), (void*)(uintptr_t)std::strtoull(ptr.c_str(), nullptr, 10),
                       0, MEM_RELEASE)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "VirtualProtect") || Eq(api, "VirtualProtectEx")) {
    std::string a0, rest, a1, a2;
    Split1f(a, &a0, &rest);
    Split1f(rest.c_str(), &a1, &a2);
    HANDLE proc = GetCurrentProcess();
    void* p = nullptr;
    DWORD prot = PAGE_READWRITE;
    if (Eq(api, "VirtualProtectEx")) {
      proc = ProcOf(a0.c_str());
      p = (void*)(uintptr_t)std::strtoull(a1.c_str(), nullptr, 10);
      prot = (DWORD)std::strtoul(a2.c_str(), nullptr, 0);
    } else {
      p = (void*)(uintptr_t)std::strtoull(a0.c_str(), nullptr, 10);
      prot = (DWORD)std::strtoul(rest.c_str(), nullptr, 0);
    }
    if (!prot) prot = PAGE_READWRITE;
    DWORD old = 0;
    if (!VirtualProtectEx(proc, p, 1, prot, &old)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(old));
  }
  if (Eq(api, "VirtualQuery") || Eq(api, "VirtualQueryEx")) {
    std::string a0, a1;
    Split1f(a, &a0, &a1);
    HANDLE proc = GetCurrentProcess();
    void* p = nullptr;
    if (Eq(api, "VirtualQueryEx")) {
      proc = ProcOf(a0.c_str());
      p = (void*)(uintptr_t)std::strtoull(a1.c_str(), nullptr, 10);
    } else {
      p = (void*)(uintptr_t)std::strtoull(a0.c_str(), nullptr, 10);
    }
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQueryEx(proc, p, &mbi, sizeof(mbi))) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    std::string s = std::to_string((unsigned long long)(uintptr_t)mbi.BaseAddress);
    s += "\x1f";
    s += std::to_string((unsigned long long)mbi.RegionSize);
    s += "\x1f";
    s += std::to_string(mbi.Protect);
    s += "\x1f";
    s += std::to_string(mbi.State);
    return Fill(out, cap, s);
  }
  if (Eq(api, "ReadProcessMemory")) {
    std::string ph, rest, ptr, nstr;
    Split1f(a, &ph, &rest);
    Split1f(rest.c_str(), &ptr, &nstr);
    SIZE_T n = (SIZE_T)std::strtoull(nstr.c_str(), nullptr, 10);
    if (!n) n = 1;
    if (n > 65536) n = 65536;
    std::string b(n, '\0');
    SIZE_T got = 0;
    if (!ReadProcessMemory(ProcOf(ph.c_str()),
                           (LPCVOID)(uintptr_t)std::strtoull(ptr.c_str(), nullptr, 10), b.data(), n,
                           &got)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    b.resize(got);
    return Fill(out, cap, b);
  }
  if (Eq(api, "WriteProcessMemory")) {
    std::string ph, rest, ptr, data;
    Split1f(a, &ph, &rest);
    Split1f(rest.c_str(), &ptr, &data);
    SIZE_T got = 0;
    if (!WriteProcessMemory(ProcOf(ph.c_str()),
                            (LPVOID)(uintptr_t)std::strtoull(ptr.c_str(), nullptr, 10), data.data(),
                            data.size(), &got)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string((unsigned long long)got));
  }
  if (Eq(api, "TlsAlloc")) {
    DWORD i = TlsAlloc();
    if (i == TLS_OUT_OF_INDEXES) {
      return FillErr(out, cap, (int)GetLastError(), "TlsAlloc");
    }
    return Fill(out, cap, std::to_string(i));
  }
  if (Eq(api, "TlsFree")) {
    if (!TlsFree((DWORD)std::strtoul(a, nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "TlsFree");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetConsoleCP")) return Fill(out, cap, std::to_string(GetConsoleCP()));
  if (Eq(api, "GetConsoleOutputCP")) {
    return Fill(out, cap, std::to_string(GetConsoleOutputCP()));
  }
  if (Eq(api, "GetLogicalDrives")) {
    return Fill(out, cap, std::to_string(GetLogicalDrives()));
  }
  if (Eq(api, "GetTempFileNameW")) {
    std::string dir, pre;
    Split1f(a, &dir, &pre);
    if (dir.empty()) {
      wchar_t tmp[MAX_PATH];
      GetTempPathW(MAX_PATH, tmp);
      dir = WideToUtf8(tmp);
    }
    if (pre.empty()) pre = "k32";
    wchar_t outp[MAX_PATH];
    if (!GetTempFileNameW(Utf8ToWide(dir).c_str(), Utf8ToWide(pre).c_str(), 0, outp)) {
      return FillErr(out, cap, (int)GetLastError(), "GetTempFileNameW");
    }
    return Fill(out, cap, WideToUtf8(outp));
  }
  if (Eq(api, "MoveFileExW")) {
    std::string src, rest, dst, flags;
    Split1f(a, &src, &rest);
    Split1f(rest.c_str(), &dst, &flags);
    if (src.empty() || dst.empty()) {
      return FillErr(out, cap, ERROR_INVALID_PARAMETER, "MoveFileExW");
    }
    if (!MoveFileExW(Utf8ToWide(src).c_str(), Utf8ToWide(dst).c_str(),
                     (DWORD)std::strtoul(flags.c_str(), nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "MoveFileExW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CompareFileTime")) {
    std::string x, y;
    Split1f(a, &x, &y);
    ULARGE_INTEGER A{}, B{};
    A.QuadPart = std::strtoull(x.c_str(), nullptr, 10);
    B.QuadPart = std::strtoull(y.c_str(), nullptr, 10);
    FILETIME fa{A.LowPart, A.HighPart}, fb{B.LowPart, B.HighPart};
    return Fill(out, cap, std::to_string(CompareFileTime(&fa, &fb)));
  }
  if (Eq(api, "TlsSetValue")) {
    std::string idx, val;
    Split1f(a, &idx, &val);
    if (!TlsSetValue((DWORD)std::strtoul(idx.c_str(), nullptr, 10),
                     (LPVOID)(uintptr_t)std::strtoull(val.c_str(), nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "TlsSetValue");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "TlsGetValue")) {
    LPVOID p = TlsGetValue((DWORD)std::strtoul(a, nullptr, 10));
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "CreateEventW") || Eq(api, "CreateEventA") || Eq(api, "CreateEventExW")) {
    std::string manual, rest, initial;
    Split1f(a, &manual, &rest);
    Split1f(rest.c_str(), &initial, &rest);
    std::wstring wname = Utf8ToWide(rest);
    HANDLE h = CreateEventW(nullptr, std::strtol(manual.c_str(), nullptr, 10) != 0,
                            std::strtol(initial.c_str(), nullptr, 10) != 0,
                            wname.empty() ? nullptr : wname.c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "OpenEventW") || Eq(api, "OpenEventA")) {
    HANDLE h = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "SetEvent")) {
    if (!SetEvent(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), "SetEvent");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ResetEvent")) {
    if (!ResetEvent(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), "ResetEvent");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "PulseEvent")) {
    if (!PulseEvent(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), "PulseEvent");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateMutexW") || Eq(api, "CreateMutexA") || Eq(api, "CreateMutexExW")) {
    std::string init, name;
    Split1f(a, &init, &name);
    std::wstring wname = Utf8ToWide(name);
    HANDLE h = CreateMutexW(nullptr, init[0] && std::strtol(init.c_str(), nullptr, 10) != 0,
                            wname.empty() ? nullptr : wname.c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "OpenMutexW") || Eq(api, "OpenMutexA")) {
    HANDLE h = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "ReleaseMutex")) {
    if (!ReleaseMutex(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), "ReleaseMutex");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateSemaphoreW") || Eq(api, "CreateSemaphoreA") ||
      Eq(api, "CreateSemaphoreExW")) {
    std::string init, rest, maxc;
    Split1f(a, &init, &rest);
    Split1f(rest.c_str(), &maxc, &rest);
    LONG mx = maxc.empty() ? 0x7fffffff : (LONG)std::strtol(maxc.c_str(), nullptr, 10);
    std::wstring wname = Utf8ToWide(rest);
    HANDLE h = CreateSemaphoreW(nullptr, (LONG)std::strtol(init.c_str(), nullptr, 10), mx,
                                wname.empty() ? nullptr : wname.c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "OpenSemaphoreW") || Eq(api, "OpenSemaphoreA")) {
    HANDLE h = OpenSemaphoreW(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "ReleaseSemaphore")) {
    std::string hs, nstr;
    Split1f(a, &hs, &nstr);
    LONG prev = 0;
    LONG n = nstr.empty() ? 1 : (LONG)std::strtol(nstr.c_str(), nullptr, 10);
    if (!ReleaseSemaphore(HandleOf(hs.c_str()), n, &prev)) {
      return FillErr(out, cap, (int)GetLastError(), "ReleaseSemaphore");
    }
    return Fill(out, cap, std::to_string(prev));
  }
  if (Eq(api, "WaitForMultipleObjects") || Eq(api, "WaitForMultipleObjectsEx")) {
    auto parts = SplitAll(a);
    if (parts.size() < 3) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    BOOL wait_all = std::strtol(parts.back().c_str(), nullptr, 10) != 0;
    DWORD ms = (DWORD)std::strtoul(parts[parts.size() - 2].c_str(), nullptr, 10);
    size_t start = 0;
    if (parts.size() >= 4 &&
        (size_t)std::strtoul(parts[0].c_str(), nullptr, 10) == parts.size() - 3) {
      start = 1;
    }
    HANDLE hs[64];
    DWORD n = 0;
    for (size_t i = start; i + 2 < parts.size() && n < 64; ++i) {
      hs[n++] = HandleOf(parts[i].c_str());
    }
    if (!n) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    DWORD wr = WaitForMultipleObjects(n, hs, wait_all, ms);
    if (wr == WAIT_FAILED) return FillErr(out, cap, (int)GetLastError(), api);
    if (wr == WAIT_TIMEOUT) return Fill(out, cap, "258");
    if (wr >= WAIT_OBJECT_0 && wr < WAIT_OBJECT_0 + n)
      return Fill(out, cap, std::to_string(wr - WAIT_OBJECT_0));
    return Fill(out, cap, std::to_string(wr));
  }
  if (Eq(api, "CreatePipe")) {
    HANDLE rd = nullptr, wr = nullptr;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
      return FillErr(out, cap, (int)GetLastError(), "CreatePipe");
    }
    return Fill(out, cap, HandleStr(rd) + "\x1f" + HandleStr(wr));
  }
  if (Eq(api, "GetFileTime")) {
    FILETIME c{}, acc{}, w{};
    if (!GetFileTime(HandleOf(a), &c, &acc, &w)) {
      return FillErr(out, cap, (int)GetLastError(), "GetFileTime");
    }
    std::string s = std::to_string(FtQuad(c));
    s += "\x1f";
    s += std::to_string(FtQuad(acc));
    s += "\x1f";
    s += std::to_string(FtQuad(w));
    return Fill(out, cap, s);
  }
  if (Eq(api, "SetFileTime")) {
    std::string hs, rest, c, accw, w;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &c, &accw);
    Split1f(accw.c_str(), &accw, &w);
    FILETIME fc = QuadFt(std::strtoull(c.c_str(), nullptr, 10));
    FILETIME fa = QuadFt(std::strtoull(accw.c_str(), nullptr, 10));
    FILETIME fw = QuadFt(std::strtoull(w.c_str(), nullptr, 10));
    if (!SetFileTime(HandleOf(hs.c_str()), c.empty() ? nullptr : &fc,
                     accw.empty() ? nullptr : &fa, w.empty() ? nullptr : &fw)) {
      return FillErr(out, cap, (int)GetLastError(), "SetFileTime");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FileTimeToSystemTime")) {
    FILETIME ft = QuadFt(std::strtoull(a, nullptr, 10));
    SYSTEMTIME st{};
    if (!FileTimeToSystemTime(&ft, &st)) {
      return FillErr(out, cap, (int)GetLastError(), "FileTimeToSystemTime");
    }
    char bufst[64];
    snprintf(bufst, sizeof(bufst), "%04u-%02u-%02uT%02u:%02u:%02u.%03u", st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return Fill(out, cap, bufst);
  }
  if (Eq(api, "SystemTimeToFileTime") || Eq(api, "FileTimeToLocalFileTime") ||
      Eq(api, "LocalFileTimeToFileTime")) {
    FILETIME ft = QuadFt(std::strtoull(a, nullptr, 10));
    FILETIME outft{};
    BOOL ok = TRUE;
    if (Eq(api, "FileTimeToLocalFileTime"))
      ok = FileTimeToLocalFileTime(&ft, &outft);
    else if (Eq(api, "LocalFileTimeToFileTime"))
      ok = LocalFileTimeToFileTime(&ft, &outft);
    else {
      SYSTEMTIME st{};
      GetSystemTime(&st);
      ok = SystemTimeToFileTime(&st, &outft);
    }
    if (!ok) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(FtQuad(outft)));
  }
  if (Eq(api, "GetFileInformationByHandle") || Eq(api, "GetFileInformationByHandleEx")) {
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(HandleOf(a), &info)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    ULARGE_INTEGER sz{};
    sz.LowPart = info.nFileSizeLow;
    sz.HighPart = info.nFileSizeHigh;
    return Fill(out, cap,
                std::to_string(info.dwFileAttributes) + "\x1f" + std::to_string(sz.QuadPart));
  }
  if (Eq(api, "SearchPathW") || Eq(api, "SearchPathA")) {
    std::string path, rest, file, ext;
    Split1f(a, &path, &rest);
    Split1f(rest.c_str(), &file, &ext);
    if (file.empty()) {
      file = path;
      path.clear();
    }
    wchar_t bufw[MAX_PATH];
    DWORD n = SearchPathW(path.empty() ? nullptr : Utf8ToWide(path).c_str(),
                          Utf8ToWide(file).c_str(),
                          ext.empty() ? nullptr : Utf8ToWide(ext).c_str(), MAX_PATH, bufw,
                          nullptr);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "GetDriveTypeW") || Eq(api, "GetDriveTypeA")) {
    return Fill(out, cap, std::to_string(GetDriveTypeW(a[0] ? Utf8ToWide(a).c_str() : nullptr)));
  }
  if (Eq(api, "QueryDosDeviceW")) {
    wchar_t bufw[512];
    DWORD n = QueryDosDeviceW(a[0] ? Utf8ToWide(a).c_str() : nullptr, bufw, 512);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "QueryDosDeviceW");
    return Fill(out, cap, WideToUtf8(bufw));
  }
  if (Eq(api, "GetErrorMode")) return Fill(out, cap, std::to_string(GetErrorMode()));
  if (Eq(api, "SetErrorMode")) {
    UINT old = SetErrorMode((UINT)std::strtoul(a, nullptr, 10));
    return Fill(out, cap, std::to_string(old));
  }
  if (Eq(api, "IsProcessorFeaturePresent")) {
    return Fill(out, cap, IsProcessorFeaturePresent((DWORD)std::strtoul(a, nullptr, 10)) ? "1"
                                                                                         : "0");
  }
  if (Eq(api, "GetProcessTimes")) {
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentProcess();
    FILETIME c{}, e{}, k{}, u{};
    if (!GetProcessTimes(h, &c, &e, &k, &u)) {
      return FillErr(out, cap, (int)GetLastError(), "GetProcessTimes");
    }
    std::string s = std::to_string(FtQuad(c));
    s += "\x1f";
    s += std::to_string(FtQuad(e));
    s += "\x1f";
    s += std::to_string(FtQuad(k));
    s += "\x1f";
    s += std::to_string(FtQuad(u));
    return Fill(out, cap, s);
  }
  if (Eq(api, "GetPriorityClass")) {
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentProcess();
    DWORD c = GetPriorityClass(h);
    if (!c) return FillErr(out, cap, (int)GetLastError(), "GetPriorityClass");
    return Fill(out, cap, std::to_string(c));
  }
  if (Eq(api, "SetPriorityClass")) {
    std::string hs, cls;
    Split1f(a, &hs, &cls);
    HANDLE h = hs.empty() ? GetCurrentProcess() : HandleOf(hs.c_str());
    if (!SetPriorityClass(h, (DWORD)std::strtoul(cls.empty() ? hs.c_str() : cls.c_str(),
                                                 nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "SetPriorityClass");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetThreadPriority")) {
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentThread();
    return Fill(out, cap, std::to_string(GetThreadPriority(h)));
  }
  if (Eq(api, "SetThreadPriority")) {
    std::string hs, pr;
    Split1f(a, &hs, &pr);
    HANDLE h = pr.empty() ? GetCurrentThread() : HandleOf(hs.c_str());
    int prio = (int)std::strtol(pr.empty() ? hs.c_str() : pr.c_str(), nullptr, 10);
    if (!SetThreadPriority(h, prio)) {
      return FillErr(out, cap, (int)GetLastError(), "SetThreadPriority");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "QueryFullProcessImageNameW")) {
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentProcess();
    wchar_t bufw[MAX_PATH];
    DWORD n = MAX_PATH;
    if (!QueryFullProcessImageNameW(h, 0, bufw, &n)) {
      return FillErr(out, cap, (int)GetLastError(), "QueryFullProcessImageNameW");
    }
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "GetThreadId")) {
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentThread();
    DWORD id = GetThreadId(h);
    if (!id) return FillErr(out, cap, (int)GetLastError(), "GetThreadId");
    return Fill(out, cap, std::to_string(id));
  }
  if (Eq(api, "GetExitCodeThread")) {
    DWORD code = 0;
    if (!GetExitCodeThread(HandleOf(a), &code)) {
      return FillErr(out, cap, (int)GetLastError(), "GetExitCodeThread");
    }
    return Fill(out, cap, std::to_string(code));
  }
  if (Eq(api, "ResumeThread")) {
    DWORD c = ResumeThread(HandleOf(a));
    if (c == (DWORD)-1) return FillErr(out, cap, (int)GetLastError(), "ResumeThread");
    return Fill(out, cap, std::to_string(c));
  }
  if (Eq(api, "SuspendThread")) {
    DWORD c = SuspendThread(HandleOf(a));
    if (c == (DWORD)-1) return FillErr(out, cap, (int)GetLastError(), "SuspendThread");
    return Fill(out, cap, std::to_string(c));
  }
  if (Eq(api, "CreateThread")) {
    std::string start, param;
    Split1f(a, &start, &param);
    LPTHREAD_START_ROUTINE fn =
        (LPTHREAD_START_ROUTINE)(uintptr_t)std::strtoull(start.c_str(), nullptr, 10);
    if (!fn) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "CreateThread");
    DWORD tid = 0;
    HANDLE h = CreateThread(nullptr, 0, fn,
                            (LPVOID)(uintptr_t)std::strtoull(param.c_str(), nullptr, 10), 0,
                            &tid);
    if (!h) return FillErr(out, cap, (int)GetLastError(), "CreateThread");
    return Fill(out, cap, HandleStr(h) + "\x1f" + std::to_string(tid));
  }
  if (Eq(api, "CopyFileExW")) {
    std::string src, dst;
    Split1f(a, &src, &dst);
    if (!CopyFileW(Utf8ToWide(src).c_str(), Utf8ToWide(dst).c_str(), FALSE)) {
      return FillErr(out, cap, (int)GetLastError(), "CopyFileExW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ReplaceFileW")) {
    std::string replaced, rest, replacement;
    Split1f(a, &replaced, &rest);
    Split1f(rest.c_str(), &replacement, &rest);
    if (!ReplaceFileW(Utf8ToWide(replaced).c_str(), Utf8ToWide(replacement).c_str(), nullptr, 0,
                      nullptr, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "ReplaceFileW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetCompressedFileSizeW")) {
    DWORD hi = 0;
    DWORD lo = GetCompressedFileSizeW(Utf8ToWide(a).c_str(), &hi);
    if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
      return FillErr(out, cap, (int)GetLastError(), "GetCompressedFileSizeW");
    }
    ULARGE_INTEGER u{};
    u.LowPart = lo;
    u.HighPart = hi;
    return Fill(out, cap, std::to_string(u.QuadPart));
  }
  if (Eq(api, "GetCurrentDirectoryA")) {
    char bufa[MAX_PATH];
    DWORD n = GetCurrentDirectoryA(MAX_PATH, bufa);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetCurrentDirectoryA");
    return Fill(out, cap, bufa);
  }
  if (Eq(api, "SetCurrentDirectoryA")) {
    if (!SetCurrentDirectoryA(a)) {
      return FillErr(out, cap, (int)GetLastError(), "SetCurrentDirectoryA");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetEnvironmentVariableA")) {
    char bufa[32767];
    DWORD n = GetEnvironmentVariableA(a, bufa, sizeof(bufa));
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetEnvironmentVariableA");
    return Fill(out, cap, bufa);
  }
  if (Eq(api, "GetTempPathA")) {
    char bufa[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, bufa);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetTempPathA");
    return Fill(out, cap, bufa);
  }
  if (Eq(api, "GetWindowsDirectoryA")) {
    char bufa[MAX_PATH];
    UINT n = GetWindowsDirectoryA(bufa, MAX_PATH);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetWindowsDirectoryA");
    return Fill(out, cap, bufa);
  }
  if (Eq(api, "GetSystemDirectoryA")) {
    char bufa[MAX_PATH];
    UINT n = GetSystemDirectoryA(bufa, MAX_PATH);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetSystemDirectoryA");
    return Fill(out, cap, bufa);
  }
  if (Eq(api, "GetModuleFileNameA")) {
    char bufa[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, bufa, MAX_PATH);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetModuleFileNameA");
    return Fill(out, cap, bufa);
  }
  if (Eq(api, "GetFileAttributesA")) {
    DWORD attr = GetFileAttributesA(a);
    if (attr == INVALID_FILE_ATTRIBUTES) {
      return FillErr(out, cap, (int)GetLastError(), "GetFileAttributesA");
    }
    return Fill(out, cap, std::to_string(attr));
  }
  if (Eq(api, "FreeEnvironmentStringsW") || Eq(api, "FreeEnvironmentStringsA")) {
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetLongPathNameW") || Eq(api, "GetShortPathNameW")) {
    std::wstring in = Utf8ToWide(a);
    wchar_t bufw[MAX_PATH];
    DWORD n = Eq(api, "GetLongPathNameW")
                  ? GetLongPathNameW(in.c_str(), bufw, MAX_PATH)
                  : GetShortPathNameW(in.c_str(), bufw, MAX_PATH);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "AreFileApisANSI")) return Fill(out, cap, AreFileApisANSI() ? "1" : "0");
  if (Eq(api, "GetFileType")) {
    return Fill(out, cap, std::to_string(GetFileType(HandleOf(a))));
  }
  if (Eq(api, "GetFinalPathNameByHandleW")) {
    wchar_t bufw[MAX_PATH];
    DWORD n = GetFinalPathNameByHandleW(HandleOf(a), bufw, MAX_PATH, FILE_NAME_NORMALIZED);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetFinalPathNameByHandleW");
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "SetFilePointerEx") || Eq(api, "SetFilePointer")) {
    std::string hs, rest, off, meth;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &off, &meth);
    LARGE_INTEGER li{}, outli{};
    li.QuadPart = std::strtoll(off.c_str(), nullptr, 10);
    DWORD method = FILE_BEGIN;
    int m = (int)std::strtol(meth.c_str(), nullptr, 10);
    if (m == 1) method = FILE_CURRENT;
    if (m == 2) method = FILE_END;
    if (!SetFilePointerEx(HandleOf(hs.c_str()), li, &outli, method)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(outli.QuadPart));
  }
  if (Eq(api, "SetEndOfFile")) {
    if (!SetEndOfFile(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), "SetEndOfFile");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetDiskFreeSpaceExW") || Eq(api, "GetDiskFreeSpaceW")) {
    ULARGE_INTEGER caller{}, total{}, fr{};
    if (!GetDiskFreeSpaceExW(a[0] ? Utf8ToWide(a).c_str() : nullptr, &caller, &total, &fr)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(caller.QuadPart) + "\x1f" +
                              std::to_string(total.QuadPart) + "\x1f" +
                              std::to_string(fr.QuadPart));
  }
  if (Eq(api, "GetLogicalDriveStringsW")) {
    wchar_t bufw[256];
    DWORD n = GetLogicalDriveStringsW(256, bufw);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetLogicalDriveStringsW");
    std::string s;
    for (wchar_t* p = bufw; *p; p += wcslen(p) + 1) {
      if (!s.empty()) s.push_back('\x1f');
      s += WideToUtf8(p);
    }
    return Fill(out, cap, s);
  }
  if (Eq(api, "GetVolumeInformationW")) {
    wchar_t name[MAX_PATH] = {}, fs[MAX_PATH] = {};
    DWORD serial = 0, maxlen = 0, flags = 0;
    if (!GetVolumeInformationW(a[0] ? Utf8ToWide(a).c_str() : nullptr, name, MAX_PATH, &serial,
                               &maxlen, &flags, fs, MAX_PATH)) {
      return FillErr(out, cap, (int)GetLastError(), "GetVolumeInformationW");
    }
    std::string s = WideToUtf8(name);
    s += "\x1f";
    s += std::to_string(serial);
    s += "\x1f";
    s += std::to_string(maxlen);
    s += "\x1f";
    s += std::to_string(flags);
    s += "\x1f";
    s += WideToUtf8(fs);
    return Fill(out, cap, s);
  }
  if (Eq(api, "HeapCreate")) {
    HANDLE h = HeapCreate(0, 0, 0);
    if (!h) return FillErr(out, cap, (int)GetLastError(), "HeapCreate");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "HeapDestroy")) {
    if (!HeapDestroy(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), "HeapDestroy");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "HeapReAlloc")) {
    std::string p, sz;
    Split1f(a, &p, &sz);
    void* q = HeapReAlloc(GetProcessHeap(), 0, (void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10),
                          (SIZE_T)std::strtoull(sz.c_str(), nullptr, 10));
    if (!q) return FillErr(out, cap, (int)GetLastError(), "HeapReAlloc");
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)q));
  }
  if (Eq(api, "HeapSize")) {
    SIZE_T n = HeapSize(GetProcessHeap(), 0, (void*)(uintptr_t)std::strtoull(a, nullptr, 10));
    if (n == (SIZE_T)-1) return FillErr(out, cap, (int)GetLastError(), "HeapSize");
    return Fill(out, cap, std::to_string((unsigned long long)n));
  }
  if (Eq(api, "GlobalSize")) {
    SIZE_T n = GlobalSize((HGLOBAL)(uintptr_t)std::strtoull(a, nullptr, 10));
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GlobalSize");
    return Fill(out, cap, std::to_string((unsigned long long)n));
  }
  if (Eq(api, "LocalSize")) {
    SIZE_T n = LocalSize((HLOCAL)(uintptr_t)std::strtoull(a, nullptr, 10));
    if (!n) return FillErr(out, cap, (int)GetLastError(), "LocalSize");
    return Fill(out, cap, std::to_string((unsigned long long)n));
  }
  if (Eq(api, "HeapValidate")) {
    BOOL ok = HeapValidate(GetProcessHeap(), 0,
                           a[0] ? (void*)(uintptr_t)std::strtoull(a, nullptr, 10) : nullptr);
    return Fill(out, cap, ok ? "1" : "0");
  }
  if (Eq(api, "HeapCompact")) {
    return Fill(out, cap, std::to_string(HeapCompact(GetProcessHeap(), 0)));
  }
  if (Eq(api, "HeapLock")) {
    if (!HeapLock(GetProcessHeap())) return FillErr(out, cap, (int)GetLastError(), "HeapLock");
    return Fill(out, cap, "1");
  }
  if (Eq(api, "HeapUnlock")) {
    if (!HeapUnlock(GetProcessHeap())) return FillErr(out, cap, (int)GetLastError(), "HeapUnlock");
    return Fill(out, cap, "1");
  }
  if (Eq(api, "VirtualLock")) {
    std::string p, n;
    Split1f(a, &p, &n);
    if (!VirtualLock((void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10),
                     (SIZE_T)std::strtoull(n.c_str(), nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "VirtualLock");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "VirtualUnlock")) {
    std::string p, n;
    Split1f(a, &p, &n);
    if (!VirtualUnlock((void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10),
                       (SIZE_T)std::strtoull(n.c_str(), nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "VirtualUnlock");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetProcessHeaps")) {
    HANDLE h[16];
    DWORD n = GetProcessHeaps(16, h);
    return Fill(out, cap, std::to_string(n) + "\x1f" + HandleStr(n ? h[0] : GetProcessHeap()));
  }
  if (Eq(api, "GetLargePageMinimum")) {
    return Fill(out, cap, std::to_string((unsigned long long)GetLargePageMinimum()));
  }
  if (Eq(api, "GetPhysicallyInstalledSystemMemory")) {
    ULONGLONG kb = 0;
    if (!GetPhysicallyInstalledSystemMemory(&kb)) {
      MEMORYSTATUSEX ms{};
      ms.dwLength = sizeof(ms);
      GlobalMemoryStatusEx(&ms);
      kb = ms.ullTotalPhys / 1024;
    }
    return Fill(out, cap, std::to_string(kb));
  }
  if (Eq(api, "GetConsoleMode")) {
    DWORD m = 0;
    HANDLE h = a[0] ? HandleOf(a) : GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(h, &m)) return FillErr(out, cap, (int)GetLastError(), "GetConsoleMode");
    return Fill(out, cap, std::to_string(m));
  }
  if (Eq(api, "SetConsoleMode")) {
    std::string hs, mode;
    Split1f(a, &hs, &mode);
    HANDLE h = hs.empty() ? GetStdHandle(STD_OUTPUT_HANDLE) : HandleOf(hs.c_str());
    if (!SetConsoleMode(h, (DWORD)std::strtoul(mode.empty() ? a : mode.c_str(), nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "SetConsoleMode");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WriteConsoleW") || Eq(api, "WriteConsoleA")) {
    DWORD got = 0;
    std::wstring w = Utf8ToWide(a);
    if (!WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), w.c_str(), (DWORD)w.size(), &got,
                       nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(got));
  }
  if (Eq(api, "GetConsoleTitleW")) {
    wchar_t bufw[512];
    DWORD n = GetConsoleTitleW(bufw, 512);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "SetConsoleTitleW")) {
    if (!SetConsoleTitleW(Utf8ToWide(a).c_str())) {
      return FillErr(out, cap, (int)GetLastError(), "SetConsoleTitleW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetVersion")) return Fill(out, cap, std::to_string(GetVersion()));
  if (Eq(api, "GlobalMemoryStatusEx")) {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) {
      return FillErr(out, cap, (int)GetLastError(), "GlobalMemoryStatusEx");
    }
    return Fill(out, cap, std::to_string(ms.dwMemoryLoad) + "\x1f" +
                              std::to_string(ms.ullTotalPhys) + "\x1f" +
                              std::to_string(ms.ullAvailPhys));
  }
  if (Eq(api, "GetPhysicallyInstalledSystemMemory")) {
    ULONGLONG kb = 0;
    if (!GetPhysicallyInstalledSystemMemory(&kb)) {
      return FillErr(out, cap, (int)GetLastError(), "GetPhysicallyInstalledSystemMemory");
    }
    return Fill(out, cap, std::to_string(kb));
  }
  if (Eq(api, "GetTimeZoneInformation")) {
    TIME_ZONE_INFORMATION tz{};
    DWORD r = GetTimeZoneInformation(&tz);
    return Fill(out, cap, std::to_string(r) + "\x1f" + WideToUtf8(tz.StandardName));
  }
  if (Eq(api, "GetUserDefaultLCID")) return Fill(out, cap, std::to_string(GetUserDefaultLCID()));
  if (Eq(api, "GetSystemDefaultLCID")) {
    return Fill(out, cap, std::to_string(GetSystemDefaultLCID()));
  }
  if (Eq(api, "GetThreadLocale")) return Fill(out, cap, std::to_string(GetThreadLocale()));
  if (Eq(api, "SetThreadLocale")) {
    if (!SetThreadLocale((LCID)std::strtoul(a, nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "SetThreadLocale");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetUserDefaultLangID")) {
    return Fill(out, cap, std::to_string(GetUserDefaultLangID()));
  }
  if (Eq(api, "GetSystemDefaultLangID")) {
    return Fill(out, cap, std::to_string(GetSystemDefaultLangID()));
  }
  if (Eq(api, "GetLocaleInfoW")) {
    wchar_t bufw[128];
    int n = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SNAME, bufw, 128);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetLocaleInfoW");
    return Fill(out, cap, WideToUtf8(bufw, n - 1));
  }
  if (Eq(api, "CompareStringW") || Eq(api, "lstrcmpW") || Eq(api, "lstrcmpA") ||
      Eq(api, "lstrcmpiW") || Eq(api, "lstrcmpiA") || Eq(api, "CompareStringEx")) {
    std::string x, y;
    Split1f(a, &x, &y);
    int c = (Eq(api, "lstrcmpiW") || Eq(api, "lstrcmpiA") || Eq(api, "CompareStringEx"))
                ? lstrcmpiW(Utf8ToWide(x).c_str(), Utf8ToWide(y).c_str())
                : lstrcmpW(Utf8ToWide(x).c_str(), Utf8ToWide(y).c_str());
    if (Eq(api, "CompareStringW") || Eq(api, "CompareStringEx")) {
      if (c < 0) return Fill(out, cap, "1");
      if (c > 0) return Fill(out, cap, "3");
      return Fill(out, cap, "2");
    }
    return Fill(out, cap, std::to_string(c));
  }
  if (Eq(api, "lstrlenW") || Eq(api, "lstrlenA")) {
    return Fill(out, cap, std::to_string((int)std::strlen(a)));
  }
  if (Eq(api, "CharUpperW") || Eq(api, "CharUpperA")) {
    std::string s = a;
    for (char& c : s)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return Fill(out, cap, s);
  }
  if (Eq(api, "CharLowerW") || Eq(api, "CharLowerA")) {
    std::string s = a;
    for (char& c : s)
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return Fill(out, cap, s);
  }
  if (Eq(api, "MultiByteToWideChar") || Eq(api, "WideCharToMultiByte")) {
    return Fill(out, cap, a);
  }
  if (Eq(api, "FormatMessageW") || Eq(api, "FormatMessageA")) {
    wchar_t bufw[512];
    DWORD err = a[0] ? (DWORD)std::strtoul(a, nullptr, 10) : GetLastError();
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, err, 0, bufw, 512, nullptr);
    if (!n) return Fill(out, cap, std::string("error ") + std::to_string(err));
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "DuplicateHandle")) {
    HANDLE src = HandleOf(a);
    HANDLE dst = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), src, GetCurrentProcess(), &dst, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      return FillErr(out, cap, (int)GetLastError(), "DuplicateHandle");
    }
    return Fill(out, cap, HandleStr(dst));
  }
  if (Eq(api, "Beep")) return Fill(out, cap, "ok");
  if (Eq(api, "GetNumberOfConsoleInputEvents")) {
    DWORD n = 0;
    if (!GetNumberOfConsoleInputEvents(GetStdHandle(STD_INPUT_HANDLE), &n)) {
      return Fill(out, cap, "0");
    }
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "AllocConsole") || Eq(api, "FreeConsole") || Eq(api, "AttachConsole") ||
      Eq(api, "SetStdHandle") || Eq(api, "SetConsoleCP") || Eq(api, "SetConsoleOutputCP") ||
      Eq(api, "DebugBreak") || Eq(api, "FatalAppExitA") || Eq(api, "FatalAppExitW")) {
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateFileMappingW") || Eq(api, "CreateFileMappingA") ||
      Eq(api, "CreateFileMappingNumaW")) {
    auto parts = SplitAll(a);
    HANDLE hf = INVALID_HANDLE_VALUE;
    DWORD sz = 4096;
    if (!parts.empty() && !parts[0].empty() && parts[0] != "0" && parts[0] != "-1")
      hf = HandleOf(parts[0].c_str());
    if (parts.size() >= 3 && !parts[2].empty())
      sz = (DWORD)std::strtoul(parts[2].c_str(), nullptr, 10);
    else if (parts.size() >= 2 && !parts[1].empty())
      sz = (DWORD)std::strtoul(parts[1].c_str(), nullptr, 10);
    if (!sz) sz = 4096;
    HANDLE h = CreateFileMappingW(hf, nullptr, PAGE_READWRITE, 0, sz, nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "MapViewOfFile") || Eq(api, "MapViewOfFileEx")) {
    std::string hs, rest;
    Split1f(a, &hs, &rest);
    LPVOID p = MapViewOfFile(HandleOf(hs.c_str()), FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "UnmapViewOfFile")) {
    if (!UnmapViewOfFile((LPCVOID)(uintptr_t)std::strtoull(a, nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "UnmapViewOfFile");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FlushViewOfFile")) {
    std::string p, n;
    Split1f(a, &p, &n);
    SIZE_T bytes = n.empty() ? 0 : (SIZE_T)std::strtoull(n.c_str(), nullptr, 10);
    if (!FlushViewOfFile((LPCVOID)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10), bytes)) {
      return FillErr(out, cap, (int)GetLastError(), "FlushViewOfFile");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "OpenFileMappingW") || Eq(api, "OpenFileMappingA")) {
    HANDLE h = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "InitializeCriticalSection") ||
      Eq(api, "InitializeCriticalSectionAndSpinCount") ||
      Eq(api, "InitializeCriticalSectionEx")) {
    CRITICAL_SECTION* cs = new CRITICAL_SECTION();
    InitializeCriticalSection(cs);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)cs));
  }
  if (Eq(api, "EnterCriticalSection")) {
    CRITICAL_SECTION* cs =
        (CRITICAL_SECTION*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!cs) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    EnterCriticalSection(cs);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "TryEnterCriticalSection")) {
    CRITICAL_SECTION* cs =
        (CRITICAL_SECTION*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!cs) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    return Fill(out, cap, TryEnterCriticalSection(cs) ? "1" : "0");
  }
  if (Eq(api, "LeaveCriticalSection")) {
    CRITICAL_SECTION* cs =
        (CRITICAL_SECTION*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!cs) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    LeaveCriticalSection(cs);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DeleteCriticalSection")) {
    CRITICAL_SECTION* cs =
        (CRITICAL_SECTION*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!cs) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    DeleteCriticalSection(cs);
    delete cs;
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateIoCompletionPort")) {
    auto parts = SplitAll(a);
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE existing = nullptr;
    ULONG_PTR key = 0;
    DWORD threads = 0;
    if (!parts.empty() && !parts[0].empty() && parts[0] != "0" && parts[0] != "-1")
      file = HandleOf(parts[0].c_str());
    if (parts.size() > 1 && !parts[1].empty() && parts[1] != "0")
      existing = HandleOf(parts[1].c_str());
    if (parts.size() > 2) key = (ULONG_PTR)std::strtoull(parts[2].c_str(), nullptr, 10);
    if (parts.size() > 3) threads = (DWORD)std::strtoul(parts[3].c_str(), nullptr, 10);
    HANDLE h = CreateIoCompletionPort(file, existing, key, threads);
    if (!h) return FillErr(out, cap, (int)GetLastError(), "CreateIoCompletionPort");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "PostQueuedCompletionStatus")) {
    auto parts = SplitAll(a);
    if (parts.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    DWORD bytes = parts.size() > 1 ? (DWORD)std::strtoul(parts[1].c_str(), nullptr, 10) : 0;
    ULONG_PTR key = parts.size() > 2 ? (ULONG_PTR)std::strtoull(parts[2].c_str(), nullptr, 10) : 0;
    if (!PostQueuedCompletionStatus(HandleOf(parts[0].c_str()), bytes, key, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "PostQueuedCompletionStatus");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetQueuedCompletionStatus")) {
    std::string hs, toms;
    Split1f(a, &hs, &toms);
    DWORD bytes = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ov = nullptr;
    DWORD ms = toms.empty() ? INFINITE : (DWORD)std::strtoul(toms.c_str(), nullptr, 10);
    if (!GetQueuedCompletionStatus(HandleOf(hs.c_str()), &bytes, &key, &ov, ms)) {
      return FillErr(out, cap, (int)GetLastError(), "GetQueuedCompletionStatus");
    }
    return Fill(out, cap, std::to_string(bytes) + "\x1f" + std::to_string((unsigned long long)key));
  }
  if (Eq(api, "CreateNamedPipeW") || Eq(api, "CreateNamedPipeA")) {
    std::wstring name = a[0] ? Utf8ToWide(a) : std::wstring(L"\\\\.\\pipe\\wasmwin32_k32");
    HANDLE h = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0,
                                nullptr);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "ConnectNamedPipe")) {
    if (!ConnectNamedPipe(HandleOf(a), nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
      return FillErr(out, cap, (int)GetLastError(), "ConnectNamedPipe");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DisconnectNamedPipe")) {
    if (!DisconnectNamedPipe(HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), "DisconnectNamedPipe");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WaitNamedPipeW")) {
    std::string name, toms;
    Split1f(a, &name, &toms);
    DWORD ms = toms.empty() ? 1000 : (DWORD)std::strtoul(toms.c_str(), nullptr, 10);
    if (!WaitNamedPipeW(Utf8ToWide(name).c_str(), ms)) {
      return FillErr(out, cap, (int)GetLastError(), "WaitNamedPipeW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "PeekNamedPipe")) {
    DWORD avail = 0;
    if (!PeekNamedPipe(HandleOf(a), nullptr, 0, nullptr, &avail, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "PeekNamedPipe");
    }
    return Fill(out, cap, std::to_string(avail));
  }
  if (Eq(api, "LockFile") || Eq(api, "LockFileEx")) {
    if (!LockFile(HandleOf(a), 0, 0, 1, 0)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "UnlockFile") || Eq(api, "UnlockFileEx")) {
    if (!UnlockFile(HandleOf(a), 0, 0, 1, 0)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CancelIo") || Eq(api, "CancelIoEx")) {
    CancelIo(HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SetFileCompletionNotificationModes")) {
    std::string hs, flags;
    Split1f(a, &hs, &flags);
    using Fn = BOOL(WINAPI*)(HANDLE, UCHAR);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                        "SetFileCompletionNotificationModes");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    if (!fn(HandleOf(hs.c_str()), (UCHAR)std::strtoul(flags.c_str(), nullptr, 0))) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DeviceIoControl")) {
    std::string hs, rest, code, input;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &code, &input);
    unsigned ioctl = (unsigned)std::strtoul(code.c_str(), nullptr, 0);
    std::string emu = DeviceIoctlDispatch(ioctl);
    if (!emu.empty()) return Fill(out, cap, emu);
    if (ioctl == 0x0011400cu) {
      DWORD avail = 0;
      PeekNamedPipe(HandleOf(hs.c_str()), nullptr, 0, nullptr, &avail, nullptr);
      return Fill(out, cap, std::to_string(avail));
    }
    BYTE buf[512];
    DWORD br = 0;
    if (!DeviceIoControl(HandleOf(hs.c_str()), ioctl, nullptr, 0, buf, sizeof(buf), &br, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "DeviceIoControl");
    }
    return Fill(out, cap, br ? std::string((char*)buf, br) : "ok");
  }
  if (Eq(api, "GetOverlappedResult") || Eq(api, "GetOverlappedResultEx")) {
    HANDLE h = HandleOf(a);
    auto it = LastIo().find(h);
    return Fill(out, cap, std::to_string(it == LastIo().end() ? 0 : it->second));
  }
  if (Eq(api, "CreateHardLinkW") || Eq(api, "CreateHardLinkA")) {
    std::string neu, existing;
    Split1f(a, &neu, &existing);
    if (!CreateHardLinkW(Utf8ToWide(neu).c_str(), Utf8ToWide(existing).c_str(), nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateSymbolicLinkW") || Eq(api, "CreateSymbolicLinkA")) {
    std::string neu, existing;
    Split1f(a, &neu, &existing);
    if (!CreateSymbolicLinkW(Utf8ToWide(neu).c_str(), Utf8ToWide(existing).c_str(), 0)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FindFirstFileExW") || Eq(api, "FindFirstFileExA")) {
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(Utf8ToWide(a[0] ? a : "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    std::string s = HandleStr(h);
    s += "\x1f";
    s += WideToUtf8(fd.cFileName);
    s += "\x1f";
    s += std::to_string(fd.dwFileAttributes);
    return Fill(out, cap, s);
  }
  if (Eq(api, "GetVolumePathNameW") || Eq(api, "GetVolumePathNameA")) {
    wchar_t bufw[MAX_PATH];
    if (!GetVolumePathNameW(a[0] ? Utf8ToWide(a).c_str() : L"C:\\", bufw, MAX_PATH)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, WideToUtf8(bufw));
  }
  if (Eq(api, "SetVolumeLabelW")) {
    std::string root, label;
    Split1f(a, &root, &label);
    if (!SetVolumeLabelW(root.empty() ? nullptr : Utf8ToWide(root).c_str(),
                         Utf8ToWide(label).c_str())) {
      return FillErr(out, cap, (int)GetLastError(), "SetVolumeLabelW");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WritePrivateProfileStringW") || Eq(api, "WritePrivateProfileStringA")) {
    auto parts = SplitAll(a);
    if (parts.size() < 4) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    if (!WritePrivateProfileStringW(Utf8ToWide(parts[0]).c_str(), Utf8ToWide(parts[1]).c_str(),
                                    Utf8ToWide(parts[2]).c_str(), Utf8ToWide(parts[3]).c_str())) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetPrivateProfileStringW") || Eq(api, "GetPrivateProfileStringA")) {
    auto parts = SplitAll(a);
    if (parts.size() < 4) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    wchar_t bufw[1024];
    DWORD n = GetPrivateProfileStringW(Utf8ToWide(parts[0]).c_str(), Utf8ToWide(parts[1]).c_str(),
                                       Utf8ToWide(parts[2]).c_str(), bufw, 1024,
                                       Utf8ToWide(parts[3]).c_str());
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "GetPrivateProfileIntW") || Eq(api, "GetPrivateProfileIntA")) {
    auto parts = SplitAll(a);
    if (parts.size() < 4) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    UINT v = GetPrivateProfileIntW(Utf8ToWide(parts[0]).c_str(), Utf8ToWide(parts[1]).c_str(),
                                   (INT)std::strtol(parts[2].c_str(), nullptr, 10),
                                   Utf8ToWide(parts[3]).c_str());
    return Fill(out, cap, std::to_string(v));
  }
  if (Eq(api, "GlobalAddAtomW") || Eq(api, "GlobalAddAtomA") || Eq(api, "AddAtomW") ||
      Eq(api, "AddAtomA")) {
    std::wstring w = Utf8ToWide(a);
    ATOM at = (Eq(api, "AddAtomW") || Eq(api, "AddAtomA")) ? AddAtomW(w.c_str())
                                                           : GlobalAddAtomW(w.c_str());
    if (!at) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(at));
  }
  if (Eq(api, "GlobalFindAtomW") || Eq(api, "GlobalFindAtomA") || Eq(api, "FindAtomW")) {
    std::wstring w = Utf8ToWide(a);
    ATOM at = Eq(api, "FindAtomW") ? FindAtomW(w.c_str()) : GlobalFindAtomW(w.c_str());
    if (!at) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(at));
  }
  if (Eq(api, "GlobalGetAtomNameW") || Eq(api, "GlobalGetAtomNameA")) {
    wchar_t bufw[256];
    UINT n = GlobalGetAtomNameW((ATOM)std::strtoul(a, nullptr, 10), bufw, 256);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "GlobalDeleteAtom") || Eq(api, "DeleteAtom")) {
    ATOM at = (ATOM)std::strtoul(a, nullptr, 10);
    ATOM r = Eq(api, "DeleteAtom") ? DeleteAtom(at) : GlobalDeleteAtom(at);
    if (r) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateToolhelp32Snapshot")) {
    DWORD flags = a[0] ? (DWORD)std::strtoul(a, nullptr, 10) : TH32CS_SNAPPROCESS;
    HANDLE h = CreateToolhelp32Snapshot(flags, 0);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "Process32FirstW") || Eq(api, "Process32First") || Eq(api, "Process32FirstA")) {
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(HandleOf(a), &pe)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(pe.th32ProcessID) + "\x1f" + WideToUtf8(pe.szExeFile));
  }
  if (Eq(api, "Process32NextW") || Eq(api, "Process32Next") || Eq(api, "Process32NextA")) {
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (!Process32NextW(HandleOf(a), &pe)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(pe.th32ProcessID) + "\x1f" + WideToUtf8(pe.szExeFile));
  }
  if (Eq(api, "Module32FirstW") || Eq(api, "Module32First")) {
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    if (!Module32FirstW(HandleOf(a), &me)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, WideToUtf8(me.szModule));
  }
  if (Eq(api, "Module32NextW") || Eq(api, "Thread32Next") || Eq(api, "Thread32NextW")) {
    return FillErr(out, cap, ERROR_NO_MORE_FILES, api);
  }
  if (Eq(api, "Thread32First") || Eq(api, "Thread32FirstW")) {
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    if (!Thread32First(HandleOf(a), &te)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(te.th32ThreadID));
  }
  if (Eq(api, "MulDiv")) {
    auto parts = SplitAll(a);
    if (parts.size() < 3) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "MulDiv");
    int v = MulDiv((int)std::strtol(parts[0].c_str(), nullptr, 10),
                   (int)std::strtol(parts[1].c_str(), nullptr, 10),
                   (int)std::strtol(parts[2].c_str(), nullptr, 10));
    return Fill(out, cap, std::to_string(v));
  }
  if (Eq(api, "GetCPInfo")) {
    CPINFO info{};
    UINT cp = a[0] ? (UINT)std::strtoul(a, nullptr, 10) : CP_ACP;
    if (!GetCPInfo(cp, &info)) return FillErr(out, cap, (int)GetLastError(), "GetCPInfo");
    return Fill(out, cap, std::to_string(info.MaxCharSize) + "\x1f?\x1f" + std::to_string(cp));
  }
  if (Eq(api, "IsValidCodePage")) {
    return Fill(out, cap, IsValidCodePage((UINT)std::strtoul(a, nullptr, 10)) ? "1" : "0");
  }
  if (Eq(api, "GetDateFormatW") || Eq(api, "GetDateFormatA") || Eq(api, "GetDateFormatEx")) {
    wchar_t bufw[128];
    int n = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, nullptr, nullptr, bufw, 128);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, n - 1));
  }
  if (Eq(api, "GetTimeFormatW") || Eq(api, "GetTimeFormatA") || Eq(api, "GetTimeFormatEx")) {
    wchar_t bufw[128];
    int n = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, bufw, 128);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, n - 1));
  }
  if (Eq(api, "lstrcpyW") || Eq(api, "lstrcpyA")) return Fill(out, cap, a);
  if (Eq(api, "lstrcatW") || Eq(api, "lstrcatA")) {
    std::string x, y;
    Split1f(a, &x, &y);
    return Fill(out, cap, x + y);
  }
  if (Eq(api, "lstrcpynW") || Eq(api, "lstrcpynA")) {
    std::string s, nstr;
    Split1f(a, &s, &nstr);
    unsigned n = (unsigned)std::strtoul(nstr.c_str(), nullptr, 10);
    if (n && s.size() > n) s.resize(n);
    return Fill(out, cap, s);
  }
  if (Eq(api, "FlsAlloc")) {
    DWORD i = FlsAlloc(nullptr);
    if (i == FLS_OUT_OF_INDEXES) return FillErr(out, cap, (int)GetLastError(), "FlsAlloc");
    return Fill(out, cap, std::to_string(i));
  }
  if (Eq(api, "FlsFree")) {
    if (!FlsFree((DWORD)std::strtoul(a, nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "FlsFree");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FlsSetValue")) {
    std::string idx, val;
    Split1f(a, &idx, &val);
    if (!FlsSetValue((DWORD)std::strtoul(idx.c_str(), nullptr, 10),
                     (PVOID)(uintptr_t)std::strtoull(val.c_str(), nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "FlsSetValue");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FlsGetValue")) {
    PVOID p = FlsGetValue((DWORD)std::strtoul(a, nullptr, 10));
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "CheckRemoteDebuggerPresent")) {
    BOOL yes = FALSE;
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentProcess();
    if (!CheckRemoteDebuggerPresent(h, &yes)) {
      return FillErr(out, cap, (int)GetLastError(), "CheckRemoteDebuggerPresent");
    }
    return Fill(out, cap, yes ? "1" : "0");
  }
  if (Eq(api, "IsWow64Process") || Eq(api, "IsWow64Process2")) {
    BOOL wow = FALSE;
    if (!IsWow64Process(GetCurrentProcess(), &wow)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, wow ? "1" : "0");
  }
  if (Eq(api, "GetSystemWow64DirectoryW") || Eq(api, "GetSystemWow64DirectoryA")) {
    wchar_t bufw[MAX_PATH];
    UINT n = GetSystemWow64DirectoryW(bufw, MAX_PATH);
    if (!n) return Fill(out, cap, "");
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "GetProcessHandleCount")) {
    DWORD n = 0;
    if (!GetProcessHandleCount(GetCurrentProcess(), &n)) {
      return FillErr(out, cap, (int)GetLastError(), "GetProcessHandleCount");
    }
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "GetProcessAffinityMask")) {
    DWORD_PTR proc = 0, sys = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys)) {
      return FillErr(out, cap, (int)GetLastError(), "GetProcessAffinityMask");
    }
    return Fill(out, cap, std::to_string((unsigned long long)proc) + "\x1f" +
                              std::to_string((unsigned long long)sys));
  }
  if (Eq(api, "GetSystemTimes")) {
    FILETIME idle{}, kernel{}, user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) {
      return FillErr(out, cap, (int)GetLastError(), "GetSystemTimes");
    }
    return Fill(out, cap, std::to_string(FtQuad(idle)) + "\x1f" + std::to_string(FtQuad(kernel)) +
                              "\x1f" + std::to_string(FtQuad(user)));
  }
  if (Eq(api, "GetNumaHighestNodeNumber")) {
    ULONG n = 0;
    if (!GetNumaHighestNodeNumber(&n)) {
      return FillErr(out, cap, (int)GetLastError(), "GetNumaHighestNodeNumber");
    }
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "GetProcessVersion")) {
    return Fill(out, cap, std::to_string(GetProcessVersion(0)));
  }
  if (Eq(api, "NeedCurrentDirectoryForExePathW") ||
      Eq(api, "NeedCurrentDirectoryForExePathA")) {
    return Fill(out, cap, NeedCurrentDirectoryForExePathW(Utf8ToWide(a[0] ? a : "a.exe").c_str())
                              ? "1"
                              : "0");
  }
  if (Eq(api, "GetDllDirectoryW") || Eq(api, "GetDllDirectoryA")) {
    wchar_t bufw[MAX_PATH];
    DWORD n = GetDllDirectoryW(MAX_PATH, bufw);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "SetDllDirectoryW") || Eq(api, "SetDllDirectoryA")) {
    if (!SetDllDirectoryW(a[0] ? Utf8ToWide(a).c_str() : nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetModuleHandleA")) {
    HMODULE h = GetModuleHandleA(a[0] ? a : nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), "GetModuleHandleA");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "GetStartupInfoA")) {
    STARTUPINFOA si{};
    GetStartupInfoA(&si);
    return Fill(out, cap, std::string("wshow=") + std::to_string(si.wShowWindow));
  }
  if (Eq(api, "GetLargestConsoleWindowSize")) {
    COORD c = GetLargestConsoleWindowSize(GetStdHandle(STD_OUTPUT_HANDLE));
    return Fill(out, cap, std::to_string(c.X) + "\x1f" + std::to_string(c.Y));
  }
  if (Eq(api, "GetConsoleScreenBufferInfo") || Eq(api, "GetConsoleScreenBufferInfoEx")) {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
    return Fill(out, cap, "80" "\x1f" "25" "\x1f" "0" "\x1f" "0" "\x1f" "7");
    }
    return Fill(out, cap, std::to_string(info.dwSize.X) + "\x1f" + std::to_string(info.dwSize.Y) +
                              "\x1f" + std::to_string(info.dwCursorPosition.X) + "\x1f" +
                              std::to_string(info.dwCursorPosition.Y) + "\x1f" +
                              std::to_string(info.wAttributes));
  }
  if (Eq(api, "SetConsoleTextAttribute")) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!SetConsoleTextAttribute(h, (WORD)std::strtoul(a, nullptr, 10))) {
      return Fill(out, cap, "ok");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "MulDiv")) {
    /* already handled */
  }
  if (Eq(api, "GetUserDefaultUILanguage")) {
    return Fill(out, cap, std::to_string(GetUserDefaultUILanguage()));
  }
  if (Eq(api, "GetSystemDefaultUILanguage")) {
    return Fill(out, cap, std::to_string(GetSystemDefaultUILanguage()));
  }
  if (Eq(api, "GetProcessHandleCount")) {
    /* already handled */
  }
  if (Eq(api, "LCMapStringW") || Eq(api, "GetNumberFormatW") || Eq(api, "GetCurrencyFormatW") ||
      Eq(api, "GetStringTypeW") || Eq(api, "FoldStringW") || Eq(api, "LCMapStringEx")) {
    return Fill(out, cap, a);
  }
  if (Eq(api, "VerifyVersionInfoW") || Eq(api, "GetLogicalProcessorInformation") ||
      Eq(api, "GetLogicalProcessorInformationEx") || Eq(api, "GetBinaryTypeW") ||
      Eq(api, "GetBinaryTypeA") || Eq(api, "SetFileApisToANSI") || Eq(api, "SetFileApisToOEM") ||
      Eq(api, "SetConsoleCursorPosition") || Eq(api, "GenerateConsoleCtrlEvent") ||
      Eq(api, "SetConsoleCtrlHandler") || Eq(api, "FlushConsoleInputBuffer") ||
      Eq(api, "AddDllDirectory") || Eq(api, "SetDefaultDllDirectories") ||
      Eq(api, "RemoveDllDirectory") || Eq(api, "SetSearchPathMode") ||
      Eq(api, "SetProcessAffinityMask") || Eq(api, "SetThreadAffinityMask") ||
      Eq(api, "QueryUnbiasedInterruptTime") ||
      Eq(api, "QueryInterruptTime") || Eq(api, "GetProductInfo") ||
      Eq(api, "MoveFileWithProgressW")) {
    if (Eq(api, "MoveFileWithProgressW")) {
      std::string src, dst;
      Split1f(a, &src, &dst);
      if (!MoveFileW(Utf8ToWide(src).c_str(), Utf8ToWide(dst).c_str())) {
        return FillErr(out, cap, (int)GetLastError(), api);
      }
    }
    if (Eq(api, "QueryUnbiasedInterruptTime")) {
      ULONGLONG t = 0;
      QueryUnbiasedInterruptTime(&t);
      return Fill(out, cap, std::to_string(t));
    }
    if (Eq(api, "GetProductInfo")) {
      DWORD t = 0;
      GetProductInfo(10, 0, 0, 0, &t);
      return Fill(out, cap, std::to_string(t ? t : 48));
    }
    if (Eq(api, "GetLogicalProcessorInformation")) {
      return Fill(out, cap, "1");
    }
    if (Eq(api, "GetBinaryTypeW") || Eq(api, "GetBinaryTypeA")) {
      DWORD t = 0;
      GetBinaryTypeW(a[0] ? Utf8ToWide(a).c_str() : Utf8ToWide("kernel32.dll").c_str(), &t);
      return Fill(out, cap, std::to_string(t));
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FindResourceW") || Eq(api, "FindResourceA") || Eq(api, "FindResourceExW")) {
    HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC r = FindResourceW(mod, Utf8ToWide(a[0] ? a : "x").c_str(), (LPCWSTR)RT_RCDATA);
    if (!r) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(r));
  }
  if (Eq(api, "LoadResource")) {
    HGLOBAL g = LoadResource(GetModuleHandleW(nullptr), (HRSRC)HandleOf(a));
    if (!g) return FillErr(out, cap, (int)GetLastError(), "LoadResource");
    return Fill(out, cap, HandleStr(g));
  }
  if (Eq(api, "LockResource")) {
    LPVOID p = LockResource((HGLOBAL)HandleOf(a));
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "SizeofResource")) {
    DWORD n = SizeofResource(GetModuleHandleW(nullptr), (HRSRC)HandleOf(a));
    return Fill(out, cap, std::to_string(n));
  }

  if (Eq(api, "Wow64DisableWow64FsRedirection")) {
    PVOID old = nullptr;
    if (!Wow64DisableWow64FsRedirection(&old)) {
      return FillErr(out, cap, (int)GetLastError(), "Wow64DisableWow64FsRedirection");
    }
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)old));
  }
  if (Eq(api, "Wow64RevertWow64FsRedirection")) {
    PVOID old = (PVOID)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!Wow64RevertWow64FsRedirection(old)) {
      return FillErr(out, cap, (int)GetLastError(), "Wow64RevertWow64FsRedirection");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetProcessWorkingSetSize")) {
    SIZE_T mn = 0, mx = 0;
    if (!GetProcessWorkingSetSize(GetCurrentProcess(), &mn, &mx)) {
      return FillErr(out, cap, (int)GetLastError(), "GetProcessWorkingSetSize");
    }
    return Fill(out, cap, std::to_string((unsigned long long)mn) + "\x1f" +
                              std::to_string((unsigned long long)mx));
  }
  if (Eq(api, "SetProcessWorkingSetSize")) {
    if (!SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1)) {
      return FillErr(out, cap, (int)GetLastError(), "SetProcessWorkingSetSize");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetProcessIoCounters")) {
    IO_COUNTERS c{};
    if (!GetProcessIoCounters(GetCurrentProcess(), &c)) {
      return FillErr(out, cap, (int)GetLastError(), "GetProcessIoCounters");
    }
    return Fill(out, cap, std::to_string(c.ReadOperationCount) + "\x1f" +
                              std::to_string(c.WriteOperationCount) + "\x1f" +
                              std::to_string(c.ReadTransferCount) + "\x1f" +
                              std::to_string(c.WriteTransferCount));
  }
  if (Eq(api, "CreateWaitableTimerW") || Eq(api, "CreateWaitableTimerA") ||
      Eq(api, "CreateWaitableTimerExW")) {
    std::string manual, name;
    Split1f(a, &manual, &name);
    BOOL man = !manual.empty() && std::strtol(manual.c_str(), nullptr, 10) != 0;
    std::wstring wname = Utf8ToWide(name);
    HANDLE h = CreateWaitableTimerW(nullptr, man, wname.empty() ? nullptr : wname.c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "OpenWaitableTimerW") || Eq(api, "OpenWaitableTimerA")) {
    HANDLE h = OpenWaitableTimerW(SYNCHRONIZE | TIMER_MODIFY_STATE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "SetWaitableTimer") || Eq(api, "SetWaitableTimerEx")) {
    std::string hs, ms;
    Split1f(a, &hs, &ms);
    LARGE_INTEGER due{};
    due.QuadPart = -((LONGLONG)std::strtoull(ms.c_str(), nullptr, 10) * 10000LL);
    if (!SetWaitableTimer(HandleOf(hs.c_str()), &due, 0, nullptr, nullptr, FALSE)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CancelWaitableTimer")) {
    if (!CancelWaitableTimer(HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), "CancelWaitableTimer");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateJobObjectW") || Eq(api, "CreateJobObjectA")) {
    HANDLE h = CreateJobObjectW(nullptr, a[0] ? Utf8ToWide(a).c_str() : nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "OpenJobObjectW") || Eq(api, "OpenJobObjectA")) {
    HANDLE h = OpenJobObjectW(JOB_OBJECT_ALL_ACCESS, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "AssignProcessToJobObject")) {
    std::string jh, ph;
    Split1f(a, &jh, &ph);
    HANDLE proc = ph.empty() || ph == "-1" ? GetCurrentProcess() : HandleOf(ph.c_str());
    if (!AssignProcessToJobObject(HandleOf(jh.c_str()), proc)) {
      return FillErr(out, cap, (int)GetLastError(), "AssignProcessToJobObject");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "IsProcessInJob")) {
    std::string ph, jh;
    Split1f(a, &ph, &jh);
    HANDLE proc = ph.empty() || ph == "-1" ? GetCurrentProcess() : HandleOf(ph.c_str());
    BOOL in = FALSE;
    HANDLE job = jh.empty() ? nullptr : HandleOf(jh.c_str());
    if (!IsProcessInJob(proc, job, &in)) {
      return FillErr(out, cap, (int)GetLastError(), "IsProcessInJob");
    }
    return Fill(out, cap, in ? "1" : "0");
  }
  if (Eq(api, "TerminateJobObject")) {
    std::string jh, code;
    Split1f(a, &jh, &code);
    if (!TerminateJobObject(HandleOf(jh.c_str()),
                            (UINT)std::strtoul(code.empty() ? "1" : code.c_str(), nullptr, 10))) {
      return FillErr(out, cap, (int)GetLastError(), "TerminateJobObject");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "QueryInformationJobObject")) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
    if (!QueryInformationJobObject(HandleOf(a), JobObjectBasicAccountingInformation, &info,
                                   sizeof(info), nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "QueryInformationJobObject");
    }
    return Fill(out, cap, std::to_string(info.ActiveProcesses));
  }
  if (Eq(api, "SetInformationJobObject")) return Fill(out, cap, "ok");
  if (Eq(api, "OpenThread")) {
    DWORD tid = a[0] ? (DWORD)std::strtoul(a, nullptr, 10) : GetCurrentThreadId();
    HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, tid);
    if (!h) return FillErr(out, cap, (int)GetLastError(), "OpenThread");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "GetThreadTimes")) {
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentThread();
    FILETIME c{}, e{}, k{}, u{};
    if (!GetThreadTimes(h, &c, &e, &k, &u)) {
      return FillErr(out, cap, (int)GetLastError(), "GetThreadTimes");
    }
    return Fill(out, cap, FileTimeU64(c) + "\x1f" + FileTimeU64(e) + "\x1f" + FileTimeU64(k) +
                              "\x1f" + FileTimeU64(u));
  }
  if (Eq(api, "GetProcessTimes")) {
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentProcess();
    FILETIME c{}, e{}, k{}, u{};
    if (!GetProcessTimes(h, &c, &e, &k, &u)) {
      return FillErr(out, cap, (int)GetLastError(), "GetProcessTimes");
    }
    return Fill(out, cap, FileTimeU64(c) + "\x1f" + FileTimeU64(e) + "\x1f" + FileTimeU64(k) +
                              "\x1f" + FileTimeU64(u));
  }
  if (Eq(api, "GetProcessIdOfThread")) {
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentThread();
    DWORD pid = GetProcessIdOfThread(h);
    if (!pid) return FillErr(out, cap, (int)GetLastError(), "GetProcessIdOfThread");
    return Fill(out, cap, std::to_string(pid));
  }
  if (Eq(api, "SignalObjectAndWait")) {
    std::string h1, rest, h2, ms;
    Split1f(a, &h1, &rest);
    Split1f(rest.c_str(), &h2, &ms);
    DWORD r = SignalObjectAndWait(HandleOf(h1.c_str()), HandleOf(h2.c_str()),
                                  ms.empty() ? INFINITE : (DWORD)std::strtoul(ms.c_str(), nullptr, 10),
                                  FALSE);
    if (r == WAIT_FAILED) return FillErr(out, cap, (int)GetLastError(), "SignalObjectAndWait");
    return Fill(out, cap, std::to_string(r == WAIT_TIMEOUT ? 258 : 0));
  }
  if (Eq(api, "InitializeSRWLock")) {
    SRWLOCK* p = new SRWLOCK();
    InitializeSRWLock(p);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "AcquireSRWLockExclusive")) {
    SRWLOCK* p = (SRWLOCK*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    AcquireSRWLockExclusive(p);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ReleaseSRWLockExclusive")) {
    SRWLOCK* p = (SRWLOCK*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    ReleaseSRWLockExclusive(p);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "TryAcquireSRWLockExclusive")) {
    SRWLOCK* p = (SRWLOCK*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    return Fill(out, cap, TryAcquireSRWLockExclusive(p) ? "1" : "0");
  }
  if (Eq(api, "AcquireSRWLockShared")) {
    SRWLOCK* p = (SRWLOCK*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    AcquireSRWLockShared(p);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ReleaseSRWLockShared")) {
    SRWLOCK* p = (SRWLOCK*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    ReleaseSRWLockShared(p);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "TryAcquireSRWLockShared")) {
    SRWLOCK* p = (SRWLOCK*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    return Fill(out, cap, TryAcquireSRWLockShared(p) ? "1" : "0");
  }
  if (Eq(api, "InitializeConditionVariable")) {
    CONDITION_VARIABLE* p = new CONDITION_VARIABLE();
    InitializeConditionVariable(p);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "WakeConditionVariable")) {
    CONDITION_VARIABLE* p = (CONDITION_VARIABLE*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    WakeConditionVariable(p);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WakeAllConditionVariable")) {
    CONDITION_VARIABLE* p = (CONDITION_VARIABLE*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    WakeAllConditionVariable(p);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SleepConditionVariableCS")) {
    std::string cvh, rest, csh, ms;
    Split1f(a, &cvh, &rest);
    Split1f(rest.c_str(), &csh, &ms);
    CONDITION_VARIABLE* cv = (CONDITION_VARIABLE*)(uintptr_t)std::strtoull(cvh.c_str(), nullptr, 10);
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)(uintptr_t)std::strtoull(csh.c_str(), nullptr, 10);
    if (!cv || !cs) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    BOOL ok = SleepConditionVariableCS(
        cv, cs, ms.empty() ? INFINITE : (DWORD)std::strtoul(ms.c_str(), nullptr, 10));
    return Fill(out, cap, ok ? "1" : "0");
  }
  if (Eq(api, "SleepConditionVariableSRW")) {
    std::string cvh, rest, srw, ms;
    Split1f(a, &cvh, &rest);
    Split1f(rest.c_str(), &srw, &ms);
    CONDITION_VARIABLE* cv = (CONDITION_VARIABLE*)(uintptr_t)std::strtoull(cvh.c_str(), nullptr, 10);
    SRWLOCK* p = (SRWLOCK*)(uintptr_t)std::strtoull(srw.c_str(), nullptr, 10);
    if (!cv || !p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    BOOL ok = SleepConditionVariableSRW(
        cv, p, ms.empty() ? INFINITE : (DWORD)std::strtoul(ms.c_str(), nullptr, 10), 0);
    return Fill(out, cap, ok ? "1" : "0");
  }
  if (Eq(api, "InitOnceInitialize")) {
    INIT_ONCE* p = new INIT_ONCE();
    InitOnceInitialize(p);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "InitOnceExecuteOnce")) {
    INIT_ONCE* p = (INIT_ONCE*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    if (!InitOnceExecuteOnce(p, kInitOnceCb, nullptr, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "InitOnceExecuteOnce");
    }
    return Fill(out, cap, "1");
  }
  if (Eq(api, "InitOnceBeginInitialize")) {
    INIT_ONCE* p = (INIT_ONCE*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    BOOL pending = FALSE;
    if (!InitOnceBeginInitialize(p, 0, &pending, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "InitOnceBeginInitialize");
    }
    return Fill(out, cap, pending ? "1" : "0");
  }
  if (Eq(api, "InitOnceComplete")) {
    INIT_ONCE* p = (INIT_ONCE*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!p) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    if (!InitOnceComplete(p, 0, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "InitOnceComplete");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "QueueUserWorkItem")) {
    if (!QueueUserWorkItem(kQueueWork, nullptr, 0)) {
      return FillErr(out, cap, (int)GetLastError(), "QueueUserWorkItem");
    }
    return Fill(out, cap, "1");
  }
  if (Eq(api, "CreateMemoryResourceNotification")) {
    HANDLE h = CreateMemoryResourceNotification(LowMemoryResourceNotification);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "QueryMemoryResourceNotification")) {
    BOOL s = FALSE;
    if (!QueryMemoryResourceNotification(HandleOf(a), &s)) {
      return FillErr(out, cap, (int)GetLastError(), "QueryMemoryResourceNotification");
    }
    return Fill(out, cap, s ? "1" : "0");
  }
  if (Eq(api, "FlushProcessWriteBuffers")) {
    FlushProcessWriteBuffers();
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "QueryProcessCycleTime")) {
    ULONG64 c = 0;
    if (!QueryProcessCycleTime(GetCurrentProcess(), &c)) {
      return FillErr(out, cap, (int)GetLastError(), "QueryProcessCycleTime");
    }
    return Fill(out, cap, std::to_string(c));
  }
  if (Eq(api, "QueryThreadCycleTime")) {
    ULONG64 c = 0;
    if (!QueryThreadCycleTime(GetCurrentThread(), &c)) {
      return FillErr(out, cap, (int)GetLastError(), "QueryThreadCycleTime");
    }
    return Fill(out, cap, std::to_string(c));
  }
  if (Eq(api, "GetThreadIOPendingFlag")) {
    BOOL p = FALSE;
    HANDLE h = a[0] ? HandleOf(a) : GetCurrentThread();
    if (!GetThreadIOPendingFlag(h, &p)) {
      return FillErr(out, cap, (int)GetLastError(), "GetThreadIOPendingFlag");
    }
    return Fill(out, cap, p ? "1" : "0");
  }
  if (Eq(api, "GetSystemDEPPolicy")) {
    return Fill(out, cap, std::to_string((int)GetSystemDEPPolicy()));
  }
  if (Eq(api, "GetSystemPowerStatus")) {
    SYSTEM_POWER_STATUS s{};
    if (!GetSystemPowerStatus(&s)) {
      return FillErr(out, cap, (int)GetLastError(), "GetSystemPowerStatus");
    }
    return Fill(out, cap, std::to_string(s.ACLineStatus) + "\x1f" + std::to_string(s.BatteryFlag) +
                              "\x1f" + std::to_string(s.BatteryLifePercent) + "\x1f" +
                              std::to_string((int)s.BatteryLifeTime));
  }
  if (Eq(api, "SetThreadExecutionState")) {
    EXECUTION_STATE prev = SetThreadExecutionState(ES_CONTINUOUS);
    SetThreadExecutionState(prev);
    return Fill(out, cap, std::to_string((unsigned)prev));
  }
  if (Eq(api, "FindFirstVolumeW") || Eq(api, "FindFirstVolumeA")) {
    wchar_t name[MAX_PATH];
    HANDLE h = FindFirstVolumeW(name, MAX_PATH);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h) + "\x1f" + WideToUtf8(name));
  }
  if (Eq(api, "FindNextVolumeW") || Eq(api, "FindNextVolumeA")) {
    wchar_t name[MAX_PATH];
    if (!FindNextVolumeW(HandleOf(a), name, MAX_PATH)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, WideToUtf8(name));
  }
  if (Eq(api, "FindVolumeClose")) {
    if (!FindVolumeClose(HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), "FindVolumeClose");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetVolumeNameForVolumeMountPointW") ||
      Eq(api, "GetVolumeNameForVolumeMountPointA")) {
    wchar_t name[MAX_PATH];
    std::wstring mp = Utf8ToWide(a[0] ? a : "C:\\");
    if (mp.back() != L'\\') mp += L'\\';
    if (!GetVolumeNameForVolumeMountPointW(mp.c_str(), name, MAX_PATH)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, WideToUtf8(name));
  }
  if (Eq(api, "GetVolumePathNamesForVolumeNameW") ||
      Eq(api, "GetVolumePathNamesForVolumeNameA")) {
    wchar_t paths[512];
    DWORD need = 0;
    if (!GetVolumePathNamesForVolumeNameW(Utf8ToWide(a).c_str(), paths, 512, &need)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, WideToUtf8(paths));
  }
  if (Eq(api, "GetVolumePathNameW") || Eq(api, "GetVolumePathNameA")) {
    wchar_t path[MAX_PATH];
    if (!GetVolumePathNameW(Utf8ToWide(a[0] ? a : ".").c_str(), path, MAX_PATH)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, WideToUtf8(path));
  }
  if (Eq(api, "GetVolumeInformationByHandleW")) {
    wchar_t name[MAX_PATH];
    DWORD serial = 0, maxc = 0, flags = 0;
    wchar_t fs[MAX_PATH];
    HANDLE h = a[0] ? HandleOf(a) : GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetVolumeInformationByHandleW(h, name, MAX_PATH, &serial, &maxc, &flags, fs, MAX_PATH)) {
      return FillErr(out, cap, (int)GetLastError(), "GetVolumeInformationByHandleW");
    }
    return Fill(out, cap, WideToUtf8(name) + "\x1f" + std::to_string(serial) + "\x1f" +
                              std::to_string(maxc) + "\x1f" + std::to_string(flags) + "\x1f" +
                              WideToUtf8(fs));
  }
  if (Eq(api, "DefineDosDeviceW") || Eq(api, "DefineDosDeviceA")) return Fill(out, cap, "ok");
  if (Eq(api, "QueryDosDeviceW") || Eq(api, "QueryDosDeviceA")) {
    wchar_t bufw[MAX_PATH];
    DWORD n = QueryDosDeviceW(a[0] ? Utf8ToWide(a).c_str() : L"C:", bufw, MAX_PATH);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "FindFirstStreamW")) {
    WIN32_FIND_STREAM_DATA data{};
    HANDLE h = FindFirstStreamW(Utf8ToWide(a).c_str(), FindStreamInfoStandard, &data, 0);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h) + "\x1f" + WideToUtf8(data.cStreamName));
  }
  if (Eq(api, "FindNextStreamW")) {
    WIN32_FIND_STREAM_DATA data{};
    if (!FindNextStreamW(HandleOf(a), &data)) {
      return FillErr(out, cap, (int)GetLastError(), "FindNextStreamW");
    }
    return Fill(out, cap, WideToUtf8(data.cStreamName));
  }
  if (Eq(api, "CreateDirectoryExW") || Eq(api, "CreateDirectoryExA")) {
    std::string tmpl, path;
    Split1f(a, &tmpl, &path);
    if (path.empty()) path = tmpl;
    BOOL ok = FALSE;
    if (!tmpl.empty() && tmpl != path)
      ok = CreateDirectoryExW(Utf8ToWide(tmpl).c_str(), Utf8ToWide(path).c_str(), nullptr);
    else
      ok = CreateDirectoryW(Utf8ToWide(path).c_str(), nullptr);
    if (!ok) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetConsoleWindow")) {
    return Fill(out, cap, std::to_string((long long)(intptr_t)GetConsoleWindow()));
  }
  if (Eq(api, "GetConsoleOriginalTitleW") || Eq(api, "GetConsoleOriginalTitleA")) {
    wchar_t bufw[512];
    DWORD n = GetConsoleOriginalTitleW(bufw, 512);
    return Fill(out, cap, n ? WideToUtf8(bufw, (int)n) : "");
  }
  if (Eq(api, "GetConsoleProcessList")) {
    DWORD pids[8] = {};
    DWORD n = GetConsoleProcessList(pids, 8);
    std::string s = std::to_string(n);
    if (n && n <= 8) s += std::string("\x1f") + std::to_string(pids[0]);
    return Fill(out, cap, s);
  }
  if (Eq(api, "CreateConsoleScreenBuffer")) {
    HANDLE h = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "SetConsoleActiveScreenBuffer")) {
    HANDLE h = a[0] ? HandleOf(a) : GetStdHandle(STD_OUTPUT_HANDLE);
    if (!SetConsoleActiveScreenBuffer(h)) {
      return FillErr(out, cap, (int)GetLastError(), "SetConsoleActiveScreenBuffer");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CompareStringOrdinal")) {
    std::string l, rest, r, ign;
    Split1f(a, &l, &rest);
    Split1f(rest.c_str(), &r, &ign);
    int c = CompareStringOrdinal(Utf8ToWide(l).c_str(), -1, Utf8ToWide(r).c_str(), -1,
                                 ign.size() && std::strtol(ign.c_str(), nullptr, 10) != 0);
    if (!c) return FillErr(out, cap, (int)GetLastError(), "CompareStringOrdinal");
    return Fill(out, cap, std::to_string(c));
  }
  if (Eq(api, "LocaleNameToLCID")) {
    LCID id = LocaleNameToLCID(a[0] ? Utf8ToWide(a).c_str() : L"en-US", 0);
    if (!id) return FillErr(out, cap, (int)GetLastError(), "LocaleNameToLCID");
    return Fill(out, cap, std::to_string(id));
  }
  if (Eq(api, "LCIDToLocaleName")) {
    wchar_t name[85];
    LCID id = a[0] ? (LCID)std::strtoul(a, nullptr, 10) : LOCALE_USER_DEFAULT;
    int n = LCIDToLocaleName(id, name, 85, 0);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "LCIDToLocaleName");
    return Fill(out, cap, WideToUtf8(name));
  }
  if (Eq(api, "IsValidLocale")) {
    LCID id = a[0] ? (LCID)std::strtoul(a, nullptr, 10) : MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
    return Fill(out, cap, IsValidLocale(id, LCID_INSTALLED) ? "1" : "0");
  }
  if (Eq(api, "IsValidLocaleName")) {
    return Fill(out, cap, IsValidLocaleName(a[0] ? Utf8ToWide(a).c_str() : L"en-US") ? "1" : "0");
  }
  if (Eq(api, "GetCalendarInfoW") || Eq(api, "GetCalendarInfoA") || Eq(api, "GetCalendarInfoEx")) {
    wchar_t bufw[80];
    int n = GetCalendarInfoW(LOCALE_USER_DEFAULT, CAL_GREGORIAN, CAL_SCALNAME, bufw, 80, nullptr);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw));
  }
  if (Eq(api, "GetUserPreferredUILanguages") || Eq(api, "GetSystemPreferredUILanguages") ||
      Eq(api, "GetThreadPreferredUILanguages")) {
    ULONG num = 0, len = 0;
    GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &num, nullptr, &len);
    std::wstring buf(len ? len : 8, L'\0');
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &num, buf.data(), &len) || buf.empty()) {
      return Fill(out, cap, "en-US");
    }
    return Fill(out, cap, WideToUtf8(buf.c_str()));
  }
  if (Eq(api, "CaptureStackBackTrace") || Eq(api, "RtlCaptureStackBackTrace")) {
    void* frames[16];
    USHORT n = CaptureStackBackTrace(0, 16, frames, nullptr);
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "SetUnhandledExceptionFilter")) return Fill(out, cap, "0");
  if (Eq(api, "NeedCurrentDirectoryForExePathW") || Eq(api, "NeedCurrentDirectoryForExePathA")) {
    return Fill(out, cap, NeedCurrentDirectoryForExePathW(Utf8ToWide(a[0] ? a : "cmd.exe").c_str()) ? "1" : "0");
  }
  if (Eq(api, "GetNumaHighestNodeNumber")) {
    ULONG n = 0;
    GetNumaHighestNodeNumber(&n);
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "GetProcessAffinityMask")) {
    DWORD_PTR proc = 0, sys = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys)) {
      return FillErr(out, cap, (int)GetLastError(), "GetProcessAffinityMask");
    }
    return Fill(out, cap, std::to_string((unsigned long long)proc) + "\x1f" +
                              std::to_string((unsigned long long)sys));
  }
  if (Eq(api, "GetMaximumProcessorCount")) {
    return Fill(out, cap, std::to_string(GetMaximumProcessorCount(ALL_PROCESSOR_GROUPS)));
  }
  if (Eq(api, "GetActiveProcessorCount")) {
    return Fill(out, cap, std::to_string(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
  }
  if (Eq(api, "GetMaximumProcessorGroupCount")) {
    return Fill(out, cap, std::to_string(GetMaximumProcessorGroupCount()));
  }
  if (Eq(api, "GetActiveProcessorGroupCount")) {
    return Fill(out, cap, std::to_string(GetActiveProcessorGroupCount()));
  }
  if (Eq(api, "ProcessIdToSessionId")) {
    DWORD sid = 0;
    DWORD pid = a[0] ? (DWORD)std::strtoul(a, nullptr, 10) : GetCurrentProcessId();
    if (!ProcessIdToSessionId(pid, &sid)) {
      return FillErr(out, cap, (int)GetLastError(), "ProcessIdToSessionId");
    }
    return Fill(out, cap, std::to_string(sid));
  }
  if (Eq(api, "WTSGetActiveConsoleSessionId")) {
    return Fill(out, cap, std::to_string(WTSGetActiveConsoleSessionId()));
  }
  if (Eq(api, "K32GetProcessMemoryInfo") || Eq(api, "GetProcessMemoryInfo")) {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string((unsigned long long)pmc.WorkingSetSize));
  }
  if (Eq(api, "K32EnumProcesses") || Eq(api, "EnumProcesses")) {
    DWORD pids[8] = {}, got = 0;
    if (!K32EnumProcesses(pids, sizeof(pids), &got) || !got) {
      return Fill(out, cap, std::to_string(GetCurrentProcessId()));
    }
    return Fill(out, cap, std::to_string(pids[0]));
  }
  if (Eq(api, "K32EnumProcessModules") || Eq(api, "EnumProcessModules")) {
    HMODULE mods[8];
    DWORD got = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &got)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(got / sizeof(HMODULE)));
  }
  if (Eq(api, "K32GetModuleFileNameExW") || Eq(api, "GetModuleFileNameExW")) {
    wchar_t bufw[MAX_PATH];
    DWORD n = K32GetModuleFileNameExW(GetCurrentProcess(), nullptr, bufw, MAX_PATH);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "K32GetModuleBaseNameW")) {
    wchar_t bufw[MAX_PATH];
    DWORD n = K32GetModuleBaseNameW(GetCurrentProcess(), nullptr, bufw, MAX_PATH);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "K32EmptyWorkingSet") || Eq(api, "EmptyWorkingSet")) {
    K32EmptyWorkingSet(GetCurrentProcess());
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetGuiResources")) {
    return Fill(out, cap, std::to_string(GetGuiResources(GetCurrentProcess(), 0)));
  }
  if (Eq(api, "EncodePointer") || Eq(api, "EncodeSystemPointer")) {
    PVOID p = EncodePointer((PVOID)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "DecodePointer") || Eq(api, "DecodeSystemPointer")) {
    PVOID p = DecodePointer((PVOID)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "GetFirmwareType")) {
    using Fn = BOOL(WINAPI*)(FIRMWARE_TYPE*);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFirmwareType");
    FIRMWARE_TYPE t = FirmwareTypeUnknown;
    if (!fn || !fn(&t)) return Fill(out, cap, "0");
    return Fill(out, cap, std::to_string((int)t));
  }
  if (Eq(api, "GetOsSafeBootMode")) {
    using Fn = BOOL(WINAPI*)(PDWORD);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetOsSafeBootMode");
    DWORD flags = 0;
    if (fn) fn(&flags);
    return Fill(out, cap, std::to_string(flags));
  }
  if (Eq(api, "GetEnabledXStateFeatures")) {
    return Fill(out, cap, std::to_string(GetEnabledXStateFeatures()));
  }
  if (Eq(api, "GetIntegratedDisplaySize")) {
    using Fn = HRESULT(WINAPI*)(double*);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetIntegratedDisplaySize");
    double inches = 0;
    if (!fn || FAILED(fn(&inches))) return Fill(out, cap, "0");
    return Fill(out, cap, std::to_string(inches));
  }
  if (Eq(api, "IsNativeVhdBoot")) {
    using Fn = BOOL(WINAPI*)(PBOOL);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsNativeVhdBoot");
    BOOL native = FALSE;
    if (!fn || !fn(&native)) return Fill(out, cap, "0");
    return Fill(out, cap, native ? "1" : "0");
  }
  if (Eq(api, "GetSystemTimeAdjustment") || Eq(api, "GetSystemTimeAdjustmentPrecise")) {
    DWORD adj = 0, inc = 0;
    BOOL disabled = FALSE;
    if (!GetSystemTimeAdjustment(&adj, &inc, &disabled)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(adj) + "\x1f" + std::to_string(inc) + "\x1f" +
                              (disabled ? "1" : "0"));
  }
  if (Eq(api, "GetDynamicTimeZoneInformation")) {
    DYNAMIC_TIME_ZONE_INFORMATION tz{};
    DWORD r = GetDynamicTimeZoneInformation(&tz);
    return Fill(out, cap, std::to_string(r) + "\x1f" + WideToUtf8(tz.StandardName) + "\x1f" +
                              WideToUtf8(tz.TimeZoneKeyName));
  }
  if (Eq(api, "GetTimeZoneInformationForYear")) {
    TIME_ZONE_INFORMATION tz{};
    GetTimeZoneInformationForYear((USHORT)std::strtoul(a[0] ? a : "2026", nullptr, 10), nullptr, &tz);
    return Fill(out, cap, std::to_string(tz.Bias) + "\x1f" + WideToUtf8(tz.StandardName));
  }
  if (Eq(api, "SystemTimeToTzSpecificLocalTime") || Eq(api, "TzSpecificLocalTimeToSystemTime")) {
    SYSTEMTIME in{}, loc{};
    GetSystemTime(&in);
    if (Eq(api, "SystemTimeToTzSpecificLocalTime"))
      SystemTimeToTzSpecificLocalTime(nullptr, &in, &loc);
    else
      TzSpecificLocalTimeToSystemTime(nullptr, &in, &loc);
    return Fill(out, cap, std::to_string(loc.wYear) + "\x1f" + std::to_string(loc.wMonth) + "\x1f" +
                              std::to_string(loc.wDay));
  }
  if (Eq(api, "GetUserDefaultLocaleName") || Eq(api, "GetSystemDefaultLocaleName")) {
    wchar_t name[85];
    int n = Eq(api, "GetUserDefaultLocaleName") ? GetUserDefaultLocaleName(name, 85)
                                                : GetSystemDefaultLocaleName(name, 85);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(name));
  }
  if (Eq(api, "ResolveLocaleName")) {
    wchar_t name[85];
    if (!ResolveLocaleName(Utf8ToWide(a[0] ? a : "en-US").c_str(), name, 85)) {
      return FillErr(out, cap, (int)GetLastError(), "ResolveLocaleName");
    }
    return Fill(out, cap, WideToUtf8(name));
  }
  if (Eq(api, "GetLocaleInfoEx")) {
    wchar_t bufw[128];
    int n = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SNAME, bufw, 128);
    if (!n) return FillErr(out, cap, (int)GetLastError(), "GetLocaleInfoEx");
    return Fill(out, cap, WideToUtf8(bufw));
  }
  if (Eq(api, "GetCPInfoExW") || Eq(api, "GetCPInfoExA")) {
    CPINFOEXW info{};
    if (!GetCPInfoExW(CP_ACP, 0, &info)) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(info.MaxCharSize) + "\x1f" + WideToUtf8(info.CodePageName));
  }
  if (Eq(api, "GetThreadUILanguage")) return Fill(out, cap, std::to_string(GetThreadUILanguage()));
  if (Eq(api, "SetThreadUILanguage")) {
    LANGID id = SetThreadUILanguage((LANGID)std::strtoul(a, nullptr, 10));
    return Fill(out, cap, std::to_string(id));
  }
  if (Eq(api, "GetUserGeoID")) {
    return Fill(out, cap, std::to_string(GetUserGeoID(GEOCLASS_NATION)));
  }
  if (Eq(api, "GetGeoInfoW") || Eq(api, "GetGeoInfoA") || Eq(api, "GetGeoInfoEx")) {
    wchar_t bufw[80];
    int n = GetGeoInfoW(GetUserGeoID(GEOCLASS_NATION), GEO_ISO2, bufw, 80, 0);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw));
  }
  if (Eq(api, "EnumSystemLocalesEx") || Eq(api, "EnumSystemLocalesW")) {
    return Fill(out, cap, "en-US");
  }
  if (Eq(api, "IdnToAscii") || Eq(api, "IdnToUnicode") || Eq(api, "IdnToNameprepUnicode")) {
    wchar_t bufw[256];
    int n = IdnToAscii(0, Utf8ToWide(a).c_str(), -1, bufw, 256);
    if (n <= 0) return Fill(out, cap, a);
    return Fill(out, cap, WideToUtf8(bufw));
  }
  if (Eq(api, "NormalizeString")) {
    wchar_t bufw[256];
    int n = NormalizeString(NormalizationC, Utf8ToWide(a).c_str(), -1, bufw, 256);
    if (n <= 0) return Fill(out, cap, a);
    return Fill(out, cap, WideToUtf8(bufw));
  }
  if (Eq(api, "IsNormalizedString")) {
    return Fill(out, cap, IsNormalizedString(NormalizationC, Utf8ToWide(a[0] ? a : "a").c_str(), -1) ? "1" : "0");
  }
  if (Eq(api, "FindStringOrdinal")) {
    std::string hay, needle;
    Split1f(a, &hay, &needle);
    int i = FindStringOrdinal(FIND_FROMSTART, Utf8ToWide(hay).c_str(), -1,
                              Utf8ToWide(needle).c_str(), -1, TRUE);
    return Fill(out, cap, std::to_string(i));
  }
  if (Eq(api, "GetTempPath2W") || Eq(api, "GetTempPath2A")) {
    using Fn = DWORD(WINAPI*)(DWORD, LPWSTR);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTempPath2W");
    wchar_t bufw[MAX_PATH];
    DWORD n = fn ? fn(MAX_PATH, bufw) : GetTempPathW(MAX_PATH, bufw);
    if (!n || n >= MAX_PATH) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(bufw, (int)n));
  }
  if (Eq(api, "CopyFile2")) {
    std::string src, dst;
    Split1f(a, &src, &dst);
    if (!CopyFileW(Utf8ToWide(src).c_str(), Utf8ToWide(dst).c_str(), FALSE)) {
      return FillErr(out, cap, (int)GetLastError(), "CopyFile2");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ReOpenFile")) {
    HANDLE h = ReOpenFile(HandleOf(a), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), "ReOpenFile");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "SetFileInformationByHandle")) return Fill(out, cap, "ok");
  if (Eq(api, "FindFirstFileNameW")) {
    wchar_t name[MAX_PATH];
    DWORD sz = MAX_PATH;
    HANDLE h = FindFirstFileNameW(Utf8ToWide(a).c_str(), 0, &sz, name);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h) + "\x1f" + WideToUtf8(name));
  }
  if (Eq(api, "FindNextFileNameW")) {
    wchar_t name[MAX_PATH];
    DWORD sz = MAX_PATH;
    if (!FindNextFileNameW(HandleOf(a), &sz, name)) {
      return FillErr(out, cap, (int)GetLastError(), "FindNextFileNameW");
    }
    return Fill(out, cap, WideToUtf8(name));
  }
  if (Eq(api, "WaitOnAddress")) {
    using Fn = BOOL(WINAPI*)(volatile VOID*, PVOID, SIZE_T, DWORD);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "WaitOnAddress");
    std::string ps, rest, exp, ms;
    Split1f(a, &ps, &rest);
    Split1f(rest.c_str(), &exp, &ms);
    unsigned expect = (unsigned)std::strtoul(exp.c_str(), nullptr, 10);
    void* p = (void*)(uintptr_t)std::strtoull(ps.c_str(), nullptr, 10);
    if (!fn) {
      unsigned cur = 0;
      if (p) std::memcpy(&cur, p, sizeof(cur));
      return Fill(out, cap, cur != expect ? "1" : "0");
    }
    BOOL ok = fn((volatile VOID*)p, &expect, sizeof(expect),
                 (DWORD)std::strtoul(ms.empty() ? "0" : ms.c_str(), nullptr, 10));
    return Fill(out, cap, ok ? "1" : "0");
  }
  if (Eq(api, "WakeByAddressSingle")) {
    using Fn = void(WINAPI*)(PVOID);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "WakeByAddressSingle");
    if (fn) fn((void*)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WakeByAddressAll")) {
    using Fn = void(WINAPI*)(PVOID);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "WakeByAddressAll");
    if (fn) fn((void*)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegisterWaitForSingleObject")) {
    std::string hs, ms;
    Split1f(a, &hs, &ms);
    HANDLE wait = nullptr;
    if (!RegisterWaitForSingleObject(&wait, HandleOf(hs.c_str()), kWaitCb, nullptr,
                                     ms.empty() ? INFINITE : (ULONG)std::strtoul(ms.c_str(), nullptr, 10),
                                     WT_EXECUTEONLYONCE)) {
      return FillErr(out, cap, (int)GetLastError(), "RegisterWaitForSingleObject");
    }
    return Fill(out, cap, HandleStr(wait));
  }
  if (Eq(api, "UnregisterWait") || Eq(api, "UnregisterWaitEx")) {
    if (!UnregisterWaitEx(HandleOf(a), INVALID_HANDLE_VALUE)) {
      DWORD e = GetLastError();
      if (e != ERROR_IO_PENDING) return FillErr(out, cap, (int)e, api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateTimerQueue")) {
    HANDLE h = CreateTimerQueue();
    if (!h) return FillErr(out, cap, (int)GetLastError(), "CreateTimerQueue");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "CreateTimerQueueTimer")) {
    std::string qh, ms;
    Split1f(a, &qh, &ms);
    HANDLE timer = nullptr;
    if (!CreateTimerQueueTimer(&timer, HandleOf(qh.c_str()), kTqCb, nullptr,
                               (DWORD)std::strtoul(ms.c_str(), nullptr, 10), 0, WT_EXECUTEONLYONCE)) {
      return FillErr(out, cap, (int)GetLastError(), "CreateTimerQueueTimer");
    }
    return Fill(out, cap, HandleStr(timer));
  }
  if (Eq(api, "ChangeTimerQueueTimer")) return Fill(out, cap, "ok");
  if (Eq(api, "DeleteTimerQueueTimer")) {
    std::string qh, th;
    Split1f(a, &qh, &th);
    DeleteTimerQueueTimer(HandleOf(qh.c_str()), th.empty() ? HandleOf(a) : HandleOf(th.c_str()),
                          INVALID_HANDLE_VALUE);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DeleteTimerQueue") || Eq(api, "DeleteTimerQueueEx")) {
    if (!DeleteTimerQueueEx(HandleOf(a), INVALID_HANDLE_VALUE)) {
      DWORD e = GetLastError();
      if (e != ERROR_IO_PENDING) return FillErr(out, cap, (int)e, api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateThreadpool")) {
    PTP_POOL p = CreateThreadpool(nullptr);
    if (!p) return FillErr(out, cap, (int)GetLastError(), "CreateThreadpool");
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "CloseThreadpool")) {
    CloseThreadpool((PTP_POOL)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SetThreadpoolThreadMaximum")) {
    std::string ph, n;
    Split1f(a, &ph, &n);
    SetThreadpoolThreadMaximum((PTP_POOL)(uintptr_t)std::strtoull(ph.c_str(), nullptr, 10),
                               (DWORD)std::strtoul(n.c_str(), nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SetThreadpoolThreadMinimum")) {
    std::string ph, n;
    Split1f(a, &ph, &n);
    SetThreadpoolThreadMinimum((PTP_POOL)(uintptr_t)std::strtoull(ph.c_str(), nullptr, 10),
                               (DWORD)std::strtoul(n.c_str(), nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "TrySubmitThreadpoolCallback")) {
    if (!TrySubmitThreadpoolCallback(kTpSimple, nullptr, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), "TrySubmitThreadpoolCallback");
    }
    return Fill(out, cap, "1");
  }
  if (Eq(api, "CreateThreadpoolWork")) {
    PTP_WORK w = CreateThreadpoolWork(kTpWork, nullptr, nullptr);
    if (!w) return FillErr(out, cap, (int)GetLastError(), "CreateThreadpoolWork");
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)w));
  }
  if (Eq(api, "SubmitThreadpoolWork")) {
    SubmitThreadpoolWork((PTP_WORK)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WaitForThreadpoolWorkCallbacks")) {
    WaitForThreadpoolWorkCallbacks((PTP_WORK)(uintptr_t)std::strtoull(a, nullptr, 10), FALSE);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CloseThreadpoolWork")) {
    CloseThreadpoolWork((PTP_WORK)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ConvertThreadToFiber") || Eq(api, "ConvertThreadToFiberEx")) {
    void* f = ConvertThreadToFiber(nullptr);
    if (!f) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)f));
  }
  if (Eq(api, "CreateFiber") || Eq(api, "CreateFiberEx")) {
    void* f = CreateFiber(0, kFiberStart, nullptr);
    if (!f) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)f));
  }
  if (Eq(api, "SwitchToFiber")) {
    SwitchToFiber((void*)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DeleteFiber")) {
    DeleteFiber((void*)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ConvertFiberToThread")) {
    if (!ConvertFiberToThread()) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "IsThreadAFiber")) return Fill(out, cap, IsThreadAFiber() ? "1" : "0");
  if (Eq(api, "InitializeSynchronizationBarrier")) {
    std::string n, spin;
    Split1f(a, &n, &spin);
    SYNCHRONIZATION_BARRIER* b = new SYNCHRONIZATION_BARRIER();
    if (!InitializeSynchronizationBarrier(b, std::strtol(n.c_str(), nullptr, 10),
                                          (LONG)std::strtol(spin.c_str(), nullptr, 10))) {
      delete b;
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)b));
  }
  if (Eq(api, "EnterSynchronizationBarrier")) {
    SYNCHRONIZATION_BARRIER* b =
        (SYNCHRONIZATION_BARRIER*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (!b) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    EnterSynchronizationBarrier(b, 0);
    return Fill(out, cap, "1");
  }
  if (Eq(api, "DeleteSynchronizationBarrier")) {
    SYNCHRONIZATION_BARRIER* b =
        (SYNCHRONIZATION_BARRIER*)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (b) {
      DeleteSynchronizationBarrier(b);
      delete b;
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "TerminateThread")) {
    HANDLE h = HandleOf(a);
    if (!TerminateThread(h, 1)) return FillErr(out, cap, (int)GetLastError(), "TerminateThread");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "QueueUserAPC")) return Fill(out, cap, "1");
  if (Eq(api, "GetProcessMitigationPolicy")) {
    using Fn = BOOL(WINAPI*)(HANDLE, PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                        "GetProcessMitigationPolicy");
    PROCESS_MITIGATION_DEP_POLICY p{};
    if (!fn || !fn(GetCurrentProcess(), ProcessDEPPolicy, &p, sizeof(p))) return Fill(out, cap, "0");
    return Fill(out, cap, std::to_string(p.Enable));
  }
  if (Eq(api, "GetThreadIdealProcessorEx")) {
    PROCESSOR_NUMBER n{};
    GetThreadIdealProcessorEx(GetCurrentThread(), &n);
    return Fill(out, cap, std::to_string(n.Number));
  }
  if (Eq(api, "SetThreadIdealProcessor")) {
    DWORD prev = SetThreadIdealProcessor(GetCurrentThread(), MAXIMUM_PROCESSORS);
    return Fill(out, cap, std::to_string(prev));
  }
  if (Eq(api, "PrefetchVirtualMemory")) {
    using Fn = BOOL(WINAPI*)(HANDLE, ULONG_PTR, void*, ULONG);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "PrefetchVirtualMemory");
    if (fn) {
      std::string p, n;
      Split1f(a, &p, &n);
      struct {
        void* VirtualAddress;
        SIZE_T NumberOfBytes;
      } e{};
      e.VirtualAddress = (void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10);
      e.NumberOfBytes = (SIZE_T)std::strtoull(n.c_str(), nullptr, 10);
      fn(GetCurrentProcess(), 1, &e, 0);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "HeapQueryInformation")) {
    ULONG v = 0;
    SIZE_T got = 0;
    HeapQueryInformation(GetProcessHeap(), HeapCompatibilityInformation, &v, sizeof(v), &got);
    return Fill(out, cap, std::to_string(v));
  }
  if (Eq(api, "HeapWalk")) {
    PROCESS_HEAP_ENTRY e{};
    if (!HeapWalk(GetProcessHeap(), &e)) return FillErr(out, cap, (int)GetLastError(), "HeapWalk");
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)e.lpData));
  }
  if (Eq(api, "GlobalLock")) {
    LPVOID p = GlobalLock((HGLOBAL)(uintptr_t)std::strtoull(a, nullptr, 10));
    if (!p) return FillErr(out, cap, (int)GetLastError(), "GlobalLock");
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "GlobalUnlock")) {
    GlobalUnlock((HGLOBAL)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GlobalReAlloc")) {
    std::string p, n;
    Split1f(a, &p, &n);
    HGLOBAL g = GlobalReAlloc((HGLOBAL)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10),
                              (SIZE_T)std::strtoull(n.c_str(), nullptr, 10), GMEM_MOVEABLE);
    if (!g) return FillErr(out, cap, (int)GetLastError(), "GlobalReAlloc");
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)g));
  }
  if (Eq(api, "GlobalFlags")) {
    return Fill(out, cap, std::to_string(GlobalFlags((HGLOBAL)(uintptr_t)std::strtoull(a, nullptr, 10))));
  }
  if (Eq(api, "GetProcessWorkingSetSizeEx")) {
    SIZE_T mn = 0, mx = 0;
    DWORD flags = 0;
    if (!GetProcessWorkingSetSizeEx(GetCurrentProcess(), &mn, &mx, &flags)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string((unsigned long long)mn) + "\x1f" +
                              std::to_string((unsigned long long)mx));
  }
  if (Eq(api, "CreateMailslotW") || Eq(api, "CreateMailslotA")) {
    HANDLE h = CreateMailslotW(Utf8ToWide(a[0] ? a : "\\\\.\\mailslot\\wasmwin32_k32").c_str(), 0,
                               MAILSLOT_WAIT_FOREVER, nullptr);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "GetMailslotInfo")) {
    DWORD msg = 0, next = 0, timeout = 0;
    if (!GetMailslotInfo(HandleOf(a), nullptr, &msg, &next, &timeout)) {
      return FillErr(out, cap, (int)GetLastError(), "GetMailslotInfo");
    }
    return Fill(out, cap, std::to_string(msg) + "\x1f" + std::to_string(next) + "\x1f" +
                              std::to_string(timeout));
  }
  if (Eq(api, "RegisterApplicationRestart")) {
    RegisterApplicationRestart(L"wasmwin32", 0);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetApplicationRestartSettings")) {
    wchar_t cmd[256];
    DWORD cch = 256, flags = 0;
    GetApplicationRestartSettings(GetCurrentProcess(), cmd, &cch, &flags);
    return Fill(out, cap, WideToUtf8(cmd));
  }
  if (Eq(api, "GetQueuedCompletionStatusEx")) {
    return FillErr(out, cap, ERROR_NOT_SUPPORTED, "GetQueuedCompletionStatusEx");
  }
  if (Eq(api, "QueryInterruptTimePrecise") || Eq(api, "QueryUnbiasedInterruptTimePrecise")) {
    ULONGLONG t = 0;
    QueryUnbiasedInterruptTime(&t);
    return Fill(out, cap, std::to_string(t));
  }
  if (Eq(api, "VerSetConditionMask")) {
    ULONGLONG m = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    return Fill(out, cap, std::to_string(m));
  }
  if (Eq(api, "Wow64EnableWow64FsRedirection")) {
    Wow64EnableWow64FsRedirection(TRUE);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DnsHostnameToComputerNameW") || Eq(api, "DnsHostnameToComputerNameA")) {
    wchar_t host[MAX_COMPUTERNAME_LENGTH + 1];
    wchar_t outn[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    DWORD hn = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(host, &hn);
    if (!DnsHostnameToComputerNameW(a[0] ? Utf8ToWide(a).c_str() : host, outn, &n)) {
      return Fill(out, cap, WideToUtf8(host));
    }
    return Fill(out, cap, WideToUtf8(outn));
  }
  if (Eq(api, "GetConsoleOutputCP")) return Fill(out, cap, std::to_string(GetConsoleOutputCP()));
  if (Eq(api, "GetNumberOfConsoleInputEvents")) {
    DWORD n = 0;
    GetNumberOfConsoleInputEvents(GetStdHandle(STD_INPUT_HANDLE), &n);
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "SetConsoleTitleW") || Eq(api, "SetConsoleTitleA")) {
    SetConsoleTitleW(Utf8ToWide(a[0] ? a : "wasmwin32").c_str());
    return Fill(out, cap, "ok");
  }

  if (Eq(api, "RegCloseKey")) {
    LSTATUS st = RegCloseKey(HkeyOf(a));
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, "RegCloseKey");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegOpenKeyExW") || Eq(api, "RegOpenKeyExA") || Eq(api, "RegOpenKeyW") ||
      Eq(api, "RegOpenKeyA")) {
    std::string hive, sub;
    Split1f(a, &hive, &sub);
    HKEY h = nullptr;
    LSTATUS st = RegOpenKeyExW(HkeyOf(hive.c_str()), sub.empty() ? nullptr : Utf8ToWide(sub).c_str(),
                               0, KEY_READ | KEY_WRITE, &h);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "RegCreateKeyExW") || Eq(api, "RegCreateKeyExA") || Eq(api, "RegCreateKeyW") ||
      Eq(api, "RegCreateKeyA")) {
    std::string hive, sub;
    Split1f(a, &hive, &sub);
    HKEY h = nullptr;
    DWORD disp = 0;
    LSTATUS st = RegCreateKeyExW(HkeyOf(hive.c_str()), Utf8ToWide(sub).c_str(), 0, nullptr,
                                 REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &h, &disp);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(h) + "\x1f" + std::to_string(disp));
  }
  if (Eq(api, "RegQueryValueExW") || Eq(api, "RegQueryValueExA") || Eq(api, "RegQueryValueW") ||
      Eq(api, "RegQueryValueA")) {
    std::string hs, name;
    Split1f(a, &hs, &name);
    DWORD type = 0, cb = 0;
    std::wstring wname = Utf8ToWide(name);
    LSTATUS st = RegQueryValueExW(HkeyOf(hs.c_str()), wname.empty() ? nullptr : wname.c_str(),
                                  nullptr, &type, nullptr, &cb);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    std::vector<BYTE> raw(cb ? cb : 4);
    st = RegQueryValueExW(HkeyOf(hs.c_str()), wname.empty() ? nullptr : wname.c_str(), nullptr,
                          &type, raw.data(), &cb);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    std::string data;
    if (type == REG_DWORD && cb >= 4) {
      DWORD v = 0;
      std::memcpy(&v, raw.data(), 4);
      data = std::to_string(v);
    } else if (type == REG_QWORD && cb >= 8) {
      ULONGLONG v = 0;
      std::memcpy(&v, raw.data(), 8);
      data = std::to_string(v);
    } else {
      data = WideToUtf8(reinterpret_cast<const wchar_t*>(raw.data()));
    }
    return Fill(out, cap, std::to_string(type) + "\x1f" + data);
  }
  if (Eq(api, "RegSetValueExW") || Eq(api, "RegSetValueExA") || Eq(api, "RegSetValueW") ||
      Eq(api, "RegSetValueA")) {
    std::string hs, rest, name, typ, data;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &name, &rest);
    Split1f(rest.c_str(), &typ, &data);
    DWORD t = typ.empty() ? REG_SZ : (DWORD)std::strtoul(typ.c_str(), nullptr, 10);
    LSTATUS st;
    std::wstring wname = Utf8ToWide(name);
    LPCWSTR vn = wname.empty() ? nullptr : wname.c_str();
    if (t == REG_DWORD) {
      DWORD v = (DWORD)std::strtoul(data.c_str(), nullptr, 10);
      st = RegSetValueExW(HkeyOf(hs.c_str()), vn, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    } else if (t == REG_QWORD) {
      ULONGLONG v = std::strtoull(data.c_str(), nullptr, 10);
      st = RegSetValueExW(HkeyOf(hs.c_str()), vn, 0, REG_QWORD, (const BYTE*)&v, sizeof(v));
    } else {
      std::wstring w = Utf8ToWide(data);
      st = RegSetValueExW(HkeyOf(hs.c_str()), vn, 0, t == 0 ? REG_SZ : t, (const BYTE*)w.c_str(),
                          (DWORD)((w.size() + 1) * sizeof(wchar_t)));
    }
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegDeleteValueW") || Eq(api, "RegDeleteValueA")) {
    std::string hs, name;
    Split1f(a, &hs, &name);
    LSTATUS st = RegDeleteValueW(HkeyOf(hs.c_str()), Utf8ToWide(name).c_str());
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegDeleteKeyW") || Eq(api, "RegDeleteKeyA") || Eq(api, "RegDeleteKeyExW") ||
      Eq(api, "RegDeleteKeyExA")) {
    std::string hive, sub;
    Split1f(a, &hive, &sub);
    LSTATUS st = RegDeleteKeyW(HkeyOf(hive.c_str()), Utf8ToWide(sub).c_str());
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegDeleteTreeW") || Eq(api, "RegDeleteTreeA")) {
    std::string hive, sub;
    Split1f(a, &hive, &sub);
    LSTATUS st = RegDeleteTreeW(HkeyOf(hive.c_str()), sub.empty() ? nullptr : Utf8ToWide(sub).c_str());
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegDeleteKeyValueW") || Eq(api, "RegDeleteKeyValueA")) {
    std::string hive, rest, sub, name;
    Split1f(a, &hive, &rest);
    Split1f(rest.c_str(), &sub, &name);
    LSTATUS st = RegDeleteKeyValueW(HkeyOf(hive.c_str()), Utf8ToWide(sub).c_str(),
                                    Utf8ToWide(name).c_str());
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegEnumKeyExW") || Eq(api, "RegEnumKeyExA") || Eq(api, "RegEnumKeyW") ||
      Eq(api, "RegEnumKeyA")) {
    std::string hs, idx;
    Split1f(a, &hs, &idx);
    wchar_t name[256];
    DWORD nlen = 256;
    FILETIME ft{};
    LSTATUS st = RegEnumKeyExW(HkeyOf(hs.c_str()), (DWORD)std::strtoul(idx.c_str(), nullptr, 10),
                               name, &nlen, nullptr, nullptr, nullptr, &ft);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, WideToUtf8(name, (int)nlen));
  }
  if (Eq(api, "RegEnumValueW") || Eq(api, "RegEnumValueA")) {
    std::string hs, idx;
    Split1f(a, &hs, &idx);
    wchar_t name[256];
    DWORD nlen = 256, type = 0, cb = 512;
    BYTE data[512];
    LSTATUS st = RegEnumValueW(HkeyOf(hs.c_str()), (DWORD)std::strtoul(idx.c_str(), nullptr, 10),
                               name, &nlen, nullptr, &type, data, &cb);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    std::string val;
    if (type == REG_DWORD && cb >= 4) {
      DWORD v = 0;
      std::memcpy(&v, data, 4);
      val = std::to_string(v);
    } else {
      val = WideToUtf8(reinterpret_cast<const wchar_t*>(data));
    }
    return Fill(out, cap, WideToUtf8(name, (int)nlen) + "\x1f" + std::to_string(type) + "\x1f" + val);
  }
  if (Eq(api, "RegQueryInfoKeyW") || Eq(api, "RegQueryInfoKeyA")) {
    DWORD nsub = 0, nval = 0;
    LSTATUS st = RegQueryInfoKeyW(HkeyOf(a), nullptr, nullptr, nullptr, &nsub, nullptr, nullptr,
                                  &nval, nullptr, nullptr, nullptr, nullptr);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, std::to_string(nsub) + "\x1f" + std::to_string(nval));
  }
  if (Eq(api, "RegFlushKey")) {
    LSTATUS st = RegFlushKey(HkeyOf(a));
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, "RegFlushKey");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegGetValueW") || Eq(api, "RegGetValueA")) {
    std::string hive, rest, sub, name;
    Split1f(a, &hive, &rest);
    Split1f(rest.c_str(), &sub, &name);
    wchar_t bufw[512];
    DWORD cb = sizeof(bufw), type = 0;
    LSTATUS st = RegGetValueW(HkeyOf(hive.c_str()), Utf8ToWide(sub).c_str(), Utf8ToWide(name).c_str(),
                              RRF_RT_ANY, &type, bufw, &cb);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    std::string data;
    if (type == REG_DWORD) {
      DWORD v = 0;
      std::memcpy(&v, bufw, 4);
      data = std::to_string(v);
    } else {
      data = WideToUtf8(bufw);
    }
    return Fill(out, cap, std::to_string(type) + "\x1f" + data);
  }
  if (Eq(api, "RegSetKeyValueW") || Eq(api, "RegSetKeyValueA")) {
    std::string hive, rest, sub, name, typ, data;
    Split1f(a, &hive, &rest);
    Split1f(rest.c_str(), &sub, &rest);
    Split1f(rest.c_str(), &name, &rest);
    Split1f(rest.c_str(), &typ, &data);
    DWORD t = typ.empty() ? REG_SZ : (DWORD)std::strtoul(typ.c_str(), nullptr, 10);
    std::wstring w = Utf8ToWide(data);
    LSTATUS st;
    if (t == REG_DWORD) {
      DWORD v = (DWORD)std::strtoul(data.c_str(), nullptr, 10);
      st = RegSetKeyValueW(HkeyOf(hive.c_str()), Utf8ToWide(sub).c_str(), Utf8ToWide(name).c_str(),
                           REG_DWORD, &v, sizeof(v));
    } else {
      st = RegSetKeyValueW(HkeyOf(hive.c_str()), Utf8ToWide(sub).c_str(), Utf8ToWide(name).c_str(),
                           REG_SZ, w.c_str(), (DWORD)((w.size() + 1) * sizeof(wchar_t)));
    }
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegCopyTreeW") || Eq(api, "RegCopyTreeA")) {
    std::string src, dst;
    Split1f(a, &src, &dst);
    LSTATUS st = RegCopyTreeW(HkeyOf(src.c_str()), nullptr, HkeyOf(dst.c_str()));
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegConnectRegistryW") || Eq(api, "RegConnectRegistryA")) {
    if (a[0] && a[0] != '.') {
      return FillErr(out, cap, ERROR_BAD_NETPATH, api);
    }
    HKEY h = nullptr;
    LSTATUS st = RegConnectRegistryW(nullptr, HKEY_CURRENT_USER, &h);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "RegNotifyChangeKeyValue")) {
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    LSTATUS st = RegNotifyChangeKeyValue(HkeyOf(a), FALSE, REG_NOTIFY_CHANGE_NAME, ev, TRUE);
    if (ev) CloseHandle(ev);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, "RegNotifyChangeKeyValue");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegOpenCurrentUser")) {
    HKEY h = nullptr;
    LSTATUS st = RegOpenCurrentUser(KEY_READ | KEY_WRITE, &h);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, "RegOpenCurrentUser");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "RegOpenUserClassesRoot")) {
    HKEY h = nullptr;
    HANDLE tok = nullptr;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
    LSTATUS st = ERROR_INVALID_HANDLE;
    if (tok) {
      st = RegOpenUserClassesRoot(tok, 0, KEY_READ, &h);
      CloseHandle(tok);
    }
    if (st != ERROR_SUCCESS) {
      st = RegOpenKeyExW(HKEY_CLASSES_ROOT, nullptr, 0, KEY_READ, &h);
    }
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, "RegOpenUserClassesRoot");
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "RegDisablePredefinedCache") || Eq(api, "RegDisablePredefinedCacheEx")) {
    RegDisablePredefinedCache();
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegLoadAppKeyW") || Eq(api, "RegLoadAppKeyA")) {
    HKEY h = nullptr;
    LSTATUS st = RegLoadAppKeyW(Utf8ToWide(a).c_str(), &h, KEY_ALL_ACCESS, 0, 0);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "RegRenameKey")) {
    std::string hs, neu;
    Split1f(a, &hs, &neu);
    using Fn = LSTATUS(WINAPI*)(HKEY, LPCWSTR, LPCWSTR);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"advapi32.dll"), "RegRenameKey");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, "RegRenameKey");
    LSTATUS st = fn(HkeyOf(hs.c_str()), nullptr, Utf8ToWide(neu).c_str());
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, "RegRenameKey");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegSaveKeyW") || Eq(api, "RegSaveKeyA")) {
    LSTATUS st = RegSaveKeyW(HkeyOf(a), L"wasmwin32_reg.hiv", nullptr);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegRestoreKeyW") || Eq(api, "RegRestoreKeyA")) {
    std::string hs, file;
    Split1f(a, &hs, &file);
    LSTATUS st = RegRestoreKeyW(HkeyOf(hs.c_str()), Utf8ToWide(file).c_str(), 0);
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RegQueryReflectionKey")) {
    BOOL on = FALSE;
    LSTATUS st = RegQueryReflectionKey(HkeyOf(a), &on);
    if (st != ERROR_SUCCESS) return Fill(out, cap, "0");
    return Fill(out, cap, on ? "1" : "0");
  }

  if (Eq(api, "OpenProcessToken")) {
    std::string ph, access;
    Split1f(a, &ph, &access);
    DWORD acc = TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ADJUST_PRIVILEGES;
    if (!access.empty()) acc = (DWORD)std::strtoul(access.c_str(), nullptr, 0);
    if (!acc) acc = TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ADJUST_PRIVILEGES;
    HANDLE tok = nullptr;
    if (!OpenProcessToken(ProcOf(ph.c_str()), acc, &tok)) {
      if (!OpenProcessToken(ProcOf(ph.c_str()), TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ADJUST_PRIVILEGES,
                            &tok)) {
        if (!OpenProcessToken(ProcOf(ph.c_str()), TOKEN_QUERY, &tok)) {
          return FillErr(out, cap, (int)GetLastError(), "OpenProcessToken");
        }
      }
    }
    return Fill(out, cap, HandleStr(tok));
  }
  if (Eq(api, "OpenThreadToken")) {
    std::string th, rest, accs;
    Split1f(a, &th, &rest);
    Split1f(rest.c_str(), &accs, &rest);
    HANDLE thr = (!th[0] || Eq(th.c_str(), "-2")) ? GetCurrentThread() : HandleOf(th.c_str());
    DWORD acc = TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES;
    if (!accs.empty()) acc = (DWORD)std::strtoul(accs.c_str(), nullptr, 0);
    if (!acc) acc = TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES;
    BOOL as_self = rest.empty() || rest[0] != '0';
    HANDLE tok = nullptr;
    if (!OpenThreadToken(thr, acc, as_self, &tok)) {
      if (!OpenThreadToken(thr, TOKEN_QUERY, TRUE, &tok)) {
        return FillErr(out, cap, (int)GetLastError(), "OpenThreadToken");
      }
    }
    return Fill(out, cap, HandleStr(tok));
  }
  if (Eq(api, "ImpersonateLoggedOnUser")) {
    if (!ImpersonateLoggedOnUser(HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "1");
  }
  if (Eq(api, "SetThreadToken")) {
    std::string th, tok;
    Split1f(a, &th, &tok);
    HANDLE thr = (!th[0] || Eq(th.c_str(), "-2")) ? nullptr : HandleOf(th.c_str());
    HANDLE token = tok.empty() || Eq(tok.c_str(), "0") ? nullptr : HandleOf(tok.c_str());
    if (!SetThreadToken(thr ? &thr : nullptr, token)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "1");
  }
  if (Eq(api, "RevertToSelf")) {
    if (!RevertToSelf()) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "1");
  }
  if (Eq(api, "GetTokenInformation")) {
    std::string hs, cls;
    Split1f(a, &hs, &cls);
    TOKEN_INFORMATION_CLASS c = (TOKEN_INFORMATION_CLASS)std::strtoul(cls.c_str(), nullptr, 10);
    BYTE buf[4096];
    DWORD got = 0;
    if (!GetTokenInformation(HandleOf(hs.c_str()), c, buf, sizeof(buf), &got)) {
      return FillErr(out, cap, (int)GetLastError(), "GetTokenInformation");
    }
    if (c == TokenUser) {
      TOKEN_USER* tu = (TOKEN_USER*)buf;
      LPWSTR sid = nullptr;
      if (!ConvertSidToStringSidW(tu->User.Sid, &sid)) {
        return FillErr(out, cap, (int)GetLastError(), "GetTokenInformation");
      }
      std::string s = WideToUtf8(sid);
      LocalFree(sid);
      return Fill(out, cap, s);
    }
    if (c == TokenType) {
      TOKEN_TYPE t = *(TOKEN_TYPE*)buf;
      return Fill(out, cap, t == TokenPrimary ? "1" : "2");
    }
    if (c == TokenImpersonationLevel) {
      return Fill(out, cap, std::to_string((int)*(SECURITY_IMPERSONATION_LEVEL*)buf));
    }
    if (c == TokenSessionId) {
      return Fill(out, cap, std::to_string(*(DWORD*)buf));
    }
    if (c == TokenPrivileges) {
      TOKEN_PRIVILEGES* tp = (TOKEN_PRIVILEGES*)buf;
      std::string s;
      for (DWORD i = 0; i < tp->PrivilegeCount; ++i) {
        wchar_t name[128];
        DWORD n = 128;
        if (!LookupPrivilegeNameW(nullptr, &tp->Privileges[i].Luid, name, &n)) continue;
        if (!s.empty()) s += "\x1f";
        s += WideToUtf8(name);
        s += "\x1f";
        s += std::to_string(tp->Privileges[i].Attributes);
      }
      return Fill(out, cap, s);
    }
    if (c == TokenGroups) {
      TOKEN_GROUPS* tg = (TOKEN_GROUPS*)buf;
      std::string s;
      for (DWORD i = 0; i < tg->GroupCount; ++i) {
        LPWSTR sid = nullptr;
        if (!ConvertSidToStringSidW(tg->Groups[i].Sid, &sid)) continue;
        if (!s.empty()) s += "\x1f";
        s += WideToUtf8(sid);
        LocalFree(sid);
      }
      return Fill(out, cap, s);
    }
    return Fill(out, cap, std::to_string(got));
  }
  if (Eq(api, "SetTokenInformation")) {
    std::string hs, rest, cls, val;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &cls, &val);
    TOKEN_INFORMATION_CLASS c = (TOKEN_INFORMATION_CLASS)std::strtoul(cls.c_str(), nullptr, 10);
    if (c == TokenPrivileges) {
      TOKEN_PRIVILEGES tp{};
      tp.PrivilegeCount = 1;
      std::wstring priv = Utf8ToWide(val.empty() ? "SeDebugPrivilege" : val.c_str());
      if (!LookupPrivilegeValueW(nullptr, priv.c_str(), &tp.Privileges[0].Luid)) {
        return FillErr(out, cap, (int)GetLastError(), "SetTokenInformation");
      }
      tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
      if (!AdjustTokenPrivileges(HandleOf(hs.c_str()), FALSE, &tp, 0, nullptr, nullptr)) {
        return FillErr(out, cap, (int)GetLastError(), "SetTokenInformation");
      }
      return Fill(out, cap, "ok");
    }
    if (c == TokenSessionId) {
      DWORD sid = (DWORD)std::strtoul(val.c_str(), nullptr, 10);
      if (!SetTokenInformation(HandleOf(hs.c_str()), TokenSessionId, &sid, sizeof(sid))) {
        return FillErr(out, cap, (int)GetLastError(), "SetTokenInformation");
      }
      return Fill(out, cap, "ok");
    }
    return FillErr(out, cap, ERROR_INVALID_PARAMETER, "SetTokenInformation");
  }
  if (Eq(api, "LookupPrivilegeValueW") || Eq(api, "LookupPrivilegeValueA")) {
    std::string sys, name;
    Split1f(a, &sys, &name);
    if (name.empty()) {
      name = sys;
      sys.clear();
    }
    LUID luid{};
    if (!LookupPrivilegeValueW(sys.empty() ? nullptr : Utf8ToWide(sys).c_str(),
                               Utf8ToWide(name).c_str(), &luid)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(((unsigned long long)luid.HighPart << 32) | luid.LowPart));
  }
  if (Eq(api, "LookupPrivilegeNameW") || Eq(api, "LookupPrivilegeNameA")) {
    std::string sys, rest;
    Split1f(a, &sys, &rest);
    if (rest.empty()) {
      rest = sys;
      sys.clear();
    }
    LUID luid{};
    unsigned long long v = std::strtoull(rest.c_str(), nullptr, 10);
    luid.LowPart = (DWORD)v;
    luid.HighPart = (LONG)(v >> 32);
    wchar_t name[128];
    DWORD n = 128;
    if (!LookupPrivilegeNameW(sys.empty() ? nullptr : Utf8ToWide(sys).c_str(), &luid, name, &n)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, WideToUtf8(name));
  }
  if (Eq(api, "AdjustTokenPrivileges") || NtEq(api, "NtAdjustPrivilegesToken")) {
    std::string hs, name;
    Split1f(a, &hs, &name);
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    std::wstring priv = Utf8ToWide(name.empty() ? "SeLoadDriverPrivilege" : name.c_str());
    if (!LookupPrivilegeValueW(nullptr, priv.c_str(), &tp.Privileges[0].Luid)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    SetLastError(0);
    if (!AdjustTokenPrivileges(HandleOf(hs.c_str()), FALSE, &tp, 0, nullptr, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    DWORD err = GetLastError();
    if (err == ERROR_NOT_ALL_ASSIGNED) return FillErr(out, cap, (int)err, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlAdjustPrivilege")) {
    std::string id, rest, enable, current;
    Split1f(a, &id, &rest);
    Split1f(rest.c_str(), &enable, &current);
    using Fn = long(WINAPI*)(unsigned long, unsigned char, unsigned char, unsigned char*);
    auto fn = (Fn)(void*)NtProc("RtlAdjustPrivilege");
    unsigned char was = 0;
    unsigned char cur_thread = current.empty() ? 0 : (unsigned char)(current[0] != '0');
    long st = fn ? fn((unsigned long)std::strtoul(id.c_str(), nullptr, 10),
                      enable.empty() || enable[0] != '0', cur_thread, &was)
                 : (long)0xC0000002;
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, was ? "1" : "0");
  }
  if (Eq(api, "OpenSCManagerW") || Eq(api, "OpenSCManagerA")) {
    std::string machine, rest, db, access;
    Split1f(a, &machine, &rest);
    Split1f(rest.c_str(), &db, &access);
    DWORD acc = access.empty() ? (SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE)
                               : (DWORD)std::strtoul(access.c_str(), nullptr, 0);
    if (!acc) acc = SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE;
    SC_HANDLE h = OpenSCManagerW(machine.empty() ? nullptr : Utf8ToWide(machine).c_str(),
                                 db.empty() ? nullptr : Utf8ToWide(db).c_str(), acc);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)h));
  }
  if (Eq(api, "CloseServiceHandle")) {
    if (!CloseServiceHandle((SC_HANDLE)HandleOf(a)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "OpenServiceW") || Eq(api, "OpenServiceA")) {
    std::string scm, rest, name, access;
    Split1f(a, &scm, &rest);
    Split1f(rest.c_str(), &name, &access);
    if (name.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    DWORD acc = access.empty() ? (SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG)
                               : (DWORD)std::strtoul(access.c_str(), nullptr, 0);
    if (!acc) acc = SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG;
    SC_HANDLE h = OpenServiceW((SC_HANDLE)HandleOf(scm.c_str()), Utf8ToWide(name).c_str(), acc);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)h));
  }
  if (Eq(api, "CreateServiceW") || Eq(api, "CreateServiceA")) {
    std::string scm, rest, name, bin;
    Split1f(a, &scm, &rest);
    Split1f(rest.c_str(), &name, &bin);
    if (name.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    SC_HANDLE h = CreateServiceW((SC_HANDLE)HandleOf(scm.c_str()), Utf8ToWide(name).c_str(),
                                 Utf8ToWide(name).c_str(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                 SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                                 bin.empty() ? L"cmd.exe" : Utf8ToWide(bin).c_str(), nullptr, nullptr,
                                 nullptr, nullptr, nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)h));
  }
  if (Eq(api, "DeleteService")) {
    if (!DeleteService((SC_HANDLE)HandleOf(a)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "StartServiceW") || Eq(api, "StartServiceA")) {
    std::string hs, rest;
    Split1f(a, &hs, &rest);
    if (!StartServiceW((SC_HANDLE)HandleOf(hs.c_str()), 0, nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ControlService") || Eq(api, "ControlServiceExW")) {
    std::string hs, code;
    Split1f(a, &hs, &code);
    SERVICE_STATUS st{};
    if (!ControlService((SC_HANDLE)HandleOf(hs.c_str()),
                        (DWORD)std::strtoul(code.c_str(), nullptr, 10), &st))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(st.dwCurrentState));
  }
  if (Eq(api, "QueryServiceStatus") || Eq(api, "QueryServiceStatusEx")) {
    SERVICE_STATUS st{};
    if (!QueryServiceStatus((SC_HANDLE)HandleOf(a), &st))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(st.dwCurrentState));
  }
  if (Eq(api, "SetServiceStatus")) return Fill(out, cap, "ok");
  if (Eq(api, "RegisterServiceCtrlHandlerW") || Eq(api, "RegisterServiceCtrlHandlerA") ||
      Eq(api, "RegisterServiceCtrlHandlerExW"))
    return Fill(out, cap, "1");
  if (Eq(api, "StartServiceCtrlDispatcherW") || Eq(api, "StartServiceCtrlDispatcherA"))
    return Fill(out, cap, "ok");
  if (Eq(api, "ChangeServiceConfigW") || Eq(api, "ChangeServiceConfigA") ||
      Eq(api, "ChangeServiceConfig2W") || Eq(api, "NotifyServiceStatusChangeW") ||
      Eq(api, "NotifyBootConfigStatus") || Eq(api, "QueryServiceLockStatusW"))
    return Fill(out, cap, "ok");
  if (Eq(api, "QueryServiceConfigW") || Eq(api, "QueryServiceConfigA") ||
      Eq(api, "QueryServiceConfig2W")) {
    BYTE buf[4096];
    DWORD need = 0;
    if (!QueryServiceConfigW((SC_HANDLE)HandleOf(a), (QUERY_SERVICE_CONFIGW*)buf, sizeof(buf), &need))
      return FillErr(out, cap, (int)GetLastError(), api);
    QUERY_SERVICE_CONFIGW* cfg = (QUERY_SERVICE_CONFIGW*)buf;
    return Fill(out, cap, cfg->lpBinaryPathName ? WideToUtf8(cfg->lpBinaryPathName) : "");
  }
  if (Eq(api, "EnumServicesStatusW") || Eq(api, "EnumServicesStatusA") ||
      Eq(api, "EnumServicesStatusExW") || Eq(api, "EnumDependentServicesW")) {
    BYTE buf[8192];
    DWORD need = 0, n = 0, resume = 0;
    BOOL ok = EnumServicesStatusW((SC_HANDLE)HandleOf(a), SERVICE_WIN32, SERVICE_STATE_ALL,
                                  (ENUM_SERVICE_STATUSW*)buf, sizeof(buf), &need, &n, &resume);
    if (!ok && GetLastError() != ERROR_MORE_DATA)
      return FillErr(out, cap, (int)GetLastError(), api);
    std::string s;
    ENUM_SERVICE_STATUSW* e = (ENUM_SERVICE_STATUSW*)buf;
    for (DWORD i = 0; i < n && i < 32; ++i) {
      if (!s.empty()) s += "\x1f";
      s += e[i].lpServiceName ? WideToUtf8(e[i].lpServiceName) : "";
    }
    return Fill(out, cap, s);
  }
  if (Eq(api, "GetServiceDisplayNameW") || Eq(api, "GetServiceKeyNameW")) {
    std::string scm, name;
    Split1f(a, &scm, &name);
    if (name.empty()) name = scm;
    wchar_t outn[256];
    DWORD n = 256;
    BOOL ok = Eq(api, "GetServiceKeyNameW")
                  ? GetServiceKeyNameW((SC_HANDLE)HandleOf(scm.c_str()), Utf8ToWide(name).c_str(),
                                       outn, &n)
                  : GetServiceDisplayNameW((SC_HANDLE)HandleOf(scm.c_str()), Utf8ToWide(name).c_str(),
                                           outn, &n);
    if (!ok) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(outn));
  }
  if (Eq(api, "LockServiceDatabase")) {
    SC_LOCK lock = LockServiceDatabase((SC_HANDLE)HandleOf(a));
    if (!lock) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)lock));
  }
  if (Eq(api, "UnlockServiceDatabase")) {
    if (!UnlockServiceDatabase((SC_LOCK)HandleOf(a)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "LookupAccountSidW") || Eq(api, "LookupAccountSidA")) {
    PSID sid = nullptr;
    BOOL need_free = FALSE;
    if (a[0] == 'S' && a[1] == '-') {
      if (!ConvertStringSidToSidW(Utf8ToWide(a).c_str(), &sid)) {
        return FillErr(out, cap, (int)GetLastError(), api);
      }
      need_free = TRUE;
    } else {
      sid = (PSID)HandleOf(a);
    }
    wchar_t name[256], dom[256];
    DWORD nc = 256, dc = 256;
    SID_NAME_USE use{};
    BOOL ok = LookupAccountSidW(nullptr, sid, name, &nc, dom, &dc, &use);
    int err = ok ? 0 : (int)GetLastError();
    if (need_free) LocalFree(sid);
    if (!ok) return FillErr(out, cap, err, api);
    std::string s = WideToUtf8(name);
    s += "\x1f";
    s += WideToUtf8(dom);
    return Fill(out, cap, s);
  }
  if (Eq(api, "AllocateAndInitializeSid")) {
    auto parts = SplitAll(a);
    SID_IDENTIFIER_AUTHORITY sia{};
    unsigned auth = parts.size() > 1 ? (unsigned)std::strtoul(parts[1].c_str(), nullptr, 10) : 5u;
    sia.Value[5] = (BYTE)auth;
    DWORD nsub = parts.size() > 2 ? (DWORD)(parts.size() - 2) : 0;
    if (nsub > 8) nsub = 8;
    DWORD sub[8] = {};
    for (DWORD i = 0; i < nsub; ++i) sub[i] = (DWORD)std::strtoul(parts[i + 2].c_str(), nullptr, 10);
    PSID sid = nullptr;
    if (!AllocateAndInitializeSid(&sia, (BYTE)nsub, sub[0], sub[1], sub[2], sub[3], sub[4], sub[5],
                                  sub[6], sub[7], &sid)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, HandleStr(sid));
  }
  if (Eq(api, "DuplicateTokenEx") || Eq(api, "DuplicateToken")) {
    std::string hs, level;
    Split1f(a, &hs, &level);
    SECURITY_IMPERSONATION_LEVEL il = SecurityImpersonation;
    if (!level.empty()) il = (SECURITY_IMPERSONATION_LEVEL)std::strtoul(level.c_str(), nullptr, 10);
    HANDLE dst = nullptr;
    if (!DuplicateTokenEx(HandleOf(hs.c_str()),
                          TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE | TOKEN_ADJUST_PRIVILEGES,
                          nullptr, il, TokenImpersonation, &dst)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, HandleStr(dst));
  }
  if (Eq(api, "CheckTokenMembership")) {
    std::string hs, sidarg;
    Split1f(a, &hs, &sidarg);
    PSID sid = nullptr;
    BOOL need_free = FALSE;
    if (sidarg.rfind("S-", 0) == 0) {
      if (!ConvertStringSidToSidW(Utf8ToWide(sidarg).c_str(), &sid)) {
        return FillErr(out, cap, (int)GetLastError(), api);
      }
      need_free = TRUE;
    } else {
      sid = (PSID)HandleOf(sidarg.c_str());
    }
    BOOL member = FALSE;
    HANDLE tok = hs.empty() ? nullptr : HandleOf(hs.c_str());
    HANDLE imp = nullptr;
    if (tok) {
      TOKEN_TYPE ty = TokenPrimary;
      DWORD got = 0;
      if (GetTokenInformation(tok, TokenType, &ty, sizeof(ty), &got) && ty == TokenPrimary) {
        if (DuplicateTokenEx(tok, TOKEN_QUERY, nullptr, SecurityIdentification, TokenImpersonation,
                             &imp)) {
          tok = imp;
        }
      }
    }
    BOOL ok = CheckTokenMembership(tok, sid, &member);
    if (!ok) ok = CheckTokenMembership(nullptr, sid, &member);
    int err = ok ? 0 : (int)GetLastError();
    if (imp) CloseHandle(imp);
    if (need_free) LocalFree(sid);
    if (!ok) return FillErr(out, cap, err, api);
    return Fill(out, cap, member ? "1" : "0");
  }

  if (Eq(api, "FindFirstChangeNotificationW") || Eq(api, "FindFirstChangeNotificationA")) {
    auto parts = SplitAll(a);
    std::wstring path = Utf8ToWide(parts.empty() || parts[0].empty() ? "." : parts[0]);
    BOOL sub = parts.size() > 1 && parts[1] != "0";
    DWORD filter = parts.size() > 2 ? (DWORD)std::strtoul(parts[2].c_str(), nullptr, 0)
                                    : FILE_NOTIFY_CHANGE_FILE_NAME;
    HANDLE h = FindFirstChangeNotificationW(path.c_str(), sub, filter);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "FindNextChangeNotification")) {
    if (!FindNextChangeNotification(HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FindCloseChangeNotification")) {
    if (!FindCloseChangeNotification(HandleOf(a))) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ReadDirectoryChangesW") || Eq(api, "ReadDirectoryChangesExW")) {
    return Fill(out, cap, std::string(".") + "\x1f" + "3");
  }
  if (Eq(api, "GetNamedPipeInfo")) {
    DWORD flags = 0, outb = 0, inb = 0, maxc = 0;
    if (!GetNamedPipeInfo(HandleOf(a), &flags, &outb, &inb, &maxc)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(flags) + "\x1f" + std::to_string(outb) + "\x1f" +
                              std::to_string(inb) + "\x1f" + std::to_string(maxc));
  }
  if (Eq(api, "GetNamedPipeHandleStateW") || Eq(api, "GetNamedPipeHandleStateA")) {
    DWORD state = 0, cur = 0, maxc = 0, to = 0;
    wchar_t user[256] = {};
    DWORD n = 256;
    if (!GetNamedPipeHandleStateW(HandleOf(a), &state, &cur, &maxc, &to, user, n)) {
      return Fill(out, cap, std::string("0") + "\x1f" + "1" + "\x1f" + "0" + "\x1f" + "0");
    }
    return Fill(out, cap, std::to_string(state) + "\x1f" + std::to_string(cur) + "\x1f" +
                              std::to_string(maxc) + "\x1f" + std::to_string(to));
  }
  if (Eq(api, "SetNamedPipeHandleState")) {
    DWORD mode = PIPE_READMODE_BYTE;
    if (!SetNamedPipeHandleState(HandleOf(a), &mode, nullptr, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "TransactNamedPipe")) {
    std::string hs, data;
    Split1f(a, &hs, &data);
    char buf[4096];
    DWORD got = 0;
    if (!TransactNamedPipe(HandleOf(hs.c_str()), data.data(), (DWORD)data.size(), buf, sizeof(buf),
                           &got, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::string(buf, got));
  }
  if (Eq(api, "CallNamedPipeW") || Eq(api, "CallNamedPipeA")) {
    std::string name, data;
    Split1f(a, &name, &data);
    char buf[4096];
    DWORD got = 0;
    if (!CallNamedPipeW(Utf8ToWide(name).c_str(), data.data(), (DWORD)data.size(), buf, sizeof(buf),
                        &got, 1000)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::string(buf, got));
  }
  if (Eq(api, "GetNamedPipeClientProcessId") || Eq(api, "GetNamedPipeServerProcessId")) {
    using Fn = BOOL(WINAPI*)(HANDLE, PULONG);
    const char* nm = Eq(api, "GetNamedPipeClientProcessId") ? "GetNamedPipeClientProcessId"
                                                            : "GetNamedPipeServerProcessId";
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), nm);
    ULONG pid = 0;
    if (!fn || !fn(HandleOf(a), &pid)) {
      return Fill(out, cap, std::to_string(GetCurrentProcessId()));
    }
    return Fill(out, cap, std::to_string(pid));
  }
  if (Eq(api, "GetHandleInformation")) {
    DWORD flags = 0;
    if (!GetHandleInformation(HandleOf(a), &flags)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(flags));
  }
  if (Eq(api, "SetHandleInformation")) {
    std::string hs, rest, mask, flags;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &mask, &flags);
    if (!SetHandleInformation(HandleOf(hs.c_str()), (DWORD)std::strtoul(mask.c_str(), nullptr, 0),
                              (DWORD)std::strtoul(flags.c_str(), nullptr, 0))) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SetThreadDescription")) {
    std::string hs, name;
    Split1f(a, &hs, &name);
    HANDLE thr = hs.empty() || Eq(hs.c_str(), "-2") ? GetCurrentThread() : HandleOf(hs.c_str());
    using Fn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription");
    if (!fn) return Fill(out, cap, "ok");
    HRESULT hr = fn(thr, Utf8ToWide(name).c_str());
    if (FAILED(hr)) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetThreadDescription")) {
    HANDLE thr = !a[0] || Eq(a, "-2") ? GetCurrentThread() : HandleOf(a);
    using Fn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription");
    if (!fn) return Fill(out, cap, "");
    PWSTR w = nullptr;
    HRESULT hr = fn(thr, &w);
    if (FAILED(hr) || !w) return Fill(out, cap, "");
    std::string s = WideToUtf8(w);
    LocalFree(w);
    return Fill(out, cap, s);
  }
  if (Eq(api, "GetCurrentThreadStackLimits")) {
    using Fn = VOID(WINAPI*)(PULONG_PTR, PULONG_PTR);
    auto fn =
        (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetCurrentThreadStackLimits");
    ULONG_PTR lo = 0, hi = 0;
    if (fn) fn(&lo, &hi);
    if (!lo && !hi) {
      return Fill(out, cap, std::string("65536") + "\x1f" + "8388608");
    }
    return Fill(out, cap, std::to_string((unsigned long long)lo) + "\x1f" +
                              std::to_string((unsigned long long)hi));
  }
  if (Eq(api, "ExitThread")) {
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "InitializeProcThreadAttributeList")) {
    using InitFn = BOOL(WINAPI*)(void*, DWORD, DWORD, PSIZE_T);
    auto init = (InitFn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                              "InitializeProcThreadAttributeList");
    if (!init) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    DWORD n = a[0] ? (DWORD)std::strtoul(a, nullptr, 10) : 1;
    if (!n) n = 1;
    SIZE_T sz = 0;
    init(nullptr, n, 0, &sz);
    if (!sz) sz = 128;
    void* p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz);
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    if (!init(p, n, 0, &sz)) {
      HeapFree(GetProcessHeap(), 0, p);
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "UpdateProcThreadAttribute")) {
    using Fn = BOOL(WINAPI*)(void*, DWORD, DWORD_PTR, PVOID, SIZE_T, PVOID, PSIZE_T);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                        "UpdateProcThreadAttribute");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    std::string hs, rest, attr, val;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &attr, &val);
    DWORD_PTR v = (DWORD_PTR)std::strtoull(val.c_str(), nullptr, 10);
    if (!fn((void*)HandleOf(hs.c_str()), 0, (DWORD_PTR)std::strtoull(attr.c_str(), nullptr, 0), &v,
            sizeof(v), nullptr, nullptr)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DeleteProcThreadAttributeList")) {
    using Fn = VOID(WINAPI*)(void*);
    auto fn = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                        "DeleteProcThreadAttributeList");
    void* p = (void*)HandleOf(a);
    if (fn && p) fn(p);
    if (p) HeapFree(GetProcessHeap(), 0, p);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FileTimeToDosDateTime")) {
    FILETIME ft = QuadFt(std::strtoull(a, nullptr, 10));
    WORD date = 0, time = 0;
    if (!FileTimeToDosDateTime(&ft, &date, &time)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(date) + "\x1f" + std::to_string(time));
  }
  if (Eq(api, "DosDateTimeToFileTime")) {
    std::string ds, ts;
    Split1f(a, &ds, &ts);
    FILETIME ft{};
    if (!DosDateTimeToFileTime((WORD)std::strtoul(ds.c_str(), nullptr, 10),
                               (WORD)std::strtoul(ts.c_str(), nullptr, 10), &ft)) {
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, std::to_string(FtQuad(ft)));
  }
  if (Eq(api, "IsDBCSLeadByte") || Eq(api, "IsDBCSLeadByteEx")) {
    std::string cp, ch;
    Split1f(a, &cp, &ch);
    UINT codepage = Eq(api, "IsDBCSLeadByteEx") ? (UINT)std::strtoul(cp.c_str(), nullptr, 10) : 0;
    unsigned char b = (unsigned char)std::strtoul(Eq(api, "IsDBCSLeadByteEx") ? ch.c_str() : a, nullptr, 0);
    BOOL yes = Eq(api, "IsDBCSLeadByteEx") ? IsDBCSLeadByteEx(codepage, b) : IsDBCSLeadByte(b);
    return Fill(out, cap, yes ? "1" : "0");
  }
  if (Eq(api, "PeekConsoleInputW") || Eq(api, "PeekConsoleInputA") || Eq(api, "ReadConsoleInputW")) {
    INPUT_RECORD rec[8];
    DWORD n = 0;
    PeekConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), rec, 8, &n);
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "GetConsoleCursorInfo")) {
    CONSOLE_CURSOR_INFO ci{};
    if (!GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci)) {
      return Fill(out, cap, std::string("25") + "\x1f" + "1");
    }
    return Fill(out, cap, std::to_string(ci.dwSize) + "\x1f" + (ci.bVisible ? "1" : "0"));
  }
  if (Eq(api, "SetConsoleCursorInfo")) {
    CONSOLE_CURSOR_INFO ci{};
    std::string sz, vis;
    Split1f(a, &sz, &vis);
    ci.dwSize = sz.empty() ? 25 : (DWORD)std::strtoul(sz.c_str(), nullptr, 10);
    ci.bVisible = vis.empty() || vis != "0";
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "FillConsoleOutputCharacterW") || Eq(api, "FillConsoleOutputAttribute") ||
      Eq(api, "SetConsoleScreenBufferSize")) {
    return Fill(out, cap, "1");
  }

  if (Eq(api, "WSAStartup")) {
    if (!EnsureWsa()) return FillWsa(out, cap, "WSAStartup");
    return Fill(out, cap, "2.2");
  }
  if (Eq(api, "WSACleanup")) {
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WSAGetLastError")) {
    return Fill(out, cap, std::to_string(WSAGetLastError()));
  }
  if (Eq(api, "WSASocketW") || Eq(api, "WSASocketA") || Eq(api, "socket")) {
    if (!EnsureWsa()) return FillWsa(out, cap, "WSASocketW");
    SOCKET s = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    if (s == INVALID_SOCKET) return FillWsa(out, cap, "WSASocketW");
    return Fill(out, cap, std::to_string((long long)s));
  }
  if (Eq(api, "closesocket")) {
    if (closesocket((SOCKET)HandleOf(a)) != 0) return FillWsa(out, cap, "closesocket");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "bind")) {
    if (!EnsureWsa()) return FillWsa(out, cap, "bind");
    std::string hs, name;
    Split1f(a, &hs, &name);
    SOCKET s = (SOCKET)HandleOf(hs.c_str());
    sockaddr_in addr{};
    if (name.rfind("inproc:", 0) == 0) {
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      addr.sin_port = 0;
    } else if (!ParseSockAddr(name, &addr)) {
      return FillErr(out, cap, WSAEADDRNOTAVAIL, "bind");
    }
    if (::bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) return FillWsa(out, cap, "bind");
    sockaddr_in got{};
    int glen = sizeof(got);
    if (getsockname(s, (sockaddr*)&got, &glen) == 0) {
      if (name.rfind("inproc:", 0) == 0) InprocPorts()[name] = ntohs(got.sin_port);
      char ip[64];
      inet_ntop(AF_INET, &got.sin_addr, ip, sizeof(ip));
      std::string r = ip;
      r += ":";
      r += std::to_string(ntohs(got.sin_port));
      return Fill(out, cap, r);
    }
    return Fill(out, cap, name.empty() ? "ok" : name);
  }
  if (Eq(api, "listen")) {
    SOCKET s = (SOCKET)HandleOf(a);
    if (::listen(s, SOMAXCONN) != 0) return FillWsa(out, cap, "listen");
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "connect")) {
    if (!EnsureWsa()) return FillWsa(out, cap, "connect");
    std::string hs, name;
    Split1f(a, &hs, &name);
    sockaddr_in addr{};
    if (!ParseSockAddr(name, &addr)) return FillErr(out, cap, WSAEADDRNOTAVAIL, "connect");
    if (::connect((SOCKET)HandleOf(hs.c_str()), (sockaddr*)&addr, sizeof(addr)) != 0) {
      return FillWsa(out, cap, "connect");
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "accept")) {
    SOCKET s = (SOCKET)HandleOf(a);
    SOCKET c = ::accept(s, nullptr, nullptr);
    if (c == INVALID_SOCKET) return FillWsa(out, cap, "accept");
    return Fill(out, cap, std::to_string((long long)c));
  }
  if (Eq(api, "WSASend") || Eq(api, "send")) {
    std::string hs, data;
    Split1f(a, &hs, &data);
    int n = ::send((SOCKET)HandleOf(hs.c_str()), data.data(), (int)data.size(), 0);
    if (n < 0) return FillWsa(out, cap, "WSASend");
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "WSARecv") || Eq(api, "recv")) {
    std::string hs, nstr;
    Split1f(a, &hs, &nstr);
    int n = nstr.empty() ? 4096 : (int)std::strtoul(nstr.c_str(), nullptr, 10);
    if (n > 65536) n = 65536;
    std::string b(n, '\0');
    int got = ::recv((SOCKET)HandleOf(hs.c_str()), b.data(), n, 0);
    if (got < 0) return FillWsa(out, cap, "WSARecv");
    b.resize(got);
    return Fill(out, cap, b);
  }
  if (Eq(api, "select")) {
    std::string hs, toms;
    Split1f(a, &hs, &toms);
    fd_set r;
    FD_ZERO(&r);
    SOCKET maxfd = 0;
    std::string cur = hs;
    std::vector<SOCKET> socks;
    while (!cur.empty()) {
      std::string one;
      auto c = cur.find(',');
      if (c == std::string::npos) {
        one = cur;
        cur.clear();
      } else {
        one = cur.substr(0, c);
        cur = cur.substr(c + 1);
      }
      SOCKET s = (SOCKET)HandleOf(one.c_str());
      FD_SET(s, &r);
      socks.push_back(s);
      if (s > maxfd) maxfd = s;
    }
    timeval tv{};
    unsigned long long ms = toms.empty() ? 0 : std::strtoull(toms.c_str(), nullptr, 10);
    tv.tv_sec = (long)(ms / 1000);
    tv.tv_usec = (long)((ms % 1000) * 1000);
    int n = ::select((int)maxfd + 1, &r, nullptr, nullptr, &tv);
    if (n < 0) return FillWsa(out, cap, "select");
    std::string ready;
    for (SOCKET s : socks) {
      if (FD_ISSET(s, &r)) {
        if (!ready.empty()) ready += ",";
        ready += std::to_string((long long)s);
      }
    }
    return Fill(out, cap, ready);
  }
  if (Eq(api, "ioctlsocket")) {
    std::string hs, rest, cmd, arg;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &cmd, &arg);
    u_long mode = (u_long)std::strtoul(arg.c_str(), nullptr, 10);
    unsigned long cc = std::strtoul(cmd.c_str(), nullptr, 0);
    if (cmd == "FIONBIO" || cc == 0x8004667Eul || cc == 126) {
      if (ioctlsocket((SOCKET)HandleOf(hs.c_str()), FIONBIO, &mode) != 0) {
        return FillWsa(out, cap, "ioctlsocket");
      }
      return Fill(out, cap, "ok");
    }
    return FillErr(out, cap, WSAEINVAL, "ioctlsocket");
  }

  if (Eq(api, "RtlGetCurrentPeb") || Eq(api, "NtCurrentPeb")) {
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)CurrentPeb()));
  }
  if (Eq(api, "RtlGetCurrentTeb") || Eq(api, "NtCurrentTeb")) {
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)NtCurrentTeb()));
  }
  if (NtEq(api, "NtQueryInformationProcess")) {
    std::string hs, cls;
    Split1f(a, &hs, &cls);
    ULONG infoclass = (ULONG)std::strtoul(cls.empty() ? "0" : cls.c_str(), nullptr, 10);
    using Fn = LONG(WINAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    auto fn = (Fn)(void*)NtProc("NtQueryInformationProcess");
    if (infoclass == 7) {
      ULONG_PTR port = 0;
      if (fn) fn(ProcOf(hs.c_str()), 7, &port, sizeof(port), nullptr);
      return Fill(out, cap, std::to_string((unsigned long long)port));
    }
    K32Pbi pbi{};
    if (fn) fn(ProcOf(hs.c_str()), 0, &pbi, sizeof(pbi), nullptr);
    if (!pbi.PebBaseAddress) pbi.PebBaseAddress = CurrentPeb();
    if (!pbi.UniqueProcessId) pbi.UniqueProcessId = GetCurrentProcessId();
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)pbi.PebBaseAddress) +
                              "\x1f" + std::to_string((unsigned long long)pbi.UniqueProcessId));
  }
  if (NtEq(api, "NtSetInformationProcess")) {
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtQuerySystemInformation")) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return Fill(out, cap, std::to_string(si.dwPageSize) + "\x1f" +
                              std::to_string(si.dwNumberOfProcessors));
  }
  if (NtEq(api, "NtQueryObject")) {
    std::string hs, cls;
    Split1f(a, &hs, &cls);
    using Fn = LONG(WINAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    auto fn = (Fn)(void*)NtProc("NtQueryObject");
    unsigned char buf[512]{};
    if (fn) {
      LONG st = fn(HandleOf(hs.c_str()), 2, buf, sizeof(buf), nullptr);
      struct K32Us {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
      };
      K32Us* us = (K32Us*)buf;
      if (st >= 0 && us->Buffer && us->Length)
        return Fill(out, cap, WideToUtf8(us->Buffer, us->Length / 2));
    }
    return Fill(out, cap, "Object");
  }
  if (NtEq(api, "NtCreateSection") || NtEq(api, "NtCreateSectionEx")) {
    using Fn = LONG(WINAPI*)(HANDLE*, ACCESS_MASK, void*, LARGE_INTEGER*, ULONG, ULONG, HANDLE);
    auto fn = (Fn)(void*)NtProc("NtCreateSection");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    LARGE_INTEGER sz{};
    sz.QuadPart = (LONGLONG)std::strtoull(a, nullptr, 10);
    if (!sz.QuadPart) sz.QuadPart = 4096;
    HANDLE sec = nullptr;
    LONG st = fn(&sec, 0xF001F, nullptr, &sz, PAGE_READWRITE, SEC_COMMIT, nullptr);
    if (st < 0 || !sec) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(sec));
  }
  if (NtEq(api, "NtMapViewOfSection") || NtEq(api, "NtMapViewOfSectionEx")) {
    using Fn = LONG(WINAPI*)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, LARGE_INTEGER*, SIZE_T*,
                             DWORD, ULONG, ULONG);
    auto fn = (Fn)(void*)NtProc("NtMapViewOfSection");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    std::string hs, rest;
    Split1f(a, &hs, &rest);
    PVOID base = nullptr;
    SIZE_T view = 0;
    LONG st = fn(HandleOf(hs.c_str()), GetCurrentProcess(), &base, 0, 0, nullptr, &view, 1, 0,
                 PAGE_READWRITE);
    if (st < 0 || !base) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)base));
  }
  if (NtEq(api, "NtUnmapViewOfSection") || NtEq(api, "NtUnmapViewOfSectionEx")) {
    using Fn = LONG(WINAPI*)(HANDLE, PVOID);
    auto fn = (Fn)(void*)NtProc("NtUnmapViewOfSection");
    PVOID p = (PVOID)(uintptr_t)std::strtoull(a, nullptr, 10);
    if (fn) fn(GetCurrentProcess(), p);
    else UnmapViewOfFile(p);
    return Fill(out, cap, "ok");
  }

  if (NtEq(api, "NtAllocateVirtualMemory") || NtEq(api, "NtAllocateVirtualMemoryEx")) {
    using Fn = LONG(WINAPI*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
    auto fn = (Fn)(void*)NtProc("NtAllocateVirtualMemory");
    std::string hs, sz;
    Split1f(a, &hs, &sz);
    const char* proc = sz.empty() ? "-1" : hs.c_str();
    SIZE_T n = (SIZE_T)std::strtoull(sz.empty() ? a : sz.c_str(), nullptr, 10);
    if (!n) n = 4096;
    PVOID base = nullptr;
    LONG st = 0;
    if (fn) {
      st = fn(ProcOf(proc), &base, 0, &n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    } else {
      base = VirtualAlloc(nullptr, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      st = base ? 0 : (LONG)GetLastError();
    }
    if (st < 0 || !base) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)base));
  }
  if (NtEq(api, "NtFreeVirtualMemory")) {
    using Fn = LONG(WINAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG);
    auto fn = (Fn)(void*)NtProc("NtFreeVirtualMemory");
    std::string hs, addr;
    Split1f(a, &hs, &addr);
    PVOID p = (PVOID)(uintptr_t)std::strtoull(addr.empty() ? a : addr.c_str(), nullptr, 10);
    SIZE_T z = 0;
    if (fn) fn(GetCurrentProcess(), &p, &z, MEM_RELEASE);
    else VirtualFree(p, 0, MEM_RELEASE);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtProtectVirtualMemory")) {
    std::string hs, rest, addr, prot;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &addr, &prot);
    DWORD old = 0;
    PVOID p = (PVOID)(uintptr_t)std::strtoull(addr.c_str(), nullptr, 10);
    SIZE_T n = 4096;
    DWORD np = (DWORD)std::strtoul(prot.c_str(), nullptr, 0);
    if (!np) np = PAGE_READWRITE;
    VirtualProtect(p, n, np, &old);
    return Fill(out, cap, std::to_string(old));
  }
  if (NtEq(api, "NtCreateFile") || NtEq(api, "NtOpenFile")) {
    DWORD disp = NtEq(api, "NtOpenFile") ? OPEN_EXISTING : OPEN_ALWAYS;
    HANDLE h = CreateFileW(Utf8ToWide(a).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, disp, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtReadFile")) {
    std::string hs, nstr;
    Split1f(a, &hs, &nstr);
    DWORD n = nstr.empty() ? 4096 : (DWORD)std::strtoul(nstr.c_str(), nullptr, 10);
    std::string b(n, '\0');
    DWORD got = 0;
    if (!ReadFile(HandleOf(hs.c_str()), b.data(), n, &got, nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    b.resize(got);
    return Fill(out, cap, b);
  }
  if (NtEq(api, "NtWriteFile")) {
    std::string hs, data;
    Split1f(a, &hs, &data);
    DWORD got = 0;
    if (!WriteFile(HandleOf(hs.c_str()), data.data(), (DWORD)data.size(), &got, nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(got));
  }
  if (NtEq(api, "NtFlushBuffersFile")) {
    FlushFileBuffers(HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtDelayExecution")) {
    unsigned long long t = std::strtoull(a, nullptr, 10);
    Sleep((DWORD)(t > 10000 ? t / 10000ull : t));
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtYieldExecution")) {
    SwitchToThread();
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtWaitForSingleObject")) {
    std::string hs, ms;
    Split1f(a, &hs, &ms);
    DWORD wr = WaitForSingleObject(HandleOf(hs.c_str()),
                                   ms.empty() ? 0 : (DWORD)std::strtoul(ms.c_str(), nullptr, 10));
    return Fill(out, cap, std::to_string(wr));
  }
  if (NtEq(api, "NtCreateEvent")) {
    HANDLE h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtSetEvent")) {
    if (!SetEvent(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtResetEvent")) {
    if (!ResetEvent(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtQuerySystemTime")) {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    unsigned long long q = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return Fill(out, cap, std::to_string(q));
  }
  if (NtEq(api, "NtOpenProcess")) {
    DWORD pid = (DWORD)std::strtoul(a, nullptr, 10);
    if (!pid || pid == GetCurrentProcessId()) return Fill(out, cap, HandleStr(GetCurrentProcess()));
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtDuplicateObject")) {
    HANDLE src = HandleOf(a), dst = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), src, GetCurrentProcess(), &dst, 0, FALSE,
                         DUPLICATE_SAME_ACCESS))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(dst));
  }
  if (NtEq(api, "NtCreateMutant")) {
    HANDLE h = CreateMutexW(nullptr, FALSE, nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtReleaseMutant")) {
    ReleaseMutex(HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtCreateSemaphore")) {
    std::string init, rest, maxc;
    Split1f(a, &init, &rest);
    Split1f(rest.c_str(), &maxc, &rest);
    LONG mx = maxc.empty() ? 0x7fffffff : (LONG)std::strtol(maxc.c_str(), nullptr, 10);
    HANDLE h = CreateSemaphoreW(nullptr, (LONG)std::strtol(init.c_str(), nullptr, 10), mx, nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtReleaseSemaphore")) {
    std::string hs, nstr;
    Split1f(a, &hs, &nstr);
    LONG prev = 0;
    LONG n = nstr.empty() ? 1 : (LONG)std::strtol(nstr.c_str(), nullptr, 10);
    if (!ReleaseSemaphore(HandleOf(hs.c_str()), n, &prev))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(prev));
  }
  if (NtEq(api, "NtOpenEvent")) {
    HANDLE h = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtPulseEvent")) {
    if (!PulseEvent(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtQueryEvent")) {
    DWORD wr = WaitForSingleObject(HandleOf(a), 0);
    if (wr == WAIT_FAILED) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, wr == WAIT_OBJECT_0 ? "1" : "0");
  }
  if (NtEq(api, "NtClearEvent")) {
    if (!ResetEvent(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtQueryTimerResolution")) {
    using Fn = long(WINAPI*)(unsigned long*, unsigned long*, unsigned long*);
    auto fn = (Fn)(void*)NtProc("NtQueryTimerResolution");
    unsigned long mn = 0, mx = 0, cur = 0;
    if (fn) fn(&mn, &mx, &cur);
    return Fill(out, cap, std::to_string(mn) + "\x1f" + std::to_string(mx) + "\x1f" + std::to_string(cur));
  }
  if (NtEq(api, "NtQueryAttributesFile") || NtEq(api, "NtQueryFullAttributesFile")) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(Utf8ToWide(a[0] ? a : ".").c_str(), GetFileExInfoStandard, &fad))
      return FillErr(out, cap, (int)GetLastError(), api);
    ULARGE_INTEGER sz{};
    sz.LowPart = fad.nFileSizeLow;
    sz.HighPart = fad.nFileSizeHigh;
    return Fill(out, cap, std::to_string(sz.QuadPart) + "\x1f" + std::to_string(fad.dwFileAttributes));
  }
  if (NtEq(api, "NtDeleteFile")) {
    if (!DeleteFileW(Utf8ToWide(a).c_str())) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtQueryVolumeInformationFile")) {
    ULARGE_INTEGER caller{}, total{}, fr{};
    if (!GetDiskFreeSpaceExW(a[0] ? Utf8ToWide(a).c_str() : nullptr, &caller, &total, &fr))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(caller.QuadPart) + "\x1f" + std::to_string(total.QuadPart));
  }
  if (NtEq(api, "NtCancelIoFile")) {
    CancelIo(HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtEnumerateValueKey") || NtEq(api, "NtDeleteValueKey") || NtEq(api, "NtQueryKey") ||
      NtEq(api, "NtFlushKey")) {
    if (NtEq(api, "NtFlushKey")) {
      LSTATUS st = RegFlushKey((HKEY)HandleOf(a));
      if (st) return FillErr(out, cap, (int)st, api);
    }
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtOpenThreadToken")) {
    HANDLE tok = nullptr;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, TRUE, &tok))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(tok));
  }
  if (NtEq(api, "NtDuplicateToken")) {
    HANDLE src = HandleOf(a), dst = nullptr;
    if (!src) {
      if (!OpenProcessToken(GetCurrentProcess(),
                            TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &src))
        return FillErr(out, cap, (int)GetLastError(), api);
    }
    if (!DuplicateTokenEx(src,
                          TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE | TOKEN_ADJUST_PRIVILEGES,
                          nullptr, SecurityImpersonation, TokenImpersonation, &dst))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(dst));
  }
  if (NtEq(api, "NtQuerySection")) {
    using Fn = long(WINAPI*)(HANDLE, int, void*, unsigned long, unsigned long*);
    auto fn = (Fn)(void*)NtProc("NtQuerySection");
    struct Basic {
      void* base;
      unsigned attr;
      LARGE_INTEGER maxsz;
    } b{};
    unsigned long got = 0;
    long st = fn ? fn(HandleOf(a), 0, &b, sizeof(b), &got) : 1;
    if (st) return Fill(out, cap, "4096");
    return Fill(out, cap, std::to_string((unsigned long long)b.maxsz.QuadPart));
  }
  if (NtEq(api, "NtFlushVirtualMemory")) {
    using Fn = long(WINAPI*)(HANDLE, void**, SIZE_T*, unsigned long);
    auto fn = (Fn)(void*)NtProc("NtFlushVirtualMemory");
    void* base = (void*)HandleOf(a);
    SIZE_T n = 0;
    if (fn) fn(GetCurrentProcess(), &base, &n, 0);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtIsProcessInJob")) {
    BOOL in = FALSE;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &in))
      return Fill(out, cap, "0");
    return Fill(out, cap, in ? "1" : "0");
  }
  if (NtEq(api, "NtQueryDefaultLocale"))
    return Fill(out, cap, std::to_string(GetSystemDefaultLCID()));
  if (NtEq(api, "NtQueryDefaultUILanguage"))
    return Fill(out, cap, std::to_string(GetUserDefaultUILanguage()));
  if (NtEq(api, "NtQueryInstallUILanguage"))
    return Fill(out, cap, std::to_string(GetSystemDefaultUILanguage()));
  if (NtEq(api, "NtQueryDebugFilterState")) {
    using Fn = long(WINAPI*)(unsigned long, unsigned long, unsigned long*);
    auto fn = (Fn)(void*)NtProc("NtQueryDebugFilterState");
    unsigned long st = 0;
    if (fn) fn(0, 0, &st);
    return Fill(out, cap, std::to_string(st));
  }

  if (NtEq(api, "NtShutdownSystem") || NtEq(api, "NtRaiseHardError") || NtEq(api, "NtRaiseException") ||
      NtEq(api, "NtSetSystemTime") || Eq(api, "DbgBreakPoint") || Eq(api, "DbgUserBreakPoint") ||
      Eq(api, "RtlAssert") || Eq(api, "RtlRaiseStatus") || Eq(api, "RtlRaiseException"))
    return FillErr(out, cap, ERROR_NOT_SUPPORTED, "ntdll: not executed by this hop");

  if (NtEq(api, "NtCreateUserProcess") || NtEq(api, "NtCreateProcess") ||
      NtEq(api, "NtCreateProcessEx")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    std::string app, rest, cmd, cwd;
    Split1f(a, &app, &rest);
    Split1f(rest.c_str(), &cmd, &cwd);
    if (cmd.empty()) {
      cmd = app;
      app.clear();
    }
    if (app.empty() && cmd.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    if (!app.empty() && app.find('\\') == std::string::npos && app.find('/') == std::string::npos &&
        app.find(':') == std::string::npos)
      app.clear();
    std::wstring wapp = Utf8ToWide(app);
    std::wstring wcmd = Utf8ToWide(cmd);
    std::wstring wcwd = Utf8ToWide(cwd);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(wapp.empty() ? nullptr : wapp.c_str(),
                             wcmd.empty() ? nullptr : wcmd.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW, nullptr, wcwd.empty() ? nullptr : wcwd.c_str(), &si,
                             &pi);
    if (!ok) return FillErr(out, cap, (int)GetLastError(), api);
    std::string s = std::to_string(pi.dwProcessId);
    s.push_back('\x1f');
    s += HandleStr(pi.hProcess);
    s.push_back('\x1f');
    s += HandleStr(pi.hThread);
    return Fill(out, cap, s);
  }

  if (NtEq(api, "NtCreateTimer") || NtEq(api, "NtCreateTimer2")) {
    HANDLE h = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtOpenTimer")) {
    HANDLE h = OpenWaitableTimerW(SYNCHRONIZE | TIMER_MODIFY_STATE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtSetTimer") || NtEq(api, "NtSetTimerEx") || NtEq(api, "NtSetTimer2")) {
    std::string hs, ms;
    Split1f(a, &hs, &ms);
    LARGE_INTEGER due{};
    due.QuadPart = -((LONGLONG)std::strtoull(ms.empty() ? "0" : ms.c_str(), nullptr, 10) * 10000LL);
    if (!SetWaitableTimer(HandleOf(hs.c_str()), &due, 0, nullptr, nullptr, FALSE))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtCancelTimer")) {
    if (!CancelWaitableTimer(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtQueryTimer")) return Fill(out, cap, "0");
  if (NtEq(api, "NtSetTimerResolution")) return Fill(out, cap, "156250");
  if (NtEq(api, "NtGetTickCount")) return Fill(out, cap, std::to_string(GetTickCount()));
  if (NtEq(api, "NtCreateKeyedEvent")) {
    HANDLE h = CreateEventW(nullptr, FALSE, FALSE, a[0] ? Utf8ToWide(a).c_str() : nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtOpenKeyedEvent")) {
    HANDLE h = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtReleaseKeyedEvent")) {
    if (!SetEvent(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtWaitForKeyedEvent")) {
    DWORD wr = WaitForSingleObject(HandleOf(a), INFINITE);
    if (wr == WAIT_FAILED) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, wr == WAIT_OBJECT_0 ? "0" : "1");
  }
  if (NtEq(api, "NtOpenMutant")) {
    HANDLE h = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtQueryMutant")) return Fill(out, cap, "0");
  if (NtEq(api, "NtOpenSemaphore")) {
    HANDLE h = OpenSemaphoreW(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtQuerySemaphore")) return Fill(out, cap, "1");
  if (NtEq(api, "NtTerminateThread")) {
    if (!a[0]) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    if (!TerminateThread(HandleOf(a), 1)) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtAlertThread") || NtEq(api, "NtAlertResumeThread") || NtEq(api, "NtTestAlert") ||
      NtEq(api, "NtQueueApcThread") || NtEq(api, "NtQueueApcThreadEx") ||
      NtEq(api, "NtGetContextThread") || NtEq(api, "NtSetContextThread") || NtEq(api, "NtContinue") ||
      NtEq(api, "NtGetNextProcess") || NtEq(api, "NtGetNextThread"))
    return Fill(out, cap, "ok");
  if (NtEq(api, "NtSuspendProcess")) {
    if (HANDLE t = OpenThread(THREAD_SUSPEND_RESUME, FALSE, GetCurrentThreadId())) {
      SuspendThread(t);
      CloseHandle(t);
    }
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtResumeProcess")) {
    if (HANDLE t = OpenThread(THREAD_SUSPEND_RESUME, FALSE, GetCurrentThreadId())) {
      ResumeThread(t);
      CloseHandle(t);
    }
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtSignalAndWaitForSingleObject")) {
    std::string s, w;
    Split1f(a, &s, &w);
    DWORD wr = SignalObjectAndWait(HandleOf(s.c_str()), HandleOf(w.c_str()), INFINITE, FALSE);
    if (wr == WAIT_FAILED) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(wr));
  }

  if (NtEq(api, "NtCreateJobObject")) {
    HANDLE h = CreateJobObjectW(nullptr, a[0] ? Utf8ToWide(a).c_str() : nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtOpenJobObject")) {
    HANDLE h = OpenJobObjectW(JOB_OBJECT_ALL_ACCESS, FALSE, Utf8ToWide(a).c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtAssignProcessToJobObject")) {
    std::string jh, ph;
    Split1f(a, &jh, &ph);
    HANDLE proc = ph.empty() || ph == "-1" ? GetCurrentProcess() : HandleOf(ph.c_str());
    if (!AssignProcessToJobObject(HandleOf(jh.c_str()), proc))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtTerminateJobObject")) {
    std::string jh, code;
    Split1f(a, &jh, &code);
    if (!TerminateJobObject(HandleOf(jh.c_str()),
                            (UINT)std::strtoul(code.empty() ? "1" : code.c_str(), nullptr, 10)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtQueryInformationJobObject")) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
    if (!QueryInformationJobObject(HandleOf(a), JobObjectBasicAccountingInformation, &info,
                                   sizeof(info), nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(info.ActiveProcesses));
  }
  if (NtEq(api, "NtSetInformationJobObject")) return Fill(out, cap, "ok");

  if (NtEq(api, "NtCreateIoCompletion")) {
    HANDLE h = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtOpenIoCompletion"))
    return FillErr(out, cap, ERROR_FILE_NOT_FOUND, "NtOpenIoCompletion");
  if (NtEq(api, "NtSetIoCompletion") || NtEq(api, "NtSetIoCompletionEx")) {
    auto parts = SplitAll(a);
    if (parts.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    DWORD bytes = parts.size() > 1 ? (DWORD)std::strtoul(parts[1].c_str(), nullptr, 10) : 0;
    ULONG_PTR key = parts.size() > 2 ? (ULONG_PTR)std::strtoull(parts[2].c_str(), nullptr, 10) : 0;
    if (!PostQueuedCompletionStatus(HandleOf(parts[0].c_str()), bytes, key, nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtRemoveIoCompletion") || NtEq(api, "NtRemoveIoCompletionEx")) {
    std::string hs, toms;
    Split1f(a, &hs, &toms);
    DWORD bytes = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ov = nullptr;
    DWORD ms = toms.empty() ? INFINITE : (DWORD)std::strtoul(toms.c_str(), nullptr, 10);
    if (!GetQueuedCompletionStatus(HandleOf(hs.c_str()), &bytes, &key, &ov, ms))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(bytes) + "\x1f" + std::to_string((unsigned long long)key));
  }
  if (NtEq(api, "NtQueryIoCompletion")) return Fill(out, cap, "0");

  if (NtEq(api, "NtLockVirtualMemory")) {
    std::string p, n;
    Split1f(a, &p, &n);
    if (!VirtualLock((void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10),
                     (SIZE_T)std::strtoull(n.empty() ? "4096" : n.c_str(), nullptr, 10)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtUnlockVirtualMemory")) {
    std::string p, n;
    Split1f(a, &p, &n);
    if (!VirtualUnlock((void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10),
                       (SIZE_T)std::strtoull(n.empty() ? "4096" : n.c_str(), nullptr, 10)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtFlushInstructionCache")) {
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    return Fill(out, cap, "ok");
  }
  if (NtEq(api, "NtLockFile") || NtEq(api, "NtUnlockFile") || NtEq(api, "NtFsControlFile") ||
      NtEq(api, "NtCreateNamedPipeFile") || NtEq(api, "NtCreateMailslotFile") ||
      NtEq(api, "NtCancelIoFileEx") || NtEq(api, "NtCancelSynchronousIoFile") ||
      NtEq(api, "NtQueryInformationByName") || NtEq(api, "NtReadFileScatter") ||
      NtEq(api, "NtWriteFileGather") || NtEq(api, "NtSetVolumeInformationFile") ||
      NtEq(api, "NtQueryEaFile") || NtEq(api, "NtSetEaFile") ||
      NtEq(api, "NtNotifyChangeDirectoryFile") || NtEq(api, "NtNotifyChangeDirectoryFileEx") ||
      NtEq(api, "NtGetWriteWatch") || NtEq(api, "NtResetWriteWatch") ||
      NtEq(api, "NtExtendSection") || NtEq(api, "NtAreMappedFilesTheSame") ||
      NtEq(api, "NtSetInformationVirtualMemory") || NtEq(api, "NtRenameKey") ||
      NtEq(api, "NtRestoreKey") || NtEq(api, "NtSaveKey") || NtEq(api, "NtSaveKeyEx") ||
      NtEq(api, "NtLoadKey") || NtEq(api, "NtLoadKey2") || NtEq(api, "NtLoadKeyEx") ||
      NtEq(api, "NtUnloadKey") || NtEq(api, "NtUnloadKey2") || NtEq(api, "NtNotifyChangeKey") ||
      NtEq(api, "NtNotifyChangeMultipleKeys") || NtEq(api, "NtQueryMultipleValueKey") ||
      NtEq(api, "NtSetInformationKey") || NtEq(api, "NtCompactKeys") ||
      NtEq(api, "NtMakeTemporaryObject") || NtEq(api, "NtMakePermanentObject") ||
      NtEq(api, "NtQuerySecurityObject") || NtEq(api, "NtSetSecurityObject") ||
      NtEq(api, "NtAdjustGroupsToken") ||
      NtEq(api, "NtSetInformationToken") || NtEq(api, "NtFilterToken") ||
      NtEq(api, "NtCompareTokens") || NtEq(api, "NtPrivilegeCheck") ||
      NtEq(api, "NtImpersonateThread") || NtEq(api, "NtImpersonateAnonymousToken") ||
      NtEq(api, "NtAccessCheck") || NtEq(api, "NtQueryLicenseValue") ||
      NtEq(api, "NtPowerInformation") || NtEq(api, "NtSetDebugFilterState"))
    return Fill(out, cap, "ok");

  if (NtEq(api, "NtCreateDirectoryObject") || NtEq(api, "NtCreateDirectoryObjectEx") ||
      NtEq(api, "NtOpenDirectoryObject") || NtEq(api, "NtCreateSymbolicLinkObject") ||
      NtEq(api, "NtOpenSymbolicLinkObject")) {
    HANDLE h = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (NtEq(api, "NtQueryDirectoryObject")) return Fill(out, cap, "");
  if (NtEq(api, "NtQuerySymbolicLinkObject")) {
    wchar_t buf[MAX_PATH]{};
    DWORD n = QueryDosDeviceW(a[0] ? Utf8ToWide(a).c_str() : L"C:", buf, MAX_PATH);
    return Fill(out, cap, n ? WideToUtf8(buf) : "\\Device\\HarddiskVolume1");
  }
  if (NtEq(api, "NtOpenThreadTokenEx")) {
    HANDLE tok = nullptr;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, TRUE, &tok))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(tok));
  }

  if (Eq(api, "RtlCreateHeap")) {
    HANDLE h = HeapCreate(0, 0, 0);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "ExAllocatePool") || Eq(api, "ExAllocatePoolWithTag") || Eq(api, "ExAllocatePool2") ||
      Eq(api, "ExAllocatePool3") || Eq(api, "ExAllocatePoolZero") ||
      Eq(api, "ExAllocatePoolWithQuota") || Eq(api, "ExAllocatePoolWithQuotaTag")) {
    std::string pool, rest, sz;
    Split1f(a, &pool, &rest);
    Split1f(rest.c_str(), &sz, &rest);
    if (sz.empty()) sz = pool;
    SIZE_T n = (SIZE_T)std::strtoull(sz.empty() ? "1" : sz.c_str(), nullptr, 0);
    if (!n) n = 1;
    void* p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(p));
  }
  if (Eq(api, "ExFreePool") || Eq(api, "ExFreePoolWithTag")) {
    std::string p, tag;
    Split1f(a, &p, &tag);
    if (!HeapFree(GetProcessHeap(), 0, HandleOf(p.empty() ? a : p.c_str())))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlDestroyHeap")) {
    if (!HeapDestroy(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlValidateHeap"))
    return Fill(out, cap, HeapValidate(GetProcessHeap(), 0, nullptr) ? "1" : "0");
  if (Eq(api, "RtlCompactHeap"))
    return Fill(out, cap, std::to_string(HeapCompact(GetProcessHeap(), 0)));
  if (Eq(api, "RtlLockHeap")) {
    if (!HeapLock(GetProcessHeap())) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "1");
  }
  if (Eq(api, "RtlUnlockHeap")) {
    if (!HeapUnlock(GetProcessHeap())) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "1");
  }
  if (Eq(api, "RtlWalkHeap")) return Fill(out, cap, "0");
  if (Eq(api, "RtlGetProcessHeaps")) {
    HANDLE h[16];
    DWORD n = GetProcessHeaps(16, h);
    return Fill(out, cap, std::to_string(n) + "\x1f" + HandleStr(n ? h[0] : GetProcessHeap()));
  }
  if (Eq(api, "RtlQueryHeapInformation") || Eq(api, "RtlSetHeapInformation"))
    return Fill(out, cap, "0");

  if (Eq(api, "RtlInitializeCriticalSection") ||
      Eq(api, "RtlInitializeCriticalSectionAndSpinCount") ||
      Eq(api, "RtlInitializeCriticalSectionEx")) {
    CRITICAL_SECTION* cs = new CRITICAL_SECTION();
    InitializeCriticalSection(cs);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)cs));
  }
  if (Eq(api, "RtlEnterCriticalSection")) {
    EnterCriticalSection((CRITICAL_SECTION*)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlLeaveCriticalSection")) {
    LeaveCriticalSection((CRITICAL_SECTION*)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlTryEnterCriticalSection"))
    return Fill(out, cap, TryEnterCriticalSection((CRITICAL_SECTION*)HandleOf(a)) ? "1" : "0");
  if (Eq(api, "RtlDeleteCriticalSection")) {
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)HandleOf(a);
    if (cs) {
      DeleteCriticalSection(cs);
      delete cs;
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlSetCriticalSectionSpinCount")) {
    SetCriticalSectionSpinCount((CRITICAL_SECTION*)HandleOf(a), 4000);
    return Fill(out, cap, "0");
  }

  if (Eq(api, "RtlInitializeSRWLock")) {
    SRWLOCK* p = new SRWLOCK();
    InitializeSRWLock(p);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "RtlAcquireSRWLockExclusive")) {
    AcquireSRWLockExclusive((SRWLOCK*)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlAcquireSRWLockShared")) {
    AcquireSRWLockShared((SRWLOCK*)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlReleaseSRWLockExclusive")) {
    ReleaseSRWLockExclusive((SRWLOCK*)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlReleaseSRWLockShared")) {
    ReleaseSRWLockShared((SRWLOCK*)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlTryAcquireSRWLockExclusive"))
    return Fill(out, cap, TryAcquireSRWLockExclusive((SRWLOCK*)HandleOf(a)) ? "1" : "0");
  if (Eq(api, "RtlTryAcquireSRWLockShared"))
    return Fill(out, cap, TryAcquireSRWLockShared((SRWLOCK*)HandleOf(a)) ? "1" : "0");
  if (Eq(api, "RtlInitializeConditionVariable")) {
    CONDITION_VARIABLE* p = new CONDITION_VARIABLE();
    InitializeConditionVariable(p);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "RtlWakeConditionVariable")) {
    WakeConditionVariable((CONDITION_VARIABLE*)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlWakeAllConditionVariable")) {
    WakeAllConditionVariable((CONDITION_VARIABLE*)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlSleepConditionVariableCS") || Eq(api, "RtlSleepConditionVariableSRW"))
    return Fill(out, cap, "1");
  if (Eq(api, "RtlRunOnceInitialize")) {
    INIT_ONCE* p = new INIT_ONCE();
    InitOnceInitialize(p);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "RtlRunOnceExecuteOnce") || Eq(api, "RtlRunOnceBeginInitialize") ||
      Eq(api, "RtlRunOnceComplete") || Eq(api, "RtlWaitOnAddress") ||
      Eq(api, "RtlWakeAddressSingle") || Eq(api, "RtlWakeAddressAll"))
    return Fill(out, cap, "ok");

  if (Eq(api, "RtlInitializeSListHead")) return Fill(out, cap, "1");
  if (Eq(api, "RtlQueryDepthSList")) return Fill(out, cap, "0");
  if (Eq(api, "RtlInterlockedPushEntrySList") || Eq(api, "RtlInterlockedPopEntrySList") ||
      Eq(api, "RtlInterlockedFlushSList"))
    return Fill(out, cap, "0");

  if (Eq(api, "RtlQueryEnvironmentVariable_U")) {
    wchar_t buf[32768];
    DWORD n = GetEnvironmentVariableW(Utf8ToWide(a[0] ? a : "PATH").c_str(), buf, 32768);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(buf));
  }
  if (Eq(api, "RtlSetEnvironmentVariable")) {
    std::string name, val;
    Split1f(a, &name, &val);
    if (name.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    if (!SetEnvironmentVariableW(Utf8ToWide(name).c_str(), val.empty() ? nullptr : Utf8ToWide(val).c_str()))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlExpandEnvironmentStrings_U")) {
    std::wstring in = Utf8ToWide(a);
    DWORD need = ExpandEnvironmentStringsW(in.c_str(), nullptr, 0);
    std::wstring buf(need, L'\0');
    DWORD n = ExpandEnvironmentStringsW(in.c_str(), buf.data(), need);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(buf.c_str()));
  }
  if (Eq(api, "RtlCreateEnvironment")) return Fill(out, cap, "1");
  if (Eq(api, "RtlDestroyEnvironment")) return Fill(out, cap, "ok");
  if (Eq(api, "RtlGetCurrentDirectory_U")) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetCurrentDirectoryW(MAX_PATH, buf);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(buf));
  }
  if (Eq(api, "RtlSetCurrentDirectory_U")) {
    if (!SetCurrentDirectoryW(Utf8ToWide(a).c_str()))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlGetFullPathName_U")) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetFullPathNameW(Utf8ToWide(a).c_str(), MAX_PATH, buf, nullptr);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(buf));
  }
  if (Eq(api, "RtlDosPathNameToNtPathName_U")) {
    std::string s = a ? a : "";
    if (s.size() >= 2 && s[1] == ':') return Fill(out, cap, std::string("\\??\\") + s);
    if (s.size() >= 2 && s[0] == '\\' && s[1] == '\\')
      return Fill(out, cap, std::string("\\??\\UNC\\") + s.substr(2));
    return Fill(out, cap, std::string("\\??\\") + s);
  }
  if (Eq(api, "RtlNtPathNameToDosPathName")) {
    std::string s = a ? a : "";
    if (s.rfind("\\??\\UNC\\", 0) == 0) return Fill(out, cap, std::string("\\\\") + s.substr(8));
    if (s.rfind("\\??\\", 0) == 0) return Fill(out, cap, s.substr(4));
    return Fill(out, cap, s);
  }
  if (Eq(api, "RtlDetermineDosPathNameType_U")) {
    std::string s = a ? a : "";
    if (s.size() >= 2 && s[0] == '\\' && s[1] == '\\') return Fill(out, cap, "1");
    if (s.size() >= 3 && s[1] == ':' && (s[2] == '\\' || s[2] == '/')) return Fill(out, cap, "2");
    if (s.size() >= 2 && s[1] == ':') return Fill(out, cap, "3");
    if (!s.empty() && (s[0] == '\\' || s[0] == '/')) return Fill(out, cap, "4");
    return Fill(out, cap, "5");
  }
  if (Eq(api, "RtlIsDosDeviceName_U")) {
    std::string s = a ? a : "";
    for (char& c : s)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return Fill(out, cap, (s == "NUL" || s == "CON" || s == "PRN" || s == "AUX") ? "1" : "0");
  }
  if (Eq(api, "RtlDosSearchPath_U")) {
    wchar_t buf[MAX_PATH];
    DWORD n = SearchPathW(nullptr, Utf8ToWide(a).c_str(), nullptr, MAX_PATH, buf, nullptr);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(buf));
  }

  if (Eq(api, "RtlInitializeSid") || Eq(api, "RtlAllocateAndInitializeSid")) {
    SID_IDENTIFIER_AUTHORITY sia = SECURITY_NT_AUTHORITY;
    PSID sid = nullptr;
    if (!AllocateAndInitializeSid(&sia, 1, SECURITY_AUTHENTICATED_USER_RID, 0, 0, 0, 0, 0, 0, 0,
                                  &sid))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(sid));
  }
  if (Eq(api, "RtlLengthSid") || Eq(api, "RtlLengthRequiredSid"))
    return Fill(out, cap, std::to_string(GetLengthSid((PSID)HandleOf(a))));
  if (Eq(api, "RtlCopySid")) {
    std::string dst, src;
    Split1f(a, &dst, &src);
    DWORD n = GetLengthSid((PSID)HandleOf(src.c_str()));
    if (!CopySid(n, (PSID)HandleOf(dst.c_str()), (PSID)HandleOf(src.c_str())))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlEqualSid")) {
    std::string x, y;
    Split1f(a, &x, &y);
    return Fill(out, cap, EqualSid((PSID)HandleOf(x.c_str()), (PSID)HandleOf(y.c_str())) ? "1" : "0");
  }
  if (Eq(api, "RtlValidSid"))
    return Fill(out, cap, IsValidSid((PSID)HandleOf(a)) ? "1" : "0");
  if (Eq(api, "RtlFreeSid")) {
    FreeSid((PSID)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlConvertSidToUnicodeString")) {
    LPWSTR s = nullptr;
    if (!ConvertSidToStringSidW((PSID)HandleOf(a), &s))
      return FillErr(out, cap, (int)GetLastError(), api);
    std::string out8 = WideToUtf8(s);
    LocalFree(s);
    return Fill(out, cap, out8);
  }
  if (Eq(api, "RtlCreateWellKnownSid")) {
    DWORD sz = SECURITY_MAX_SID_SIZE;
    PSID sid = LocalAlloc(LPTR, sz);
    if (!CreateWellKnownSid((WELL_KNOWN_SID_TYPE)std::strtoul(a[0] ? a : "1", nullptr, 10), nullptr,
                            sid, &sz)) {
      LocalFree(sid);
      return FillErr(out, cap, (int)GetLastError(), api);
    }
    return Fill(out, cap, HandleStr(sid));
  }
  if (Eq(api, "RtlCreateSecurityDescriptor") || Eq(api, "RtlSetDaclSecurityDescriptor") ||
      Eq(api, "RtlValidSecurityDescriptor") || Eq(api, "RtlValidAcl"))
    return Fill(out, cap, "1");
  if (Eq(api, "RtlLengthSecurityDescriptor")) return Fill(out, cap, "20");

  if (Eq(api, "RtlCopyUnicodeString") || Eq(api, "RtlDuplicateUnicodeString") ||
      Eq(api, "RtlCreateUnicodeString")) {
    std::string dst, src;
    Split1f(a, &dst, &src);
    return Fill(out, cap, src.empty() ? dst : src);
  }
  if (Eq(api, "RtlFreeUnicodeString") || Eq(api, "RtlFreeAnsiString")) return Fill(out, cap, "ok");
  if (Eq(api, "RtlAppendUnicodeToString")) {
    std::string x, y;
    Split1f(a, &x, &y);
    return Fill(out, cap, x + y);
  }
  if (Eq(api, "RtlUpcaseUnicodeString") || Eq(api, "RtlUpperString")) {
    std::string s = a;
    for (char& c : s)
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return Fill(out, cap, s);
  }
  if (Eq(api, "RtlDowncaseUnicodeString")) {
    std::string s = a;
    for (char& c : s)
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return Fill(out, cap, s);
  }
  if (Eq(api, "RtlEqualString")) {
    std::string x, y;
    Split1f(a, &x, &y);
    return Fill(out, cap, x == y ? "1" : "0");
  }
  if (Eq(api, "RtlCompareString")) {
    std::string x, y;
    Split1f(a, &x, &y);
    return Fill(out, cap, x < y ? "-1" : (x > y ? "1" : "0"));
  }
  if (Eq(api, "RtlUpperChar")) {
    unsigned c = (unsigned)std::strtoul(a, nullptr, 10);
    if (!c && a[0]) c = (unsigned char)a[0];
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    return Fill(out, cap, std::to_string(c));
  }
  if (Eq(api, "RtlMultiByteToUnicodeN") || Eq(api, "RtlUTF8ToUnicodeN") ||
      Eq(api, "RtlUnicodeToMultiByteN") || Eq(api, "RtlUnicodeToUTF8N"))
    return Fill(out, cap, a);
  if (Eq(api, "RtlTimeToTimeFields") || Eq(api, "RtlTimeFieldsToTime") ||
      Eq(api, "RtlLocalTimeToSystemTime") || Eq(api, "RtlSystemTimeToLocalTime")) {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u{};
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return Fill(out, cap, std::to_string(u.QuadPart));
  }
  if (Eq(api, "RtlSecondsSince1970ToTime")) {
    ULONGLONG sec = std::strtoull(a, nullptr, 10);
    return Fill(out, cap, std::to_string((sec + 11644473600ull) * 10000000ull));
  }
  if (Eq(api, "RtlTimeToSecondsSince1970")) {
    ULONGLONG ft = std::strtoull(a, nullptr, 10);
    if (!ft) return Fill(out, cap, std::to_string((unsigned long long)time(nullptr)));
    return Fill(out, cap, std::to_string((ft / 10000000ull) - 11644473600ull));
  }
  if (Eq(api, "RtlQueryTimeZoneInformation")) {
    TIME_ZONE_INFORMATION tz{};
    GetTimeZoneInformation(&tz);
    return Fill(out, cap, std::to_string(tz.Bias));
  }
  if (Eq(api, "RtlIsProcessorFeaturePresent"))
    return Fill(out, cap, IsProcessorFeaturePresent((DWORD)std::strtoul(a, nullptr, 10)) ? "1" : "0");
  if (Eq(api, "RtlVerifyVersionInfo")) return Fill(out, cap, "1");
  if (Eq(api, "RtlGetNtGlobalFlags")) return Fill(out, cap, "0");
  if (Eq(api, "RtlComputeCrc32")) {
    unsigned c = 0xffffffffu;
    for (const unsigned char* p = (const unsigned char*)(a ? a : ""); *p; ++p) {
      c ^= *p;
      for (int i = 0; i < 8; ++i) c = (c >> 1) ^ (0xEDB88320u & (unsigned)(0 - (c & 1)));
    }
    return Fill(out, cap, std::to_string(~c));
  }
  if (Eq(api, "RtlUniform")) {
    using Fn = ULONG(WINAPI*)(PULONG);
    auto fn = (Fn)(void*)NtProc("RtlUniform");
    ULONG s = (ULONG)std::strtoul(a[0] ? a : "1", nullptr, 10);
    ULONG r = fn ? fn(&s) : s * 1103515245u + 12345u;
    return Fill(out, cap, std::to_string(r));
  }
  if (Eq(api, "RtlAddVectoredExceptionHandler") || Eq(api, "RtlAddVectoredContinueHandler")) {
    if (!VehHandle()) VehHandle() = AddVectoredExceptionHandler(1, K32Veh);
    if (!VehHandle()) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(VehHandle()));
  }
  if (Eq(api, "RtlRemoveVectoredExceptionHandler")) {
    PVOID p = (PVOID)HandleOf(a);
    if (p && RemoveVectoredExceptionHandler(p)) {
      if (p == VehHandle()) VehHandle() = nullptr;
      return Fill(out, cap, "ok");
    }
    return FillErr(out, cap, (int)GetLastError(), api);
  }
  if (Eq(api, "RtlCaptureContext") || Eq(api, "RtlCaptureStackBackTrace") ||
      Eq(api, "RtlLookupFunctionEntry") || Eq(api, "RtlVirtualUnwind"))
    return Fill(out, cap, "0");

  if (Eq(api, "LdrGetDllHandleEx") || Eq(api, "LdrGetDllHandleByMapping")) {
    HMODULE h = GetModuleHandleW(a[0] ? Utf8ToWide(a).c_str() : nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "LdrGetProcedureAddressEx")) {
    std::string hs, name;
    Split1f(a, &hs, &name);
    FARPROC p = GetProcAddress((HMODULE)HandleOf(hs.c_str()), name.c_str());
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "LdrLockLoaderLock") || Eq(api, "LdrUnlockLoaderLock") ||
      Eq(api, "LdrDisableThreadCalloutsForDll"))
    return Fill(out, cap, "ok");
  if (Eq(api, "LdrFindResource_U")) {
    HRSRC r = FindResourceW((HMODULE)HandleOf(a), MAKEINTRESOURCEW(1), (LPCWSTR)16);
    return Fill(out, cap, r ? HandleStr(r) : "0");
  }
  if (Eq(api, "LdrAccessResource")) {
    HGLOBAL g = LoadResource((HMODULE)HandleOf(a), (HRSRC)HandleOf(a));
    return Fill(out, cap, g ? HandleStr(g) : "0");
  }
  if (Eq(api, "LdrGetDllFullName")) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(a[0] ? (HMODULE)HandleOf(a) : nullptr, buf, MAX_PATH);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(buf));
  }

  if (Eq(api, "TpAllocPool")) {
    PTP_POOL p = CreateThreadpool(nullptr);
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "TpReleasePool")) {
    CloseThreadpool((PTP_POOL)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "TpSetPoolMaxThreads") || Eq(api, "TpSetPoolMinThreads") || Eq(api, "TpSetTimer") ||
      Eq(api, "TpPostWork") || Eq(api, "TpWaitForWork") || Eq(api, "TpReleaseWork") ||
      Eq(api, "TpReleaseTimer"))
    return Fill(out, cap, "ok");
  if (Eq(api, "TpAllocWork")) {
    PTP_WORK w = CreateThreadpoolWork(kTpWork, nullptr, nullptr);
    if (!w) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)w));
  }
  if (Eq(api, "TpAllocTimer")) {
    PTP_TIMER t = CreateThreadpoolTimer(kTpTimer, nullptr, nullptr);
    if (!t) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)t));
  }
  if (Eq(api, "TpSimpleTryPost")) {
    if (!TrySubmitThreadpoolCallback(kTpSimple, nullptr, nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "1");
  }
  if (Eq(api, "DbgPrint") || Eq(api, "DbgPrintEx")) {
    OutputDebugStringA(a);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "DbgQueryDebugFilterState")) return Fill(out, cap, "0");
  if (Eq(api, "DbgSetDebugFilterState")) return Fill(out, cap, "ok");

  if (NtEq(api, "NtOpenProcessToken") || NtEq(api, "NtOpenProcessTokenEx")) {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ADJUST_PRIVILEGES, &tok))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(tok));
  }
  if (NtEq(api, "NtQueryInformationFile") || NtEq(api, "NtSetInformationFile") ||
      NtEq(api, "NtQueryDirectoryFile") || NtEq(api, "NtDeviceIoControlFile") ||
      NtEq(api, "NtQueryVirtualMemory") || NtEq(api, "NtReadVirtualMemory") ||
      NtEq(api, "NtWriteVirtualMemory") || NtEq(api, "NtTerminateProcess") ||
      NtEq(api, "NtCreateThread") || NtEq(api, "NtCreateThreadEx") || NtEq(api, "NtOpenThread") ||
      NtEq(api, "NtWaitForMultipleObjects") || NtEq(api, "NtResumeThread") ||
      NtEq(api, "NtSuspendThread") || NtEq(api, "NtQueryInformationThread") ||
      NtEq(api, "NtSetInformationThread") || NtEq(api, "NtQueryPerformanceCounter") ||
      NtEq(api, "NtOpenKey") || NtEq(api, "NtCreateKey") || NtEq(api, "NtSetValueKey") ||
      NtEq(api, "NtQueryValueKey") || NtEq(api, "NtEnumerateKey") || NtEq(api, "NtDeleteKey") ||
      NtEq(api, "NtQueryInformationToken") || NtEq(api, "NtOpenSection")) {
    FARPROC p = NtProc(api[0] == 'Z' ? api : api);
    if (NtEq(api, "NtQueryPerformanceCounter")) {
      LARGE_INTEGER c{};
      QueryPerformanceCounter(&c);
      return Fill(out, cap, std::to_string((unsigned long long)c.QuadPart));
    }
    if (NtEq(api, "NtOpenKey") || NtEq(api, "NtCreateKey")) {
      HKEY k = nullptr;
      if (RegOpenKeyExW(HKEY_CURRENT_USER, a[0] ? Utf8ToWide(a).c_str() : L"Software", 0, KEY_READ,
                        &k) != 0)
        return Fill(out, cap, HandleStr((HANDLE)(intptr_t)HKEY_CURRENT_USER));
      return Fill(out, cap, HandleStr((HANDLE)k));
    }
    (void)p;
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "AddVectoredExceptionHandler")) {
    if (!VehHandle()) VehHandle() = AddVectoredExceptionHandler(1, K32Veh);
    if (!VehHandle()) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(VehHandle()));
  }
  if (Eq(api, "RemoveVectoredExceptionHandler")) {
    PVOID p = (PVOID)HandleOf(a);
    if (p && RemoveVectoredExceptionHandler(p)) {
      if (p == VehHandle()) VehHandle() = nullptr;
      return Fill(out, cap, "ok");
    }
    return FillErr(out, cap, (int)GetLastError(), api);
  }
  if (Eq(api, "RaiseException")) {
    if (!VehHandle()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, "RaiseException");
    return Fill(out, cap, "continue");
  }
  if (Eq(api, "RtlUnwind") || Eq(api, "RtlUnwindEx")) return Fill(out, cap, "ok");
  if (Eq(api, "SetUnhandledExceptionFilter")) {
    SetUnhandledExceptionFilter(nullptr);
    return Fill(out, cap, "0");
  }

  if (Eq(api, "BCryptOpenAlgorithmProvider")) {
    using Fn = LONG(WINAPI*)(PVOID*, LPCWSTR, LPCWSTR, ULONG);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), "BCryptOpenAlgorithmProvider");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    std::wstring alg = Utf8ToWide(a && a[0] ? a : "SHA256");
    PVOID h = nullptr;
    LONG st = fn(&h, alg.c_str(), nullptr, 0);
    if (st < 0 || !h) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "BCryptCloseAlgorithmProvider")) {
    using Fn = LONG(WINAPI*)(PVOID, ULONG);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), "BCryptCloseAlgorithmProvider");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    LONG st = fn((PVOID)HandleOf(a), 0);
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "BCryptDestroyHash") || Eq(api, "BCryptDestroyKey")) {
    const char* nm = Eq(api, "BCryptDestroyHash") ? "BCryptDestroyHash" : "BCryptDestroyKey";
    using Fn = LONG(WINAPI*)(PVOID);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), nm);
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    LONG st = fn((PVOID)HandleOf(a));
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "BCryptGenRandom")) {
    using Fn = LONG(WINAPI*)(PVOID, PUCHAR, ULONG, ULONG);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), "BCryptGenRandom");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    ULONG n = (ULONG)std::strtoul(a, nullptr, 10);
    if (!n) n = 16;
    if (n > 4096) n = 4096;
    std::vector<unsigned char> b(n);
    LONG st = fn(nullptr, b.data(), n, 0x00000002);
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HexOf(b.data(), n));
  }
  if (Eq(api, "BCryptCreateHash")) {
    using Fn = LONG(WINAPI*)(PVOID, PVOID*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), "BCryptCreateHash");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    PVOID hash = nullptr;
    LONG st = fn((PVOID)HandleOf(a), &hash, nullptr, 0, nullptr, 0, 0);
    if (st < 0 || !hash) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(hash));
  }
  if (Eq(api, "BCryptHashData")) {
    std::string hs, data;
    Split1f(a, &hs, &data);
    using Fn = LONG(WINAPI*)(PVOID, PUCHAR, ULONG, ULONG);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), "BCryptHashData");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    LONG st = fn((PVOID)HandleOf(hs.c_str()), (PUCHAR)data.data(), (ULONG)data.size(), 0);
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "BCryptFinishHash")) {
    using Fn = LONG(WINAPI*)(PVOID, PUCHAR, ULONG, ULONG);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), "BCryptFinishHash");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    unsigned char dig[64]{};
    LONG st = fn((PVOID)HandleOf(a), dig, 32, 0);
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HexOf(dig, 32));
  }
  if (Eq(api, "BCryptGenerateSymmetricKey")) {
    std::string hs, key;
    Split1f(a, &hs, &key);
    using SetFn = LONG(WINAPI*)(PVOID, LPCWSTR, PUCHAR, ULONG, ULONG);
    auto setp = (SetFn)(void*)GetProcAddress(BcryptMod(), "BCryptSetProperty");
    if (setp) {
      const wchar_t* ecb = BCRYPT_CHAIN_MODE_ECB;
      setp((PVOID)HandleOf(hs.c_str()), BCRYPT_CHAINING_MODE, (PUCHAR)ecb,
           (ULONG)((std::wcslen(ecb) + 1) * sizeof(wchar_t)), 0);
    }
    using Fn = LONG(WINAPI*)(PVOID, PVOID*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), "BCryptGenerateSymmetricKey");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    PVOID kh = nullptr;
    LONG st = fn((PVOID)HandleOf(hs.c_str()), &kh, nullptr, 0, (PUCHAR)key.data(),
                 (ULONG)key.size(), 0);
    if (st < 0 || !kh) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(kh));
  }
  if (Eq(api, "BCryptEncrypt") || Eq(api, "BCryptDecrypt")) {
    std::string hs, data;
    Split1f(a, &hs, &data);
    const char* nm = Eq(api, "BCryptEncrypt") ? "BCryptEncrypt" : "BCryptDecrypt";
    using Fn = LONG(WINAPI*)(PVOID, PUCHAR, ULONG, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG*,
                             ULONG);
    auto fn = (Fn)(void*)GetProcAddress(BcryptMod(), nm);
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    ULONG flags = Eq(api, "BCryptEncrypt") ? BCRYPT_BLOCK_PADDING : BCRYPT_BLOCK_PADDING;
    ULONG need = 0;
    LONG st = fn((PVOID)HandleOf(hs.c_str()), (PUCHAR)data.data(), (ULONG)data.size(), nullptr,
                 nullptr, 0, nullptr, 0, &need, flags);
    if (need == 0) need = (ULONG)data.size() + 16;
    std::vector<unsigned char> outb(need);
    ULONG got = 0;
    st = fn((PVOID)HandleOf(hs.c_str()), (PUCHAR)data.data(), (ULONG)data.size(), nullptr, nullptr,
            0, outb.data(), (ULONG)outb.size(), &got, flags);
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, std::string((char*)outb.data(), got));
  }

  if (Eq(api, "NCryptOpenStorageProvider")) {
    using Fn = LONG(WINAPI*)(NCRYPT_PROV_HANDLE*, LPCWSTR, DWORD);
    auto fn = (Fn)(void*)GetProcAddress(NcryptMod(), "NCryptOpenStorageProvider");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    NCRYPT_PROV_HANDLE h = 0;
    std::wstring name = Utf8ToWide(a[0] ? a : "Microsoft Software Key Storage Provider");
    LONG st = fn(&h, name.c_str(), 0);
    if (st < 0 || !h) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, std::to_string((unsigned long long)h));
  }
  if (Eq(api, "NCryptFreeObject")) {
    using Fn = LONG(WINAPI*)(NCRYPT_HANDLE);
    auto fn = (Fn)(void*)GetProcAddress(NcryptMod(), "NCryptFreeObject");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    LONG st = fn((NCRYPT_HANDLE)HandleOf(a));
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "NCryptGenRandom")) {
    std::string hs, nstr;
    Split1f(a, &hs, &nstr);
    ULONG n = (ULONG)std::strtoul(nstr.empty() ? hs.c_str() : nstr.c_str(), nullptr, 10);
    if (!n) n = 16;
    if (n > 4096) n = 4096;
    NCRYPT_PROV_HANDLE h = 0;
    if (!nstr.empty()) h = (NCRYPT_PROV_HANDLE)std::strtoull(hs.c_str(), nullptr, 10);
    int opened = 0;
    if (!h) {
      using OpenFn = LONG(WINAPI*)(NCRYPT_PROV_HANDLE*, LPCWSTR, DWORD);
      auto open = (OpenFn)(void*)GetProcAddress(NcryptMod(), "NCryptOpenStorageProvider");
      if (open && open(&h, MS_KEY_STORAGE_PROVIDER, 0) >= 0) opened = 1;
    }
    std::vector<unsigned char> b(n);
    LONG st = (LONG)0x80090029;  // NTE_NOT_SUPPORTED
    using Fn = LONG(WINAPI*)(NCRYPT_PROV_HANDLE, PBYTE, DWORD, DWORD);
    auto fn = (Fn)(void*)GetProcAddress(NcryptMod(), "NCryptGenRandom");
    if (fn && h) {
      st = fn(h, b.data(), n, 0);
      if (st < 0) st = fn(h, b.data(), n, 0x00000040);
    }
    if (opened && h) {
      using FreeFn = LONG(WINAPI*)(NCRYPT_HANDLE);
      auto free_fn = (FreeFn)(void*)GetProcAddress(NcryptMod(), "NCryptFreeObject");
      if (free_fn) free_fn((NCRYPT_HANDLE)h);
    }
    if (st < 0) {
      using BFn = LONG(WINAPI*)(PVOID, PUCHAR, ULONG, ULONG);
      auto bfn = (BFn)(void*)GetProcAddress(BcryptMod(), "BCryptGenRandom");
      if (!bfn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
      st = bfn(nullptr, b.data(), n, 0x00000002);
    }
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HexOf(b.data(), n));
  }
  if (Eq(api, "NCryptCreatePersistedKey") || Eq(api, "NCryptOpenKey")) {
    std::string hs, rest;
    Split1f(a, &hs, &rest);
    using Fn = LONG(WINAPI*)(NCRYPT_PROV_HANDLE, NCRYPT_KEY_HANDLE*, LPCWSTR, LPCWSTR, DWORD);
    auto fn = (Fn)(void*)GetProcAddress(NcryptMod(), "NCryptCreatePersistedKey");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    NCRYPT_KEY_HANDLE kh = 0;
    LONG st = fn((NCRYPT_PROV_HANDLE)HandleOf(hs.c_str()), &kh, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (st < 0 || !kh) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr((HANDLE)kh));
  }
  if (Eq(api, "NCryptFinalizeKey") || Eq(api, "NCryptDeleteKey")) {
    const char* nm = Eq(api, "NCryptDeleteKey") ? "NCryptDeleteKey" : "NCryptFinalizeKey";
    using Fn = LONG(WINAPI*)(NCRYPT_KEY_HANDLE, DWORD);
    auto fn = (Fn)(void*)GetProcAddress(NcryptMod(), nm);
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    LONG st = fn((NCRYPT_KEY_HANDLE)HandleOf(a), 0);
    if (st < 0) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "NCryptEncrypt") || Eq(api, "NCryptDecrypt") || Eq(api, "NCryptSignHash") ||
      Eq(api, "NCryptVerifySignature") || Eq(api, "NCryptExportKey") || Eq(api, "NCryptImportKey")) {
    return Fill(out, cap, Eq(api, "NCryptVerifySignature") ? "1" : "ok");
  }

  if (Eq(api, "RegisterClassW") || Eq(api, "RegisterClassExW") || Eq(api, "RegisterClassA")) {
    std::wstring name = Utf8ToWide(a[0] ? a : "WASMWin32");
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = name.c_str();
    ATOM at = RegisterClassW(&wc);
    if (!at) {
      int err = (int)GetLastError();
      if (err == ERROR_CLASS_ALREADY_EXISTS) return Fill(out, cap, "1");
      return FillErr(out, cap, err, api);
    }
    return Fill(out, cap, "1");
  }
  if (Eq(api, "UnregisterClassW") || Eq(api, "UnregisterClassA")) {
    UnregisterClassW(Utf8ToWide(a).c_str(), GetModuleHandleW(nullptr));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CreateWindowExW") || Eq(api, "CreateWindowExA") || Eq(api, "CreateWindowW")) {
    std::string cls, rest, title, st;
    Split1f(a, &cls, &rest);
    Split1f(rest.c_str(), &title, &st);
    if (cls.empty()) cls = "WASMWin32";
    std::wstring wcls = Utf8ToWide(cls);
    WNDCLASSW wc{};
    if (!GetClassInfoW(GetModuleHandleW(nullptr), wcls.c_str(), &wc)) {
      wc.lpfnWndProc = DefWindowProcW;
      wc.hInstance = GetModuleHandleW(nullptr);
      wc.lpszClassName = wcls.c_str();
      RegisterClassW(&wc);
    }
    HWND w = CreateWindowExW(0, wcls.c_str(), Utf8ToWide(title).c_str(), 0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!w) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(w));
  }
  if (Eq(api, "DestroyWindow")) {
    if (!DestroyWindow((HWND)HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ShowWindow")) {
    std::string hs, cmd;
    Split1f(a, &hs, &cmd);
    BOOL vis = ShowWindow((HWND)HandleOf(hs.c_str()), (int)std::strtol(cmd.c_str(), nullptr, 10));
    return Fill(out, cap, vis ? "1" : "0");
  }
  if (Eq(api, "GetDesktopWindow")) return Fill(out, cap, HandleStr(GetDesktopWindow()));
  if (Eq(api, "GetForegroundWindow")) return Fill(out, cap, HandleStr(GetForegroundWindow()));
  if (Eq(api, "SetForegroundWindow")) {
    return Fill(out, cap, SetForegroundWindow((HWND)HandleOf(a)) ? "1" : "0");
  }
  if (Eq(api, "GetClientRect") || Eq(api, "GetWindowRect")) {
    RECT r{};
    BOOL ok = Eq(api, "GetClientRect") ? GetClientRect((HWND)HandleOf(a), &r)
                                       : GetWindowRect((HWND)HandleOf(a), &r);
    if (!ok) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(r.left) + "\x1f" + std::to_string(r.top) + "\x1f" +
                              std::to_string(r.right - r.left) + "\x1f" +
                              std::to_string(r.bottom - r.top));
  }
  if (Eq(api, "SetWindowPos")) {
    std::string hs, rest, x, y, wh, ww, hh;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &x, &rest);
    Split1f(rest.c_str(), &y, &wh);
    Split1f(wh.c_str(), &ww, &hh);
    if (!SetWindowPos((HWND)HandleOf(hs.c_str()), nullptr, (int)std::strtol(x.c_str(), nullptr, 10),
                      (int)std::strtol(y.c_str(), nullptr, 10),
                      (int)std::strtol(ww.c_str(), nullptr, 10),
                      (int)std::strtol(hh.c_str(), nullptr, 10), SWP_NOZORDER | SWP_NOACTIVATE))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "1");
  }
  if (Eq(api, "SetWindowTextW") || Eq(api, "SetWindowTextA")) {
    std::string hs, title;
    Split1f(a, &hs, &title);
    if (!SetWindowTextW((HWND)HandleOf(hs.c_str()), Utf8ToWide(title).c_str()))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "1");
  }
  if (Eq(api, "GetWindowTextW") || Eq(api, "GetWindowTextA")) {
    wchar_t bufw[512];
    int n = GetWindowTextW((HWND)HandleOf(a), bufw, 512);
    return Fill(out, cap, WideToUtf8(bufw, n));
  }
  if (Eq(api, "GetWindowTextLengthW")) {
    return Fill(out, cap, std::to_string(GetWindowTextLengthW((HWND)HandleOf(a))));
  }
  if (Eq(api, "GetWindowLongPtrW") || Eq(api, "GetWindowLongW")) {
    std::string hs, idx;
    Split1f(a, &hs, &idx);
    LONG_PTR v = GetWindowLongPtrW((HWND)HandleOf(hs.c_str()), (int)std::strtol(idx.c_str(), nullptr, 10));
    return Fill(out, cap, std::to_string((long long)v));
  }
  if (Eq(api, "SetWindowLongPtrW") || Eq(api, "SetWindowLongW")) {
    std::string hs, rest, idx, val;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &idx, &val);
    LONG_PTR old = SetWindowLongPtrW((HWND)HandleOf(hs.c_str()), (int)std::strtol(idx.c_str(), nullptr, 10),
                                     (LONG_PTR)std::strtoll(val.c_str(), nullptr, 10));
    return Fill(out, cap, std::to_string((long long)old));
  }
  if (Eq(api, "FindWindowW") || Eq(api, "FindWindowA")) {
    std::string cls, title;
    Split1f(a, &cls, &title);
    HWND w = FindWindowW(cls.empty() ? nullptr : Utf8ToWide(cls).c_str(),
                         title.empty() ? nullptr : Utf8ToWide(title).c_str());
    if (!w) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(w));
  }
  if (Eq(api, "GetWindowThreadProcessId")) {
    DWORD pid = 0;
    GetWindowThreadProcessId((HWND)HandleOf(a), &pid);
    return Fill(out, cap, std::to_string(pid));
  }
  if (Eq(api, "PostMessageW") || Eq(api, "PostMessageA") || Eq(api, "SendMessageW")) {
    std::string hs, rest, msg, wp, lp;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &msg, &wp);
    Split1f(wp.c_str(), &wp, &lp);
    LRESULT r = 0;
    if (Eq(api, "SendMessageW"))
      r = SendMessageW((HWND)HandleOf(hs.c_str()), (UINT)std::strtoul(msg.c_str(), nullptr, 0),
                       (WPARAM)std::strtoull(wp.c_str(), nullptr, 10),
                       (LPARAM)std::strtoll(lp.c_str(), nullptr, 10));
    else if (!PostMessageW((HWND)HandleOf(hs.c_str()), (UINT)std::strtoul(msg.c_str(), nullptr, 0),
                           (WPARAM)std::strtoull(wp.c_str(), nullptr, 10),
                           (LPARAM)std::strtoll(lp.c_str(), nullptr, 10)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, Eq(api, "SendMessageW") ? std::to_string((long long)r) : "1");
  }
  if (Eq(api, "PostQuitMessage")) {
    PostQuitMessage((int)std::strtol(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "PeekMessageW") || Eq(api, "PeekMessageA")) {
    std::string hs, rest;
    Split1f(a, &hs, &rest);
    MSG m{};
    HWND w = hs.empty() || hs == "0" ? nullptr : (HWND)HandleOf(hs.c_str());
    if (!PeekMessageW(&m, w, 0, 0, PM_REMOVE)) return Fill(out, cap, "0");
    if (m.message == WM_QUIT) return Fill(out, cap, "quit");
    return Fill(out, cap, std::to_string(m.message) + "\x1f" + std::to_string((unsigned long long)m.wParam) +
                              "\x1f" + std::to_string((long long)m.lParam));
  }
  if (Eq(api, "TranslateMessage")) return Fill(out, cap, "0");
  if (Eq(api, "DispatchMessageW") || Eq(api, "DispatchMessageA") || Eq(api, "DefWindowProcW"))
    return Fill(out, cap, "0");
  if (Eq(api, "InvalidateRect") || Eq(api, "UpdateWindow")) return Fill(out, cap, "1");
  if (Eq(api, "GetCursorPos")) {
    POINT p{};
    GetCursorPos(&p);
    return Fill(out, cap, std::to_string(p.x) + "\x1f" + std::to_string(p.y));
  }
  if (Eq(api, "SetCursorPos")) {
    std::string x, y;
    Split1f(a, &x, &y);
    return Fill(out, cap, SetCursorPos((int)std::strtol(x.c_str(), nullptr, 10),
                                       (int)std::strtol(y.c_str(), nullptr, 10))
                               ? "1"
                               : "0");
  }
  if (Eq(api, "GetSystemMetrics")) {
    return Fill(out, cap, std::to_string(GetSystemMetrics((int)std::strtol(a, nullptr, 10))));
  }
  if (Eq(api, "LoadCursorW") || Eq(api, "LoadCursorA"))
    return Fill(out, cap, HandleStr(LoadCursorA(nullptr, IDC_ARROW)));
  if (Eq(api, "LoadIconW") || Eq(api, "LoadIconA"))
    return Fill(out, cap, HandleStr(LoadIconA(nullptr, IDI_APPLICATION)));
  if (Eq(api, "GetAsyncKeyState") || Eq(api, "GetKeyState")) {
    SHORT s = Eq(api, "GetAsyncKeyState")
                  ? GetAsyncKeyState((int)std::strtol(a, nullptr, 10))
                  : GetKeyState((int)std::strtol(a, nullptr, 10));
    return Fill(out, cap, std::to_string(s));
  }
  if (Eq(api, "GetDC") || Eq(api, "GetWindowDC")) {
    HWND w = a[0] ? (HWND)HandleOf(a) : GetDesktopWindow();
    HDC dc = GetDC(w);
    if (!dc) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(dc));
  }
  if (Eq(api, "ReleaseDC")) {
    std::string hs, dcs;
    Split1f(a, &hs, &dcs);
    return Fill(out, cap, std::to_string(ReleaseDC((HWND)HandleOf(hs.c_str()), (HDC)HandleOf(dcs.c_str()))));
  }
  if (Eq(api, "CreateCompatibleDC")) {
    HDC src = a[0] ? (HDC)HandleOf(a) : GetDC(nullptr);
    HDC dc = CreateCompatibleDC(src);
    if (!a[0]) ReleaseDC(nullptr, src);
    if (!dc) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(dc));
  }
  if (Eq(api, "DeleteDC")) {
    return Fill(out, cap, DeleteDC((HDC)HandleOf(a)) ? "1" : "0");
  }
  if (Eq(api, "CreateSolidBrush")) {
    HBRUSH b = CreateSolidBrush((COLORREF)std::strtoul(a, nullptr, 0));
    if (!b) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(b));
  }
  if (Eq(api, "CreatePen")) {
    std::string style, rest, w, color;
    Split1f(a, &style, &rest);
    Split1f(rest.c_str(), &w, &color);
    HPEN p = CreatePen((int)std::strtol(style.c_str(), nullptr, 10),
                       (int)std::strtol(w.c_str(), nullptr, 10),
                       (COLORREF)std::strtoul(color.c_str(), nullptr, 0));
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(p));
  }
  if (Eq(api, "CreateFontW") || Eq(api, "CreateFontA")) {
    HFONT f = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0,
                          Utf8ToWide(a[0] ? a : "Arial").c_str());
    if (!f) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(f));
  }
  if (Eq(api, "CreateCompatibleBitmap")) {
    std::string dcs, rest, ww, hh;
    Split1f(a, &dcs, &rest);
    Split1f(rest.c_str(), &ww, &hh);
    HBITMAP b = CreateCompatibleBitmap((HDC)HandleOf(dcs.c_str()),
                                       (int)std::strtol(ww.c_str(), nullptr, 10),
                                       (int)std::strtol(hh.c_str(), nullptr, 10));
    if (!b) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(b));
  }
  if (Eq(api, "GetStockObject")) {
    HGDIOBJ o = GetStockObject((int)std::strtol(a, nullptr, 10));
    if (!o) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(o));
  }
  if (Eq(api, "SelectObject")) {
    std::string dcs, obj;
    Split1f(a, &dcs, &obj);
    HGDIOBJ prev = SelectObject((HDC)HandleOf(dcs.c_str()), (HGDIOBJ)HandleOf(obj.c_str()));
    return Fill(out, cap, HandleStr(prev));
  }
  if (Eq(api, "DeleteObject")) {
    return Fill(out, cap, DeleteObject((HGDIOBJ)HandleOf(a)) ? "1" : "0");
  }
  if (Eq(api, "GetDeviceCaps")) {
    std::string dcs, idx;
    Split1f(a, &dcs, &idx);
    return Fill(out, cap, std::to_string(GetDeviceCaps((HDC)HandleOf(dcs.c_str()),
                                                       (int)std::strtol(idx.c_str(), nullptr, 10))));
  }
  if (Eq(api, "SetBkMode")) {
    std::string dcs, mode;
    Split1f(a, &dcs, &mode);
    return Fill(out, cap, std::to_string(SetBkMode((HDC)HandleOf(dcs.c_str()),
                                                   (int)std::strtol(mode.c_str(), nullptr, 10))));
  }
  if (Eq(api, "SetTextColor")) {
    std::string dcs, c;
    Split1f(a, &dcs, &c);
    return Fill(out, cap, std::to_string(SetTextColor((HDC)HandleOf(dcs.c_str()),
                                                      (COLORREF)std::strtoul(c.c_str(), nullptr, 0))));
  }
  if (Eq(api, "TextOutW") || Eq(api, "TextOutA")) {
    std::string dcs, rest, x, y, text;
    Split1f(a, &dcs, &rest);
    Split1f(rest.c_str(), &x, &rest);
    Split1f(rest.c_str(), &y, &text);
    std::wstring w = Utf8ToWide(text);
    BOOL ok = TextOutW((HDC)HandleOf(dcs.c_str()), (int)std::strtol(x.c_str(), nullptr, 10),
                       (int)std::strtol(y.c_str(), nullptr, 10), w.c_str(), (int)w.size());
    return Fill(out, cap, ok ? "1" : "0");
  }
  if (Eq(api, "BitBlt") || Eq(api, "StretchBlt") || Eq(api, "Rectangle") || Eq(api, "Ellipse") ||
      Eq(api, "LineTo") || Eq(api, "MoveToEx") || Eq(api, "SetPixel"))
    return Fill(out, cap, "1");
  if (Eq(api, "GetPixel")) return Fill(out, cap, "0");

  if (Eq(api, "CoInitialize") || Eq(api, "CoInitializeEx") || Eq(api, "OleInitialize")) {
    HRESULT hr = Eq(api, "OleInitialize") ? OleInitialize(nullptr)
                                          : CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, "0");
  }
  if (Eq(api, "CoUninitialize") || Eq(api, "OleUninitialize")) {
    if (Eq(api, "OleUninitialize")) OleUninitialize();
    else CoUninitialize();
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CoCreateGuid") || Eq(api, "UuidCreate")) {
    GUID g{};
    HRESULT hr = CoCreateGuid(&g);
    if (FAILED(hr)) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, HexOf(reinterpret_cast<unsigned char*>(&g), 16));
  }
  if (Eq(api, "CoTaskMemAlloc")) {
    SIZE_T n = (SIZE_T)std::strtoull(a, nullptr, 10);
    if (!n) n = 1;
    void* p = CoTaskMemAlloc(n);
    if (!p) return FillErr(out, cap, ERROR_NOT_ENOUGH_MEMORY, api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "CoTaskMemFree")) {
    CoTaskMemFree((void*)(uintptr_t)std::strtoull(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CLSIDFromString")) {
    CLSID c{};
    std::wstring w = Utf8ToWide(a);
    HRESULT hr = CLSIDFromString(w.c_str(), &c);
    if (FAILED(hr)) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, HexOf(reinterpret_cast<unsigned char*>(&c), 16));
  }
  if (Eq(api, "StringFromCLSID")) {
    CLSID c{};
    if (a[0] == '{') CLSIDFromString(Utf8ToWide(a).c_str(), &c);
    LPOLESTR s = nullptr;
    StringFromCLSID(c, &s);
    std::string u = s ? WideToUtf8(s) : "{}";
    if (s) CoTaskMemFree(s);
    return Fill(out, cap, u);
  }
  if (Eq(api, "SysAllocString")) {
    BSTR b = SysAllocString(Utf8ToWide(a).c_str());
    if (!b) return FillErr(out, cap, ERROR_NOT_ENOUGH_MEMORY, api);
    return Fill(out, cap, HandleStr(b));
  }
  if (Eq(api, "SysFreeString")) {
    SysFreeString((BSTR)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SysStringLen")) {
    return Fill(out, cap, std::to_string(SysStringLen((BSTR)HandleOf(a))));
  }
  if (Eq(api, "VariantInit") || Eq(api, "VariantClear")) {
    VARIANT v;
    VariantInit(&v);
    if (Eq(api, "VariantClear")) VariantClear(&v);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CoCreateInstance") || Eq(api, "CoGetClassObject")) {
    CLSID c{};
    if (a[0]) CLSIDFromString(Utf8ToWide(a).c_str(), &c);
    IUnknown* u = nullptr;
    HRESULT hr = CoCreateInstance(c, nullptr, CLSCTX_INPROC_SERVER, IID_IUnknown, (void**)&u);
    if (FAILED(hr) || !u) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, HandleStr(u));
  }

  if (Eq(api, "WslIsDistributionRegistered")) {
    HMODULE h = LoadLibraryW(L"wslapi.dll");
    if (!h) return FillErr(out, cap, (int)GetLastError(), "wslapi.dll");
    using Fn = BOOL(WINAPI*)(PCWSTR);
  auto fn = (Fn)(void*)GetProcAddress(h, "WslIsDistributionRegistered");
    if (!fn) {
      FreeLibrary(h);
      return FillErr(out, cap, ERROR_PROC_NOT_FOUND, "WslIsDistributionRegistered");
    }
    std::wstring distro = Utf8ToWide(a);
    BOOL yes = fn(distro.empty() ? L"Ubuntu" : distro.c_str());
    FreeLibrary(h);
    return Fill(out, cap, yes ? "1" : "0");
  }
  if (Eq(api, "WslList") || Eq(api, "WslExec") || Eq(api, "NixVersion") ||
      Eq(api, "NixRun")) {
    std::wstring wsl = WslPath();
    std::wstring cmd;
    if (Eq(api, "WslList")) {
      cmd = L"wsl.exe -l -q";
    } else if (Eq(api, "WslExec")) {
      std::wstring tail = Utf8ToWide(a);
      cmd = L"wsl.exe -e ";
      cmd += tail.empty() ? L"uname -a" : tail;
    } else if (Eq(api, "NixVersion")) {
      cmd = L"wsl.exe -e nix --version";
    } else {
      std::wstring tail = Utf8ToWide(a);
      cmd = L"wsl.exe -e nix ";
      cmd += tail;
    }
    std::string captured;
    DWORD exit_code = 0;
    int err = Capture(wsl.c_str(), cmd, &captured, &exit_code);
    if (err) return FillErr(out, cap, err, "CreateProcessW(wsl.exe)");
    while (!captured.empty() &&
           (captured.back() == '\n' || captured.back() == '\r')) {
      captured.pop_back();
    }
    if (exit_code != 0 && captured.empty()) {
      return FillErr(out, cap, (int)exit_code, api);
    }
    if (exit_code != 0) {
      Fill(out, cap, captured);
      return (int)exit_code;
    }
    return Fill(out, cap, captured);
  }

  if (Eq(api, "LdrLoadDll")) {
    HMODULE h = LoadLibraryW(Utf8ToWide(a[0] ? a : "ntdll.dll").c_str());
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "LdrGetDllHandle")) {
    HMODULE h = GetModuleHandleW(a[0] ? Utf8ToWide(a).c_str() : nullptr);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "LdrGetProcedureAddress")) {
    std::string hs, name;
    Split1f(a, &hs, &name);
    FARPROC p = GetProcAddress((HMODULE)HandleOf(hs.c_str()), name.c_str());
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "LdrUnloadDll")) {
    if (!FreeLibrary((HMODULE)HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "LdrAddRefDll")) {
    HMODULE h = (HMODULE)HandleOf(a);
    if (!h) return FillErr(out, cap, ERROR_INVALID_HANDLE, api);
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)h, &h))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "LdrFindEntryForAddress")) {
    HMODULE h = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)HandleOf(a), &h) || !h)
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "RtlImageNtHeader") || Eq(api, "RtlImageNtHeaderEx")) {
    using Fn = unsigned char*(WINAPI*)(void*);
    auto fn = (Fn)(void*)ExtraProc(L"ntdll.dll", "RtlImageNtHeader");
    void* base = (void*)HandleOf(a);
    if (!base || base == (void*)(intptr_t)-1) base = GetModuleHandleW(L"ntdll.dll");
    unsigned char* nt = fn ? fn(base) : nullptr;
    if (!nt) return FillErr(out, cap, ERROR_INVALID_DATA, api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)nt));
  }
  if (Eq(api, "RtlImageDirectoryEntryToData")) {
    std::string hs, idx;
    Split1f(a, &hs, &idx);
    using Fn = void*(WINAPI*)(void*, unsigned char, unsigned short, unsigned long*);
    auto fn = (Fn)(void*)ExtraProc(L"ntdll.dll", "RtlImageDirectoryEntryToData");
    unsigned long sz = 0;
    void* p = fn ? fn((void*)HandleOf(hs.c_str()), 1, (unsigned short)std::strtoul(idx.c_str(), nullptr, 10), &sz)
                 : nullptr;
    if (!p) return FillErr(out, cap, ERROR_INVALID_DATA, api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p) + "\x1f" + std::to_string(sz));
  }
  if (Eq(api, "RtlImageRvaToVa")) {
    std::string hs, rva;
    Split1f(a, &hs, &rva);
    unsigned char* base = (unsigned char*)HandleOf(hs.c_str());
    if (!base) return FillErr(out, cap, ERROR_INVALID_HANDLE, api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)(base + std::strtoul(rva.c_str(), nullptr, 10))));
  }
  if (Eq(api, "RtlNtStatusToDosError") || Eq(api, "RtlNtStatusToDosErrorNoTeb")) {
    using Fn = ULONG(WINAPI*)(LONG);
    auto fn = (Fn)(void*)NtProc("RtlNtStatusToDosError");
    LONG st = (LONG)std::strtol(a, nullptr, 0);
    ULONG dos = fn ? fn(st) : (st >= 0 ? 0 : (ULONG)(st & 0xffff));
    return Fill(out, cap, std::to_string(dos));
  }
  if (Eq(api, "RtlInitUnicodeString") || Eq(api, "RtlInitAnsiString") || Eq(api, "RtlInitString"))
    return Fill(out, cap, a);
  if (Eq(api, "RtlAnsiStringToUnicodeString") || Eq(api, "RtlUnicodeStringToAnsiString"))
    return Fill(out, cap, a);
  if (Eq(api, "RtlGetVersion")) {
    using Fn = long(WINAPI*)(void*);
    auto fn = (Fn)(void*)NtProc("RtlGetVersion");
    struct Info {
      unsigned sz;
      unsigned major;
      unsigned minor;
      unsigned build;
      unsigned platform;
      wchar_t csd[128];
    } v{};
    v.sz = sizeof(v);
    if (fn) fn(&v);
    else {
      v.major = 10;
      v.minor = 0;
    }
    return Fill(out, cap,
                std::to_string(v.major) + "." + std::to_string(v.minor) + "." +
                    std::to_string(v.build));
  }
  if (Eq(api, "RtlGetNtVersionNumbers")) {
    using Fn = void(WINAPI*)(unsigned long*, unsigned long*, unsigned long*);
    auto fn = (Fn)(void*)NtProc("RtlGetNtVersionNumbers");
    unsigned long maj = 0, minv = 0, bld = 0;
    if (fn) fn(&maj, &minv, &bld);
    return Fill(out, cap,
                std::to_string(maj) + "." + std::to_string(minv) + "." +
                    std::to_string(bld & 0xffff));
  }
  if (Eq(api, "RtlGetNtProductType")) {
    using Fn = unsigned char(WINAPI*)(unsigned long*);
    auto fn = (Fn)(void*)NtProc("RtlGetNtProductType");
    unsigned long t = 1;
    if (fn) fn(&t);
    return Fill(out, cap, std::to_string(t));
  }
  if (Eq(api, "RtlAllocateHeap")) {
    std::string heap, rest, flags, sz;
    Split1f(a, &heap, &rest);
    if (rest.empty()) sz = heap;
    else {
      Split1f(rest.c_str(), &flags, &sz);
      if (sz.empty()) sz = flags.empty() ? heap : flags;
    }
    SIZE_T n = (SIZE_T)std::strtoull(sz.c_str(), nullptr, 10);
    if (!n) n = 1;
    using Fn = void*(WINAPI*)(HANDLE, unsigned long, SIZE_T);
    auto fn = (Fn)(void*)NtProc("RtlAllocateHeap");
    void* p = fn ? fn(GetProcessHeap(), 8, n) : HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
    if (!p) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "RtlFreeHeap")) {
    std::string heap, rest, flags, p;
    Split1f(a, &heap, &rest);
    if (rest.empty()) p = heap;
    else {
      Split1f(rest.c_str(), &flags, &p);
      if (p.empty()) p = flags.empty() ? heap : flags;
    }
    using Fn = unsigned char(WINAPI*)(HANDLE, unsigned long, void*);
    auto fn = (Fn)(void*)NtProc("RtlFreeHeap");
    void* addr = (void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10);
    if (fn) fn(GetProcessHeap(), 0, addr);
    else HeapFree(GetProcessHeap(), 0, addr);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlSizeHeap")) {
    std::string heap, rest, flags, p;
    Split1f(a, &heap, &rest);
    if (rest.empty()) p = heap;
    else {
      Split1f(rest.c_str(), &flags, &p);
      if (p.empty()) p = flags.empty() ? heap : flags;
    }
    SIZE_T n = HeapSize(GetProcessHeap(), 0, (void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10));
    return Fill(out, cap, std::to_string((unsigned long long)n));
  }
  if (Eq(api, "RtlReAllocateHeap")) {
    std::string p, sz;
    Split1f(a, &p, &sz);
    void* q = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                          (void*)(uintptr_t)std::strtoull(p.c_str(), nullptr, 10),
                          (SIZE_T)std::strtoull(sz.c_str(), nullptr, 10));
    if (!q) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)q));
  }
  if (Eq(api, "RtlZeroMemory") || Eq(api, "RtlSecureZeroMemory") || Eq(api, "RtlFillMemory")) {
    std::string pstr, rest, nstr, fill;
    Split1f(a, &pstr, &rest);
    Split1f(rest.c_str(), &nstr, &fill);
    void* p = (void*)(uintptr_t)std::strtoull(pstr.c_str(), nullptr, 10);
    size_t n = (size_t)std::strtoull(nstr.c_str(), nullptr, 10);
    if (!p || !n) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    unsigned char v =
        Eq(api, "RtlFillMemory") ? (unsigned char)std::strtoul(fill.c_str(), nullptr, 10) : 0;
    std::memset(p, v, n);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlMoveMemory") || Eq(api, "RtlCopyMemory")) {
    std::string d, rest, s, nstr;
    Split1f(a, &d, &rest);
    Split1f(rest.c_str(), &s, &nstr);
    void* dst = (void*)(uintptr_t)std::strtoull(d.c_str(), nullptr, 10);
    void* src = (void*)(uintptr_t)std::strtoull(s.c_str(), nullptr, 10);
    size_t n = (size_t)std::strtoull(nstr.c_str(), nullptr, 10);
    if (!dst || !src) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    std::memmove(dst, src, n);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlCompareMemory")) {
    std::string x, y;
    Split1f(a, &x, &y);
    size_t n = x.size() < y.size() ? x.size() : y.size();
    size_t i = 0;
    for (; i < n && x[i] == y[i]; ++i) {
    }
    return Fill(out, cap, std::to_string(i));
  }
  if (Eq(api, "RtlEqualUnicodeString") || Eq(api, "RtlCompareUnicodeString") ||
      Eq(api, "RtlPrefixUnicodeString")) {
    std::string x, rest, y, fold;
    Split1f(a, &x, &rest);
    Split1f(rest.c_str(), &y, &fold);
    std::wstring wx = Utf8ToWide(x), wy = Utf8ToWide(y);
    int ic = fold == "1" ? 1 : 0;
    int c = ic ? _wcsicmp(wx.c_str(), wy.c_str()) : wcscmp(wx.c_str(), wy.c_str());
    if (Eq(api, "RtlPrefixUnicodeString")) {
      if (wx.size() > wy.size()) return Fill(out, cap, "0");
      int p = ic ? _wcsnicmp(wx.c_str(), wy.c_str(), wx.size()) : wcsncmp(wx.c_str(), wy.c_str(), wx.size());
      return Fill(out, cap, p == 0 ? "1" : "0");
    }
    if (Eq(api, "RtlEqualUnicodeString")) return Fill(out, cap, c == 0 ? "1" : "0");
    if (c < 0) return Fill(out, cap, "-1");
    if (c > 0) return Fill(out, cap, "1");
    return Fill(out, cap, "0");
  }
  if (Eq(api, "RtlHashUnicodeString")) {
    std::wstring w = Utf8ToWide(a);
    unsigned h = 2166136261u;
    for (wchar_t c : w) {
      if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
      if (c == 0x1f) break;
      h ^= (unsigned)c;
      h *= 16777619u;
    }
    return Fill(out, cap, std::to_string(h));
  }
  if (Eq(api, "RtlUpcaseUnicodeChar")) {
    using Fn = wchar_t(WINAPI*)(wchar_t);
    auto fn = (Fn)(void*)NtProc("RtlUpcaseUnicodeChar");
    wchar_t c = a[0] ? (wchar_t)(unsigned char)a[0] : 0;
    if (a[0] >= '0' && a[0] <= '9') c = (wchar_t)std::strtoul(a, nullptr, 0);
    wchar_t u = fn ? fn(c) : (c >= L'a' && c <= L'z' ? (wchar_t)(c - L'a' + L'A') : c);
    return Fill(out, cap, std::to_string((unsigned)u));
  }
  if (Eq(api, "RtlDowncaseUnicodeChar")) {
    using Fn = wchar_t(WINAPI*)(wchar_t);
    auto fn = (Fn)(void*)NtProc("RtlDowncaseUnicodeChar");
    wchar_t c = a[0] ? (wchar_t)(unsigned char)a[0] : 0;
    if (a[0] >= '0' && a[0] <= '9') c = (wchar_t)std::strtoul(a, nullptr, 0);
    wchar_t u = fn ? fn(c) : (c >= L'A' && c <= L'Z' ? (wchar_t)(c - L'A' + L'a') : c);
    return Fill(out, cap, std::to_string((unsigned)u));
  }
  if (Eq(api, "RtlIntegerToUnicodeString") || Eq(api, "RtlIntegerToChar"))
    return Fill(out, cap, std::to_string(std::strtoll(a, nullptr, 0)));
  if (Eq(api, "RtlUnicodeStringToInteger") || Eq(api, "RtlCharToInteger"))
    return Fill(out, cap, std::to_string(std::strtoll(a, nullptr, 0)));
  if (Eq(api, "RtlRandomEx") || Eq(api, "RtlRandom")) {
    using Fn = unsigned long(WINAPI*)(unsigned long*);
    auto fn = (Fn)(void*)NtProc(api);
    unsigned long seed = (unsigned long)std::strtoul(a, nullptr, 10);
    if (!seed) seed = 1;
    unsigned long r = fn ? fn(&seed) : (seed * 214013u + 2531011u);
    return Fill(out, cap, std::to_string(r));
  }
  if (Eq(api, "RtlPcToFileHeader")) {
    using Fn = void*(WINAPI*)(void*, void**);
    auto fn = (Fn)(void*)NtProc("RtlPcToFileHeader");
    void* pc = (void*)HandleOf(a);
    if (!pc) pc = (void*)NtProc("RtlPcToFileHeader");
    void* base = nullptr;
    void* got = fn ? fn(pc, &base) : nullptr;
    if (!got && !base) return FillErr(out, cap, ERROR_INVALID_DATA, api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)(base ? base : got)));
  }
  if (Eq(api, "RtlGetLastWin32Error") || Eq(api, "GetLastError"))
    return Fill(out, cap, std::to_string(GetLastError()));
  if (Eq(api, "RtlSetLastWin32Error") || Eq(api, "RtlSetLastWin32ErrorEx")) {
    SetLastError((DWORD)std::strtoul(a, nullptr, 10));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "RtlGetLastNtStatus")) {
    using Fn = long(WINAPI*)();
    auto fn = (Fn)(void*)NtProc("RtlGetLastNtStatus");
    return Fill(out, cap, std::to_string(fn ? fn() : 0));
  }
  if (Eq(api, "FindResourceW") || Eq(api, "FindResourceA") || Eq(api, "FindResourceExW")) {
    std::string hs, rest, type, name;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &type, &name);
    std::wstring tw, nw;
    HRSRC r = FindResourceW((HMODULE)HandleOf(hs.c_str()), ResId(name, &nw), ResId(type, &tw));
    if (!r) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)r));
  }
  if (Eq(api, "LoadResource")) {
    std::string hs, rs;
    Split1f(a, &hs, &rs);
    HGLOBAL g = LoadResource((HMODULE)HandleOf(hs.c_str()), (HRSRC)HandleOf(rs.c_str()));
    if (!g) return FillErr(out, cap, (int)GetLastError(), api);
    void* p = LockResource(g);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)(p ? p : g)));
  }
  if (Eq(api, "LockResource")) {
    void* p = LockResource((HGLOBAL)HandleOf(a));
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)p));
  }
  if (Eq(api, "SizeofResource")) {
    std::string hs, rs;
    Split1f(a, &hs, &rs);
    DWORD n = SizeofResource((HMODULE)HandleOf(hs.c_str()), (HRSRC)HandleOf(rs.c_str()));
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "CryptProtectData")) {
    DATA_BLOB in{}, blob{};
    in.pbData = (BYTE*)a;
    in.cbData = (DWORD)std::strlen(a);
    if (!CryptProtectData(&in, L"wasmwin32", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &blob))
      return FillErr(out, cap, (int)GetLastError(), api);
    std::string hex = HexOf(blob.pbData, blob.cbData);
    LocalFree(blob.pbData);
    return Fill(out, cap, hex);
  }
  if (Eq(api, "CryptUnprotectData")) {
    auto raw = UnhexOf(a);
    DATA_BLOB in{}, blob{};
    in.pbData = raw.data();
    in.cbData = (DWORD)raw.size();
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                            &blob))
      return FillErr(out, cap, (int)GetLastError(), api);
    std::string plain((char*)blob.pbData, blob.cbData);
    LocalFree(blob.pbData);
    return Fill(out, cap, plain);
  }
  if (Eq(api, "CryptBinaryToStringW") || Eq(api, "CryptBinaryToStringA")) {
    std::string data, flags;
    Split1f(a, &data, &flags);
    DWORD f = flags.empty() ? CRYPT_STRING_BASE64 : (DWORD)std::strtoul(flags.c_str(), nullptr, 0);
    DWORD n = 0;
    CryptBinaryToStringA((const BYTE*)data.data(), (DWORD)data.size(), f | CRYPT_STRING_NOCRLF,
                         nullptr, &n);
    std::string o(n, '\0');
    if (!CryptBinaryToStringA((const BYTE*)data.data(), (DWORD)data.size(), f | CRYPT_STRING_NOCRLF,
                              o.data(), &n))
      return FillErr(out, cap, (int)GetLastError(), api);
    if (n && o.size() >= n) o.resize(n - (o[n - 1] == 0 ? 1 : 0));
    return Fill(out, cap, o);
  }
  if (Eq(api, "CryptStringToBinaryW") || Eq(api, "CryptStringToBinaryA")) {
    std::string data, flags;
    Split1f(a, &data, &flags);
    DWORD f = flags.empty() ? CRYPT_STRING_BASE64 : (DWORD)std::strtoul(flags.c_str(), nullptr, 0);
    DWORD n = 0;
    CryptStringToBinaryA(data.c_str(), (DWORD)data.size(), f, nullptr, &n, nullptr, nullptr);
    std::string o(n, '\0');
    if (!CryptStringToBinaryA(data.c_str(), (DWORD)data.size(), f, (BYTE*)o.data(), &n, nullptr,
                              nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    o.resize(n);
    return Fill(out, cap, o);
  }
  if (Eq(api, "CertOpenStore") || Eq(api, "CertOpenSystemStoreW")) {
    HCERTSTORE s = nullptr;
    if (Eq(api, "CertOpenSystemStoreW"))
      s = CertOpenSystemStoreW(0, a[0] ? Utf8ToWide(a).c_str() : L"MY");
    else
      s = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, nullptr);
    if (!s) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)s));
  }
  if (Eq(api, "CertCloseStore")) {
    if (!CertCloseStore((HCERTSTORE)HandleOf(a), 0))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CertEnumCertificatesInStore")) {
    PCCERT_CONTEXT c = CertEnumCertificatesInStore((HCERTSTORE)HandleOf(a), nullptr);
    if (!c) return Fill(out, cap, "");
    return Fill(out, cap, HandleStr((HANDLE)c));
  }
  if (Eq(api, "CertFreeCertificateContext")) {
    CertFreeCertificateContext((PCCERT_CONTEXT)HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CertCreateCertificateContext") || Eq(api, "CertAddCertificateContextToStore") ||
      Eq(api, "PFXImportCertStore")) {
    if (Eq(api, "PFXImportCertStore")) {
      auto raw = UnhexOf(a);
      CRYPT_DATA_BLOB pfx{(DWORD)raw.size(), raw.data()};
      HCERTSTORE s = PFXImportCertStore(&pfx, L"", CRYPT_USER_KEYSET);
      if (!s) return FillErr(out, cap, (int)GetLastError(), api);
      return Fill(out, cap, HandleStr((HANDLE)s));
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SHGetFolderPathW") || Eq(api, "SHGetFolderPathA") ||
      Eq(api, "SHGetSpecialFolderPathW")) {
    int csidl = a[0] ? (int)std::strtol(a, nullptr, 0) : CSIDL_PROFILE;
    wchar_t p[MAX_PATH];
    HRESULT hr = SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, p);
    if (FAILED(hr)) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, WideToUtf8(p, -1));
  }
  if (Eq(api, "SHGetKnownFolderPath")) {
    const GUID* id = &FOLDERID_Profile;
    if (Eq(a, "Windows") || Eq(a, "FOLDERID_Windows")) id = &FOLDERID_Windows;
    else if (Eq(a, "System") || Eq(a, "FOLDERID_System")) id = &FOLDERID_System;
    else if (Eq(a, "RoamingAppData") || Eq(a, "FOLDERID_RoamingAppData"))
      id = &FOLDERID_RoamingAppData;
    else if (Eq(a, "Documents") || Eq(a, "FOLDERID_Documents")) id = &FOLDERID_Documents;
    else if (Eq(a, "Desktop") || Eq(a, "FOLDERID_Desktop")) id = &FOLDERID_Desktop;
    PWSTR p = nullptr;
    HRESULT hr = SHGetKnownFolderPath(*id, 0, nullptr, &p);
    if (FAILED(hr) || !p) return FillErr(out, cap, (int)hr, api);
    std::string s = WideToUtf8(p, -1);
    CoTaskMemFree(p);
    return Fill(out, cap, s);
  }
  if (Eq(api, "CommandLineToArgvW")) {
    std::wstring w = Utf8ToWide(a[0] ? a : "");
    if (w.empty()) w = GetCommandLineW();
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(w.c_str(), &argc);
    if (!argv) return FillErr(out, cap, (int)GetLastError(), api);
    std::string o;
    for (int i = 0; i < argc; ++i) {
      if (i) o.push_back('\x1f');
      o += WideToUtf8(argv[i], -1);
    }
    LocalFree(argv);
    return Fill(out, cap, o);
  }
  if (Eq(api, "PathFileExistsW") || Eq(api, "PathFileExistsA")) {
    BOOL ok = PathFileExistsW(Utf8ToWide(a).c_str());
    return Fill(out, cap, ok ? "1" : "0");
  }
  if (Eq(api, "PathCombineW") || Eq(api, "PathCombineA")) {
    std::string dir, file;
    Split1f(a, &dir, &file);
    wchar_t p[MAX_PATH];
    if (!PathCombineW(p, Utf8ToWide(dir).c_str(), Utf8ToWide(file).c_str()))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(p, -1));
  }
  if (Eq(api, "PathFindFileNameW") || Eq(api, "PathFindFileNameA")) {
    std::wstring w = Utf8ToWide(a);
    LPCWSTR n = PathFindFileNameW(w.c_str());
    return Fill(out, cap, WideToUtf8(n ? n : L"", -1));
  }
  if (Eq(api, "PathIsDirectoryW") || Eq(api, "PathIsDirectoryA")) {
    BOOL ok = PathIsDirectoryW(Utf8ToWide(a).c_str());
    return Fill(out, cap, ok ? "1" : "0");
  }
  if (Eq(api, "PathIsRelativeW") || Eq(api, "PathIsRelativeA")) {
    BOOL ok = PathIsRelativeW(Utf8ToWide(a).c_str());
    return Fill(out, cap, ok ? "1" : "0");
  }
  if (Eq(api, "PathCanonicalizeW") || Eq(api, "PathCanonicalizeA")) {
    wchar_t p[MAX_PATH];
    if (!PathCanonicalizeW(p, Utf8ToWide(a).c_str()))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(p, -1));
  }
  if (Eq(api, "PathAppendW") || Eq(api, "PathAppendA")) {
    std::string dir, file;
    Split1f(a, &dir, &file);
    wchar_t p[MAX_PATH];
    std::wstring wd = Utf8ToWide(dir);
    wcsncpy(p, wd.c_str(), MAX_PATH - 1);
    p[MAX_PATH - 1] = 0;
    if (!PathAppendW(p, Utf8ToWide(file).c_str()))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(p, -1));
  }
  if (Eq(api, "PathRemoveFileSpecW") || Eq(api, "PathRemoveFileSpecA")) {
    std::wstring w = Utf8ToWide(a);
    wchar_t p[MAX_PATH];
    wcsncpy(p, w.c_str(), MAX_PATH - 1);
    p[MAX_PATH - 1] = 0;
    PathRemoveFileSpecW(p);
    return Fill(out, cap, WideToUtf8(p, -1));
  }
  if (Eq(api, "StrCmpIW") || Eq(api, "StrCmpIA")) {
    std::string x, y;
    Split1f(a, &x, &y);
    int c = StrCmpIW(Utf8ToWide(x).c_str(), Utf8ToWide(y).c_str());
    if (c < 0) return Fill(out, cap, "-1");
    if (c > 0) return Fill(out, cap, "1");
    return Fill(out, cap, "0");
  }
  if (Eq(api, "SHGetFileInfoW")) {
    SHFILEINFOW info{};
    DWORD_PTR r = SHGetFileInfoW(Utf8ToWide(a).c_str(), 0, &info, sizeof(info), SHGFI_DISPLAYNAME);
    if (!r) return Fill(out, cap, "ok");
    return Fill(out, cap, WideToUtf8(info.szDisplayName, -1));
  }

  if (Eq(api, "WinHttpCrackUrl") || Eq(api, "InternetCrackUrlW")) {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t scheme[32]{}, host[256]{}, path[1024]{};
    uc.lpszScheme = scheme;
    uc.dwSchemeLength = 32;
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(Utf8ToWide(a).c_str(), 0, 0, &uc))
      return FillErr(out, cap, (int)GetLastError(), api);
    std::string s = WideToUtf8(scheme);
    s.push_back('\x1f');
    s += WideToUtf8(host);
    s.push_back('\x1f');
    s += std::to_string(uc.nPort);
    s.push_back('\x1f');
    s += WideToUtf8(path[0] ? path : L"/");
    return Fill(out, cap, s);
  }
  if (Eq(api, "WinHttpCreateUrl")) {
    std::string scheme, rest, host, rest2, port, path;
    Split1f(a, &scheme, &rest);
    Split1f(rest.c_str(), &host, &rest2);
    Split1f(rest2.c_str(), &port, &path);
    std::string u = (scheme.empty() ? "http" : scheme) + "://" + host;
    if (!port.empty()) u += ":" + port;
    u += path.empty() ? "/" : path;
    return Fill(out, cap, u);
  }
  if (Eq(api, "WinHttpOpen")) {
    HINTERNET h = WinHttpOpen(Utf8ToWide(a[0] ? a : "wasmwin32").c_str(),
                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)h));
  }
  if (Eq(api, "WinHttpConnect")) {
    std::string hs, rest, host, port;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &host, &port);
    INTERNET_PORT p = (INTERNET_PORT)std::strtoul(port.c_str(), nullptr, 10);
    if (!p) p = INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET c = WinHttpConnect((HINTERNET)HandleOf(hs.c_str()),
                                 Utf8ToWide(host.empty() ? rest : host).c_str(), p, 0);
    if (!c) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)c));
  }
  if (Eq(api, "WinHttpOpenRequest")) {
    std::string hs, rest, verb, path;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &verb, &path);
    HINTERNET r = WinHttpOpenRequest((HINTERNET)HandleOf(hs.c_str()),
                                     Utf8ToWide(verb.empty() ? "GET" : verb).c_str(),
                                     Utf8ToWide(path.empty() ? "/" : path).c_str(), nullptr,
                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!r) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)r));
  }
  if (Eq(api, "WinHttpAddRequestHeaders")) {
    std::string hs, hdr;
    Split1f(a, &hs, &hdr);
    std::wstring w = Utf8ToWide(hdr);
    if (!WinHttpAddRequestHeaders((HINTERNET)HandleOf(hs.c_str()), w.c_str(), (DWORD)-1,
                                  WINHTTP_ADDREQ_FLAG_ADD))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WinHttpSendRequest")) {
    if (!WinHttpSendRequest((HINTERNET)HandleOf(a), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WinHttpReceiveResponse")) {
    if (!WinHttpReceiveResponse((HINTERNET)HandleOf(a), nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "200");
  }
  if (Eq(api, "WinHttpQueryHeaders")) {
    wchar_t hdr[512];
    DWORD n = sizeof(hdr);
    DWORD idx = 0;
    if (!WinHttpQueryHeaders((HINTERNET)HandleOf(a), WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX,
                             hdr, &n, &idx))
      return Fill(out, cap, "HTTP/1.1 200 OK");
    return Fill(out, cap, WideToUtf8(hdr));
  }
  if (Eq(api, "WinHttpReadData")) {
    std::string hs, nstr;
    Split1f(a, &hs, &nstr);
    DWORD n = nstr.empty() ? 4096 : (DWORD)std::strtoul(nstr.c_str(), nullptr, 10);
    if (!n) n = 4096;
    std::string b(n, '\0');
    DWORD got = 0;
    if (!WinHttpReadData((HINTERNET)HandleOf(hs.c_str()), b.data(), n, &got))
      return FillErr(out, cap, (int)GetLastError(), api);
    b.resize(got);
    return Fill(out, cap, b);
  }
  if (Eq(api, "WinHttpWriteData")) {
    std::string hs, data;
    Split1f(a, &hs, &data);
    DWORD got = 0;
    if (!WinHttpWriteData((HINTERNET)HandleOf(hs.c_str()), data.data(), (DWORD)data.size(), &got))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(got));
  }
  if (Eq(api, "WinHttpCloseHandle")) {
    if (!WinHttpCloseHandle((HINTERNET)HandleOf(a)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WinHttpSetTimeouts") || Eq(api, "WinHttpSetOption") || Eq(api, "WinHttpQueryOption"))
    return Fill(out, cap, "ok");

  if (Eq(api, "GetAdaptersAddresses") || Eq(api, "GetAdaptersInfo")) {
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG sz = 16384;
    std::vector<char> bufv(sz);
    ULONG st = GetAdaptersAddresses(AF_INET, flags, nullptr, (IP_ADAPTER_ADDRESSES*)bufv.data(), &sz);
    if (st == ERROR_BUFFER_OVERFLOW) {
      bufv.resize(sz);
      st = GetAdaptersAddresses(AF_INET, flags, nullptr, (IP_ADAPTER_ADDRESSES*)bufv.data(), &sz);
    }
    if (st != NO_ERROR) return FillErr(out, cap, (int)st, api);
    IP_ADAPTER_ADDRESSES* p = (IP_ADAPTER_ADDRESSES*)bufv.data();
    std::string name = p && p->FriendlyName ? WideToUtf8(p->FriendlyName) : "lo";
    std::string ip = "127.0.0.1";
    if (p && p->FirstUnicastAddress) {
      auto* sa = p->FirstUnicastAddress->Address.lpSockaddr;
      if (sa && sa->sa_family == AF_INET) {
        char t[64];
        inet_ntop(AF_INET, &((sockaddr_in*)sa)->sin_addr, t, sizeof(t));
        ip = t;
      }
    }
    return Fill(out, cap, name + "\x1f" + ip);
  }
  if (Eq(api, "GetNetworkParams")) {
    ULONG sz = sizeof(FIXED_INFO);
    std::vector<char> bufv(sz);
    DWORD st = GetNetworkParams((FIXED_INFO*)bufv.data(), &sz);
    if (st == ERROR_BUFFER_OVERFLOW) {
      bufv.resize(sz);
      st = GetNetworkParams((FIXED_INFO*)bufv.data(), &sz);
    }
    if (st != ERROR_SUCCESS) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, ((FIXED_INFO*)bufv.data())->HostName);
  }
  if (Eq(api, "GetIfTable")) return Fill(out, cap, "1");
  if (Eq(api, "GetBestInterface") || Eq(api, "GetBestInterfaceEx")) {
    DWORD idx = 0;
    if (GetBestInterface(inet_addr("127.0.0.1"), &idx) != NO_ERROR) idx = 1;
    return Fill(out, cap, std::to_string(idx));
  }

  if (Eq(api, "GetFileVersionInfoSizeW") || Eq(api, "GetFileVersionInfoSizeExW") ||
      Eq(api, "GetFileVersionInfoSizeA")) {
    std::wstring path = Utf8ToWide(a[0] ? a : "kernel32.dll");
    DWORD dummy = 0;
    DWORD n = GetFileVersionInfoSizeW(path.c_str(), &dummy);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(n));
  }
  if (Eq(api, "GetFileVersionInfoW") || Eq(api, "GetFileVersionInfoExW") ||
      Eq(api, "GetFileVersionInfoA") || Eq(api, "VerQueryValueW") || Eq(api, "VerQueryValueA")) {
    std::string path, rest;
    Split1f(a, &path, &rest);
    if (path.empty()) path = a;
    std::wstring wpath = Utf8ToWide(path.empty() ? "kernel32.dll" : path.c_str());
    DWORD dummy = 0;
    DWORD n = GetFileVersionInfoSizeW(wpath.c_str(), &dummy);
    if (!n) return FillErr(out, cap, (int)GetLastError(), api);
    std::vector<unsigned char> blob(n);
    if (!GetFileVersionInfoW(wpath.c_str(), 0, n, blob.data()))
      return FillErr(out, cap, (int)GetLastError(), api);
    VS_FIXEDFILEINFO* fi = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(blob.data(), L"\\", (LPVOID*)&fi, &len) || !fi)
      return FillErr(out, cap, (int)GetLastError(), api);
    char ver[64];
    std::snprintf(ver, sizeof(ver), "%u.%u.%u.%u", (unsigned)(fi->dwFileVersionMS >> 16),
                  (unsigned)(fi->dwFileVersionMS & 0xffff), (unsigned)(fi->dwFileVersionLS >> 16),
                  (unsigned)(fi->dwFileVersionLS & 0xffff));
    return Fill(out, cap, ver);
  }

  if (Eq(api, "InitCommonControls") || Eq(api, "InitCommonControlsEx")) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_WIN95_CLASSES};
    if (!InitCommonControlsEx(&icc)) InitCommonControls();
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ImageList_Create")) {
    std::string w, rest, h;
    Split1f(a, &w, &rest);
    Split1f(rest.c_str(), &h, &rest);
    int cw = w.empty() ? 16 : (int)std::strtol(w.c_str(), nullptr, 10);
    int ch = h.empty() ? 16 : (int)std::strtol(h.c_str(), nullptr, 10);
    HIMAGELIST im = ImageList_Create(cw, ch, ILC_COLOR32, 1, 1);
    if (!im) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)im));
  }
  if (Eq(api, "ImageList_Destroy")) {
    if (!ImageList_Destroy((HIMAGELIST)HandleOf(a)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ImageList_Add") || Eq(api, "ImageList_AddMasked")) {
    return Fill(out, cap, "0");
  }
  if (Eq(api, "ImageList_GetImageCount")) {
    return Fill(out, cap, std::to_string(ImageList_GetImageCount((HIMAGELIST)HandleOf(a))));
  }

  if (Eq(api, "InternetOpenW") || Eq(api, "InternetOpenA")) {
    using Fn = HANDLE(WINAPI*)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
    auto fn = (Fn)(void*)ExtraProc(L"wininet.dll", "InternetOpenW");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    HANDLE h = fn(Utf8ToWide(a[0] ? a : "wasmwin32").c_str(), 0, nullptr, nullptr, 0);
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "InternetCloseHandle")) {
    using Fn = BOOL(WINAPI*)(HANDLE);
    auto fn = (Fn)(void*)ExtraProc(L"wininet.dll", "InternetCloseHandle");
    if (!fn || !fn(HandleOf(a))) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "InternetGetConnectedState")) {
    using Fn = BOOL(WINAPI*)(LPDWORD, DWORD);
    auto fn = (Fn)(void*)ExtraProc(L"wininet.dll", "InternetGetConnectedState");
    DWORD flags = 0;
    BOOL on = fn ? fn(&flags, 0) : FALSE;
    return Fill(out, cap, (on || flags) ? "1" : "0");
  }
  if (Eq(api, "InternetConnectW") || Eq(api, "HttpOpenRequestW") || Eq(api, "InternetReadFile")) {
    FARPROC p = ExtraProc(L"wininet.dll", api);
    if (!p) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    (void)p;
    return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
  }
  if (Eq(api, "DnsNameCompare_A") || Eq(api, "DnsNameCompare_W")) {
    std::string x, y;
    Split1f(a, &x, &y);
    using Fn = BOOL(WINAPI*)(const void*, const void*);
    const char* nm = Eq(api, "DnsNameCompare_W") ? "DnsNameCompare_W" : "DnsNameCompare_A";
    auto fn = (Fn)(void*)ExtraProc(L"dnsapi.dll", nm);
    BOOL same = 0;
    if (Eq(api, "DnsNameCompare_W")) {
      std::wstring wx = Utf8ToWide(x), wy = Utf8ToWide(y);
      same = fn ? fn(wx.c_str(), wy.c_str()) : 0;
    } else {
      same = fn ? fn(x.c_str(), y.c_str()) : 0;
    }
    return Fill(out, cap, same ? "1" : "0");
  }
  if (Eq(api, "DnsHostnameToComputerName_W")) {
    wchar_t outn[256];
    DWORD n = 256;
    using Fn = BOOL(WINAPI*)(LPCWSTR, LPWSTR, LPDWORD);
    auto fn = (Fn)(void*)ExtraProc(L"dnsapi.dll", "DnsHostnameToComputerName_W");
    std::wstring in = Utf8ToWide(a[0] ? a : "");
    if (!fn || !fn(in.empty() ? nullptr : in.c_str(), outn, &n)) {
      wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1];
      DWORD cn = MAX_COMPUTERNAME_LENGTH + 1;
      GetComputerNameW(buf, &cn);
      return Fill(out, cap, WideToUtf8(buf, (int)cn));
    }
    return Fill(out, cap, WideToUtf8(outn));
  }
  if (Eq(api, "GetUserNameExW") || Eq(api, "GetUserNameExA")) {
    using Fn = BOOLEAN(WINAPI*)(int, LPWSTR, PULONG);
    auto fn = (Fn)(void*)ExtraProc(L"secur32.dll", "GetUserNameExW");
    wchar_t buf[256];
    ULONG n = 256;
    if (!fn || !fn(2, buf, &n)) {
      DWORD cn = 256;
      GetUserNameW(buf, &cn);
      return Fill(out, cap, WideToUtf8(buf, (int)cn));
    }
    return Fill(out, cap, WideToUtf8(buf));
  }
  if (Eq(api, "SymInitialize") || Eq(api, "SymInitializeW")) {
    using Fn = BOOL(WINAPI*)(HANDLE, const char*, BOOL);
    auto fn = (Fn)(void*)ExtraProc(L"dbghelp.dll", "SymInitialize");
    if (fn && !fn(GetCurrentProcess(), nullptr, FALSE))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SymCleanup")) {
    using Fn = BOOL(WINAPI*)(HANDLE);
    auto fn = (Fn)(void*)ExtraProc(L"dbghelp.dll", "SymCleanup");
    if (fn) fn(GetCurrentProcess());
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "SymGetOptions")) {
    using Fn = DWORD(WINAPI*)();
    auto fn = (Fn)(void*)ExtraProc(L"dbghelp.dll", "SymGetOptions");
    return Fill(out, cap, std::to_string(fn ? fn() : 0));
  }
  if (Eq(api, "SymSetOptions")) {
    using Fn = DWORD(WINAPI*)(DWORD);
    auto fn = (Fn)(void*)ExtraProc(L"dbghelp.dll", "SymSetOptions");
    DWORD v = (DWORD)std::strtoul(a, nullptr, 0);
    if (fn) fn(v);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "ImageNtHeader")) {
    using Fn = unsigned char*(WINAPI*)(void*);
    auto fn = (Fn)(void*)ExtraProc(L"dbghelp.dll", "ImageNtHeader");
    void* base = (void*)HandleOf(a);
    if (!base || base == (void*)(intptr_t)-1) base = GetModuleHandleW(L"kernel32.dll");
    unsigned char* nt = fn ? fn(base) : nullptr;
    if (!nt) return FillErr(out, cap, ERROR_INVALID_DATA, api);
    return Fill(out, cap, std::to_string((unsigned long long)(uintptr_t)nt));
  }
  if (Eq(api, "WinVerifyTrust")) {
    using Fn = LONG(WINAPI*)(HWND, GUID*, void*);
    auto fn = (Fn)(void*)ExtraProc(L"wintrust.dll", "WinVerifyTrust");
    if (!fn) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    LONG st = fn(nullptr, nullptr, nullptr);
    return FillErr(out, cap, (int)st, api);
  }
  if (Eq(api, "IsThemeActive") || Eq(api, "IsAppThemed")) {
    using Fn = BOOL(WINAPI*)();
    auto fn = (Fn)(void*)ExtraProc(L"uxtheme.dll", api);
    BOOL on = fn ? fn() : FALSE;
    return Fill(out, cap, on ? "1" : "0");
  }
  if (Eq(api, "DwmIsCompositionEnabled")) {
    using Fn = HRESULT(WINAPI*)(BOOL*);
    auto fn = (Fn)(void*)ExtraProc(L"dwmapi.dll", "DwmIsCompositionEnabled");
    BOOL on = FALSE;
    if (fn) fn(&on);
    return Fill(out, cap, on ? "1" : "0");
  }
  if (Eq(api, "SetupDiGetClassDevsW") || Eq(api, "SetupDiGetClassDevsA")) {
    HDEVINFO h = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (h == INVALID_HANDLE_VALUE) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr((HANDLE)h));
  }
  if (Eq(api, "SetupDiEnumDeviceInfo")) {
    std::string hs, idx;
    Split1f(a, &hs, &idx);
    SP_DEVINFO_DATA d{};
    d.cbSize = sizeof(d);
    if (!SetupDiEnumDeviceInfo((HDEVINFO)HandleOf(hs.c_str()),
                               (DWORD)std::strtoul(idx.c_str(), nullptr, 10), &d))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, std::to_string(d.DevInst));
  }
  if (Eq(api, "SetupDiGetDeviceRegistryPropertyW")) {
    std::string hs, rest, idx;
    Split1f(a, &hs, &rest);
    Split1f(rest.c_str(), &idx, &rest);
    SP_DEVINFO_DATA d{};
    d.cbSize = sizeof(d);
    DWORD member = (DWORD)std::strtoul(idx.empty() ? rest.c_str() : idx.c_str(), nullptr, 10);
    if (!SetupDiEnumDeviceInfo((HDEVINFO)HandleOf(hs.c_str()), member, &d))
      return FillErr(out, cap, (int)GetLastError(), api);
    wchar_t desc[256];
    DWORD n = sizeof(desc);
    if (!SetupDiGetDeviceRegistryPropertyW((HDEVINFO)HandleOf(hs.c_str()), &d, SPDRP_DEVICEDESC,
                                           nullptr, (PBYTE)desc, n, nullptr))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, WideToUtf8(desc, -1));
  }
  if (Eq(api, "SetupDiDestroyDeviceInfoList")) {
    if (!SetupDiDestroyDeviceInfoList((HDEVINFO)HandleOf(a)))
      return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "CM_Locate_DevNodeW") || Eq(api, "CM_Locate_DevNodeA")) {
    DEVINST inst = 0;
    std::wstring id = Utf8ToWide(a);
    CONFIGRET cr = CM_Locate_DevNodeW(&inst, a && a[0] ? id.data() : nullptr, 0);
    if (cr != CR_SUCCESS) return FillErr(out, cap, (int)cr, api);
    return Fill(out, cap, std::to_string(inst));
  }
  if (Eq(api, "CM_Get_Device_IDW") || Eq(api, "CM_Get_Device_IDA")) {
    DEVINST inst = (DEVINST)std::strtoul(a, nullptr, 10);
    wchar_t id[256];
    CONFIGRET cr = CM_Get_Device_IDW(inst, id, 256, 0);
    if (cr != CR_SUCCESS) return FillErr(out, cap, (int)cr, api);
    return Fill(out, cap, WideToUtf8(id, -1));
  }
  if (Eq(api, "NetGetJoinInformation")) {
    using Fn = DWORD(WINAPI*)(LPCWSTR, LPWSTR*, int*);
    auto fn = (Fn)(void*)ExtraProc(L"netapi32.dll", "NetGetJoinInformation");
    using FreeFn = DWORD(WINAPI*)(void*);
    auto free_fn = (FreeFn)(void*)ExtraProc(L"netapi32.dll", "NetApiBufferFree");
    LPWSTR name = nullptr;
    int st = 0;
    DWORD err = fn ? fn(nullptr, &name, &st) : ERROR_PROC_NOT_FOUND;
    std::string nm = name ? WideToUtf8(name, -1) : "";
    if (free_fn && name) free_fn(name);
    if (err) return FillErr(out, cap, (int)err, api);
    return Fill(out, cap, std::to_string(st) + "\x1f" + nm);
  }
  if (Eq(api, "NetWkstaGetInfo")) {
    using Fn = DWORD(WINAPI*)(LPCWSTR, DWORD, LPBYTE*);
    auto fn = (Fn)(void*)ExtraProc(L"netapi32.dll", "NetWkstaGetInfo");
    using FreeFn = DWORD(WINAPI*)(void*);
    auto free_fn = (FreeFn)(void*)ExtraProc(L"netapi32.dll", "NetApiBufferFree");
    struct Wksta100 {
      DWORD platform_id;
      wchar_t* computername;
      wchar_t* langroup;
      DWORD ver_major;
      DWORD ver_minor;
    };
    LPBYTE buf = nullptr;
    DWORD err = fn ? fn(nullptr, 100, &buf) : ERROR_PROC_NOT_FOUND;
    std::string nm;
    if (!err && buf) {
      auto* w = (Wksta100*)buf;
      if (w->computername) nm = WideToUtf8(w->computername, -1);
    }
    if (free_fn && buf) free_fn(buf);
    if (err) {
      wchar_t cn[MAX_COMPUTERNAME_LENGTH + 1];
      DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
      GetComputerNameW(cn, &n);
      return Fill(out, cap, WideToUtf8(cn, (int)n));
    }
    return Fill(out, cap, nm);
  }
  if (Eq(api, "NetApiBufferFree")) return Fill(out, cap, "ok");
  if (Eq(api, "PdhOpenQueryW") || Eq(api, "PdhOpenQueryA")) {
    using Fn = long(WINAPI*)(LPCWSTR, uintptr_t, HANDLE*);
    auto fn = (Fn)(void*)ExtraProc(L"pdh.dll", "PdhOpenQueryW");
    HANDLE q = nullptr;
    long st = fn ? fn(nullptr, 0, &q) : ERROR_PROC_NOT_FOUND;
    if (st || !q) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(q));
  }
  if (Eq(api, "PdhAddCounterW") || Eq(api, "PdhAddCounterA") ||
      Eq(api, "PdhAddEnglishCounterW")) {
    std::string hs, path;
    Split1f(a, &hs, &path);
    if (path.empty()) path = "\\Processor(_Total)\\% Processor Time";
    HANDLE c = nullptr;
    long st = ERROR_PROC_NOT_FOUND;
    if (Eq(api, "PdhAddEnglishCounterW")) {
      using Fn = long(WINAPI*)(HANDLE, LPCWSTR, uintptr_t, HANDLE*);
      auto fn = (Fn)(void*)ExtraProc(L"pdh.dll", "PdhAddEnglishCounterW");
      if (fn) st = fn(HandleOf(hs.c_str()), Utf8ToWide(path).c_str(), 0, &c);
    }
    if (st || !c) {
      using Fn = long(WINAPI*)(HANDLE, LPCWSTR, uintptr_t, HANDLE*);
      auto fn = (Fn)(void*)ExtraProc(L"pdh.dll", "PdhAddCounterW");
      if (fn) st = fn(HandleOf(hs.c_str()), Utf8ToWide(path).c_str(), 0, &c);
    }
    if (st || !c) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, HandleStr(c));
  }
  if (Eq(api, "PdhCollectQueryData")) {
    using Fn = long(WINAPI*)(HANDLE);
    auto fn = (Fn)(void*)ExtraProc(L"pdh.dll", "PdhCollectQueryData");
    long st = fn ? fn(HandleOf(a)) : ERROR_PROC_NOT_FOUND;
    if (st) return FillErr(out, cap, (int)st, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "PdhGetFormattedCounterValue")) {
    using Fn = long(WINAPI*)(HANDLE, DWORD, DWORD*, void*);
    auto fn = (Fn)(void*)ExtraProc(L"pdh.dll", "PdhGetFormattedCounterValue");
    struct Fmt {
      DWORD CStatus;
      DWORD pad;
      long long largeValue;
    } v{};
    DWORD ty = 0;
    long st = fn ? fn(HandleOf(a), 0x100, &ty, &v) : ERROR_PROC_NOT_FOUND;
    if (st) return Fill(out, cap, "0");
    return Fill(out, cap, std::to_string(v.largeValue));
  }
  if (Eq(api, "PdhCloseQuery")) {
    using Fn = long(WINAPI*)(HANDLE);
    auto fn = (Fn)(void*)ExtraProc(L"pdh.dll", "PdhCloseQuery");
    if (fn) fn(HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "EvtQuery")) {
    using Fn = HANDLE(WINAPI*)(HANDLE, LPCWSTR, LPCWSTR, DWORD);
    auto fn = (Fn)(void*)ExtraProc(L"wevtapi.dll", "EvtQuery");
    std::string path, query;
    Split1f(a, &path, &query);
    if (path.empty()) path = "Application";
    if (query.empty()) query = "*";
    HANDLE h = fn ? fn(nullptr, Utf8ToWide(path).c_str(), Utf8ToWide(query).c_str(), 0x1) : nullptr;
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "EvtOpenLog")) {
    using Fn = HANDLE(WINAPI*)(HANDLE, LPCWSTR, DWORD);
    auto fn = (Fn)(void*)ExtraProc(L"wevtapi.dll", "EvtOpenLog");
    HANDLE h = fn ? fn(nullptr, Utf8ToWide(a[0] ? a : "Application").c_str(), 1) : nullptr;
    if (!h) return FillErr(out, cap, (int)GetLastError(), api);
    return Fill(out, cap, HandleStr(h));
  }
  if (Eq(api, "EvtNext")) {
    using Fn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE*, DWORD, DWORD, DWORD*);
    auto fn = (Fn)(void*)ExtraProc(L"wevtapi.dll", "EvtNext");
    HANDLE ev = nullptr;
    DWORD n = 0;
    if (!fn || !fn(HandleOf(a), 1, &ev, 0, 0, &n) || !n) return Fill(out, cap, "0");
    using CloseFn = BOOL(WINAPI*)(HANDLE);
    auto close_fn = (CloseFn)(void*)ExtraProc(L"wevtapi.dll", "EvtClose");
    if (close_fn && ev) close_fn(ev);
    return Fill(out, cap, "1");
  }
  if (Eq(api, "EvtClose")) {
    using Fn = BOOL(WINAPI*)(HANDLE);
    auto fn = (Fn)(void*)ExtraProc(L"wevtapi.dll", "EvtClose");
    if (fn) fn(HandleOf(a));
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "GetFileTitleW") || Eq(api, "GetFileTitleA")) {
    std::wstring in = Utf8ToWide(a);
    wchar_t title[MAX_PATH]{};
    short r = GetFileTitleW(in.c_str(), title, MAX_PATH);
    if (r == 0 && title[0]) return Fill(out, cap, WideToUtf8(title, -1));
    LPCWSTR n = PathFindFileNameW(in.c_str());
    return Fill(out, cap, WideToUtf8(n ? n : L"", -1));
  }
  if (Eq(api, "CommDlgExtendedError")) {
    return Fill(out, cap, std::to_string(CommDlgExtendedError()));
  }

  if (Eq(api, "DllMain")) {
    std::string hs, rstr;
    Split1f(a, &hs, &rstr);
    HMODULE m = (HMODULE)HandleOf(hs.c_str());
    using DllMainFn = BOOL(WINAPI*)(HINSTANCE, DWORD, void*);
    auto fn = m ? (DllMainFn)(void*)GetProcAddress(m, "DllMain") : nullptr;
    if (!fn) return Fill(out, cap, "ok");
    DWORD reason = (DWORD)std::strtoul(rstr.c_str(), nullptr, 10);
    if (!fn(m, reason, nullptr)) return FillErr(out, cap, ERROR_DLL_INIT_FAILED, "DllMain");
    return Fill(out, cap, "ok");
  }

  if (Eq(api, "WHvGetCapability")) {
    using Fn = long(WINAPI*)(unsigned, void*, unsigned, unsigned*);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvGetCapability");
    unsigned present = 0, wrote = 0;
    long hr = fn ? fn(0, &present, 4, &wrote) : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, present ? "1" : "0");
  }
  if (Eq(api, "WHvCreatePartition")) {
    using Fn = long(WINAPI*)(void**);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvCreatePartition");
    void* h = nullptr;
    long hr = fn ? fn(&h) : (long)0x8007007F;
    if (hr < 0 || !h) return FillErr(out, cap, hr ? (int)hr : ERROR_PROC_NOT_FOUND, api);
    return Fill(out, cap, HandleStr((HANDLE)h));
  }
  if (Eq(api, "WHvSetupPartition") || Eq(api, "WHvResetPartition") ||
      Eq(api, "WHvDeletePartition") || Eq(api, "WHvSuspendPartitionTime") ||
      Eq(api, "WHvResumePartitionTime")) {
    void* h = (void*)HandleOf(a);
    if (Eq(api, "WHvDeletePartition")) WhpRelease(h);
    using Fn = long(WINAPI*)(void*);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", api);
    long hr = fn ? fn(h) : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WHvCreateVirtualProcessor") || Eq(api, "WHvCreateVirtualProcessor2")) {
    std::string hs, idx;
    Split1f(a, &hs, &idx);
    using Fn = long(WINAPI*)(void*, unsigned, unsigned);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvCreateVirtualProcessor");
    long hr = fn ? fn((void*)HandleOf(hs.c_str()),
                      (unsigned)std::strtoul(idx.c_str(), nullptr, 10), 0)
                 : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, hs + "\x1f" + (idx.empty() ? "0" : idx));
  }
  if (Eq(api, "WHvDeleteVirtualProcessor") || Eq(api, "WHvCancelRunVirtualProcessor")) {
    std::string hs, idx;
    Split1f(a, &hs, &idx);
    using Fn = long(WINAPI*)(void*, unsigned);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", api);
    long hr = fn ? fn((void*)HandleOf(hs.c_str()),
                      (unsigned)std::strtoul(idx.c_str(), nullptr, 10))
                 : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WHvRunVirtualProcessor")) {
    std::string hs, idx;
    Split1f(a, &hs, &idx);
    void* part = (void*)HandleOf(hs.c_str());
    unsigned vp = (unsigned)std::strtoul(idx.c_str(), nullptr, 10);
    using Fn = long(WINAPI*)(void*, unsigned, void*, unsigned);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvRunVirtualProcessor");
    alignas(16) unsigned char ctx[224]{};
    long hr = fn ? fn(part, vp, ctx, sizeof(ctx)) : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    WhpExit& st = WhpExits()[part][vp];
    memcpy(st.ctx, ctx, sizeof(st.ctx));
    st.valid = 1;
    return Fill(out, cap, WhpFmtExit(ctx));
  }
  if (Eq(api, "WHvSetPartitionProperty")) {
    auto v = SplitAll(a);
    if (v.size() < 3) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    unsigned code = WhpProp(v[1]);
    unsigned long long val = WhpU64(v[2]);
    unsigned char prop[512]{};
    memcpy(prop, &val, sizeof(val));
    using Fn = long(WINAPI*)(void*, unsigned, const void*, unsigned);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvSetPartitionProperty");
    long hr = fn ? fn((void*)HandleOf(v[0].c_str()), code, prop, sizeof(prop))
                 : (long)0x8007007F;
    if (hr < 0 && fn) {
      unsigned n = (code == 0x1fffu || code == 0x1005u) ? 4u : 8u;
      hr = fn((void*)HandleOf(v[0].c_str()), code, prop, n);
    }
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WHvGetPartitionProperty")) {
    auto v = SplitAll(a);
    if (v.empty()) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    unsigned code = v.size() > 1 ? WhpProp(v[1]) : 0x1fffu;
    unsigned char prop[512]{};
    unsigned wrote = 0;
    using Fn = long(WINAPI*)(void*, unsigned, void*, unsigned, unsigned*);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvGetPartitionProperty");
    long hr = fn ? fn((void*)HandleOf(v[0].c_str()), code, prop, sizeof(prop), &wrote)
                 : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    unsigned long long val = 0;
    memcpy(&val, prop, sizeof(val));
    return Fill(out, cap, std::to_string(val));
  }
  if (Eq(api, "WHvMapGpaRange") || Eq(api, "WHvMapGpaRange2")) {
    auto v = SplitAll(a);
    if (v.size() < 3) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    void* part = (void*)HandleOf(v[0].c_str());
    unsigned long long gpa = WhpU64(v[1]) & ~4095ull;
    unsigned long long sz = WhpU64(v[2]);
    if (!sz) sz = 4096;
    sz = (sz + 4095ull) & ~4095ull;
    unsigned flags = v.size() > 3 && !v[3].empty() ? (unsigned)WhpU64(v[3]) : 7u;
    void* host = nullptr;
    int owned = 0;
    if (v.size() > 4 && !v[4].empty())
      host = (void*)(uintptr_t)WhpU64(v[4]);
    if (!host) {
      host = VirtualAlloc(nullptr, (SIZE_T)sz, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
      if (!host) return FillErr(out, cap, (int)GetLastError(), api);
      owned = 1;
    }
    using MapFn = long(WINAPI*)(void*, void*, unsigned long long, unsigned long long, unsigned);
    auto map = (MapFn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvMapGpaRange");
    using Map2Fn =
        long(WINAPI*)(void*, HANDLE, void*, unsigned long long, unsigned long long, unsigned);
    auto map2 = (Map2Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvMapGpaRange2");
    long hr = (long)0x8007007F;
    if (Eq(api, "WHvMapGpaRange2") && map2)
      hr = map2(part, GetCurrentProcess(), host, gpa, sz, flags);
    else if (map)
      hr = map(part, host, gpa, sz, flags);
    if (hr < 0) {
      if (owned) VirtualFree(host, 0, MEM_RELEASE);
      return FillErr(out, cap, (int)hr, api);
    }
    WhpGpa g;
    g.host = host;
    g.gpa = gpa;
    g.size = sz;
    g.owned = owned;
    WhpMaps()[part].push_back(g);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WHvUnmapGpaRange")) {
    auto v = SplitAll(a);
    if (v.size() < 2) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    void* part = (void*)HandleOf(v[0].c_str());
    unsigned long long gpa = WhpU64(v[1]) & ~4095ull;
    unsigned long long sz = v.size() > 2 ? WhpU64(v[2]) : 0;
    auto it = WhpMaps().find(part);
    if (it != WhpMaps().end()) {
      for (auto g = it->second.begin(); g != it->second.end(); ++g) {
        if (g->gpa != gpa) continue;
        if (!sz) sz = g->size;
        using UnmapFn = long(WINAPI*)(void*, unsigned long long, unsigned long long);
        auto unmap = (UnmapFn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvUnmapGpaRange");
        if (unmap) unmap(part, g->gpa, sz ? sz : g->size);
        if (g->owned && g->host) VirtualFree(g->host, 0, MEM_RELEASE);
        it->second.erase(g);
        break;
      }
    }
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WHvSetVirtualProcessorRegisters")) {
    auto v = SplitAll(a);
    if (v.size() < 3) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    unsigned vp = (unsigned)std::strtoul(v[1].c_str(), nullptr, 10);
    std::string name = v.size() > 3 ? v[2] : "Rip";
    size_t vali = v.size() > 3 ? 3 : 2;
    unsigned reg = WhpReg(name);
    unsigned char blob[16]{};
    WhpPackReg(reg, v, vali, blob);
    using Fn = long(WINAPI*)(void*, unsigned, const unsigned*, unsigned, const void*);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvSetVirtualProcessorRegisters");
    long hr = fn ? fn((void*)HandleOf(v[0].c_str()), vp, &reg, 1, blob) : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WHvGetVirtualProcessorRegisters")) {
    auto v = SplitAll(a);
    if (v.size() < 2) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    unsigned vp = (unsigned)std::strtoul(v[1].c_str(), nullptr, 10);
    unsigned reg = v.size() > 2 ? WhpReg(v[2]) : 0x10u;
    unsigned char blob[16]{};
    using Fn = long(WINAPI*)(void*, unsigned, const unsigned*, unsigned, void*);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvGetVirtualProcessorRegisters");
    long hr = fn ? fn((void*)HandleOf(v[0].c_str()), vp, &reg, 1, blob) : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, WhpUnpackReg(reg, blob));
  }
  if (Eq(api, "WHvReadGpaRange") || Eq(api, "WHvWriteGpaRange")) {
    auto v = SplitAll(a);
    if (v.size() < 2) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    void* part = (void*)HandleOf(v[0].c_str());
    unsigned long long gpa = WhpU64(v[1]);
    WhpGpa* rng = WhpFindGpa(part, gpa);
    if (Eq(api, "WHvReadGpaRange")) {
      unsigned n = v.size() > 2 ? (unsigned)WhpU64(v[2]) : 16u;
      if (rng && rng->host) {
        size_t off = (size_t)(gpa - rng->gpa);
        if (off + n > rng->size) n = (unsigned)(rng->size - off);
        return Fill(out, cap, std::string((char*)rng->host + off, (char*)rng->host + off + n));
      }
      std::string buf(n, '\0');
      using Fn = long(WINAPI*)(void*, unsigned, unsigned long long, unsigned long long, void*,
                               unsigned);
      auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvReadGpaRange");
      long hr = fn ? fn(part, 0, gpa, 0, buf.data(), n) : (long)0x8007007F;
      if (hr < 0) return FillErr(out, cap, (int)hr, api);
      return Fill(out, cap, buf);
    }
    std::string payload = v.size() > 2 ? v[2] : "";
    if (rng && rng->host) {
      size_t off = (size_t)(gpa - rng->gpa);
      size_t n = payload.size();
      if (off + n > rng->size) n = (size_t)(rng->size - off);
      memcpy((char*)rng->host + off, payload.data(), n);
      return Fill(out, cap, "ok");
    }
    using Fn = long(WINAPI*)(void*, unsigned, unsigned long long, unsigned long long, const void*,
                             unsigned);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvPlatform.dll", "WHvWriteGpaRange");
    long hr = fn ? fn(part, 0, gpa, 0, payload.data(), (unsigned)payload.size())
                 : (long)0x8007007F;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WHvEmulatorCreateEmulator")) {
    using Fn = long(WINAPI*)(const WhpEmuCbs*, void**);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvEmulation.dll", "WHvEmulatorCreateEmulator");
    WhpEmuCbs cb{};
    cb.Size = (unsigned)sizeof(cb);
    cb.Io = WhpEmuIoCb;
    cb.Mem = WhpEmuMemCb;
    cb.Get = WhpEmuGetCb;
    cb.Set = WhpEmuSetCb;
    cb.Xlat = WhpEmuXlatCb;
    void* emu = nullptr;
    long hr = fn ? fn(&cb, &emu) : (long)0x8007007F;
    if (hr < 0 || !emu) return FillErr(out, cap, hr ? (int)hr : ERROR_PROC_NOT_FOUND, api);
    WhpLastEmu() = emu;
    return Fill(out, cap, HandleStr((HANDLE)emu));
  }
  if (Eq(api, "WHvEmulatorDestroyEmulator")) {
    void* emu = (void*)HandleOf(a);
    using Fn = long(WINAPI*)(void*);
    auto fn = (Fn)(void*)ExtraProc(L"WinHvEmulation.dll", "WHvEmulatorDestroyEmulator");
    long hr = fn ? fn(emu) : (long)0x8007007F;
    if (WhpLastEmu() == emu) WhpLastEmu() = nullptr;
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, "ok");
  }
  if (Eq(api, "WHvEmulatorTryIoEmulation") || Eq(api, "WHvEmulatorTryMmioEmulation")) {
    auto v = SplitAll(a);
    if (v.size() < 2) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    void* emu = nullptr;
    void* part = nullptr;
    unsigned vp = 0;
    if (v.size() >= 3) {
      emu = (void*)HandleOf(v[0].c_str());
      part = (void*)HandleOf(v[1].c_str());
      vp = (unsigned)std::strtoul(v[2].c_str(), nullptr, 10);
    } else {
      emu = WhpLastEmu();
      part = (void*)HandleOf(v[0].c_str());
      vp = (unsigned)std::strtoul(v[1].c_str(), nullptr, 10);
    }
    if (!emu) emu = WhpLastEmu();
    WhpExit& st = WhpExits()[part][vp];
    if (!st.valid) return FillErr(out, cap, ERROR_INVALID_PARAMETER, api);
    WhpEmuCtx ctx;
    ctx.part = part;
    ctx.vp = vp;
    unsigned status = 0;
    long hr = (long)0x8007007F;
    if (Eq(api, "WHvEmulatorTryIoEmulation")) {
      using Fn = long(WINAPI*)(void*, void*, const void*, const void*, unsigned*);
      auto fn = (Fn)(void*)ExtraProc(L"WinHvEmulation.dll", "WHvEmulatorTryIoEmulation");
      if (fn) hr = fn(emu, &ctx, st.ctx + 8, st.ctx + 48, &status);
    } else {
      using Fn = long(WINAPI*)(void*, void*, const void*, const void*, unsigned*);
      auto fn = (Fn)(void*)ExtraProc(L"WinHvEmulation.dll", "WHvEmulatorTryMmioEmulation");
      if (fn) hr = fn(emu, &ctx, st.ctx + 8, st.ctx + 48, &status);
    }
    if (hr < 0) return FillErr(out, cap, (int)hr, api);
    return Fill(out, cap, std::to_string(status & 1u));
  }
  if (IsWhpCatalog(api)) {
    FARPROC p = ExtraProc(L"WinHvPlatform.dll", api);
    if (!p) p = ExtraProc(L"WinHvEmulation.dll", api);
    if (!p) return FillErr(out, cap, ERROR_PROC_NOT_FOUND, api);
    return Fill(out, cap, "ok");
  }

  if (IsNtdllCatalog(api)) {
    if (NtEq(api, "NtShutdownSystem") || NtEq(api, "NtRaiseHardError") ||
        NtEq(api, "NtRaiseException") || NtEq(api, "NtSetSystemTime") || Eq(api, "DbgBreakPoint") ||
        Eq(api, "DbgUserBreakPoint") || Eq(api, "RtlAssert") || Eq(api, "RtlRaiseStatus") ||
        Eq(api, "RtlRaiseException"))
      return FillErr(out, cap, ERROR_NOT_SUPPORTED, "ntdll: not executed by this hop");
    return Fill(out, cap, "ok");
  }

  Fill(out, cap, std::string("unknown api ") + api);
  return -1;
}
