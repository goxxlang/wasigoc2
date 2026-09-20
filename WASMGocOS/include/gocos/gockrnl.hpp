#ifndef WASMGOCOS_INCLUDE_GOCOS_GOCKRNL_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_GOCKRNL_HPP_

// GocKrnl — edge kernel. Session, objects, process, thread, file, vm.
// Every syscall is hv.k32. Pages are goc_alloc (gocvm vmem).

struct GocKrnl {
  bool booted = false;
  int shell = 0;
  int errorlevel = 0;
  char pid[64]{};
  char version[128]{};
  char session[64]{};
  char station[64]{};
  char desk[128]{};
  char nix[256]{};
  char hvname[32]{};
  char ntdll[64]{};
  char k32mod[64]{};
  char user32[64]{};
  char gdi32[64]{};
  char cwd[512]{};
  char title[64]{};
};

inline GocKrnl& gockrnl() {
  static GocKrnl k;
  return k;
}

inline std::string gockrnl_status() {
  const GocKrnl& k = gockrnl();
  std::string s = "krnl=";
  s += k.pid;
  s += "\x1f";
  s += "session=";
  s += k.session;
  s += "\x1f";
  s += "station=";
  s += k.station;
  s += "\x1f";
  s += "sys=";
  s += k.version;
  s += "\x1f";
  s += "mod.ntdll=";
  s += k.ntdll;
  s += "\x1f";
  s += "mod.k32=";
  s += k.k32mod;
  s += "\x1f";
  s += "desk=";
  s += k.desk;
  s += "\x1f";
  s += "gpu=gocdesk";
  s += "\x1f";
  s += "nix=";
  s += k.nix;
  s += "\x1f";
  s += "os=gocos";
  s += "\x1f";
  s += "hv=";
  s += k.hvname;
  return s;
}

inline std::string gockrnl_version() {
  std::string v = hv::k32("RtlGetVersion", "");
  if (v.rfind("error:", 0) != 0 && !v.empty()) return v;
  if (gockrnl().version[0]) return gockrnl().version;
  return v;
}

inline std::string gockrnl_close(const char* a) {
  return hv::k32("CloseHandle", a ? a : "");
}

inline std::string gockrnl_create_process(const char* image, const char* cmdline,
                                         bool wait) {
  std::string args = image ? image : "";
  args += '\x1f';
  args += (cmdline && cmdline[0]) ? cmdline : (image ? image : "");
  std::string r = hv::k32("CreateProcessW", args.c_str());
  if (r.rfind("error:", 0) == 0) return r;
  std::string pid, rest, h;
  split1f(r.c_str(), &pid, &rest);
  split1f(rest.c_str(), &h, &rest);
  if (h.empty()) h = pid;
  if (!wait) return pid;
  hv::k32("WaitForSingleObject", join1f(h.c_str(), "4294967295").c_str());
  std::string out = hv::k32("GetProcessOutput", h.c_str());
  std::string code = hv::k32("GetExitCodeProcess", h.c_str());
  hv::k32("CloseHandle", h.c_str());
  if (code.rfind("error:", 0) != 0)
    gockrnl().errorlevel = static_cast<int>(std::strtol(code.c_str(), nullptr, 10));
  return out;
}

inline std::string gockrnl_open_process(const char* a) {
  return hv::k32("OpenProcess", a ? a : "");
}

inline std::string gockrnl_terminate(const char* a) {
  return hv::k32("TerminateProcess", a ? a : "");
}

inline std::string gockrnl_create_thread(const char* a) {
  return hv::k32("CreateThread", a ? a : "");
}

inline std::string gockrnl_file(const char* api, const char* a) {
  if (eq(api, "CreateFile") || eq(api, "OpenFile"))
    return hv::k32("CreateFileW", a ? a : "");
  if (eq(api, "ReadFile")) return hv::k32("ReadFile", a ? a : "");
  if (eq(api, "WriteFile")) return hv::k32("WriteFile", a ? a : "");
  return hv::k32(api, a ? a : "");
}

inline std::string gockrnl_wait(const char* a) {
  return hv::k32("WaitForSingleObject", a ? a : "");
}

inline std::string gockrnl_delay(const char* a) {
  return hv::k32("Sleep", a ? a : "0");
}

inline std::string gockrnl_yield() { return hv::k32("SwitchToThread", ""); }

inline std::string gockrnl_event(const char* api, const char* a) {
  if (eq(api, "CreateEvent")) return hv::k32("CreateEventW", a ? a : "");
  if (eq(api, "SetEvent")) return hv::k32("SetEvent", a ? a : "");
  if (eq(api, "ResetEvent")) return hv::k32("ResetEvent", a ? a : "");
  if (eq(api, "Duplicate")) return hv::k32("DuplicateHandle", a ? a : "");
  if (eq(api, "QueryObject")) return hv::k32("NtQueryObject", a ? a : "");
  return hv::k32(api, a ? a : "");
}

inline std::string gockrnl_pool(const char* api, const char* a) {
  if (eq(api, "PoolAlloc") || eq(api, "ExAllocatePool")) return goc_alloc(a);
  if (eq(api, "PoolFree") || eq(api, "ExFreePool")) return goc_free(a);
  if (eq(api, "BugCheck") || eq(api, "KeBugCheckEx"))
    return err_msg("GocKrnl.BugCheck");
  if (eq(api, "CreateDevice") || eq(api, "IoCreateDevice"))
    return hv::k32("CreateFileW", a ? a : "");
  if (eq(api, "SystemThread") || eq(api, "PsCreateSystemThread"))
    return hv::k32("CreateThread", a ? a : "");
  return err_msg("unknown api");
}

#endif  // WASMGOCOS_INCLUDE_GOCOS_GOCKRNL_HPP_
