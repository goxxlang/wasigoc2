#include "gocos/jitrt.hpp"
#include "gocos/host.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "src/runtime/mss.h"
#include "src/runtime/vfs.h"

// #g: the vGPU, when the module links one (HeapVM: heapvm/vgpu/vgpu.cc).
// `path` is below #g; the reply's length, or -1 for a name not there.
extern "C" int gocos_vgpu(const char* verb, const char* path, const char* data, unsigned len, char* out,
                          unsigned cap) __attribute__((weak));

namespace wasmgocos {
namespace {

struct PipeBuf {
  std::string data;
  int readers = 0;
  int writers = 0;
};

struct OpenFd {
  std::string path;
  bool cons = false;
  bool read = true;
  bool write = true;
  size_t offset = 0;
  std::shared_ptr<PipeBuf> pipe;
  bool pipe_write = false;
  bool env = false;
  bool env_dir = false;
  std::string env_name;
  bool proc = false;
  bool proc_dir = false;
  std::string proc_name;
  bool unil = false;
  bool unil_dir = false;
  std::string unil_name;
  std::string unil_region;
  bool nul = false;
  bool clock = false;
  bool krnl = false;
  std::string krnl_name;
  bool svc = false;
  bool svc_dir = false;
  std::string svc_name;
  bool gpu = false;
  std::string gpu_path;
  std::string gpu_read;
  bool gpu_have = false;
  bool union_dir = false;
  std::string union_at;
};

struct Mount {
  std::string at;
  std::string device;
  char flag = 0;
};

struct Job {
  int id = 0;
  uint32_t pid = 0;
  std::string image;
  std::string proc;
  std::string thread;
  bool done = false;
  int exit_code = 0;
  std::string output;
  // A background guest (`ctr run ... &`): the container's name. Its
  // state, wait, and kill are the container's (src/ctr.cc).
  std::string guest;
};

struct Session {
  std::string cwd = "/";
  std::string cons;
  std::map<int, OpenFd> fds;
  std::map<std::string, std::string> env;
  std::vector<Job> jobs;
  std::vector<Mount> mounts;
  int next_job = 1;
  int status = 0;
  bool ready = false;
};

Session& Sess() {
  static Session session;
  return session;
}

void EnsureFds() {
  Session& session = Sess();
  if (session.ready) return;
  session.ready = true;
  for (int fd = 0; fd < 3; ++fd) {
    OpenFd slot;
    slot.path = "#c/cons";
    slot.cons = true;
    session.fds[fd] = slot;
  }
  session.env["PATH"] = "/proc";
}

void ConsAppend(const std::string& text) {
  if (text.empty() || text.rfind("error:", 0) == 0) return;
  Session& session = Sess();
  session.cons += text;
  if (session.cons.back() != '\n') session.cons.push_back('\n');
}

}  // namespace

void jit_cons(const std::string& text) { ConsAppend(text); }

namespace {

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

bool EnvFile(const std::string& path, std::string* name, bool* dir) {
  if (path == "#e") {
    *dir = true;
    name->clear();
    return true;
  }
  if (path.rfind("#e/", 0) != 0) return false;
  std::string n = path.substr(3);
  if (n.empty() || n.find('/') != std::string::npos || n.find('=') != std::string::npos) return false;
  *dir = false;
  *name = n;
  return true;
}

bool ProcFile(const std::string& path, std::string* name, bool* dir) {
  if (path == "#p") {
    *dir = true;
    name->clear();
    return true;
  }
  if (path.rfind("#p/", 0) != 0) return false;
  std::string n = path.substr(3);
  if (n.empty() || n.find('/') != std::string::npos) return false;
  if (n != "self") {
    for (char c : n)
      if (c < '0' || c > '9') return false;
  }
  *dir = false;
  *name = n;
  return true;
}

std::string ProcField(const std::string& line, int nth) {
  int seen = 0;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && line[i] == ' ') ++i;
    size_t start = i;
    while (i < line.size() && line[i] != ' ') ++i;
    if (start == i) break;
    if (seen == nth) return line.substr(start, i - start);
    ++seen;
  }
  return "";
}

std::string ProcShow() {
  std::string table = jit_proctable("");
  if (table == "empty") return table;
  std::string out;
  size_t start = 0;
  while (start < table.size()) {
    size_t end = table.find('\n', start);
    if (end == std::string::npos) end = table.size();
    std::string line = table.substr(start, end - start);
    std::string pid = ProcField(line, 1);
    std::string full;
    if (!pid.empty())
      full = jit_ctr_line(static_cast<uint32_t>(std::strtoul(pid.c_str(), nullptr, 10)));
    if (!full.empty()) {
      out += full;
    } else if (!line.empty()) {
      out += line;
    }
    out.push_back('\n');
    if (end == table.size()) break;
    start = end + 1;
  }
  return out;
}

std::string ProcImage(const std::string& pid) {
  std::string table = jit_proctable("");
  size_t start = 0;
  while (start < table.size()) {
    size_t end = table.find('\n', start);
    if (end == std::string::npos) end = table.size();
    std::string line = table.substr(start, end - start);
    if (ProcField(line, 1) == pid) {
      std::string image = ProcField(line, 2);
      if (!image.empty()) return image;
    }
    if (end == table.size()) break;
    start = end + 1;
  }
  return "walk miss";
}

