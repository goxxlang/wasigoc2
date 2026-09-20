#ifndef WASMGOCOS_INCLUDE_GOCOS_GOCSYS_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_GOCSYS_HPP_

// GocSys — edge subsystem (loader, window, draw, registry, path).
// Every call is hv.k32. Not Wine: same job, own name, own catalog.

inline std::string gocsys_load(const char* a) {
  return hv::k32("LoadLibraryW", a ? a : "");
}

inline std::string gocsys_getproc(const char* a) {
  return hv::k32("GetProcAddress", a ? a : "");
}

inline std::string gocsys_free_mod(const char* a) {
  return hv::k32("FreeLibrary", a ? a : "");
}

inline std::string gocsys_get_mod(const char* a) {
  return hv::k32("GetModuleHandleW", a ? a : "");
}

inline std::string gocsys_version() { return gockrnl_version(); }

inline std::string gocsys_build_id() { return "gocsys"; }

inline std::string gocsys_host_version() {
  if (hv::has_nix()) return hv::nix("uname", "");
  return hv::k32("RtlGetVersion", "");
}

inline std::string gocsys_wnd(const char* api, const char* a) {
  if (eq(api, "CreateWindow")) return hv::k32("CreateWindowExW", a ? a : "");
  if (eq(api, "DestroyWindow")) return hv::k32("DestroyWindow", a ? a : "");
  if (eq(api, "GetMessage")) return hv::k32("GetMessageW", a ? a : "");
  if (eq(api, "PeekMessage")) return hv::k32("PeekMessageW", a ? a : "");
  if (eq(api, "Dispatch")) return hv::k32("DispatchMessageW", a ? a : "");
  if (eq(api, "Translate")) return hv::k32("TranslateMessage", a ? a : "");
  if (eq(api, "DefProc")) return hv::k32("DefWindowProcW", a ? a : "");
  if (eq(api, "RegisterClass")) return hv::k32("RegisterClassW", a ? a : "");
  if (eq(api, "ShowWindow")) return hv::k32("ShowWindow", a ? a : "");
  if (eq(api, "UpdateWindow")) return hv::k32("UpdateWindow", a ? a : "");
  if (eq(api, "PostQuit")) return hv::k32("PostQuitMessage", a ? a : "");
  if (eq(api, "Message")) return hv::k32("MessageBoxW", a ? a : "");
  return hv::k32(api, a ? a : "");
}

inline std::string gocsys_draw(const char* api, const char* a) {
  if (eq(api, "GetDC")) return hv::k32("GetDC", a ? a : "");
  if (eq(api, "ReleaseDC")) return hv::k32("ReleaseDC", a ? a : "");
  if (eq(api, "BitBlt")) return hv::k32("BitBlt", a ? a : "");
  if (eq(api, "CreateBrush")) return hv::k32("CreateSolidBrush", a ? a : "");
  if (eq(api, "DeleteObject")) return hv::k32("DeleteObject", a ? a : "");
  if (eq(api, "CreateCompatibleDC"))
    return hv::k32("CreateCompatibleDC", a ? a : "");
  if (eq(api, "SelectObject")) return hv::k32("SelectObject", a ? a : "");
  return hv::k32(api, a ? a : "");
}

inline std::string gocsys_reg(const char* api, const char* a) {
  if (eq(api, "OpenKey")) return hv::k32("RegOpenKeyExW", a ? a : "");
  if (eq(api, "QueryValue")) return hv::k32("RegQueryValueExW", a ? a : "");
  if (eq(api, "CloseKey")) return hv::k32("RegCloseKey", a ? a : "");
  return hv::k32(api, a ? a : "");
}

inline std::string gocsys_shell(const char* api, const char* a) {
  if (eq(api, "FolderPath")) return hv::k32("SHGetFolderPathW", a ? a : "");
  if (eq(api, "ShellExec")) return hv::k32("ShellExecuteW", a ? a : "");
  return hv::k32(api, a ? a : "");
}

inline std::string gocsys_nt_to_unix(const char* a) {
  if (hv::has_nix()) {
    std::string p = hv::nix("WslPath", a ? a : "");
    if (p.rfind("error:", 0) != 0 && !p.empty()) return p;
  }
  if (!a || !a[0]) {
    std::string d = hv::k32("GetCurrentDirectoryW", "");
    if (d.rfind("error:", 0) != 0 && !d.empty()) return d;
    return gockrnl().cwd[0] ? gockrnl().cwd : "C:\\";
  }
  if (a[0] && a[1] == ':' && (a[2] == '\\' || a[2] == '/')) {
    char drive = a[0];
    if (drive >= 'A' && drive <= 'Z') drive = static_cast<char>(drive + ('a' - 'A'));
    std::string s = "/mnt/";
    s += drive;
    for (const char* p = a + 2; *p; ++p) s += (*p == '\\') ? '/' : *p;
    return s;
  }
  return a;
}

inline std::string gocsys_unix_to_nt(const char* a) {
  if (!a || !a[0]) return "C:\\";
  if (std::strncmp(a, "/mnt/", 5) == 0 && a[5] && a[6] == '/') {
    char d = a[5];
    if (d >= 'a' && d <= 'z') d = static_cast<char>(d - ('a' - 'A'));
    std::string s;
    s += d;
    s += ':';
    for (const char* p = a + 6; *p; ++p) s += (*p == '/') ? '\\' : *p;
    return s;
  }
  if (a[0] == '/') {
    std::string s = "Z:";
    for (const char* p = a; *p; ++p) s += (*p == '/') ? '\\' : *p;
    return s;
  }
  return a;
}

inline std::string gocsys_init() {
  GocKrnl& k = gockrnl();
  std::string ntdll = gocsys_load("ntdll");
  if (ntdll.rfind("error:", 0) == 0) return ntdll;
  copy_field(k.ntdll, sizeof(k.ntdll), ntdll);
  std::string k32mod = gocsys_load("kernel32");
  if (k32mod.rfind("error:", 0) == 0) return k32mod;
  copy_field(k.k32mod, sizeof(k.k32mod), k32mod);
  std::string user32 = gocsys_load("user32");
  if (user32.rfind("error:", 0) == 0) return user32;
  copy_field(k.user32, sizeof(k.user32), user32);
  std::string gdi32 = gocsys_load("gdi32");
  if (gdi32.rfind("error:", 0) == 0) return gdi32;
  copy_field(k.gdi32, sizeof(k.gdi32), gdi32);
  return "sys=gocsys";
}

#endif  // WASMGOCOS_INCLUDE_GOCOS_GOCSYS_HPP_
