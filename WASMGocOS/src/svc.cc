#include "gocos/jitrt.hpp"
#include "gocos/host.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "src/runtime/vfs.h"

namespace wasmgocos {
namespace {

enum class Kind { kInit, kDaemon, kSched };

struct Svc {
  std::string name;
  Kind kind = Kind::kInit;
  int order = 0;
  int every = 0;
  int ticks = 0;
  std::string body;
  std::string state = "stopped";
  std::string out;
  int job = 0;  // a daemon whose body started a background job
};

std::vector<Svc>& Table() {
  static std::vector<Svc> table;
  return table;
}

std::vector<std::string> Fields(const std::string& args) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= args.size()) {
    size_t sep = args.find('\x1f', start);
    if (sep == std::string::npos) {
      out.push_back(args.substr(start));
      break;
    }
    out.push_back(args.substr(start, sep - start));
    start = sep + 1;
  }
  return out;
}

bool NameOk(const std::string& name) {
  return !name.empty() && name.find(' ') == std::string::npos && name.find('/') == std::string::npos;
}

Svc* Find(const std::string& name) {
  for (Svc& svc : Table()) {
    if (svc.name == name) return &svc;
  }
  return nullptr;
}

const char* KindName(Kind kind) {
  if (kind == Kind::kDaemon) return "daemon";
  if (kind == Kind::kSched) return "sched";
  return "init";
}

std::string Line(const Svc& svc) {
  std::string line = "svc name=" + svc.name + " kind=" + KindName(svc.kind) + " state=" + svc.state;
  if (svc.kind == Kind::kInit) line += " order=" + std::to_string(svc.order);
  if (svc.kind == Kind::kSched) line += " every=" + std::to_string(svc.every);
  return line;
}

std::string Run(Svc& svc) {
  svc.out = jit_sh(svc.body.c_str());
  return "start " + svc.name + " out=[" + svc.out + "]";
}

std::string Vfs(const std::string& args) {
  return wasmturbo::Vfs::Origin().Call(ProcessRuntime(), KernelPid(), args);
}

void Publish(const Svc& svc) {
  std::string path;
  std::string body = svc.body;
  if (svc.kind == Kind::kInit) path = "/etc/rc.d/" + svc.name;
  else if (svc.kind == Kind::kDaemon) path = "/etc/daemon/" + svc.name;
  else {
    path = "/etc/sched/" + svc.name;
    body = std::to_string(svc.every) + " " + svc.body;
  }
  Vfs(std::string("write") + '\x1f' + path + '\x1f' + body);
}

void DiscoverDir(Kind kind, const char* dir, int order_base) {
  std::string listing = Vfs(std::string("ls") + '\x1f' + dir);
  if (listing == "walk miss" || listing == "empty") return;
  int order = order_base;
  size_t start = 0;
  while (start < listing.size()) {
    size_t end = listing.find('\n', start);
    if (end == std::string::npos) end = listing.size();
    std::string name = listing.substr(start, end - start);
    if (NameOk(name) && Find(name) == nullptr) {
      std::string body = Vfs(std::string("read") + '\x1f' + dir + "/" + name);
      int every = 0;
      if (kind == Kind::kSched) {
        size_t sp = body.find(' ');
        if (sp != std::string::npos && sp > 0) {
          every = std::atoi(body.substr(0, sp).c_str());
          body = body.substr(sp + 1);
        } else {
          body.clear();
        }
      }
      if (!body.empty() && body != "walk miss" && body != "ipt miss" &&
          (kind != Kind::kSched || every >= 1)) {
        Svc svc;
        svc.name = name;
        svc.kind = kind;
        svc.order = order;
        svc.every = every;
        svc.body = body;
        Table().push_back(std::move(svc));
      }
    }
    ++order;
    if (end == listing.size()) break;
    start = end + 1;
  }
}

void Discover() { DiscoverDir(Kind::kInit, "/etc/rc.d", 100); }

void DiscoverRest() {
  DiscoverDir(Kind::kDaemon, "/etc/daemon", 0);
  DiscoverDir(Kind::kSched, "/etc/sched", 0);
}

