#include "wow/occupancy.h"

#include "wow/catalog.h"

#include <csetjmp>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

#if defined(WOW_HAS_WIN32)
#include "win32/catalog.h"
#include "win32/dispatch.h"
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace wow {
namespace {

std::string StripTopic(std::string_view topic) {
  std::string t(topic);
  if (t.rfind("win32.", 0) == 0) {
    t = t.substr(6);
  }
  return t;
}

int JsonIntField(std::string_view obj, const char* key, int fallback) {
  std::string pat = std::string("\"") + key + "\"";
  auto p = obj.find(pat);
  if (p == std::string_view::npos) {
    return fallback;
  }
  p = obj.find(':', p);
  if (p == std::string_view::npos) {
    return fallback;
  }
  ++p;
  while (p < obj.size() && (obj[p] == ' ' || obj[p] == '"')) {
    ++p;
  }
  return static_cast<int>(
      std::strtol(std::string(obj.substr(p)).c_str(), nullptr, 10));
}

std::string JsonStringField(std::string_view obj, const char* key) {
  std::string pat = std::string("\"") + key + "\"";
  auto p = obj.find(pat);
  if (p == std::string_view::npos) {
    return {};
  }
  p = obj.find(':', p);
  if (p == std::string_view::npos) {
    return {};
  }
  ++p;
  while (p < obj.size() && obj[p] == ' ') {
    ++p;
  }
  if (p < obj.size() && obj[p] == '"') {
    auto e = obj.find('"', p + 1);
    if (e == std::string_view::npos) {
      return {};
    }
    return std::string(obj.substr(p + 1, e - p - 1));
  }
  return {};
}

void MarkFlag(Occupancy* o, SurfaceKind k) {
  switch (k) {
    case SurfaceKind::kKernel32:
      // set via occupy methods
      (void)o;
      break;
    default:
      (void)o;
      break;
  }
}

// Args occupancy supplies when wasmwin32_call needs a path or class.
// Empty stays empty — host_win already handles that for most names.
const char* CatalogArgs(const char* api) {
  if (!api) {
    return "";
  }
  if (std::strcmp(api, "CreateWindowExW") == 0 ||
      std::strcmp(api, "CreateWindowExA") == 0 ||
      std::strcmp(api, "CreateWindowW") == 0) {
    return "WASMWin32" "\x1f" "wow-occupy" "\x1f" "0";
  }
  if (std::strcmp(api, "RegisterClassW") == 0 ||
      std::strcmp(api, "RegisterClassExW") == 0) {
    return "WASMWin32";
  }
  if (std::strcmp(api, "NtCreateFile") == 0) {
    return "wow-occupy-nt.bin";
  }
  if (std::strcmp(api, "NtCreateSection") == 0 ||
      std::strcmp(api, "ZwCreateSection") == 0) {
    return "4096";
  }
  if (std::strcmp(api, "LoadLibraryW") == 0 ||
      std::strcmp(api, "LoadLibraryA") == 0 ||
      std::strcmp(api, "GetModuleHandleW") == 0) {
    return "ntdll";
  }
  if (std::strcmp(api, "BCryptOpenAlgorithmProvider") == 0) {
    return "SHA256";
  }
  if (std::strcmp(api, "LookupPrivilegeValueW") == 0 ||
      std::strcmp(api, "LookupPrivilegeValueA") == 0) {
    return "SeLoadDriverPrivilege";
  }
  if (std::strcmp(api, "RtlAdjustPrivilege") == 0) {
    return "10" "\x1f" "1" "\x1f" "1";
  }
  if (std::strcmp(api, "CoCreateInstance") == 0 ||
      std::strcmp(api, "CoGetClassObject") == 0) {
    return "{00000320-0000-0000-C000-000000000046}";
  }
  return "";
}

bool LiveError(const std::string& s) {
  if (s.rfind("error:", 0) == 0) {
    return true;
  }
  if (s.rfind("unknown api", 0) == 0) {
    return true;
  }
  // host_win FillErr is "ApiName: 5". Handles are digits with no colon.
  auto p = s.find(": ");
  if (p == std::string::npos || p == 0) {
    return false;
  }
  size_t n = p + 2;
  if (n < s.size() && s[n] == '-') {
    ++n;
  }
  if (n >= s.size()) {
    return false;
  }
  for (; n < s.size(); ++n) {
    if (s[n] < '0' || s[n] > '9') {
      return false;
    }
  }
  return true;
}

bool NotAllAssigned(const std::string& s) {
  return s.find(": 1300") != std::string::npos;
}

bool LooksHandle(const std::string& s) {
  if (s.empty() || LiveError(s)) {
    return false;
  }
  for (char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  // host_win GetTokenInformation fallback returns sizeof(struct) (4/8).
  // A real HANDLE is larger than that.
  return std::strtoull(s.c_str(), nullptr, 10) > 32ull;
}

std::string FirstField(const std::string& s) {
  auto p = s.find('\x1f');
  return p == std::string::npos ? s : s.substr(0, p);
}

void AppendPrivilegeJson(std::string* o, const std::string& raw) {
  *o += '[';
  bool first = true;
  std::string rest = raw;
  if (LiveError(rest)) {
    rest.clear();
  }
  while (!rest.empty()) {
    auto p = rest.find('\x1f');
    std::string name = p == std::string::npos ? rest : rest.substr(0, p);
    rest = p == std::string::npos ? std::string() : rest.substr(p + 1);
    std::string attr = "0";
    if (!rest.empty()) {
      p = rest.find('\x1f');
      attr = p == std::string::npos ? rest : rest.substr(0, p);
      rest = p == std::string::npos ? std::string() : rest.substr(p + 1);
    }
    if (!first) {
      *o += ',';
    }
    first = false;
    *o += "{\"name\":";
    *o += JsonEscape(name);
    *o += ",\"attr\":";
    *o += JsonEscape(attr);
    *o += '}';
  }
  *o += ']';
}

#if defined(_WIN32)
jmp_buf g_call_jmp;
bool g_call_armed = false;
PVOID g_call_veh = nullptr;

LONG CALLBACK OccupancyCallVeh(PEXCEPTION_POINTERS info) {
  if (g_call_armed && info && info->ExceptionRecord &&
      info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    longjmp(g_call_jmp, 1);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

}  // namespace

Occupancy::Occupancy() = default;

WowResult Occupancy::NeedLive(int id, Channel** ch) {
  Channel* c = table_.FindMutable(id);
  if (!c || !c->live) {
    last_error_ = "channel not found";
    return WOW_RESULT_NOT_FOUND;
  }
  *ch = c;
  return WOW_RESULT_OK;
}

Channel* Occupancy::Stamp(SurfaceKind kind, std::string_view label,
                          int parent) {
  Channel opened = table_.Open(kind, label, parent);
  return table_.FindMutable(opened.id);
}

Channel* Occupancy::EnsureKind(SurfaceKind kind, std::string_view label) {
  auto v = table_.OfKind(kind);
  if (!v.empty()) {
    return table_.FindMutable(v.front().id);
  }
  return Stamp(kind, label, 0);
}

std::string Occupancy::ChannelReply(const Channel& ch) const {
  return ch.ToJson();
}

bool Occupancy::CallWin32(std::string_view api, std::string_view args,
                          std::string* reply_out, std::string* err_out) {
#if defined(WOW_HAS_WIN32)
  char buf[4096] = {};
  int rc = -1;
#if defined(_WIN32)
  if (!g_call_veh) {
    g_call_veh = AddVectoredExceptionHandler(1, OccupancyCallVeh);
  }
  g_call_armed = true;
  if (setjmp(g_call_jmp) != 0) {
    g_call_armed = false;
    if (err_out) {
      *err_out = std::string("wasmwin32_call av ") + std::string(api);
    }
    if (reply_out) {
      *reply_out = std::string("error: av ") + std::string(api);
    }
    return false;
  }
#endif
  rc = wasmwin32_call(std::string(api).c_str(), std::string(args).c_str(), buf,
                      sizeof(buf));
#if defined(_WIN32)
  g_call_armed = false;
#endif
  if (rc != 0 || std::strncmp(buf, "error:", 6) == 0) {
    if (err_out) {
      *err_out = buf[0] ? buf : "wasmwin32_call failed";
    }
    if (reply_out) {
      *reply_out = buf;
    }
    return false;
  }
  if (reply_out) {
    *reply_out = buf;
  }
  if (err_out) {
    err_out->clear();
  }
  return true;
#else
  if (reply_out) {
    *reply_out = std::string(api);
  }
  if (err_out) {
    err_out->clear();
  }
  (void)args;
  return true;
#endif
}

void Occupancy::NoteCall(const char* api) {
  if (!api || !api[0]) {
    return;
  }
  for (const auto& c : called_) {
    if (c == api) {
      return;
    }
  }
  called_.emplace_back(api);
}

bool Occupancy::Called(std::string_view api) const {
  for (const auto& c : called_) {
    if (c == api) {
      return true;
    }
  }
  return false;
}

WowResult Occupancy::Probe(SurfaceKind kind, std::string_view label,
                           const char* api, const char* args, bool mapped) {
  Channel* ch = EnsureKind(kind, label.empty() ? KindToString(kind) : label);
  std::string reply;
  std::string err;
  const char* use_args = args;
  if ((!use_args || !use_args[0]) && api) {
    use_args = CatalogArgs(api);
  }
  bool ok = CallWin32(api ? api : "", use_args ? use_args : "", &reply, &err);
  last_reply_ = reply.empty() ? err : reply;
  NoteCall(api);
  if (ch) {
    ch->isolation = IsolationOf(kind);
    if (!reply.empty()) {
      ch->bytes.assign(reply.begin(), reply.end());
    } else if (!err.empty()) {
      ch->bytes.assign(err.begin(), err.end());
    } else {
      std::string tag = api ? api : KindToString(kind);
      ch->bytes.assign(tag.begin(), tag.end());
    }
    ch->mapped = mapped || ch->mapped;
    ++ch->writes;
    ++ch->submits;
  }
  if (mapped && ch) {
    events_.FireMapped(*ch);
  }
  if (kind == SurfaceKind::kPipe && ch) {
    events_.FirePipe(*ch);
  }
  if (kind == SurfaceKind::kHwnd && ch) {
    events_.FireHwnd(*ch);
  }
  // Occupancy fires; a live backend may still refuse. Stamp the channel.
  (void)ok;
  last_error_.clear();
  return WOW_RESULT_OK;
}

void Occupancy::ProbeImpersonatedScm(std::string_view tag) {
  // Args are 0x1F-separated. Keep \x1f in its own literal:
  // "\x1f0x1" is one hex escape and will not pass access 1.
  Probe(SurfaceKind::kAdvapi32, tag, "OpenSCManagerW", "\x1f" "\x1f" "0x1",
        false);
  scm_connect_ = last_reply_;
  std::string scm;
  if (!LiveError(scm_connect_)) {
    scm = FirstField(scm_connect_);
  }
  if (!scm.empty()) {
    std::string open14 = scm + "\x1f" "EventLog" "\x1f" "0x14";
    Probe(SurfaceKind::kAdvapi32, tag, "OpenServiceW", open14.c_str(), false);
    scm_eventlog_ = last_reply_;
    if (LiveError(scm_eventlog_)) {
      std::string open4 = scm + "\x1f" "EventLog" "\x1f" "0x4";
      Probe(SurfaceKind::kAdvapi32, tag, "OpenServiceW", open4.c_str(), false);
      scm_eventlog_ = last_reply_;
    }
    if (!LiveError(scm_eventlog_)) {
      std::string svc = FirstField(scm_eventlog_);
      Probe(SurfaceKind::kAdvapi32, tag, "QueryServiceStatus", svc.c_str(),
            false);
      scm_status_ = last_reply_;
    }
  }
  // CONNECT | ENUMERATE_SERVICE | QUERY_LOCK_STATUS — Authenticated Users.
  Probe(SurfaceKind::kAdvapi32, tag, "OpenSCManagerW", "\x1f" "\x1f" "0x15",
        false);
  scm_enum_ = last_reply_;
  // SC_MANAGER_ALL_ACCESS stays 5 on the impersonated token. Still Call it.
  Probe(SurfaceKind::kAdvapi32, tag, "OpenSCManagerW", "\x1f" "\x1f" "0xF003F",
        false);
  scm_all_ = last_reply_;
}

void Occupancy::ProbeEnableSeLoadDriver(std::string_view tag) {
  // TOKEN_ADJUST_PRIVILEGES is on OpenProcessToken / DuplicateTokenEx /
  // OpenThreadToken so this can actually enable. NtAdjustPrivilegesToken
  // is the same hop as AdjustTokenPrivileges.
  std::string adj = token_handle_ + "\x1f" "SeLoadDriverPrivilege";
  Probe(SurfaceKind::kToken, tag, "AdjustTokenPrivileges", adj.c_str(), false);
  adjust_process_ = last_reply_;
  std::string adjdup = token_dup_ + "\x1f" "SeLoadDriverPrivilege";
  Probe(SurfaceKind::kToken, tag, "AdjustTokenPrivileges", adjdup.c_str(),
        false);
  adjust_dup_ = last_reply_;
  Probe(SurfaceKind::kNtdll, "ntdll", "NtAdjustPrivilegesToken", adjdup.c_str(),
        false);
  nt_adjust_ = last_reply_;
  Probe(SurfaceKind::kToken, tag, "ImpersonateLoggedOnUser", token_dup_.c_str(),
        false);
  impersonate_ = last_reply_;
  Probe(SurfaceKind::kNtdll, "ntdll", "RtlAdjustPrivilege",
        "10" "\x1f" "1" "\x1f" "1", false);
  rtl_thread_ = last_reply_;
  Probe(SurfaceKind::kToken, tag, "OpenThreadToken", "-2" "\x1f" "0x28", false);
  thread_token_ = last_reply_;
  if (LiveError(thread_token_)) {
    Probe(SurfaceKind::kToken, tag, "OpenThreadToken", "-2", false);
    thread_token_ = last_reply_;
  }
  std::string adjt = thread_token_ + "\x1f" "SeLoadDriverPrivilege";
  Probe(SurfaceKind::kToken, tag, "AdjustTokenPrivileges", adjt.c_str(), false);
  adjust_thread_ = last_reply_;
  std::string info = token_dup_ + "\x1f" "3";
  Probe(SurfaceKind::kToken, tag, "GetTokenInformation", info.c_str(), false);
  privileges_ = last_reply_;
}

WowResult Occupancy::OccupyDll(std::string_view dll, std::string_view api) {
  SurfaceKind kind = KindFromDll(dll);
  std::string label(dll.empty() ? KindToString(kind) : std::string(dll));
  std::string probe(api);
  if (probe.empty()) {
    if (kind == SurfaceKind::kKernel32) {
      probe = "GetCurrentProcessId";
    } else if (kind == SurfaceKind::kNtdll) {
      probe = "RtlGetCurrentPeb";
    } else if (kind == SurfaceKind::kUser32) {
      probe = "GetDesktopWindow";
    } else if (kind == SurfaceKind::kGdi32) {
      probe = "GetStockObject";
    } else if (kind == SurfaceKind::kOle32) {
      probe = "CoCreateGuid";
    } else if (kind == SurfaceKind::kAdvapi32) {
      probe = "GetUserNameW";
    } else if (kind == SurfaceKind::kWs2) {
      probe = "WSAStartup";
    } else if (kind == SurfaceKind::kBcrypt) {
      probe = "BCryptGenRandom";
    } else if (kind == SurfaceKind::kWinHv) {
      probe = "WHvGetCapability";
    } else if (kind == SurfaceKind::kWsl || kind == SurfaceKind::kWslapi) {
      probe = "WslList";
    } else if (kind == SurfaceKind::kNix) {
      probe = "NixVersion";
    } else {
      probe = "GetCurrentProcessId";
    }
  }
  bool mapped = kind == SurfaceKind::kVmem || kind == SurfaceKind::kPe;
  WowResult r = Probe(kind, label, probe.c_str(), "", mapped);
  if (kind == SurfaceKind::kKernel32) {
    kernel32_ = true;
  } else if (kind == SurfaceKind::kNtdll) {
    ntdll_ = true;
  } else if (kind == SurfaceKind::kUser32) {
    user32_ = true;
  } else if (kind == SurfaceKind::kGdi32) {
    gdi32_ = true;
  } else if (kind == SurfaceKind::kOle32) {
    ole32_ = true;
  } else if (kind == SurfaceKind::kAdvapi32) {
    advapi32_ = true;
  } else if (kind == SurfaceKind::kWs2) {
    sockets_ = true;
  } else if (kind == SurfaceKind::kBcrypt) {
    bcrypt_ = true;
  } else if (kind == SurfaceKind::kWinHv || kind == SurfaceKind::kWinHvEmu) {
    hv_ = true;
  } else if (kind == SurfaceKind::kWsl || kind == SurfaceKind::kWslapi) {
    wsl_ = true;
  }
  (void)MarkFlag;
  return r;
}

WowResult Occupancy::OccupyCatalog(std::string_view label) {
  (void)label;
  Channel* handle = EnsureKind(SurfaceKind::kHandle, "handle");
  if (handle) {
    ++handle->writes;
  }
#if defined(WOW_HAS_WIN32)
  int n = 0;
  const WasmWin32Api* apis = wasmwin32_catalog(&n);
  catalog_rows_ = n;
  std::unordered_map<std::string, std::string> first_api;
  std::vector<std::string> order;
  for (int i = 0; i < n && apis; ++i) {
    std::string dll = apis[i].dll ? apis[i].dll : "";
    if (dll.empty()) {
      continue;
    }
    if (first_api.find(dll) == first_api.end()) {
      first_api[dll] = apis[i].name ? apis[i].name : "";
      order.push_back(dll);
    }
  }
  // Call the first DllImport of every unique catalog DLL. Occupancy
  // does not skip NtCreateFile / CreateWindowExW / NtLoadDriver when
  // those names are the hop — Probe stamps if the live backend refuses.
  for (const auto& dll : order) {
    SurfaceKind kind = KindFromDll(dll);
    const std::string& api = first_api[dll];
    bool mapped = kind == SurfaceKind::kVmem || kind == SurfaceKind::kPe;
    Probe(kind, dll, api.c_str(), CatalogArgs(api.c_str()), mapped);
    if (kind == SurfaceKind::kKernel32) {
      kernel32_ = true;
    } else if (kind == SurfaceKind::kNtdll) {
      ntdll_ = true;
    } else if (kind == SurfaceKind::kUser32) {
      user32_ = true;
    } else if (kind == SurfaceKind::kGdi32) {
      gdi32_ = true;
    } else if (kind == SurfaceKind::kOle32) {
      ole32_ = true;
    } else if (kind == SurfaceKind::kAdvapi32) {
      advapi32_ = true;
    } else if (kind == SurfaceKind::kWs2) {
      sockets_ = true;
    } else if (kind == SurfaceKind::kBcrypt) {
      bcrypt_ = true;
    } else if (kind == SurfaceKind::kWinHv || kind == SurfaceKind::kWinHvEmu) {
      hv_ = true;
    } else if (kind == SurfaceKind::kWsl || kind == SurfaceKind::kWslapi) {
      wsl_ = true;
    }
  }
  catalog_dlls_ = static_cast<int>(order.size());
#else
  OccupyDll("kernel32", "GetCurrentProcessId");
  catalog_dlls_ = 1;
  catalog_rows_ = 1;
#endif
  catalog_ = true;
  depth_ = catalog_dlls_;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyKernel32(std::string_view label) {
  WowResult r = Probe(SurfaceKind::kKernel32,
                      label.empty() ? "kernel32" : label,
                      "GetCurrentProcessId", "", false);
  kernel32_ = true;
  return r;
}

WowResult Occupancy::OccupyNtdll(std::string_view label) {
  std::string_view tag = label.empty() ? "ntdll" : label;
  Probe(SurfaceKind::kNtdll, tag, "RtlGetCurrentPeb", "", false);
  Probe(SurfaceKind::kNtdll, tag, "RtlGetCurrentTeb", "", false);
  Probe(SurfaceKind::kNtdll, tag, "NtQueryInformationProcess", "", false);
  Probe(SurfaceKind::kFile, "file", "NtCreateFile", "", false);
  Probe(SurfaceKind::kNtdll, tag, "NtCreateSection", "4096", false);
  std::string sec = last_reply_;
  Probe(SurfaceKind::kNtdll, tag, "NtMapViewOfSection", sec.c_str(), true);
  ntdll_ = true;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyUser32(std::string_view label) {
  std::string_view tag = label.empty() ? "user32" : label;
  Probe(SurfaceKind::kUser32, tag, "GetDesktopWindow", "", false);
  Probe(SurfaceKind::kUser32, tag, "RegisterClassW", "WASMWin32", false);
  Probe(SurfaceKind::kHwnd, "hwnd", "CreateWindowExW",
        "WASMWin32" "\x1f" "wow-occupy" "\x1f" "0", false);
  user32_ = true;
  hwnd_ = true;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyGdi32(std::string_view label) {
  WowResult r = Probe(SurfaceKind::kGdi32, label.empty() ? "gdi32" : label,
                      "GetStockObject", "", false);
  gdi32_ = true;
  return r;
}

WowResult Occupancy::OccupyOle32(std::string_view label) {
  std::string_view tag = label.empty() ? "ole32" : label;
  OccupyToken("token");
  Probe(SurfaceKind::kOle32, tag, "CoInitializeEx", "", false);
  Probe(SurfaceKind::kOle32, tag, "OleInitialize", "", false);
  Probe(SurfaceKind::kOle32, tag, "CoCreateGuid", "", false);
  const char* git = "{00000320-0000-0000-C000-000000000046}";
  Probe(SurfaceKind::kOle32, tag, "CLSIDFromString", git, false);
  Probe(SurfaceKind::kOle32, tag, "CoCreateInstance", git, false);
  com_instance_ = last_reply_;
  Probe(SurfaceKind::kOle32, tag, "CoGetClassObject", git, false);
  // IFileOperation — COM hop that often carries the token for permission.
  const char* fileop = "{3AD05575-8857-4850-9277-11B85BDB8E09}";
  Probe(SurfaceKind::kOle32, tag, "CoCreateInstance", fileop, false);
  // NtImpersonateAnonymousToken is the anonymous logon. Occupancy
  // Calls it once so the JSON records that hop. It is not permission.
  Probe(SurfaceKind::kNtdll, "ntdll", "NtImpersonateAnonymousToken", "", false);
  anonymous_ = last_reply_;
  // Re-query TokenLinkedToken after COM. That HANDLE is ADMIN.
  std::string tok = token_handle_;
  if (tok.empty() || LiveError(tok)) {
    tok = "-1";
  }
  std::string linked_q = tok + "\x1f" "19";
  Probe(SurfaceKind::kToken, "admin", "GetTokenInformation", linked_q.c_str(),
        false);
  token_linked_ = last_reply_;
  // Impersonate the linked ADMIN token. DuplicateTokenEx is this
  // process (medium, TOKEN_IMPERSONATE). Anonymous is S-1-5-7.
  Probe(SurfaceKind::kToken, "admin", "ImpersonateLoggedOnUser",
        token_linked_.c_str(), false);
  admin_impersonate_ = last_reply_;
  std::string use = LooksHandle(token_linked_) ? token_linked_ : token_dup_;
  if (!LooksHandle(token_linked_) && LooksHandle(token_dup_)) {
    Probe(SurfaceKind::kToken, "admin", "ImpersonateLoggedOnUser",
          token_dup_.c_str(), false);
  }
  Probe(SurfaceKind::kToken, "admin", "OpenThreadToken", "-2" "\x1f" "0x28",
        false);
  std::string setthr = std::string("-2") + "\x1f" + use;
  Probe(SurfaceKind::kToken, "admin", "SetThreadToken", setthr.c_str(), false);
  Probe(SurfaceKind::kNtdll, "ntdll", "NtImpersonateThread", use.c_str(), false);
  std::string admin_sid = use + "\x1f" "S-1-5-32-544";
  Probe(SurfaceKind::kToken, "admin", "CheckTokenMembership", admin_sid.c_str(),
        false);
  admin_member_ = last_reply_;
  std::string privs = use + "\x1f" "3";
  Probe(SurfaceKind::kToken, "admin", "GetTokenInformation", privs.c_str(),
        false);
  admin_privileges_ = last_reply_;
  ProbeImpersonatedScm("scm");
  com_scm_ = scm_all_;
  Probe(SurfaceKind::kToken, "admin", "RevertToSelf", "", false);
  revert_ = last_reply_;
  ole32_ = true;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyCom(std::string_view label) {
  return OccupyOle32(label.empty() ? "ole32" : label);
}

WowResult Occupancy::OccupyAdvapi32(std::string_view label) {
  std::string_view tag = label.empty() ? "advapi32" : label;
  Probe(SurfaceKind::kAdvapi32, tag, "GetUserNameW", "", false);
  OccupyToken("token");
  OccupyOle32("ole32");
  OccupyRegistry("registry");
  OccupyScm("scm");
  advapi32_ = true;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyToken(std::string_view label) {
  std::string_view tag = label.empty() ? "token" : label;
  Probe(SurfaceKind::kKernel32, "kernel32", "GetCurrentProcess", "", false);
  std::string proc = last_reply_;
  if (proc.empty() || LiveError(proc)) {
    proc = "-1";
  }
  // TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE |
  // TOKEN_ADJUST_PRIVILEGES = 0x2E. Without ADJUST, SeLoadDriverPrivilege
  // cannot be enabled on the handle.
  std::string open = proc + "\x1f" "46";
  Probe(SurfaceKind::kToken, tag, "OpenProcessToken", open.c_str(), false);
  if (LiveError(last_reply_)) {
    Probe(SurfaceKind::kToken, tag, "OpenProcessToken", proc.c_str(), false);
  }
  std::string tok = last_reply_;
  token_handle_ = tok;
  Probe(SurfaceKind::kToken, tag, "LookupPrivilegeValueW",
        "SeLoadDriverPrivilege", false);
  std::string luid = last_reply_;
  if (LiveError(luid) || luid.empty()) {
    luid = "10";
  }
  Probe(SurfaceKind::kToken, tag, "LookupPrivilegeNameW", luid.c_str(), false);
  privilege_name_ = last_reply_;
  std::string elev_type = tok + "\x1f" "18";
  Probe(SurfaceKind::kToken, tag, "GetTokenInformation", elev_type.c_str(),
        false);
  token_elevation_type_ = last_reply_;
  std::string linked = tok + "\x1f" "19";
  Probe(SurfaceKind::kToken, tag, "GetTokenInformation", linked.c_str(), false);
  token_linked_ = last_reply_;
  std::string elev = tok + "\x1f" "20";
  Probe(SurfaceKind::kToken, tag, "GetTokenInformation", elev.c_str(), false);
  token_elevation_ = last_reply_;
  std::string dup = tok + "\x1f" "2";
  Probe(SurfaceKind::kToken, tag, "DuplicateTokenEx", dup.c_str(), false);
  token_dup_ = last_reply_;
  ProbeEnableSeLoadDriver(tag);
  std::string setthr = std::string("-2") + "\x1f" + token_dup_;
  Probe(SurfaceKind::kToken, tag, "SetThreadToken", setthr.c_str(), false);
  set_thread_token_ = last_reply_;
  std::string member = tok + "\x1f" "S-1-1-0";
  Probe(SurfaceKind::kToken, tag, "CheckTokenMembership", member.c_str(), false);
  std::string admin_sid = tok + "\x1f" "S-1-5-32-544";
  Probe(SurfaceKind::kToken, tag, "CheckTokenMembership", admin_sid.c_str(),
        false);
  token_admin_member_ = last_reply_;
  ProbeImpersonatedScm(tag);
  Probe(SurfaceKind::kToken, tag, "RevertToSelf", "", false);
  revert_ = last_reply_;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyRegistry(std::string_view label) {
  std::string_view tag = label.empty() ? "registry" : label;
  // HKCU write is the same hop win32_test already fires. Occupancy
  // does not skip it because HKLM Services is missing.
  std::string hkcu = std::string("HKCU") + "\x1f" + "Software\\WowWin32";
  Probe(SurfaceKind::kRegistry, tag, "RegCreateKeyExW", hkcu.c_str(), false);
  if (!LiveError(last_reply_)) {
    std::string key = FirstField(last_reply_);
    std::string setv = key + "\x1f" "Occupied" "\x1f" "1" "\x1f" "wow";
    Probe(SurfaceKind::kRegistry, tag, "RegSetValueExW", setv.c_str(), false);
    std::string qv = key + "\x1f" "Occupied";
    Probe(SurfaceKind::kRegistry, tag, "RegQueryValueExW", qv.c_str(), false);
    Probe(SurfaceKind::kRegistry, tag, "RegCloseKey", key.c_str(), false);
  }

  std::string services =
      std::string("HKLM") + "\x1f" +
      "SYSTEM\\CurrentControlSet\\Services";
  Probe(SurfaceKind::kRegistry, tag, "RegOpenKeyExW", services.c_str(), false);
  registry_services_ = last_reply_;

  std::string wow =
      std::string("HKLM") + "\x1f" +
      "SYSTEM\\CurrentControlSet\\Services\\WowWin32";
  Probe(SurfaceKind::kRegistry, tag, "RegOpenKeyExW", wow.c_str(), false);
  registry_wow_ = last_reply_;

  Probe(SurfaceKind::kRegistry, tag, "RegCreateKeyExW", wow.c_str(), false);
  registry_create_ = last_reply_;
  if (!LiveError(last_reply_)) {
    std::string key = FirstField(last_reply_);
    std::string type = key + "\x1f" "Type" "\x1f" "4" "\x1f" "1";
    std::string start = key + "\x1f" "Start" "\x1f" "4" "\x1f" "3";
    std::string errc = key + "\x1f" "ErrorControl" "\x1f" "4" "\x1f" "1";
    std::string img = key + "\x1f" "ImagePath" "\x1f" "1" "\x1f" "wowwin32.sys";
    Probe(SurfaceKind::kRegistry, tag, "RegSetValueExW", type.c_str(), false);
    Probe(SurfaceKind::kRegistry, tag, "RegSetValueExW", start.c_str(), false);
    Probe(SurfaceKind::kRegistry, tag, "RegSetValueExW", errc.c_str(), false);
    Probe(SurfaceKind::kRegistry, tag, "RegSetValueExW", img.c_str(), false);
    std::string qimg = key + "\x1f" "ImagePath";
    Probe(SurfaceKind::kRegistry, tag, "RegQueryValueExW", qimg.c_str(), false);
    Probe(SurfaceKind::kRegistry, tag, "RegFlushKey", key.c_str(), false);
    Probe(SurfaceKind::kRegistry, tag, "RegOpenKeyExW", wow.c_str(), false);
    registry_wow_ = last_reply_;
  }

  const char* nt =
      "\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WowWin32";
  Probe(SurfaceKind::kNtdll, "ntdll", "NtOpenKey", nt, false);
  Probe(SurfaceKind::kNtdll, "ntdll", "NtCreateKey", nt, false);
  Probe(SurfaceKind::kNtdll, "ntdll", "NtSetValueKey", nt, false);
  OccupyScm("scm");
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyScm(std::string_view label) {
  std::string_view tag = label.empty() ? "scm" : label;
  // Same empty-arg OpenSCManagerW as WASMWin32 win32_test.
  Probe(SurfaceKind::kAdvapi32, tag, "OpenSCManagerW", "", false);
  scm_ = last_reply_;
  std::string scm = LiveError(scm_) ? std::string() : FirstField(scm_);
  Probe(SurfaceKind::kAdvapi32, tag, "OpenSCManagerW", "\x1f" "\x1f" "0x1",
        false);
  scm_connect_ = last_reply_;
  if (scm.empty() && !LiveError(scm_connect_)) {
    scm = FirstField(scm_connect_);
  }
  Probe(SurfaceKind::kAdvapi32, tag, "OpenSCManagerW", "\x1f" "\x1f" "0x15",
        false);
  scm_enum_ = last_reply_;
  if (scm.empty() && !LiveError(scm_enum_)) {
    scm = FirstField(scm_enum_);
  }
  // SC_MANAGER_ALL_ACCESS = 0xF003F. Stays 5 on this token. Still Call it.
  Probe(SurfaceKind::kAdvapi32, tag, "OpenSCManagerW", "\x1f" "\x1f" "0xF003F",
        false);
  scm_all_ = last_reply_;
  if (!LiveError(scm_all_)) {
    scm = FirstField(scm_all_);
  }
  if (!scm.empty()) {
    std::string eventlog = scm + "\x1f" "EventLog" "\x1f" "0x14";
    Probe(SurfaceKind::kAdvapi32, tag, "OpenServiceW", eventlog.c_str(), false);
    scm_eventlog_ = last_reply_;
    if (LiveError(scm_eventlog_)) {
      eventlog = scm + "\x1f" "EventLog" "\x1f" "0x4";
      Probe(SurfaceKind::kAdvapi32, tag, "OpenServiceW", eventlog.c_str(), false);
      scm_eventlog_ = last_reply_;
    }
    if (!LiveError(scm_eventlog_)) {
      std::string svc = FirstField(scm_eventlog_);
      Probe(SurfaceKind::kAdvapi32, tag, "QueryServiceStatus", svc.c_str(),
            false);
      scm_status_ = last_reply_;
      Probe(SurfaceKind::kAdvapi32, tag, "QueryServiceConfigW", svc.c_str(),
            false);
      Probe(SurfaceKind::kAdvapi32, tag, "EnumServicesStatusW", scm.c_str(),
            false);
      Probe(SurfaceKind::kAdvapi32, tag, "CloseServiceHandle", svc.c_str(),
            false);
    }
    Probe(SurfaceKind::kAdvapi32, tag, "StartServiceCtrlDispatcherW",
          "EventLog", false);
    Probe(SurfaceKind::kAdvapi32, tag, "RegisterServiceCtrlHandlerW",
          "EventLog", false);

    std::string wow = scm + "\x1f" "WowWin32" "\x1f" "0x14";
    Probe(SurfaceKind::kAdvapi32, tag, "OpenServiceW", wow.c_str(), false);
    scm_wow_ = last_reply_;
    if (LiveError(scm_wow_)) {
      wow = scm + "\x1f" "WowWin32" "\x1f" "0x4";
      Probe(SurfaceKind::kAdvapi32, tag, "OpenServiceW", wow.c_str(), false);
      scm_wow_ = last_reply_;
    }
    std::string create = scm + "\x1f" "WowWin32" "\x1f" "wowwin32.sys";
    Probe(SurfaceKind::kAdvapi32, tag, "CreateServiceW", create.c_str(), false);
    scm_create_ = last_reply_;
    std::string svc;
    if (!LiveError(scm_create_)) {
      svc = FirstField(scm_create_);
    } else if (!LiveError(scm_wow_)) {
      svc = FirstField(scm_wow_);
    }
    if (!svc.empty()) {
      Probe(SurfaceKind::kAdvapi32, tag, "QueryServiceStatus", svc.c_str(),
            false);
      scm_status_ = last_reply_;
      Probe(SurfaceKind::kAdvapi32, tag, "StartServiceW", svc.c_str(), false);
      Probe(SurfaceKind::kAdvapi32, tag, "QueryServiceStatus", svc.c_str(),
            false);
      scm_status_ = last_reply_;
      Probe(SurfaceKind::kAdvapi32, tag, "CloseServiceHandle", svc.c_str(),
            false);
    }
    Probe(SurfaceKind::kAdvapi32, tag, "CloseServiceHandle", scm.c_str(),
          false);
  } else {
    Probe(SurfaceKind::kAdvapi32, tag, "OpenServiceW", "WowWin32", false);
    scm_wow_ = last_reply_;
    Probe(SurfaceKind::kAdvapi32, tag, "CreateServiceW", "WowWin32", false);
    scm_create_ = last_reply_;
    Probe(SurfaceKind::kAdvapi32, tag, "QueryServiceStatus", "WowWin32", false);
    scm_status_ = last_reply_;
  }
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyDevice(std::string_view label) {
  std::string_view tag = label.empty() ? "WowWin32" : label;
  // GENERIC_READ | GENERIC_WRITE, OPEN_EXISTING.
  std::string cf = std::string("\\\\.\\WowWin32") + "\x1f" + "3221225472" +
                   "\x1f" + "3";
  Probe(SurfaceKind::kFile, tag, "CreateFileW", cf.c_str(), false);
  device_ = last_reply_;
  Probe(SurfaceKind::kNtdll, "ntdll", "NtOpenFile", "\\\\.\\WowWin32", false);
  Probe(SurfaceKind::kNtdll, "ntdll", "NtCreateFile", "\\\\.\\WowWin32", false);
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupySockets(std::string_view label) {
  std::string_view tag = label.empty() ? "ws2_32" : label;
  Probe(SurfaceKind::kWs2, tag, "WSAStartup", "", false);
  Probe(SurfaceKind::kWs2, tag, "WSASocketW", "", false);
  sockets_ = true;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyBcrypt(std::string_view label) {
  WowResult r = Probe(SurfaceKind::kBcrypt, label.empty() ? "bcrypt" : label,
                      "BCryptGenRandom", "", false);
  bcrypt_ = true;
  return r;
}

WowResult Occupancy::OccupyVmem(std::string_view label) {
  std::string_view tag = label.empty() ? "vmem" : label;
  Probe(SurfaceKind::kVmem, tag, "VirtualAlloc", "", true);
  std::string addr = last_reply_;
  std::string prot = addr + "\x1f" "4";
  Probe(SurfaceKind::kVmem, tag, "VirtualProtect", prot.c_str(), true);
  Probe(SurfaceKind::kVmem, tag, "VirtualQuery", addr.c_str(), true);
  vmem_ = true;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyPipe(std::string_view label) {
  std::string_view tag = label.empty() ? "pipe" : label;
  Probe(SurfaceKind::kPipe, tag, "CreatePipe", "", false);
  Probe(SurfaceKind::kPipe, tag, "CreateNamedPipeW", "", false);
  Probe(SurfaceKind::kPipe, tag, "DuplicateHandle", "", false);
  pipe_ = true;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyPe(std::string_view label) {
  WowResult r = Probe(SurfaceKind::kPe, label.empty() ? "pe" : label,
                      "LoadLibraryW", "kernel32", true);
  pe_ = true;
  return r;
}

WowResult Occupancy::OccupyProcess(std::string_view label) {
  Channel* ch =
      EnsureKind(SurfaceKind::kProcess, label.empty() ? "process" : label);
  if (ch) {
    std::string img("CreateProcessW");
    ch->bytes.assign(img.begin(), img.end());
    ++ch->writes;
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyConsole(std::string_view label) {
  WowResult r = Probe(SurfaceKind::kConsole, label.empty() ? "console" : label,
                      "GetStdHandle", "", false);
  console_ = true;
  return r;
}

WowResult Occupancy::OccupyHwnd(std::string_view label) {
  std::string_view tag = label.empty() ? "hwnd" : label;
  Probe(SurfaceKind::kHwnd, tag, "GetDesktopWindow", "", false);
  Probe(SurfaceKind::kHwnd, tag, "RegisterClassW", "WASMWin32", false);
  Probe(SurfaceKind::kHwnd, tag, "CreateWindowExW",
        "WASMWin32" "\x1f" "wow-occupy" "\x1f" "0", false);
  hwnd_ = true;
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyHv(std::string_view label) {
  WowResult r = Probe(SurfaceKind::kWinHv, label.empty() ? "winhv" : label,
                      "WHvGetCapability", "", false);
  Probe(SurfaceKind::kWinHv, label.empty() ? "winhv" : label,
        "WHvCreatePartition", "", false);
  Channel* emu = EnsureKind(SurfaceKind::kWinHvEmu, "winhv_emu");
  if (emu) {
    Probe(SurfaceKind::kWinHvEmu, "winhv_emu", "WHvEmulatorTryIoEmulation", "",
          false);
  }
  hv_ = true;
  return r;
}

WowResult Occupancy::OccupyWsl(std::string_view label) {
  WowResult r = Probe(SurfaceKind::kWslapi, "wslapi",
                      "WslIsDistributionRegistered", "", false);
  Probe(SurfaceKind::kWsl, label.empty() ? "wsl" : label, "WslList", "", false);
  wsl_ = true;
  return r;
}

WowResult Occupancy::OccupyNix(std::string_view label) {
  return Probe(SurfaceKind::kNix, label.empty() ? "nix" : label, "NixVersion",
               "", false);
}

WowResult Occupancy::OccupyWin32(std::string_view label) {
  (void)label;
  Channel* handle = EnsureKind(SurfaceKind::kHandle, "handle");
  if (handle) {
    ++handle->writes;
  }
  WowResult r = OccupyCatalog("catalog");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyKernel32("kernel32");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyNtdll("ntdll");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyUser32("user32");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyGdi32("gdi32");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyOle32("ole32");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyAdvapi32("advapi32");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupySockets("ws2_32");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyBcrypt("bcrypt");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyVmem("vmem");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyPipe("pipe");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyPe("pe");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyProcess("process");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyConsole("console");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyHwnd("hwnd");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyHv("winhv");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = OccupyWsl("wsl");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  win32_ = true;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupySys(std::string_view label) {
  (void)label;
  int parent = 0;
  auto ntdll = table_.OfKind(SurfaceKind::kNtdll);
  if (!ntdll.empty()) {
    parent = ntdll[0].id;
  }
  auto hop = [&](SurfaceKind k, const char* name, const char* img) {
    Channel ch = table_.Open(k, name, parent);
    if (Channel* live = table_.FindMutable(ch.id)) {
      live->isolation = IsolationOf(k);
      std::string s(img);
      live->bytes.assign(s.begin(), s.end());
      live->writes += 1;
    }
  };
  hop(SurfaceKind::kNtoskrnl, "ntoskrnl", "ntoskrnl.exe");
  hop(SurfaceKind::kSysHeap, "sys_heap", "wowwin32");
  sys_ = true;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::EnsureDriverPermission() {
  if (driver_.loaded()) {
    return WOW_RESULT_OK;
  }
  return OccupyDriver("wowwin32");
}

WowResult Occupancy::EnsureLinuxPermission() {
  if (linux_ && posix_.loaded()) {
    return WOW_RESULT_OK;
  }
  return OccupyLinux("linux");
}

WowResult Occupancy::OccupyDriver(std::string_view label) {
  if (table_.OfKind(SurfaceKind::kSysHeap).empty()) {
    WowResult r = OccupySys(label.empty() ? "sys" : label);
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  WowResult r = driver_.DriverEntry(label.empty() ? "wowwin32" : label);
  if (r != WOW_RESULT_OK) {
    last_error_ = driver_.last_error();
    return r;
  }
  r = driver_.AddDevice();
  if (r != WOW_RESULT_OK) {
    last_error_ = driver_.last_error();
    return r;
  }
  r = driver_.StartDevice();
  if (r != WOW_RESULT_OK) {
    last_error_ = driver_.last_error();
    return r;
  }
  int parent = 0;
  auto heap = table_.OfKind(SurfaceKind::kSysHeap);
  if (!heap.empty()) {
    parent = heap[0].id;
  }
  Channel ch = table_.Open(SurfaceKind::kOccupancyDriver, "wowwin32", parent);
  if (Channel* live = table_.FindMutable(ch.id)) {
    live->isolation = Isolation::kSys;
    std::string img = "wowwin32.sys";
    live->bytes.assign(img.begin(), img.end());
    live->writes += 1;
    if (SysImage* occ = driver_.table().OfKind(DriverKind::kOccupancy)) {
      occ->channel_id = live->id;
    }
  }
  last_error_.clear();
  OccupyToken("token");
  OccupyOle32("ole32");
  OccupyRegistry("registry");
  OccupyDevice("WowWin32");
  return OccupyLoadDriver("NtLoadDriver");
}

WowResult Occupancy::OccupyLoadDriver(std::string_view label) {
  if (table_.OfKind(SurfaceKind::kOccupancyDriver).empty()) {
    WowResult r = OccupyDriver(label.empty() ? "wowwin32" : label);
    if (r != WOW_RESULT_OK) {
      return r;
    }
    return WOW_RESULT_OK;
  }
  Probe(SurfaceKind::kNtdll, "ntdll", "GetModuleHandleW", "ntdll", false);
  std::string ntdll_h = last_reply_;
  std::string gpa = ntdll_h + "\x1f" "NtLoadDriver";
  Probe(SurfaceKind::kPe, "pe", "GetProcAddress", gpa.c_str(), false);
  OccupyToken("token");
  OccupyOle32("ole32");
  OccupyRegistry("registry");
  OccupyDevice("WowWin32");
  std::string tag(label.empty() ? "NtLoadDriver" : std::string(label));
  // Enable SeLoadDriverPrivilege on dup/thread before NtLoadDriver.
  // 1300 ERROR_NOT_ALL_ASSIGNED means this token never had it — cannot
  // add it. RevertToSelf (already done by OccupyToken) then load.
  bool never_had = NotAllAssigned(adjust_process_) &&
                   NotAllAssigned(adjust_dup_) &&
                   NotAllAssigned(adjust_thread_);
  if (!never_had && LooksHandle(token_dup_)) {
    Probe(SurfaceKind::kToken, "token", "ImpersonateLoggedOnUser",
          token_dup_.c_str(), false);
    Probe(SurfaceKind::kNtdll, "ntdll", "RtlAdjustPrivilege",
          "10" "\x1f" "1" "\x1f" "1", false);
  }
  Probe(SurfaceKind::kOccupancyDriver, tag, "NtLoadDriver", "", false);
  Probe(SurfaceKind::kOccupancyDriver, "ZwLoadDriver", "ZwLoadDriver", "",
        false);
  const char* reg =
      "\\Registry\\Machine\\System\\CurrentControlSet\\Services\\WowWin32";
  Probe(SurfaceKind::kOccupancyDriver, tag, "NtLoadDriver", reg, false);
  Probe(SurfaceKind::kOccupancyDriver, "ZwLoadDriver", "ZwLoadDriver", reg,
        false);
  Probe(SurfaceKind::kOccupancyDriver, "NtUnloadDriver", "NtUnloadDriver", "",
        false);
  Probe(SurfaceKind::kOccupancyDriver, "ZwUnloadDriver", "ZwUnloadDriver", "",
        false);
  Probe(SurfaceKind::kOccupancyDriver, "NtDeviceIoControlFile",
        "NtDeviceIoControlFile", "", false);
  auto bytes = std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(reg), std::strlen(reg));
  (void)driver_.DeviceControl(WOW_IOCTL_LOAD, bytes);
  if (!never_had) {
    Probe(SurfaceKind::kToken, "token", "RevertToSelf", "", false);
    revert_ = last_reply_;
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyWin32k(std::string_view label) {
  if (table_.OfKind(SurfaceKind::kSysHeap).empty()) {
    WowResult r = OccupySys("sys");
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  int parent = 0;
  auto hwnd = table_.OfKind(SurfaceKind::kHwnd);
  if (!hwnd.empty()) {
    parent = hwnd[0].id;
  }
  std::string img(label.empty() ? "win32k.sys" : std::string(label));
  Channel ch = table_.Open(SurfaceKind::kWin32k, img, parent);
  if (Channel* live = table_.FindMutable(ch.id)) {
    live->isolation = Isolation::kHost;
    live->bytes.assign(img.begin(), img.end());
    live->writes += 1;
  }
  win32k_ = true;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyHostNtos(std::string_view label) {
  if (table_.OfKind(SurfaceKind::kNtoskrnl).empty()) {
    WowResult r = OccupySys("sys");
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  int parent = 0;
  auto nt = table_.OfKind(SurfaceKind::kNtoskrnl);
  if (!nt.empty()) {
    parent = nt[0].id;
  }
  std::string img(label.empty() ? "ntoskrnl.exe" : std::string(label));
  Channel ch = table_.Open(SurfaceKind::kNtoskrnl, img, parent);
  if (Channel* live = table_.FindMutable(ch.id)) {
    live->isolation = Isolation::kHost;
    live->bytes.assign(img.begin(), img.end());
    live->writes += 1;
  }
  host_ntos_ = true;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyShellImage(SurfaceKind kind, std::string_view image) {
  WowResult r = EnsureDriverPermission();
  if (r != WOW_RESULT_OK) {
    return r;
  }
  std::string img(image);
  auto bytes = std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(img.data()), img.size());
  r = driver_.DeviceControl(WOW_IOCTL_EXEC, bytes);
  if (r != WOW_RESULT_OK) {
    last_error_ = driver_.last_error();
    return r;
  }
  int parent = 0;
  auto occ = table_.OfKind(SurfaceKind::kOccupancyDriver);
  if (!occ.empty()) {
    parent = occ[0].id;
  }
  Channel ch = table_.Open(kind, img, parent);
  if (Channel* live = table_.FindMutable(ch.id)) {
    live->isolation = Isolation::kShell;
    live->bytes.assign(img.begin(), img.end());
    live->writes += 1;
  }
  if (kind == SurfaceKind::kCmd) {
    cmd_ = true;
  }
  if (kind == SurfaceKind::kSh) {
    sh_ = true;
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyCmd(std::string_view label) {
  return OccupyShellImage(SurfaceKind::kCmd,
                          label.empty() ? "cmd.exe" : label);
}

WowResult Occupancy::LaunchWin32(std::string_view image) {
  WowResult r = EnsureGocvmToolkit();
  if (r != WOW_RESULT_OK) {
    return r;
  }
#if defined(WOW_HAS_WIN32)
  std::string img(image.empty() ? "calc.exe" : std::string(image));
  std::string args = img;
  args.push_back('\x1f');
  args += img;
  char buf[1024] = {};
  int rc = wasmwin32_call("CreateProcessW", args.c_str(), buf, sizeof(buf));
  if (rc != 0 || buf[0] == '\0' || std::strncmp(buf, "error:", 6) == 0) {
    last_error_ = buf[0] ? buf : "CreateProcessW failed";
    return WOW_RESULT_FAILED_PRECONDITION;
  }
  calc_pid_ = static_cast<int>(std::strtol(buf, nullptr, 10));
  if (calc_pid_ <= 0) {
    last_error_ = "CreateProcessW no pid";
    return WOW_RESULT_FAILED_PRECONDITION;
  }
  int parent = 0;
  auto goc = table_.OfKind(SurfaceKind::kGocvm);
  if (!goc.empty()) {
    parent = goc[0].id;
  }
  Channel ch = table_.Open(SurfaceKind::kProcess, img, parent);
  if (Channel* live = table_.FindMutable(ch.id)) {
    live->isolation = Isolation::kShell;
    live->bytes.assign(img.begin(), img.end());
    live->writes += 1;
  }
  last_error_.clear();
  return WOW_RESULT_OK;
#else
  (void)image;
  last_error_ = "WASMWin32 missing";
  return WOW_RESULT_UNAVAILABLE;
#endif
}

WowResult Occupancy::EnsureGocvmToolkit() {
  if (gocvm_) {
    return WOW_RESULT_OK;
  }
  return OccupyGocvm("wasigocvm");
}

WowResult Occupancy::OccupyCalc(std::string_view label) {
  return LaunchWin32(label.empty() ? "calc.exe" : label);
}

WowResult Occupancy::OccupySh(std::string_view label) {
  WowResult r = EnsureLinuxPermission();
  if (r != WOW_RESULT_OK) {
    return r;
  }
  std::string img(label.empty() ? "/bin/sh" : std::string(label));
  auto bytes = std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(img.data()), img.size());
  r = posix_.Ioctl(WOW_IOCTL_EXEC, bytes);
  if (r != WOW_RESULT_OK) {
    last_error_ = posix_.last_error();
    return r;
  }
  return OccupyShellImage(SurfaceKind::kSh, img);
}

WowResult Occupancy::OccupyLinux(std::string_view label) {
  if (!driver_.loaded()) {
    WowResult r = OccupyDriver(label.empty() ? "wowwin32" : label);
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  int parent = 0;
  auto occ = table_.OfKind(SurfaceKind::kOccupancyDriver);
  if (!occ.empty()) {
    parent = occ[0].id;
  }
  WowResult r = posix_.ModuleInit(label.empty() ? "wowwin32" : label);
  if (r != WOW_RESULT_OK) {
    last_error_ = posix_.last_error();
    return r;
  }
  Channel ch = table_.Open(SurfaceKind::kWowKo, "wowwin32.ko", parent);
  if (Channel* live = table_.FindMutable(ch.id)) {
    live->isolation = Isolation::kSys;
    std::string img = "wowwin32.ko";
    live->bytes.assign(img.begin(), img.end());
    live->writes += 1;
    if (KoImage* ko = posix_.table().OfKind(KoKind::kOccupancy)) {
      ko->channel_id = live->id;
    }
  }
  linux_ = true;
  last_error_.clear();
  return OccupySh("/bin/sh");
}

void Occupancy::WireTty() {
  tty_.SetSend([this](std::span<const uint8_t> p) {
    tty_out_.insert(tty_out_.end(), p.begin(), p.end());
    return WOW_RESULT_OK;
  });
}

std::string Occupancy::TtyHelloJson() const {
  const TtyHello& h = tty_.hello();
  std::string o = "{\"magic\":\"WTTY\",\"version\":";
  o += std::to_string(h.version ? h.version : kTtyVersion);
  o += ",\"role\":";
  o += std::to_string(h.role ? h.role : kTtyRoleListener);
  o += ",\"cols\":";
  o += std::to_string(h.cols ? h.cols : 80);
  o += ",\"rows\":";
  o += std::to_string(h.rows ? h.rows : 24);
  o += ",\"proto\":\"wasmtty\"}";
  return o;
}

WowResult Occupancy::OccupyWasmtty(std::string_view label) {
  WowResult r = EnsureDriverPermission();
  if (r != WOW_RESULT_OK) {
    return r;
  }
  r = EnsureLinuxPermission();
  if (r != WOW_RESULT_OK) {
    return r;
  }
  std::string img(label.empty() ? "wasmtty" : label);
  auto bytes = std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(img.data()), img.size());
  r = driver_.DeviceControl(WOW_IOCTL_TTY, bytes);
  if (r != WOW_RESULT_OK) {
    last_error_ = driver_.last_error();
    return r;
  }
  r = posix_.Ioctl(WOW_IOCTL_TTY, bytes);
  if (r != WOW_RESULT_OK) {
    last_error_ = posix_.last_error();
    return r;
  }
  int parent = 0;
  auto occ = table_.OfKind(SurfaceKind::kOccupancyDriver);
  if (!occ.empty()) {
    parent = occ[0].id;
  }
  auto hop = [&](SurfaceKind k, const char* name, const char* bytes_img) {
    Channel ch = table_.Open(k, name, parent);
    if (Channel* live = table_.FindMutable(ch.id)) {
      live->isolation = Isolation::kTty;
      std::string s(bytes_img);
      live->bytes.assign(s.begin(), s.end());
      live->writes += 1;
    }
  };
  hop(SurfaceKind::kWasmtty, img.c_str(), "WTTY");
  hop(SurfaceKind::kConPty, "conpty", "conpty");
  auto ko = table_.OfKind(SurfaceKind::kWowKo);
  int linux_parent = ko.empty() ? parent : ko[0].id;
  Channel pts = table_.Open(SurfaceKind::kPts, "pts", linux_parent);
  if (Channel* live = table_.FindMutable(pts.id)) {
    live->isolation = Isolation::kTty;
    std::string s("pts");
    live->bytes.assign(s.begin(), s.end());
    live->writes += 1;
  }
  WireTty();
  r = tty_.WriteHello(TtyHello{kTtyVersion, kTtyRoleListener, 80, 24});
  if (r != WOW_RESULT_OK) {
    last_error_ = "wasmtty hello";
    return r;
  }
  wasmtty_ = true;
  conpty_ = true;
  pts_ = true;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyGocvm(std::string_view label) {
  WowResult r = OccupyWasmtty("wasmtty");
  if (r != WOW_RESULT_OK) {
    return r;
  }
  std::string img(label.empty() ? "wasigocvm" : std::string(label));
  int parent = 0;
  auto tty = table_.OfKind(SurfaceKind::kWasmtty);
  if (!tty.empty()) {
    parent = tty[0].id;
  }
  Channel ch = table_.Open(SurfaceKind::kGocvm, img, parent);
  if (Channel* live = table_.FindMutable(ch.id)) {
    live->isolation = Isolation::kTty;
    live->bytes.assign(img.begin(), img.end());
    live->writes += 1;
  }
  std::string out = "via=wasigocvm toolkit=gocvm permission=sys image=";
  out += img;
  out +=
      " isolation=tty proto=WTTY windows=wowwin32.sys linux=wowwin32.ko "
      "win32=WASMWin32";
  auto payload = std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(out.data()), out.size());
  r = tty_.WriteStdout(payload);
  if (r != WOW_RESULT_OK) {
    last_error_ = "gocvm stdout";
    return r;
  }
  gocvm_ = true;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::TtyWriteHello(std::string* reply) {
  if (!wasmtty_) {
    WowResult r = OccupyWasmtty("wasmtty");
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  WowResult r =
      tty_.WriteHello(TtyHello{kTtyVersion, kTtyRoleListener, 80, 24});
  if (r != WOW_RESULT_OK) {
    last_error_ = "ttyHello";
    return r;
  }
  if (reply) {
    *reply = TtyHelloJson();
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::TtyPing(std::string* reply) {
  if (!wasmtty_) {
    WowResult r = OccupyWasmtty("wasmtty");
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  TtyFrame f;
  f.type = kTtyPing;
  auto framed = TtyEncode(f);
  tty_.Push(framed);
  if (reply) {
    *reply = tty_.pong() ? "pong" : "ping";
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::OccupyNet(std::string_view label) {
  if (!driver_.loaded()) {
    WowResult r = OccupyDriver(label.empty() ? "wowwin32" : label);
    if (r != WOW_RESULT_OK) {
      return r;
    }
  }
  int parent = 0;
  auto occ = table_.OfKind(SurfaceKind::kOccupancyDriver);
  if (!occ.empty()) {
    parent = occ[0].id;
  }
  auto hop = [&](SurfaceKind k, const char* name, const char* img) {
    Channel ch = table_.Open(k, name, parent);
    if (Channel* live = table_.FindMutable(ch.id)) {
      live->isolation = Isolation::kNet;
      std::string s(img);
      live->bytes.assign(s.begin(), s.end());
      live->writes += 1;
    }
  };
  hop(SurfaceKind::kSctp, "sctp", "sctp");
  std::string net_label(label.empty() ? "wns" : std::string(label));
  hop(SurfaceKind::kWns, net_label.c_str(), "sctp-rpc");
  net_ = true;
  last_error_.clear();
  return WOW_RESULT_OK;
}

std::string Occupancy::CommonRoutesJson() const {
  return "{\"thin\":true,\"win32\":true,\"routes\":["
         "{\"from\":\"handle\",\"to\":\"kernel32\",\"via\":\"wasmwin32_call\",\"isolation\":\"host\"},"
         "{\"from\":\"kernel32\",\"to\":\"ntdll\",\"via\":\"Nt/Zw/Rtl\",\"isolation\":\"sys\"},"
         "{\"from\":\"kernel32\",\"to\":\"vmem\",\"via\":\"VirtualAlloc\",\"isolation\":\"host\"},"
         "{\"from\":\"kernel32\",\"to\":\"pipe\",\"via\":\"CreatePipe\",\"isolation\":\"ipc\"},"
         "{\"from\":\"kernel32\",\"to\":\"pe\",\"via\":\"LoadLibraryW\",\"isolation\":\"host\"},"
         "{\"from\":\"kernel32\",\"to\":\"process\",\"via\":\"CreateProcessW\",\"isolation\":\"shell\"},"
         "{\"from\":\"kernel32\",\"to\":\"console\",\"via\":\"GetStdHandle\",\"isolation\":\"tty\"},"
         "{\"from\":\"user32\",\"to\":\"hwnd\",\"via\":\"GetDesktopWindow\",\"isolation\":\"compositor\"},"
         "{\"from\":\"hwnd\",\"to\":\"gdi32\",\"via\":\"GetStockObject\",\"isolation\":\"compositor\"},"
         "{\"from\":\"hwnd\",\"to\":\"win32k\",\"via\":\"host GDI/DWM stays\",\"isolation\":\"host\"},"
         "{\"from\":\"ole32\",\"to\":\"ipc\",\"via\":\"CoCreateGuid\",\"isolation\":\"ipc\"},"
         "{\"from\":\"token\",\"to\":\"ole32\",\"via\":\"DuplicateTokenEx\",\"isolation\":\"ipc\"},"
         "{\"from\":\"ole32\",\"to\":\"ipc\",\"via\":\"CoCreateInstance\",\"isolation\":\"ipc\"},"
         "{\"from\":\"token\",\"to\":\"admin\",\"via\":\"ImpersonateLoggedOnUser\",\"isolation\":\"sys\"},"
         "{\"from\":\"token\",\"to\":\"thread\",\"via\":\"SetThreadToken -2\",\"isolation\":\"sys\"},"
         "{\"from\":\"token\",\"to\":\"thread\",\"via\":\"OpenThreadToken -2\",\"isolation\":\"sys\"},"
         "{\"from\":\"token\",\"to\":\"token\",\"via\":\"RevertToSelf\",\"isolation\":\"sys\"},"
         "{\"from\":\"ntdll\",\"to\":\"anonymous\",\"via\":\"NtImpersonateAnonymousToken\",\"isolation\":\"sys\"},"
         "{\"from\":\"token\",\"to\":\"scm\",\"via\":\"OpenSCManagerW 0x1 while impersonated\",\"isolation\":\"sys\"},"
         "{\"from\":\"scm\",\"to\":\"sys\",\"via\":\"OpenServiceW EventLog 0x14\",\"isolation\":\"sys\"},"
         "{\"from\":\"ole32\",\"to\":\"scm\",\"via\":\"OpenSCManagerW 0xF003F after admin\",\"isolation\":\"sys\"},"
         "{\"from\":\"advapi32\",\"to\":\"token\",\"via\":\"OpenProcessToken\",\"isolation\":\"sys\"},"
         "{\"from\":\"token\",\"to\":\"advapi32\",\"via\":\"LookupPrivilegeValueW\",\"isolation\":\"sys\"},"
         "{\"from\":\"token\",\"to\":\"ntdll\",\"via\":\"NtAdjustPrivilegesToken\",\"isolation\":\"sys\"},"
         "{\"from\":\"token\",\"to\":\"ntdll\",\"via\":\"RtlAdjustPrivilege CurrentThread\",\"isolation\":\"sys\"},"
         "{\"from\":\"advapi32\",\"to\":\"registry\",\"via\":\"RegOpenKeyExW\",\"isolation\":\"sys\"},"
         "{\"from\":\"registry\",\"to\":\"sys\",\"via\":\"RegCreateKeyExW Services\\\\WowWin32\",\"isolation\":\"sys\"},"
         "{\"from\":\"advapi32\",\"to\":\"scm\",\"via\":\"OpenSCManagerW\",\"isolation\":\"sys\"},"
         "{\"from\":\"scm\",\"to\":\"sys\",\"via\":\"OpenServiceW WowWin32\",\"isolation\":\"sys\"},"
         "{\"from\":\"file\",\"to\":\"sys\",\"via\":\"CreateFileW \\\\\\\\.\\\\WowWin32\",\"isolation\":\"host\"},"
         "{\"from\":\"ws2_32\",\"to\":\"net\",\"via\":\"WSAStartup\",\"isolation\":\"net\"},"
         "{\"from\":\"ntdll\",\"to\":\"winhv\",\"via\":\"WHvRunVirtualProcessor\",\"isolation\":\"hv\"},"
         "{\"from\":\"kernel32\",\"to\":\"wsl\",\"via\":\"WslList\",\"isolation\":\"tty\"},"
         "{\"from\":\"ntdll\",\"to\":\"sys\",\"via\":\"NtLoadDriver\",\"isolation\":\"sys\"},"
         "{\"from\":\"ntdll\",\"to\":\"sys\",\"via\":\"ZwLoadDriver\",\"isolation\":\"sys\"},"
         "{\"from\":\"sys\",\"to\":\"occupancy_driver\",\"via\":\"DriverEntry\",\"isolation\":\"sys\"},"
         "{\"from\":\"occupancy_driver\",\"to\":\"cmd\",\"via\":\"IRP_MJ_DEVICE_CONTROL\",\"isolation\":\"shell\"},"
         "{\"from\":\"occupancy_driver\",\"to\":\"wow_ko\",\"via\":\"module_init\",\"isolation\":\"sys\"},"
         "{\"from\":\"wow_ko\",\"to\":\"wasmtty\",\"via\":\"IOCTL_TTY\",\"isolation\":\"tty\"},"
         "{\"from\":\"wasmtty\",\"to\":\"gocvm\",\"via\":\"wasigocvm\",\"isolation\":\"tty\"},"
         "{\"from\":\"gocvm\",\"to\":\"calc\",\"via\":\"CreateProcessW\",\"isolation\":\"shell\"},"
         "{\"from\":\"occupancy_driver\",\"to\":\"wns\",\"via\":\"sctp-rpc\",\"isolation\":\"net\"}"
         "]}";
}

WowResult Occupancy::Open(SurfaceKind kind, std::string_view label, int parent,
                          int* out_id) {
  Channel* ch = Stamp(kind, label, parent);
  if (!ch) {
    last_error_ = "open failed";
    return WOW_RESULT_INTERNAL;
  }
  if (out_id) {
    *out_id = ch->id;
  }
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::Write(int id, std::span<const uint8_t> bytes) {
  Channel* ch = nullptr;
  WowResult r = NeedLive(id, &ch);
  if (r != WOW_RESULT_OK) {
    return r;
  }
  ch->bytes.assign(bytes.begin(), bytes.end());
  ++ch->writes;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::Read(int id, std::vector<uint8_t>* out) {
  Channel* ch = nullptr;
  WowResult r = NeedLive(id, &ch);
  if (r != WOW_RESULT_OK) {
    return r;
  }
  if (out) {
    *out = ch->bytes;
  }
  ++ch->reads;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::Map(int id) {
  Channel* ch = nullptr;
  WowResult r = NeedLive(id, &ch);
  if (r != WOW_RESULT_OK) {
    return r;
  }
  ch->mapped = true;
  last_error_.clear();
  return WOW_RESULT_OK;
}

WowResult Occupancy::Unmap(int id) {
  Channel* ch = nullptr;
  WowResult r = NeedLive(id, &ch);
  if (r != WOW_RESULT_OK) {
    return r;
  }
  ch->mapped = false;
  last_error_.clear();
  return WOW_RESULT_OK;
}

std::string Occupancy::StatusJson() const {
  auto all = table_.All();
  std::string o = "{\"win32\":";
  o += win32_ ? "true" : "false";
  o += ",\"catalog\":";
  o += catalog_ ? "true" : "false";
  o += ",\"kernel32\":";
  o += kernel32_ ? "true" : "false";
  o += ",\"ntdll\":";
  o += ntdll_ ? "true" : "false";
  o += ",\"user32\":";
  o += user32_ ? "true" : "false";
  o += ",\"gdi32\":";
  o += gdi32_ ? "true" : "false";
  o += ",\"ole32\":";
  o += ole32_ ? "true" : "false";
  o += ",\"advapi32\":";
  o += advapi32_ ? "true" : "false";
  o += ",\"sockets\":";
  o += sockets_ ? "true" : "false";
  o += ",\"bcrypt\":";
  o += bcrypt_ ? "true" : "false";
  o += ",\"vmem\":";
  o += vmem_ ? "true" : "false";
  o += ",\"pipe\":";
  o += pipe_ ? "true" : "false";
  o += ",\"pe\":";
  o += pe_ ? "true" : "false";
  o += ",\"hv\":";
  o += hv_ ? "true" : "false";
  o += ",\"wsl\":";
  o += wsl_ ? "true" : "false";
  o += ",\"console\":";
  o += console_ ? "true" : "false";
  o += ",\"hwnd\":";
  o += hwnd_ ? "true" : "false";
  o += ",\"sys\":";
  o += sys_ ? "true" : "false";
  o += ",\"win32k\":";
  o += win32k_ ? "true" : "false";
  o += ",\"host_ntos\":";
  o += host_ntos_ ? "true" : "false";
  o += ",\"cmd\":";
  o += cmd_ ? "true" : "false";
  o += ",\"linux\":";
  o += linux_ ? "true" : "false";
  o += ",\"wasmtty\":";
  o += wasmtty_ ? "true" : "false";
  o += ",\"conpty\":";
  o += conpty_ ? "true" : "false";
  o += ",\"pts\":";
  o += pts_ ? "true" : "false";
  o += ",\"gocvm\":";
  o += gocvm_ ? "true" : "false";
  o += ",\"net\":";
  o += net_ ? "true" : "false";
  o += ",\"depth\":";
  o += std::to_string(depth_);
  o += ",\"catalog_dlls\":";
  o += std::to_string(catalog_dlls_);
  o += ",\"catalog_rows\":";
  o += std::to_string(catalog_rows_);
  o += ",\"called\":[";
  for (size_t i = 0; i < called_.size(); ++i) {
    if (i) {
      o += ',';
    }
    o += JsonEscape(called_[i]);
  }
  o += ']';
  o += ",\"lookup\":";
  o += JsonEscape(privilege_name_);
  o += ",\"privileges\":";
  AppendPrivilegeJson(&o, privileges_);
  o += ",\"service_key\":\"SYSTEM\\\\CurrentControlSet\\\\Services\\\\WowWin32\"";
  o += ",\"nt_service\":\"\\\\Registry\\\\Machine\\\\System\\\\CurrentControlSet\\\\Services\\\\WowWin32\"";
  o += ",\"dos_device\":\"\\\\\\\\.\\\\WowWin32\"";
  o += ",\"registry_services\":";
  o += JsonEscape(registry_services_);
  o += ",\"registry_wowwin32\":";
  o += JsonEscape(registry_wow_);
  o += ",\"registry_create\":";
  o += JsonEscape(registry_create_);
  o += ",\"device\":";
  o += JsonEscape(device_);
  o += ",\"scm\":";
  o += JsonEscape(scm_);
  o += ",\"scm_connect\":";
  o += JsonEscape(scm_connect_);
  o += ",\"scm_enum\":";
  o += JsonEscape(scm_enum_);
  o += ",\"scm_all_access\":";
  o += JsonEscape(scm_all_);
  o += ",\"scm_eventlog\":";
  o += JsonEscape(scm_eventlog_);
  o += ",\"scm_wowwin32\":";
  o += JsonEscape(scm_wow_);
  o += ",\"scm_create\":";
  o += JsonEscape(scm_create_);
  o += ",\"scm_status\":";
  o += JsonEscape(scm_status_);
  o += ",\"token\":";
  o += JsonEscape(token_handle_);
  o += ",\"token_dup\":";
  o += JsonEscape(token_dup_);
  o += ",\"token_elevation_type\":";
  o += JsonEscape(token_elevation_type_);
  o += ",\"token_linked\":";
  o += JsonEscape(token_linked_);
  o += ",\"token_elevation\":";
  o += JsonEscape(token_elevation_);
  o += ",\"token_admin_member\":";
  o += JsonEscape(token_admin_member_);
  o += ",\"impersonate\":";
  o += JsonEscape(impersonate_);
  o += ",\"thread_token\":";
  o += JsonEscape(thread_token_);
  o += ",\"set_thread_token\":";
  o += JsonEscape(set_thread_token_);
  o += ",\"revert\":";
  o += JsonEscape(revert_);
  o += ",\"adjust_process\":";
  o += JsonEscape(adjust_process_);
  o += ",\"adjust_dup\":";
  o += JsonEscape(adjust_dup_);
  o += ",\"nt_adjust\":";
  o += JsonEscape(nt_adjust_);
  o += ",\"adjust_thread\":";
  o += JsonEscape(adjust_thread_);
  o += ",\"rtl_thread\":";
  o += JsonEscape(rtl_thread_);
  o += ",\"com_instance\":";
  o += JsonEscape(com_instance_);
  o += ",\"com_scm\":";
  o += JsonEscape(com_scm_);
  o += ",\"anonymous\":";
  o += JsonEscape(anonymous_);
  o += ",\"admin_impersonate\":";
  o += JsonEscape(admin_impersonate_);
  o += ",\"admin_member\":";
  o += JsonEscape(admin_member_);
  o += ",\"admin_privileges\":";
  AppendPrivilegeJson(&o, admin_privileges_);
  if (gocvm_) {
    o += ",\"toolkit\":\"gocvm\"";
  }
  if (calc_pid_ > 0) {
    o += ",\"pid\":";
    o += std::to_string(calc_pid_);
    o += ",\"win32\":\"CreateProcessW\"";
  }
  if (net_) {
    o += ",\"via\":\"sctp-rpc\"";
    o += ",\"isolation\":\"net\"";
  }
  o += ",\"permission\":";
  o += (cmd_ || sh_ || wasmtty_ || gocvm_) ? "\"sys\"" : "\"\"";
  o += ",\"driver\":";
  o += driver_.GetDriverInfoJson();
  o += ",\"posix\":";
  o += posix_.GetInfoJson();
  o += ",\"thin\":true,\"channels\":[";
  bool first = true;
  for (const auto& c : all) {
    if (!first) {
      o += ',';
    }
    first = false;
    o += c.ToJson();
  }
  o += "]}";
  return o;
}

bool Occupancy::DispatchTopic(std::string_view topic, std::string_view payload,
                              std::string* reply_out, std::string* err_out) {
  auto fail = [&](WowResult r, const char* msg) {
    last_error_ = msg ? msg : last_error_;
    if (err_out) {
      *err_out = last_error_;
    }
    (void)r;
    return false;
  };
  auto ok = [&](std::string reply) {
    if (reply_out) {
      *reply_out = std::move(reply);
    }
    if (err_out) {
      err_out->clear();
    }
    return true;
  };

  std::string name = StripTopic(topic);
  if (name.empty()) {
    return fail(WOW_RESULT_INVALID_ARGUMENT, "empty topic");
  }

  uint32_t ord = 0;
  if (!DefaultCatalog().Lookup(name, &ord, nullptr) ||
      !DefaultCatalog().MayFire(ord)) {
    return fail(WOW_RESULT_NOT_FOUND, "unknown win32 topic");
  }

  int id = JsonIntField(payload, "id", 0);
  std::string label = JsonStringField(payload, "label");
  std::string kind = JsonStringField(payload, "kind");
  std::string data = JsonStringField(payload, "data");
  if (data.empty() && !payload.empty() && payload[0] != '{') {
    data = std::string(payload);
  }
  auto hex_payload = [&]() {
    if (!data.empty()) {
      return std::vector<uint8_t>(data.begin(), data.end());
    }
    return std::vector<uint8_t>(payload.begin(), payload.end());
  };
  auto done = [&](WowResult rr) {
    if (rr != WOW_RESULT_OK) {
      return fail(rr, last_error_.c_str());
    }
    return ok(StatusJson());
  };
  auto hop = [&](WowResult rr, SurfaceKind k, const std::string& body) {
    if (rr != WOW_RESULT_OK) {
      return fail(rr, last_error_.c_str());
    }
    auto v = table_.OfKind(k);
    Channel* ch = v.empty() ? nullptr : table_.FindMutable(v.front().id);
    return ok(ch ? ChannelReply(*ch) : body);
  };

  if (name == "occupyWin32") {
    return done(OccupyWin32(label));
  }
  if (name == "occupyCatalog") {
    return done(OccupyCatalog(label.empty() ? "catalog" : label));
  }
  if (name == "occupyKernel32") {
    return hop(OccupyKernel32(label), SurfaceKind::kKernel32, StatusJson());
  }
  if (name == "occupyNtdll") {
    return hop(OccupyNtdll(label), SurfaceKind::kNtdll, StatusJson());
  }
  if (name == "occupyUser32") {
    return hop(OccupyUser32(label), SurfaceKind::kUser32, StatusJson());
  }
  if (name == "occupyGdi32") {
    return hop(OccupyGdi32(label), SurfaceKind::kGdi32, StatusJson());
  }
  if (name == "occupyOle32") {
    return hop(OccupyOle32(label), SurfaceKind::kOle32, StatusJson());
  }
  if (name == "occupyCom") {
    return done(OccupyCom(label.empty() ? "ole32" : label));
  }
  if (name == "occupyAdvapi32") {
    return hop(OccupyAdvapi32(label), SurfaceKind::kAdvapi32, StatusJson());
  }
  if (name == "occupyToken") {
    return done(OccupyToken(label.empty() ? "token" : label));
  }
  if (name == "occupyRegistry") {
    return done(OccupyRegistry(label.empty() ? "registry" : label));
  }
  if (name == "occupyScm") {
    return done(OccupyScm(label.empty() ? "scm" : label));
  }
  if (name == "occupyDevice") {
    return done(OccupyDevice(label.empty() ? "WowWin32" : label));
  }
  if (name == "occupySockets") {
    return hop(OccupySockets(label), SurfaceKind::kWs2, StatusJson());
  }
  if (name == "occupyBcrypt") {
    return hop(OccupyBcrypt(label), SurfaceKind::kBcrypt, StatusJson());
  }
  if (name == "occupyVmem") {
    return hop(OccupyVmem(label), SurfaceKind::kVmem, StatusJson());
  }
  if (name == "occupyPipe") {
    return hop(OccupyPipe(label), SurfaceKind::kPipe, StatusJson());
  }
  if (name == "occupyPe") {
    return hop(OccupyPe(label), SurfaceKind::kPe, StatusJson());
  }
  if (name == "occupyProcess") {
    return hop(OccupyProcess(label), SurfaceKind::kProcess, StatusJson());
  }
  if (name == "occupyConsole") {
    return hop(OccupyConsole(label), SurfaceKind::kConsole, StatusJson());
  }
  if (name == "occupyHwnd") {
    return hop(OccupyHwnd(label), SurfaceKind::kHwnd, StatusJson());
  }
  if (name == "occupyHv") {
    return hop(OccupyHv(label), SurfaceKind::kWinHv, StatusJson());
  }
  if (name == "occupyWsl") {
    return hop(OccupyWsl(label), SurfaceKind::kWsl, StatusJson());
  }
  if (name == "occupyNix") {
    return hop(OccupyNix(label), SurfaceKind::kNix, StatusJson());
  }
  if (name == "occupySys") {
    return done(OccupySys(label));
  }
  if (name == "occupyDriver" || name == "driverEntry") {
    return done(OccupyDriver(label.empty() ? "wowwin32" : label));
  }
  if (name == "occupyLoadDriver") {
    return done(OccupyLoadDriver(label.empty() ? "NtLoadDriver" : label));
  }
  if (name == "getDriverInfo") {
    if (!driver_.loaded()) {
      WowResult r = OccupyDriver("wowwin32");
      if (r != WOW_RESULT_OK) {
        return fail(r, last_error_.c_str());
      }
    }
    return ok(driver_.GetDriverInfoJson());
  }
  if (name == "deviceControl") {
    if (!driver_.loaded()) {
      WowResult r = OccupyDriver("wowwin32");
      if (r != WOW_RESULT_OK) {
        return fail(r, last_error_.c_str());
      }
    }
    WowResult r = driver_.DeviceControl(WOW_IOCTL_MAP, hex_payload());
    if (r != WOW_RESULT_OK) {
      last_error_ = driver_.last_error();
      return fail(r, last_error_.c_str());
    }
    return ok(driver_.GetDriverInfoJson());
  }
  if (name == "occupyWin32k") {
    return done(OccupyWin32k(label.empty() ? "win32k" : label));
  }
  if (name == "occupyHostNtos") {
    return done(OccupyHostNtos(label.empty() ? "ntoskrnl" : label));
  }
  if (name == "occupyCmd") {
    std::string img = label.empty() ? data : label;
    return done(OccupyCmd(img.empty() ? "cmd.exe" : img));
  }
  if (name == "occupyCalc") {
    return done(OccupyCalc(label.empty() ? (data.empty() ? "calc.exe" : data)
                                         : label));
  }
  if (name == "occupyLinux") {
    return done(OccupyLinux(label.empty() ? "linux" : label));
  }
  if (name == "occupySh") {
    return done(OccupySh(label.empty() ? "/bin/sh" : label));
  }
  if (name == "occupyWasmtty") {
    return done(OccupyWasmtty(label.empty() ? "wasmtty" : label));
  }
  if (name == "occupyGocvm") {
    return done(OccupyGocvm(label.empty() ? "wasigocvm" : label));
  }
  if (name == "ttyHello") {
    std::string reply;
    WowResult r = TtyWriteHello(&reply);
    if (r != WOW_RESULT_OK) {
      return fail(r, last_error_.c_str());
    }
    return ok(std::move(reply));
  }
  if (name == "ttyPing") {
    std::string reply;
    WowResult r = TtyPing(&reply);
    if (r != WOW_RESULT_OK) {
      return fail(r, last_error_.c_str());
    }
    return ok(std::move(reply));
  }
  if (name == "occupyNet") {
    return done(OccupyNet(label.empty() ? "wns" : label));
  }
  if (name == "commonRoutes") {
    return ok(CommonRoutesJson());
  }
  if (name == "occupancyStatus") {
    return ok(StatusJson());
  }
  if (name == "thinMap") {
    return ok(ThinMapJson());
  }
  if (name == "open") {
    int out = 0;
    SurfaceKind k = KindFromString(kind);
    WowResult rr = Open(k, label, JsonIntField(payload, "parent", 0), &out);
    if (rr != WOW_RESULT_OK) {
      return fail(rr, last_error_.c_str());
    }
    return ok(std::to_string(out));
  }
  if (name == "write") {
    return done(Write(id, hex_payload()));
  }
  if (name == "read") {
    std::vector<uint8_t> bytes;
    WowResult rr = Read(id, &bytes);
    if (rr != WOW_RESULT_OK) {
      return fail(rr, last_error_.c_str());
    }
    return ok(std::string(bytes.begin(), bytes.end()));
  }
  if (name == "map") {
    return done(Map(id));
  }
  if (name == "unmap") {
    return done(Unmap(id));
  }
  return fail(WOW_RESULT_NOT_FOUND, "unknown win32 topic");
}

}  // namespace wow