bool UnilFind(const std::string& name, uint32_t* pid, std::string* line) {
  std::string table = jit_ctr("");
  size_t start = 0;
  while (start < table.size()) {
    size_t end = table.find('\n', start);
    if (end == std::string::npos) end = table.size();
    std::string row = table.substr(start, end - start);
    std::string key = " name=" + name;
    size_t at = row.find(key);
    bool hit = at != std::string::npos && (at + key.size() == row.size() || row[at + key.size()] == ' ');
    if (hit) {
      if (line != nullptr) *line = row;
      if (pid != nullptr) {
        size_t p = row.find("pid=");
        *pid = p == std::string::npos
                   ? 0
                   : static_cast<uint32_t>(std::strtoul(row.c_str() + p + 4, nullptr, 10));
      }
      return true;
    }
    if (end == table.size()) break;
    start = end + 1;
  }
  return false;
}

std::string UnilNames() {
  std::string table = jit_ctr("");
  if (table == "ctr=0" || table.empty()) return "";
  std::string out;
  size_t start = 0;
  while (start < table.size()) {
    size_t end = table.find('\n', start);
    if (end == std::string::npos) end = table.size();
    std::string row = table.substr(start, end - start);
    size_t at = row.find(" name=");
    if (at != std::string::npos) {
      size_t b = at + 6;
      size_t e = row.find(' ', b);
      if (e == std::string::npos) e = row.size();
      out.append(row, b, e - b);
      out.push_back('\n');
    }
    if (end == table.size()) break;
    start = end + 1;
  }
  return out;
}

bool IsDev(const std::string& path) {
  return path == "#c" || path.rfind("#c/", 0) == 0 || path == "#e" || path.rfind("#e/", 0) == 0 ||
         path == "#p" || path.rfind("#p/", 0) == 0 || path == "#u" || path.rfind("#u/", 0) == 0 ||
         path == "#s" || path.rfind("#s/", 0) == 0 || path == "#k" || path.rfind("#k/", 0) == 0 ||
         path == "#g" || path.rfind("#g/", 0) == 0 || path == "#n/nul" || path == "#t";
}

// A #g name, read or written through the vGPU.
std::string GpuCall(const char* verb, const std::string& path, const std::string& data, bool* ok) {
  *ok = false;
  if (!gocos_vgpu) return "walk miss";
  std::string buf(256 * 1024, '\0');
  int n = gocos_vgpu(verb, path.c_str(), data.data(), static_cast<unsigned>(data.size()), buf.data(),
                     static_cast<unsigned>(buf.size()));
  if (n < 0) return "walk miss";
  *ok = true;
  buf.resize(static_cast<size_t>(n));
  return buf;
}

// The #g names: which exist, and which take writes.
bool GpuName(const std::string& rel, bool* writable) {
  *writable = rel == "ctl" || rel == "prog" || (rel.rfind("buf/", 0) == 0 && rel.size() > 4);
  return rel.empty() || rel == "dxg" || rel == "buf" || *writable;
}

std::string DosName(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
  if (!base.empty() && base.back() == ':') base.pop_back();
  for (char& c : base)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  if (base == "con") return "#c/cons";
  if (base == "nul") return "#n/nul";
  return "";
}

std::string JoinCwd(const std::string& path) {
  if (path.empty() || path.front() != '/') {
    if (Sess().cwd == "/") return "/" + path;
    return Sess().cwd + "/" + path;
  }
  return path;
}

std::vector<std::string> UnionWalk(const std::string& path) {
  std::string joined = JoinCwd(path);
  size_t best = 0;
  bool hit = false;
  for (const Mount& mount : Sess().mounts) {
    if (joined == mount.at || joined.rfind(mount.at + "/", 0) == 0) {
      if (!hit || mount.at.size() > best) {
        best = mount.at.size();
        hit = true;
      }
    }
  }
  std::vector<std::string> out;
  if (!hit) {
    std::string dos = DosName(joined);
    if (!dos.empty()) out.push_back(dos);
    else if (IsDev(joined)) out.push_back(joined);
    else out.push_back(wasmturbo::Vfs::Canon(joined));
    return out;
  }
  for (const Mount& mount : Sess().mounts) {
    if (mount.at.size() != best) continue;
    if (joined != mount.at && joined.rfind(mount.at + "/", 0) != 0) continue;
    std::string got = joined == mount.at ? mount.device : mount.device + joined.substr(mount.at.size());
    if (IsDev(got)) out.push_back(got);
    else out.push_back(wasmturbo::Vfs::Canon(got));
  }
  return out;
}

bool NameThere(const std::string& path) {
  if (path == "#c" || path == "#c/cons" || path == "#n/nul" || path == "#t") return true;
  if (path == "#e" || path == "#p" || path == "#s" || path == "#k" || path == "#u") return true;
  if (path == "#g" || path.rfind("#g/", 0) == 0) {
    bool w = false;
    return gocos_vgpu != nullptr && GpuName(path.size() > 3 ? path.substr(3) : std::string(), &w);
  }
  if (path.rfind("#e/", 0) == 0) {
    std::string name = path.substr(3);
    return name.find('/') == std::string::npos && Sess().env.find(name) != Sess().env.end();
  }
  if (path.rfind("#p/", 0) == 0 || path.rfind("#s/", 0) == 0 || path.rfind("#k/", 0) == 0 ||
      path.rfind("#u/", 0) == 0)
    return true;
  std::string body = Vfs2("read", path);
  if (body != "walk miss" && body != "ipt miss") return true;
  std::string listing = Vfs2("ls", path);
  return listing != "walk miss" && listing != "empty";
}

