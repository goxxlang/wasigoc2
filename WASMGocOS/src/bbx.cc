#include "gocos/jitrt.hpp"
#include "gocos/host.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "src/runtime/mss.h"
#include "src/runtime/vfs.h"

namespace wasmgocos {
namespace {

// Applet ordinals from ~/project_lovelace tools/mojotool/bbx. The table
// is seated on procfs at these ordinals. File applets walk the origin
// filesystem. spawn and sh call CreateProcessW and seat the child's pid.
constexpr uint32_t kEcho = 0x72000001u;
constexpr uint32_t kLs = 0x72000002u;
constexpr uint32_t kCat = 0x72000003u;
constexpr uint32_t kSh = 0x72000004u;
constexpr uint32_t kPs = 0x72000005u;
constexpr uint32_t kSpawn = 0x72000006u;
constexpr uint32_t kTrue = 0x72000007u;
constexpr uint32_t kFalse = 0x72000008u;
constexpr uint32_t kPwd = 0x72000009u;
constexpr uint32_t kMkdir = 0x7200000au;
constexpr uint32_t kRm = 0x7200000bu;
constexpr uint32_t kCp = 0x7200000cu;
constexpr uint32_t kMv = 0x7200000du;
constexpr uint32_t kWc = 0x7200000eu;
constexpr uint32_t kEnv = 0x7200000fu;
constexpr uint32_t kGetProcessInfo = 520932818u;

static_assert(kEcho == 1912602625u);
static_assert(kLs == 1912602626u);
static_assert(kSpawn == 1912602630u);

struct Applet {
  const char* name;
  uint32_t ordinal;
  const char* kind;
};

constexpr Applet kApplets[] = {
    {"echo", kEcho, "builtin"},
    {"ls", kLs, "builtin"},
    {"cat", kCat, "builtin"},
    {"sh", kSh, "spawn"},
    {"ps", kPs, "builtin"},
    {"spawn", kSpawn, "spawn"},
    {"true", kTrue, "builtin"},
    {"false", kFalse, "builtin"},
    {"pwd", kPwd, "builtin"},
    {"mkdir", kMkdir, "builtin"},
    {"rm", kRm, "builtin"},
    {"cp", kCp, "builtin"},
    {"mv", kMv, "builtin"},
    {"wc", kWc, "builtin"},
    {"env", kEnv, "builtin"},
    {"getProcessInfo", kGetProcessInfo, "wpr"},
};

constexpr int kAppletCount = static_cast<int>(sizeof(kApplets) / sizeof(kApplets[0]));

std::string Trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
  return std::string(s);
}

void SplitArgv(std::string_view argv, std::string* applet, std::string* rest) {
  std::string t = Trim(argv);
  size_t sp = t.find(' ');
  if (sp == std::string::npos) {
    *applet = t;
    *rest = "";
    return;
  }
  *applet = t.substr(0, sp);
  *rest = Trim(t.substr(sp + 1));
}

const Applet* FindByName(std::string_view name) {
  for (const Applet& a : kApplets) {
    if (name == a.name) return &a;
  }
  return nullptr;
}

const Applet* FindByOrdinal(uint32_t ord) {
  for (const Applet& a : kApplets) {
    if (a.ordinal == ord) return &a;
  }
  return nullptr;
}

std::string Abs(std::string_view path) {
  std::string p = Trim(path);
  if (p.empty() || p == ".") return "/";
  if (p.front() != '/') p.insert(p.begin(), '/');
  return p;
}

std::string VfsCall(const std::string& args) {
  return wasmturbo::Vfs::Origin().Call(ProcessRuntime(), KernelPid(), args);
}

std::string Vfs2(const char* verb, const std::string& path) {
  std::string args = verb;
  args.push_back('\x1f');
  args += path;
  return VfsCall(args);
}

std::string Vfs3(const char* verb, const std::string& path, const std::string& body) {
  std::string args = verb;
  args.push_back('\x1f');
  args += path;
  args.push_back('\x1f');
  args += body;
  return VfsCall(args);
}

std::string Dispatch(uint32_t ordinal, std::string_view args);

std::string AppletEcho(std::string_view args) { return std::string(args); }

std::string AppletPwd() { return "/"; }

std::string AppletLs(std::string_view path) {
  std::string listing = Vfs2("ls", Abs(path));
  if (listing == "walk miss") return "ls=ERROR";
  if (listing == "empty") return "ls=0 []";
  std::string names;
  int n = 0;
  size_t start = 0;
  while (start < listing.size()) {
    size_t end = listing.find('\n', start);
    if (end == std::string::npos) end = listing.size();
    std::string name = listing.substr(start, end - start);
    if (!name.empty()) {
      if (n) names += " ";
      names += name;
      ++n;
    }
    if (end == listing.size()) break;
    start = end + 1;
  }
  return "ls=" + std::to_string(n) + " [" + names + "]";
}

std::string AppletCat(std::string_view path) {
  if (Trim(path).empty()) return "cat=EINVAL";
  std::string body = Vfs2("read", Abs(path));
  if (body == "walk miss" || body == "ipt miss") return "cat=ERROR:open";
  return body;
}

std::string AppletMkdir(std::string_view path) {
  if (Trim(path).empty()) return "mkdir=EINVAL";
  std::string reply = Vfs2("mkdir", Abs(path));
  if (reply.rfind("dir ", 0) == 0) return "mkdir=ok";
  return "mkdir=ERROR";
}

std::string AppletRm(std::string_view path) {
  if (Trim(path).empty()) return "rm=EINVAL";
  std::string reply = Vfs2("rm", Abs(path));
  if (reply.rfind("rm ", 0) == 0) return "rm=ok";
  return "rm=ERROR";
}

std::string AppletCp(std::string_view args) {
  std::string src, dst;
  SplitArgv(args, &src, &dst);
  if (src.empty() || dst.empty()) return "cp=EINVAL:want=\"src dst\"";
  std::string body = Vfs2("read", Abs(src));
  if (body == "walk miss" || body == "ipt miss") return "cp=ERROR:1";
  std::string wrote = Vfs3("write", Abs(dst), body);
  if (wrote.rfind("write ", 0) != 0) return "cp=ERROR:1";
  return "cp=ok";
}

std::string AppletMv(std::string_view args) {
  std::string src, dst;
  SplitArgv(args, &src, &dst);
  if (src.empty() || dst.empty()) return "mv=EINVAL:want=\"src dst\"";
  std::string copied = AppletCp(std::string(src) + " " + std::string(dst));
  if (copied != "cp=ok") return "mv=ERROR:1";
  std::string removed = AppletRm(src);
  if (removed != "rm=ok") return "mv=ERROR:1";
  return "mv=ok";
}

std::string AppletWc(std::string_view path) {
  if (Trim(path).empty()) return "wc=EINVAL";
  std::string data = AppletCat(path);
  if (data == "cat=ERROR:open" || data == "cat=EINVAL") return "wc=ERROR:open";
  size_t lines = 0;
  size_t words = 0;
  bool in_word = false;
  for (char c : data) {
    if (c == '\n') ++lines;
    bool space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    if (!space && !in_word) ++words;
    in_word = !space;
  }
  return "wc=[lines=" + std::to_string(lines) + " words=" + std::to_string(words) +
         " bytes=" + std::to_string(data.size()) + "]";
}

std::string AppletEnv() {
  std::string block = jit_env("");
  int n = 0;
  for (char c : block)
    if (c == '\n') ++n;
  return "env=" + std::to_string(n);
}

std::string AppletPs() {
  std::string table = wasmturbo::MssQueue::Process().ProcTable();
  if (table == "empty") return "ps=real=0 viewer={}";
  int n = 0;
  std::string viewer;
  size_t start = 0;
  while (start < table.size()) {
    size_t end = table.find('\n', start);
    if (end == std::string::npos) end = table.size();
    std::string line = table.substr(start, end - start);
    if (!line.empty()) {
      size_t sp1 = line.find(' ');
      size_t sp2 = sp1 == std::string::npos ? std::string::npos : line.find(' ', sp1 + 1);
      size_t sp3 = sp2 == std::string::npos ? std::string::npos : line.find(' ', sp2 + 1);
      if (sp2 != std::string::npos && sp3 != std::string::npos) {
        if (n) viewer += " ";
        viewer += line.substr(sp2 + 1, sp3 - sp2 - 1);
        ++n;
      }
    }
    if (end == table.size()) break;
    start = end + 1;
  }
  return "ps=real=" + std::to_string(n) + " viewer=" + viewer;
}

std::string SpawnCapture(std::string_view cmdline) {
  std::string cmd(cmdline);
  if (cmd.empty()) cmd = "cmd.exe /c echo bbx-spawn";
  GocProc launched = goc_launch(cmd.c_str(), true);
  if (!launched.error.empty()) {
    return "spawn=ERROR:CreateProcess cmdline=[" + cmd + "] err=" + launched.error;
  }
  std::string out = launched.output;
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return "spawn=ok pid=" + std::to_string(launched.pid) + " viewer=" +
         std::to_string(launched.viewer) + " running=0 exit=" +
         std::to_string(launched.exit_code) + " owned=1 out=[" + out + "]";
}

std::string AppletSh(std::string_view args) {
  std::string cmd = "cmd.exe /c ";
  if (Trim(args).empty()) cmd += "echo bbx-sh";
  else cmd += std::string(args);
  return SpawnCapture(cmd);
}

std::string Dispatch(uint32_t ordinal, std::string_view args) {
  const Applet* a = FindByOrdinal(ordinal);
  if (a == nullptr) return "ENOSYS:ordinal=" + std::to_string(ordinal);
  if (ordinal == kEcho) return AppletEcho(args);
  if (ordinal == kLs) return AppletLs(args);
  if (ordinal == kCat) return AppletCat(args);
  if (ordinal == kSh) return AppletSh(args);
  if (ordinal == kPs || ordinal == kGetProcessInfo) return AppletPs();
  if (ordinal == kSpawn) return SpawnCapture(args);
  if (ordinal == kTrue) return "true=0";
  if (ordinal == kFalse) return "false=1";
  if (ordinal == kPwd) return AppletPwd();
  if (ordinal == kMkdir) return AppletMkdir(args);
  if (ordinal == kRm) return AppletRm(args);
  if (ordinal == kCp) return AppletCp(args);
  if (ordinal == kMv) return AppletMv(args);
  if (ordinal == kWc) return AppletWc(args);
  if (ordinal == kEnv) return AppletEnv();
  return "ENOSYS:applet=" + std::string(a->name);
}

int OnApplet(void* self, char* out, size_t cap) {
  if (self == nullptr || out == nullptr || cap == 0) return -1;
  const Applet* a = static_cast<const Applet*>(self);
  std::string reply = Dispatch(a->ordinal, "");
  size_t n = reply.size();
  if (n >= cap) n = cap - 1;
  std::memcpy(out, reply.data(), n);
  out[n] = 0;
  return 0;
}

void Ensure() {
  static bool once = false;
  if (once) return;
  once = true;
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  for (const Applet& a : kApplets) {
    queue.Seat(ProcessRuntime(), a.name, a.ordinal, OnApplet, const_cast<Applet*>(&a));
  }
}

std::string Table() {
  Ensure();
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  std::string o;
  for (const Applet& a : kApplets) {
    if (!o.empty()) o += " ";
    wasmturbo::MssEntry entry;
    uint32_t looked = 0;
    if (queue.ByOrdinal(a.ordinal, &entry) && entry.kind == wasmturbo::MssKind::kService) {
      looked = entry.ordinal;
    }
    o += std::string(a.kind) + ":" + a.name + "=" + std::to_string(a.ordinal) +
         "->catalog=" + std::to_string(looked);
  }
  return o;
}

std::string Trap(int32_t ordinal) {
  Ensure();
  if (ordinal == 0) return "EINVAL:ordinal=0";
  return Dispatch(static_cast<uint32_t>(ordinal), "");
}

std::string Run(std::string argv) {
  Ensure();
  if (argv.empty()) return "EINVAL:empty";
  std::string applet, rest;
  SplitArgv(argv, &applet, &rest);
  const Applet* a = FindByName(applet);
  if (a == nullptr) return "ENOSYS:applet=" + applet;
  return Dispatch(a->ordinal, rest);
}

std::string RegisterAll() {
  Ensure();
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  int n = 0;
  for (const Applet& a : kApplets) {
    wasmturbo::MssEntry entry;
    if (queue.ByOrdinal(a.ordinal, &entry) && entry.kind == wasmturbo::MssKind::kService &&
        entry.ordinal == a.ordinal) {
      ++n;
    }
  }
  wasmturbo::MssEntry echo;
  int owns = queue.ByOrdinal(kEcho, &echo) && echo.kind == wasmturbo::MssKind::kService ? 1 : 0;
  return "registered=" + std::to_string(n) + "/" + std::to_string(kAppletCount) +
         " echo=" + std::to_string(kEcho) + " spawn=" + std::to_string(kSpawn) +
         " getProcessInfo=" + std::to_string(kGetProcessInfo) + " owns_echo=" + std::to_string(owns);
}

std::string Lower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  }
  return s;
}

}  // namespace

std::string jit_bbx(const char* args) {
  std::string spec = args != nullptr ? args : "";
  if (spec.empty()) return Table();
  size_t sep = spec.find('\x1f');
  std::string verb = Lower(sep == std::string::npos ? spec : spec.substr(0, sep));
  std::string rest = sep == std::string::npos ? "" : spec.substr(sep + 1);
  if (verb == "table") return Table();
  if (verb == "trap") {
    if (rest.empty()) return Trap(0);
    return Trap(static_cast<int32_t>(std::strtol(rest.c_str(), nullptr, 0)));
  }
  if (verb == "run") return Run(rest);
  if (verb == "ps") {
    Ensure();
    return AppletPs();
  }
  if (verb == "register" || verb == "registerall") return RegisterAll();
  if (verb == "spawn" || verb == "spawndemo") {
    Ensure();
    return SpawnCapture("cmd.exe /c echo wpr-bbx");
  }
  return "ENOSYS:method=" + verb;
}

}  // namespace wasmgocos
