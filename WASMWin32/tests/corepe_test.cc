#include <cstdlib>
#include <iostream>
#include <string>

#include "win32/wasi_host.hpp"

static int g_notes = 0;
static std::string g_last;
static void Note(const char* image) {
  ++g_notes;
  g_last = image ? image : "";
}

static std::string Call(const char* api, const std::string& args) {
  return wasmwin32::wasi_call(api, args.c_str());
}

int main() {
  k32_set_ps_register(Note);
  std::string nd = Call("LoadLibraryW", "ntdll.dll");
  if (nd.rfind("error:", 0) == 0) {
    std::cerr << nd << "\n";
    return 1;
  }
  std::string rtl = Call("GetProcAddress", nd + "\x1f" + "RtlGetVersion");
  if (rtl.rfind("error:", 0) == 0) {
    std::cerr << rtl << "\n";
    return 1;
  }
  unsigned long long addr = std::strtoull(rtl.c_str(), nullptr, 10);
  if (addr <= 0x100000ull) {
    std::cerr << "ntdll RtlGetVersion not a mapped export: " << rtl << "\n";
    return 1;
  }
  std::string missing = Call("GetProcAddress", nd + "\x1f" + "ThisExportDoesNotExist");
  if (missing.rfind("error:", 0) != 0) {
    std::cerr << "unexpected export " << missing << "\n";
    return 1;
  }
  std::string k = Call("LoadLibraryW", "kernel32.dll");
  if (k.rfind("error:", 0) == 0) {
    std::cerr << k << "\n";
    return 1;
  }
  std::string ll = Call("GetProcAddress", k + "\x1f" + "LoadLibraryW");
  if (ll.rfind("error:", 0) == 0) {
    std::cerr << ll << "\n";
    return 1;
  }
  std::string hv = Call("LoadLibraryW", "WinHvPlatform.dll");
  if (hv.rfind("error:", 0) == 0) {
    std::cerr << hv << "\n";
    return 1;
  }
  if (g_notes != 0) {
    std::cerr << "direct image registered: " << g_last << "\n";
    return 1;
  }
  std::string u = Call("LoadLibraryW", "user32.dll");
  if (u.rfind("error:", 0) == 0) {
    std::cerr << u << "\n";
    return 1;
  }
  if (g_notes != 1 || g_last != "user32.dll") {
    std::cerr << "process manager notes=" << g_notes << " last=" << g_last << "\n";
    return 1;
  }
  std::cout << "core pe ok ntdll=" << nd << " kernel32=" << k << " hv=" << hv << "\n";
  return 0;
}