std::string Resolve(const std::string& path) {
  std::string dos = DosName(path);
  if (!dos.empty()) return dos;
  if (IsDev(path)) return path;
  std::vector<std::string> cands = UnionWalk(path);
  if (cands.size() <= 1) return cands.empty() ? wasmturbo::Vfs::Canon(JoinCwd(path)) : cands[0];
  for (const std::string& cand : cands) {
    if (NameThere(cand)) return cand;
  }
  return cands[0];
}

std::string ImageOf(const char* args) {
  std::string app, rest;
  split1f(args ? args : "", &app, &rest);
  std::string line = app;
  if (!rest.empty()) {
    std::string cmd, cwd;
    split1f(rest.c_str(), &cmd, &cwd);
    if (!cmd.empty()) line = cmd;
  }
  size_t sp = line.find(' ');
  if (sp != std::string::npos) line.resize(sp);
  if (line.empty()) return "proc";
  return line;
}

uint32_t Seat(uint32_t pid, const std::string& image) {
  if (pid == 0) return 0;
  auto* rec = new uint32_t(pid);
  uint64_t address = reinterpret_cast<uint64_t>(rec);
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  if (wasmturbo::MssQueue::Classify(ProcessRuntime(), address) != wasmturbo::MssAddr::kRaw) {
    return 0;
  }
  wasmturbo::MssEntry seated;
  if (!queue.AddProcess(ProcessRuntime(), pid, address, image, &seated)) return 0;
  return seated.ordinal;
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

std::string ModeOf(const OpenFd& slot) {
  if (slot.read && slot.write) return "rw";
  if (slot.write) return "w";
  return "r";
}

int FindFd(const std::string& text) {
  if (text.empty()) return -1;
  char* end = nullptr;
  unsigned long n = std::strtoul(text.c_str(), &end, 10);
  if (end == text.c_str()) return -1;
  return static_cast<int>(n);
}

int AllocFd(Session& session) {
  int fd = 0;
  while (session.fds.find(fd) != session.fds.end()) ++fd;
  return fd;
}

int SeatJob(uint32_t pid, const std::string& image, const std::string& proc,
            const std::string& thread) {
  EnsureFds();
  Job job;
  job.id = Sess().next_job++;
  job.pid = pid;
  job.image = image;
  job.proc = proc;
  job.thread = thread;
  Sess().jobs.push_back(job);
  return job.id;
}

std::string JobLine(const Job& job) {
  std::string out = job.output;
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return "job " + std::to_string(job.id) + " exit=" + std::to_string(job.exit_code) + " out=[" +
         out + "]";
}

void Reap(Job& job) {
  if (job.done) return;
  if (!job.guest.empty()) {
    std::string line = jit_ctr((std::string("wait") + '\x1f' + job.guest).c_str());
    size_t at = line.find(" exit=");
    job.exit_code = at == std::string::npos ? 1 : std::atoi(line.c_str() + at + 6);
    unsigned pid = 0;
    if (std::sscanf(line.c_str(), "ctr pid=%u", &pid) == 1) {
      job.output = jit_ctr((std::string("read") + '\x1f' + std::to_string(pid) + '\x1f' + "stdout").c_str());
      if (job.output == "ctr miss") job.output.clear();
    } else {
      job.output = jit_ctr_job_output(job.guest);  // a background command
    }
    job.done = true;  // the run already wrote its stdout on the console
    return;
  }
  hv::k32("WaitForSingleObject", join1f(job.proc.c_str(), "4294967295").c_str());
  std::string out = hv::k32("GetProcessOutput", job.proc.c_str());
  std::string code = hv::k32("GetExitCodeProcess", job.proc.c_str());
  hv::k32("CloseHandle", job.proc.c_str());
  if (!job.thread.empty()) hv::k32("CloseHandle", job.thread.c_str());
  if (out.rfind("error:", 0) == 0) out.clear();
  if (code.rfind("error:", 0) != 0 && !code.empty()) {
    job.exit_code = static_cast<int>(std::strtol(code.c_str(), nullptr, 10));
  }
  job.output = out;
  job.done = true;
  ConsAppend(out);
}

std::string Kill(int id) {
  for (Job& job : Sess().jobs) {
    if (job.id != id) continue;
    if (job.done) return "job miss";
    if (!job.guest.empty()) {
      std::string reply = jit_ctr((std::string("kill") + '\x1f' + job.guest).c_str());
      if (reply.rfind("kill ", 0) != 0) return "job miss";
      return "kill " + std::to_string(id);
    }
    std::string args = job.proc;
    args.push_back('\x1f');
    args += "1";
    std::string reply = hv::k32("TerminateProcess", args.c_str());
    if (reply.rfind("error:", 0) == 0) return reply;
    return "kill " + std::to_string(id);
  }
  return "job miss";
}

}  // namespace

int jit_job_guest(const std::string& name, const std::string& image) {
  EnsureFds();
  Session& session = Sess();
  Job job;
  job.id = session.next_job++;
  job.image = image.empty() ? "ctr " + name : image;
  job.guest = name;
  session.jobs.push_back(job);
  return job.id;
}

std::string jit_walk(const std::string& path) {
  if (!plan9_local()) return plan9_hop("walk", path.c_str());
  EnsureFds();
  return Resolve(path);
}

namespace {

// The fd number in an `open` reply, or -1.
int OpenedFd(const std::string& reply) {
  if (reply.rfind("fd=", 0) != 0) return -1;
  return std::atoi(reply.c_str() + 3);
}

}  // namespace