std::string Add(Kind kind, const std::string& name, int order, int every, const std::string& body) {
  if (!NameOk(name) || body.empty() || Find(name) != nullptr) return "svc miss";
  if (kind == Kind::kSched && every < 1) return "svc miss";
  Svc svc;
  svc.name = name;
  svc.kind = kind;
  svc.order = order;
  svc.every = every;
  svc.body = body;
  Table().push_back(std::move(svc));
  Publish(Table().back());
  return Line(Table().back());
}

std::string StartOne(Svc& svc) {
  if (svc.kind == Kind::kDaemon && svc.state == "running") return "svc miss";
  std::string reply = Run(svc);
  if (svc.kind == Kind::kDaemon) {
    svc.state = "running";
    // `ctr run ... &` replies `job N ...`: the daemon is that job.
    size_t at = svc.out.find("job ");
    svc.job = at == 0 ? std::atoi(svc.out.c_str() + 4) : 0;
  }
  else if (svc.kind == Kind::kSched) svc.state = "waiting";
  else svc.state = "started";
  return reply;
}

std::string Start(const std::string& name) {
  if (name.empty()) {
    Discover();
    std::vector<Svc*> inits;
    for (Svc& svc : Table()) {
      if (svc.kind == Kind::kInit) inits.push_back(&svc);
    }
    std::sort(inits.begin(), inits.end(), [](const Svc* a, const Svc* b) { return a->order < b->order; });
    if (inits.empty()) return "svc=0";
    std::string out;
    for (Svc* svc : inits) {
      if (!out.empty()) out.push_back('\n');
      out += StartOne(*svc);
    }
    return out;
  }
  DiscoverRest();
  Svc* svc = Find(name);
  if (svc == nullptr) return "svc miss";
  return StartOne(*svc);
}

std::string Stop(const std::string& name) {
  Svc* svc = Find(name);
  if (svc == nullptr || svc->state == "stopped") return "svc miss";
  svc->state = "stopped";
  if (svc->job != 0) {
    std::string id = std::to_string(svc->job);
    svc->job = 0;
    jit_jobs((std::string("kill") + '\x1f' + id).c_str());
    std::string waited = jit_jobs((std::string("wait") + '\x1f' + id).c_str());
    return "stop " + name + " " + waited;
  }
  return "stop " + name;
}

std::string Tick() {
  std::string out;
  for (Svc& svc : Table()) {
    if (svc.kind != Kind::kSched || svc.state != "waiting") continue;
    ++svc.ticks;
    if (svc.every < 1 || (svc.ticks % svc.every) != 0) continue;
    std::string ran = Run(svc);
    if (!out.empty()) out.push_back('\n');
    out += "tick " + svc.name + " out=[" + svc.out + "]";
    (void)ran;
  }
  if (out.empty()) return "tick=0";
  return out;
}

}  // namespace

std::string jit_svc(const char* args) {
  std::vector<std::string> fields = Fields(args != nullptr ? args : "");
  if (fields.empty() || fields[0].empty() || fields[0] == "list") {
    Discover();
    DiscoverRest();
    if (Table().empty()) return "svc=0";
    std::string out;
    for (const Svc& svc : Table()) {
      out += Line(svc);
      out.push_back('\n');
    }
    return out;
  }
  const std::string& verb = fields[0];
  if (verb == "rc" && fields.size() >= 4)
    return Add(Kind::kInit, fields[1], std::atoi(fields[2].c_str()), 0, fields[3]);
  if (verb == "daemon" && fields.size() >= 3) return Add(Kind::kDaemon, fields[1], 0, 0, fields[2]);
  if (verb == "sched" && fields.size() >= 4)
    return Add(Kind::kSched, fields[1], 0, std::atoi(fields[2].c_str()), fields[3]);
  if (verb == "init" || verb == "start") return Start(verb == "init" ? "" : (fields.size() > 1 ? fields[1] : ""));
  if (verb == "stop") return Stop(fields.size() > 1 ? fields[1] : "");
  if (verb == "tick") return Tick();
  return "svc miss";
}

}  // namespace wasmgocos