bool jit_dev_read(const std::string& dev, std::string* out) {
  int fd = OpenedFd(jit_fd((std::string("open") + '\x1f' + dev + '\x1f' + "r").c_str()));
  if (fd < 0) return false;
  std::string num = std::to_string(fd);
  out->clear();
  bool ok = true;
  for (int i = 0; i < 64; ++i) {
    std::string part = jit_fd((std::string("read") + '\x1f' + num).c_str());
    if (part.empty()) break;
    if (part == "walk miss" || part == "env miss" || part == "fd miss" || part.rfind("error:", 0) == 0) {
      ok = i > 0;
      break;
    }
    *out += part;
  }
  jit_fd((std::string("close") + '\x1f' + num).c_str());
  return ok;
}

bool jit_dev_write(const std::string& dev, const std::string& bytes) {
  if (dev == "#n/nul") return true;
  int fd = OpenedFd(jit_fd((std::string("open") + '\x1f' + dev + '\x1f' + "w").c_str()));
  if (fd < 0) return false;
  std::string num = std::to_string(fd);
  std::string wrote = jit_fd((std::string("write") + '\x1f' + num + '\x1f' + bytes).c_str());
  jit_fd((std::string("close") + '\x1f' + num).c_str());
  return wrote.rfind("write ", 0) == 0;
}

// The session's k32 job object. Every child the session launches is in
// it, and it kills on close, so no child outlives the kernel process.
const std::string& SessionJobObject() {
  static const std::string job = [] {
    std::string h = hv::k32("CreateJobObjectW", "");
    if (h.rfind("error:", 0) == 0) return std::string();
    if (hv::k32("SetInformationJobObject", join1f(h.c_str(), "kill").c_str())
            .rfind("error:", 0) == 0) {
      hv::k32("CloseHandle", h.c_str());
      return std::string();
    }
    return h;
  }();
  return job;
}

GocProc goc_launch(const char* args, bool wait) {
  GocProc launched;
  std::string created = hv::k32("CreateProcessW", args ? args : "");
  if (created.rfind("error:", 0) == 0) {
    launched.error = created;
    return launched;
  }
  std::string pid_text, rest, proc, thread;
  split1f(created.c_str(), &pid_text, &rest);
  split1f(rest.c_str(), &proc, &thread);
  if (!thread.empty()) {
    size_t cut = thread.find('\x1f');
    if (cut != std::string::npos) thread.resize(cut);
  }
  if (!SessionJobObject().empty())
    hv::k32("AssignProcessToJobObject", join1f(SessionJobObject().c_str(), proc.c_str()).c_str());
  launched.pid = static_cast<uint32_t>(std::strtoul(pid_text.c_str(), nullptr, 10));
  launched.viewer = Seat(launched.pid, ImageOf(args));
  if (!wait) {
    launched.job = SeatJob(launched.pid, ImageOf(args), proc, thread);
    return launched;
  }
  hv::k32("WaitForSingleObject", join1f(proc.c_str(), "4294967295").c_str());
  std::string out = hv::k32("GetProcessOutput", proc.c_str());
  std::string code = hv::k32("GetExitCodeProcess", proc.c_str());
  hv::k32("CloseHandle", proc.c_str());
  if (!thread.empty()) hv::k32("CloseHandle", thread.c_str());
  if (out.rfind("error:", 0) == 0) out.clear();
  if (code.rfind("error:", 0) != 0 && !code.empty()) {
    launched.exit_code = static_cast<int>(std::strtol(code.c_str(), nullptr, 10));
  }
  launched.output = out;
  ConsAppend(out);
  return launched;
}

std::string jit_jobs(const char* args) {
  if (!plan9_local()) return plan9_hop("jobs", args != nullptr ? args : "");
  EnsureFds();
  Session& session = Sess();
  std::vector<std::string> fields = Fields(args != nullptr ? args : "");
  if (fields.empty() || fields[0].empty() || fields[0] == "list") {
    if (session.jobs.empty()) return "jobs=0";
    std::string out;
    for (const Job& job : session.jobs) {
      out += std::to_string(job.id);
      out.push_back(' ');
      out += std::to_string(job.pid);
      out.push_back(' ');
      out += job.image;
      out.push_back(' ');
      if (!job.done && !job.guest.empty())
        out += jit_ctr((std::string("state") + '\x1f' + job.guest).c_str());
      else
        out += job.done ? "exited " + std::to_string(job.exit_code) : "running";
      out.push_back('\n');
    }
    return out;
  }
  if (fields[0] == "kill") {
    if (fields.size() < 2) return "job miss";
    int id = FindFd(fields[1]);
    if (id <= 0) return "job miss";
    return Kill(id);
  }
  if (fields[0] != "wait") return "job miss";
  int only = 0;
  if (fields.size() > 1 && !fields[1].empty()) {
    only = FindFd(fields[1]);
    if (only <= 0) return "job miss";
  }
  std::string out;
  bool any = false;
  for (Job& job : session.jobs) {
    if (only != 0 && job.id != only) continue;
    any = true;
    Reap(job);
    if (!out.empty()) out.push_back('\n');
    out += JobLine(job);
  }
  if (!any) return only == 0 ? "jobs=0" : "job miss";
  return out;
}

std::string jit_status(const char* args) {
  if (!plan9_local()) return plan9_hop("rc", args != nullptr ? args : "");
  EnsureFds();
  std::vector<std::string> fields = Fields(args != nullptr ? args : "");
  if (!fields.empty() && fields[0] == "set" && fields.size() > 1)
    Sess().status = std::atoi(fields[1].c_str());
  return std::to_string(Sess().status);
}

std::string jit_env(const char* args) {
  if (!plan9_local()) return plan9_hop("env", args != nullptr ? args : "");
  EnsureFds();
  Session& session = Sess();
  std::vector<std::string> fields = Fields(args != nullptr ? args : "");
  if (fields.empty() || fields[0].empty()) {
    std::string out;
    for (const auto& kv : session.env) {
      out += kv.first;
      out.push_back('=');
      out += kv.second;
      out.push_back('\n');
    }
    return out;
  }
  const std::string& verb = fields[0];
  if (fields.size() < 2 || fields[1].empty() || fields[1].find('=') != std::string::npos)
    return "env miss";
  if (verb == "get") {
    auto it = session.env.find(fields[1]);
    if (it == session.env.end()) return "env miss";
    return it->second;
  }
  if (verb == "set") {
    if (fields.size() < 3) return "env miss";
    session.env[fields[1]] = fields[2];
    return "set " + fields[1];
  }
  if (verb == "unset") {
    auto it = session.env.find(fields[1]);
    if (it == session.env.end()) return "env miss";
    session.env.erase(it);
    return "unset " + fields[1];
  }
  return "env miss";
}

std::string jit_fd(const char* args) {
  if (!plan9_local()) return plan9_hop("fd", args != nullptr ? args : "");
  EnsureFds();
  Session& session = Sess();
  std::vector<std::string> fields = Fields(args != nullptr ? args : "");
  if (fields.empty() || fields[0].empty()) {
    std::string out;
    for (const auto& kv : session.fds) {
      out += std::to_string(kv.first);
      out.push_back(' ');
      out += kv.second.path;
      out.push_back(' ');
      out += ModeOf(kv.second);
      out.push_back('\n');
    }
    out += "cwd=" + session.cwd + "\n";
    return out;
  }
  const std::string& verb = fields[0];
  if (verb == "cwd") return session.cwd;
  if (verb == "chdir") {
    if (fields.size() < 2 || fields[1].empty()) return "walk miss";
    std::string path = Resolve(fields[1]);
    if (path != "/") {
      std::string listing = Vfs2("ls", path);
      if (listing == "walk miss") return "walk miss";
    }
    session.cwd = path;
    return session.cwd;
  }
  if (verb == "open") {
    if (fields.size() < 2 || fields[1].empty()) return "walk miss";
    std::string mode = fields.size() > 2 && !fields[2].empty() ? fields[2] : "r";
    if (mode != "r" && mode != "w" && mode != "rw") return "walk miss";
    std::string asked = JoinCwd(fields[1]);
    int stacked = 0;
    for (const Mount& mount : session.mounts)
      if (mount.at == asked) ++stacked;
    if (stacked > 1) {
      if (mode != "r") return "walk miss";
      OpenFd slot;
      slot.path = asked;
      slot.read = true;
      slot.write = false;
      slot.union_dir = true;
      slot.union_at = asked;
      int fd = 0;
      while (session.fds.find(fd) != session.fds.end()) ++fd;
      session.fds[fd] = slot;
      return "fd=" + std::to_string(fd) + " path=" + asked;
    }
    std::string path = Resolve(fields[1]);
    OpenFd slot;
    slot.path = path;
    slot.read = mode != "w";
    slot.write = mode != "r";
    if (path == "#n/nul") {
      slot.nul = true;
      slot.path = "NUL";
    } else if (path == "#c/cons") {
      slot.cons = true;
    } else if (path == "#e" || path.rfind("#e/", 0) == 0) {
      std::string name;
      bool dir = false;
      if (!EnvFile(path, &name, &dir)) return "walk miss";
      if (dir && mode != "r") return "walk miss";
      slot.env = true;
      slot.env_dir = dir;
      slot.env_name = name;
      if (!dir && mode == "r" && session.env.find(name) == session.env.end()) return "walk miss";
      if (!dir && mode != "r" && session.env.find(name) == session.env.end()) session.env[name] = "";
      if (!dir && mode == "w") session.env[name] = "";
    } else if (path == "#t") {
      if (mode != "r") return "walk miss";
      slot.clock = true;
      slot.write = false;
    } else if (path == "#p" || path.rfind("#p/", 0) == 0) {
      if (mode != "r") return "walk miss";
      std::string name;
      bool dir = false;
      if (!ProcFile(path, &name, &dir)) return "walk miss";
      if (!dir && name != "self" && ProcImage(name) == "walk miss") return "walk miss";
      slot.proc = true;
      slot.proc_dir = dir;
      slot.proc_name = name;
      slot.write = false;
    } else if (path == "#u" || path.rfind("#u/", 0) == 0) {
      slot.unil = true;
      if (path == "#u") {
        if (mode != "r") return "walk miss";
        slot.unil_dir = true;
        slot.write = false;
      } else {
        std::string rest = path.substr(3);
        size_t slash = rest.find('/');
        slot.unil_name = slash == std::string::npos ? rest : rest.substr(0, slash);
        slot.unil_region = slash == std::string::npos ? std::string() : rest.substr(slash + 1);
        if (slot.unil_name.empty() || slot.unil_region.find('/') != std::string::npos)
          return "walk miss";
        uint32_t pid = 0;
        if (!UnilFind(slot.unil_name, &pid, nullptr) || pid == 0) return "walk miss";
        bool ctl = slot.unil_region == "ctl";
        if (ctl) {
          slot.write = mode != "r";
          slot.read = mode != "w";
        } else {
          if (mode != "r") return "walk miss";
          slot.write = false;
          if (!slot.unil_region.empty()) {
            std::string body = jit_ctr((std::string("read") + '\x1f' + std::to_string(pid) + '\x1f' +
                                        slot.unil_region)
                                           .c_str());
            if (body == "ctr miss" || body == "ipt miss") return "walk miss";
          }
        }
      }
    } else if (path == "#s" || path.rfind("#s/", 0) == 0) {
      if (mode != "r") return "walk miss";
      slot.svc = true;
      slot.write = false;
      if (path == "#s") {
        slot.svc_dir = true;
      } else {
        slot.svc_name = path.substr(3);
        if (slot.svc_name.empty() || slot.svc_name.find('/') != std::string::npos) return "walk miss";
        std::string table = jit_svc("");
        std::string key = "name=" + slot.svc_name + " ";
        if (table.find(key) == std::string::npos) return "walk miss";
      }
    } else if (path == "#g" || path.rfind("#g/", 0) == 0) {
      std::string rel = path.size() > 3 ? path.substr(3) : std::string();
      bool writable = false;
      if (!gocos_vgpu || !GpuName(rel, &writable)) return "walk miss";
      if (mode != "r" && !writable) return "walk miss";
      slot.gpu = true;
      slot.gpu_path = rel;
      slot.read = mode != "w";
      slot.write = mode != "r";
    } else if (path == "#k" || path.rfind("#k/", 0) == 0) {
      if (mode != "r") return "walk miss";
      slot.krnl = true;
      slot.write = false;
      if (path != "#k") {
        slot.krnl_name = path.substr(3);
        bool ob = slot.krnl_name == "ob" || slot.krnl_name.rfind("ob/", 0) == 0;
        if (!ob && slot.krnl_name != "mem" && slot.krnl_name != "threads" && slot.krnl_name != "clock" &&
            slot.krnl_name != "cons" && slot.krnl_name != "handles" && slot.krnl_name != "pool" &&
            slot.krnl_name != "types" && slot.krnl_name != "pfn" && slot.krnl_name != "aspace")
          return "walk miss";
        if (!ob && slot.krnl_name.find('/') != std::string::npos) return "walk miss";
      }
    } else if (slot.write && mode == "w") {
      std::string wrote = Vfs3("write", path, "");
      if (wrote.rfind("write ", 0) != 0) return "create miss";
    } else {
      std::string body = Vfs2("read", path);
      if (body == "walk miss" || body == "ipt miss") {
        if (!slot.write) return "walk miss";
        std::string wrote = Vfs3("write", path, "");
        if (wrote.rfind("write ", 0) != 0) return "create miss";
      }
    }
    int fd = 0;
    while (session.fds.find(fd) != session.fds.end()) ++fd;
    session.fds[fd] = slot;
    return "fd=" + std::to_string(fd) + " path=" + path;
  }
  if (verb == "pipe") {
    auto buf = std::make_shared<PipeBuf>();
    buf->readers = 1;
    buf->writers = 1;
    int rd = AllocFd(session);
    OpenFd reader;
    reader.path = "pipe";
    reader.read = true;
    reader.write = false;
    reader.pipe = buf;
    session.fds[rd] = reader;
    int wr = AllocFd(session);
    OpenFd writer;
    writer.path = "pipe";
    writer.read = false;
    writer.write = true;
    writer.pipe = buf;
    writer.pipe_write = true;
    session.fds[wr] = writer;
    return "pipe " + std::to_string(rd) + " " + std::to_string(wr);
  }
  if (verb == "bind") {
    if (fields.size() < 2 || fields[1].empty()) {
      if (session.mounts.empty()) return "bind=0";
      std::string out;
      for (const Mount& mount : session.mounts) {
        out += "bind ";
        if (mount.flag != 0) {
          out.push_back('-');
          out.push_back(mount.flag);
          out.push_back(' ');
        }
        out += mount.device;
        out.push_back(' ');
        out += mount.at;
        out.push_back('\n');
      }
      return out;
    }
    char flag = 0;
    if (fields[1] == "-b" || fields[1] == "-a") {
      if (fields.size() < 4 || fields[2].empty() || fields[3].empty()) return "walk miss";
      flag = fields[1][1];
      fields.erase(fields.begin() + 1);
    }
    if (fields.size() < 3 || fields[2].empty()) return "walk miss";
    std::string dev = fields[1];
    const std::string& at = fields[2];
    if (at.size() < 2 || at.front() != '/' || at.find('#') != std::string::npos) return "walk miss";
    if (!IsDev(dev)) {
      dev = Resolve(dev);
      if (dev == "/" || dev == at) return "walk miss";
      if (!IsDev(dev)) {
        std::string listing = Vfs2("ls", dev);
        std::string body = Vfs2("read", dev);
        bool listed = listing != "walk miss" && listing != "empty";
        bool filed = body != "walk miss" && body != "ipt miss";
        if (!listed && !filed) return "walk miss";
      }
    }
    std::string reply = "bind ";
    if (flag != 0) {
      reply.push_back('-');
      reply.push_back(flag);
      reply.push_back(' ');
    }
    reply += dev + " " + at;
    if (flag == 0) {
      for (Mount& mount : session.mounts) {
        if (mount.at == at) {
          mount.device = dev;
          mount.flag = 0;
          return reply;
        }
      }
      session.mounts.push_back(Mount{at, dev, 0});
      return reply;
    }
    if (flag == 'b') {
      auto place = session.mounts.begin();
      for (auto it = session.mounts.begin(); it != session.mounts.end(); ++it) {
        if (it->at == at) {
          place = it;
          break;
        }
      }
      session.mounts.insert(place, Mount{at, dev, 'b'});
      return reply;
    }
    session.mounts.push_back(Mount{at, dev, 'a'});
    return reply;
  }
  if (verb == "close" || verb == "dup" || verb == "read" || verb == "write") {
    if (fields.size() < 2) return "fd miss";
    int fd = FindFd(fields[1]);
    auto it = session.fds.find(fd);
    if (it == session.fds.end()) return "fd miss";
    if (verb == "close") {
      if (it->second.pipe) {
        if (it->second.pipe_write) --it->second.pipe->writers;
        else --it->second.pipe->readers;
      }
      session.fds.erase(it);
      return "close " + std::to_string(fd);
    }
    if (verb == "dup") {
      int neu = AllocFd(session);
      OpenFd copy = it->second;
      if (copy.pipe) {
        if (copy.pipe_write) ++copy.pipe->writers;
        else ++copy.pipe->readers;
      }
      session.fds[neu] = copy;
      return "fd=" + std::to_string(neu) + " path=" + copy.path;
    }
    OpenFd& slot = it->second;
    if (verb == "read") {
      if (!slot.read) return "walk miss";
      if (slot.nul) return "";
      if (slot.clock) {
        if (slot.offset != 0) return "";
        std::string src = hv::k32("GetLocalTime", "");
        if (src.rfind("error:", 0) == 0) return src;
        slot.offset = src.size();
        return src;
      }
      if (slot.pipe) {
        std::string slice = slot.pipe->data;
        slot.pipe->data.clear();
        return slice;
      }
      if (slot.union_dir) {
        if (slot.offset != 0) return "";
        std::string src;
        for (const Mount& mount : session.mounts) {
          if (mount.at != slot.union_at) continue;
          if (mount.device == "#c") src += "cons\n";
          else if (mount.device == "#e") {
            for (const auto& kv : session.env) {
              src += kv.first;
              src.push_back('\n');
            }
          } else if (!IsDev(mount.device)) {
            std::string listing = Vfs2("ls", mount.device);
            if (listing != "walk miss" && listing != "empty") src += listing;
          }
        }
        slot.offset = src.size();
        return src;
      }
      if (slot.env) {
        std::string src;
        if (slot.env_dir) {
          for (const auto& kv : session.env) {
            src += kv.first;
            src.push_back('\n');
          }
        } else {
          auto ev = session.env.find(slot.env_name);
          if (ev == session.env.end()) return "env miss";
          src = ev->second;
        }
        if (slot.offset >= src.size()) return "";
        std::string slice = src.substr(slot.offset);
        slot.offset = src.size();
        return slice;
      }
      if (slot.unil) {
        std::string src;
        if (slot.unil_dir) {
          src = UnilNames();
        } else if (slot.unil_region.empty()) {
          if (!UnilFind(slot.unil_name, nullptr, &src)) return "walk miss";
        } else {
          uint32_t pid = 0;
          if (!UnilFind(slot.unil_name, &pid, nullptr) || pid == 0) return "walk miss";
          src = jit_ctr((std::string("read") + '\x1f' + std::to_string(pid) + '\x1f' +
                         slot.unil_region)
                            .c_str());
          if (src == "ctr miss" || src == "ipt miss") return "walk miss";
        }
        if (slot.offset >= src.size()) return "";
        std::string slice = src.substr(slot.offset);
        slot.offset = src.size();
        return slice;
      }
      if (slot.gpu) {
        // One snapshot per open: a read of #g/dxg or a buffer asks the GPU.
        if (!slot.gpu_have) {
          bool ok = false;
          slot.gpu_read = GpuCall("read", slot.gpu_path, std::string(), &ok);
          if (!ok) return "walk miss";
          slot.gpu_have = true;
        }
        if (slot.offset >= slot.gpu_read.size()) return "";
        std::string slice = slot.gpu_read.substr(slot.offset);
        slot.offset = slot.gpu_read.size();
        return slice;
      }
      if (slot.krnl) {
        std::string src;
        if (slot.krnl_name.empty()) src = jit_react("Processes");
        else if (slot.krnl_name == "ob" || slot.krnl_name.rfind("ob/", 0) == 0) {
          std::string nt = "\\";
          if (slot.krnl_name.size() > 2) nt += slot.krnl_name.substr(3);
          for (char& c : nt)
            if (c == '/') c = '\\';
          src = jit_react((std::string("ObDir") + '\x1f' + nt).c_str());
        }
        else if (slot.krnl_name == "mem") src = jit_react("MemoryMap");
        else if (slot.krnl_name == "clock") src = jit_react("HalQueryRealTimeClock");
        else if (slot.krnl_name == "cons") src = jit_react("Console");
        else if (slot.krnl_name == "handles") src = jit_react("Handles");
        else if (slot.krnl_name == "pool") src = jit_react("PoolTags");
        else if (slot.krnl_name == "types") src = jit_react("Types");
        else if (slot.krnl_name == "pfn") src = jit_react("Pfn");
        else if (slot.krnl_name == "aspace") src = jit_react("Aspace");
        else src = jit_react("Threads");
        if (src.rfind("error:", 0) == 0) return src;
        for (char& c : src)
          if (c == '\x1f') c = '\n';
        if (slot.offset >= src.size()) return "";
        std::string slice = src.substr(slot.offset);
        slot.offset = src.size();
        return slice;
      }
      if (slot.svc) {
        std::string src = jit_svc("");
        if (src == "svc=0") src.clear();
        if (!slot.svc_dir) {
          std::string line;
          std::string key = "name=" + slot.svc_name + " ";
          size_t at = src.find(key);
          if (at == std::string::npos) return "walk miss";
          size_t begin = src.rfind('\n', at);
          begin = begin == std::string::npos ? 0 : begin + 1;
          size_t end = src.find('\n', at);
          if (end == std::string::npos) end = src.size();
          line = src.substr(begin, end - begin);
          src = line;
        }
        if (slot.offset >= src.size()) return "";
        std::string slice = src.substr(slot.offset);
        slot.offset = src.size();
        return slice;
      }
      if (slot.proc) {
        std::string src;
        if (slot.proc_dir) {
          src = ProcShow();
          if (src == "empty") src.clear();
        } else if (slot.proc_name == "self") {
          src = std::to_string(KernelPid());
        } else {
          src = jit_ctr_line(static_cast<uint32_t>(std::strtoul(slot.proc_name.c_str(), nullptr, 10)));
          if (src.empty()) src = ProcImage(slot.proc_name);
          if (src == "walk miss") return src;
        }
        if (slot.offset >= src.size()) return "";
        std::string slice = src.substr(slot.offset);
        slot.offset = src.size();
        return slice;
      }
      const std::string& src = slot.cons ? session.cons : Vfs2("read", slot.path);
      if (!slot.cons && (src == "walk miss" || src == "ipt miss")) return "walk miss";
      if (slot.offset >= src.size()) return "";
      std::string slice = src.substr(slot.offset);
      slot.offset = src.size();
      return slice;
    }
    if (fields.size() < 3) return "walk miss";
    if (!slot.write) return "walk miss";
    const std::string& body = fields[2];
    if (slot.unil && slot.unil_region == "ctl") {
      uint32_t pid = 0;
      if (!UnilFind(slot.unil_name, &pid, nullptr) || pid == 0) return "walk miss";
      size_t sp = body.find(' ');
      std::string verb = sp == std::string::npos ? body : body.substr(0, sp);
      std::string rest = sp == std::string::npos ? std::string() : body.substr(sp + 1);
      std::string ran;
      if (verb == "eval") {
        if (rest.empty()) return "walk miss";
        ran = jit_ctr((std::string("eval") + '\x1f' + std::to_string(pid) + '\x1f' + rest).c_str());
      } else if (verb == "call") {
        size_t sp2 = rest.find(' ');
        std::string topic = sp2 == std::string::npos ? rest : rest.substr(0, sp2);
        std::string payload = sp2 == std::string::npos ? std::string() : rest.substr(sp2 + 1);
        if (topic.empty()) return "walk miss";
        ran = jit_ctr((std::string("call") + '\x1f' + std::to_string(pid) + '\x1f' + topic + '\x1f' +
                       payload)
                          .c_str());
      } else {
        return "walk miss";
      }
      if (ran == "ctr miss" || ran == "ipt miss" || ran.rfind("error:", 0) == 0) return ran;
      return "write " + std::to_string(fd) + " " + std::to_string(body.size());
    }
    if (slot.nul) return "write " + std::to_string(fd) + " " + std::to_string(body.size());
    if (slot.gpu) {
      bool ok = false;
      GpuCall("write", slot.gpu_path, body, &ok);
      if (!ok) return "walk miss";
      return "write " + std::to_string(fd) + " " + std::to_string(body.size());
    }
    if (slot.pipe) {
      if (slot.pipe->readers <= 0) return "walk miss";
      slot.pipe->data.append(body);
      return "write " + std::to_string(fd) + " " + std::to_string(body.size());
    }
    if (slot.proc) return "walk miss";
    if (slot.env) {
      if (slot.env_dir) return "walk miss";
      std::string& val = session.env[slot.env_name];
      if (slot.offset > val.size()) slot.offset = val.size();
      if (slot.offset + body.size() > val.size()) val.resize(slot.offset + body.size());
      if (!body.empty()) std::memcpy(&val[slot.offset], body.data(), body.size());
      slot.offset += body.size();
      return "write " + std::to_string(fd) + " " + std::to_string(body.size());
    }
    if (slot.cons) {
      session.cons.append(body);
      slot.offset = session.cons.size();
    } else {
      std::string cur = Vfs2("read", slot.path);
      if (cur == "walk miss" || cur == "ipt miss") cur.clear();
      if (slot.offset > cur.size()) slot.offset = cur.size();
      std::string next = cur;
      if (slot.offset + body.size() > next.size()) next.resize(slot.offset + body.size());
      if (!body.empty()) std::memcpy(&next[slot.offset], body.data(), body.size());
      std::string wrote = Vfs3("write", slot.path, next);
      if (wrote.rfind("write ", 0) != 0) return "create miss";
      slot.offset += body.size();
    }
    return "write " + std::to_string(fd) + " " + std::to_string(body.size());
  }
  return "walk miss";
}

#if !defined(WASMGOCOS_HAS_NET)
bool plan9_local() { return false; }

std::string plan9_hop(const char* verb, const char* args) {
  (void)verb;
  (void)args;
  return "error: net: link";
}
#endif

}  // namespace wasmgocos
