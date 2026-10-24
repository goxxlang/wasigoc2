#include "gocos/jitrt.hpp"
#include "gocos/host.hpp"
#include "gocos/files.hpp"
#include "gocos/klock.hpp"
#include "gocos/stream.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <string_view>
#include <vector>

namespace wasmgocos {
namespace {

std::string Trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
  return std::string(s);
}

std::vector<std::string> Tokens(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

int FdNum(const std::string& reply) {
  if (reply.rfind("fd=", 0) != 0) return -1;
  return std::atoi(reply.c_str() + 3);
}

std::string Fd1(const char* verb, const std::string& a) {
  std::string args = verb;
  args.push_back('\x1f');
  args += a;
  return jit_fd(args.c_str());
}

std::string Fd2(const char* verb, const std::string& a, const std::string& b) {
  std::string args = verb;
  args.push_back('\x1f');
  args += a;
  args.push_back('\x1f');
  args += b;
  return jit_fd(args.c_str());
}

std::string CloseFd(int fd) { return Fd1("close", std::to_string(fd)); }

std::string Slurp(const std::string& path) {
  std::string opened = Fd2("open", path, "r");
  int fd = FdNum(opened);
  if (fd < 0) return opened;
  std::string body = Fd1("read", std::to_string(fd));
  CloseFd(fd);
  return body;
}

std::string Store(const std::string& path, const std::string& body) {
  std::string opened = Fd2("open", path, "w");
  int fd = FdNum(opened);
  if (fd < 0) return opened;
  std::string wrote = Fd2("write", std::to_string(fd), body);
  CloseFd(fd);
  if (wrote.rfind("write ", 0) != 0) return wrote;
  return "";
}

std::string Append(const std::string& path, const std::string& body) {
  std::string prev = Slurp(path);
  if (prev == "walk miss" || prev == "create miss") prev.clear();
  else if (prev.rfind("fd=", 0) == 0) return prev;
  return Store(path, prev + body);
}

std::string ThroughPipe(const std::string& bytes) {
  std::string created = jit_fd("pipe");
  int rd = 0;
  int wr = 0;
  if (std::sscanf(created.c_str(), "pipe %d %d", &rd, &wr) != 2) return created;
  std::string wrote = Fd2("write", std::to_string(wr), bytes);
  std::string got = Fd1("read", std::to_string(rd));
  CloseFd(rd);
  CloseFd(wr);
  if (wrote.rfind("write ", 0) != 0) return wrote;
  return got;
}

std::string WithCwd(const std::string& path) {
  if (path.empty() || path.front() == '/' || path.front() == '#') return path;
  std::string cwd = jit_fd("cwd");
  if (cwd == "/") return "/" + path;
  return cwd + "/" + path;
}

bool TakesPath(const std::string& name) {
  return name == "cat" || name == "ls" || name == "mkdir" || name == "rm" || name == "wc" ||
         name == "cp" || name == "mv";
}

std::string PathArgs(const std::string& name, const std::string& rest) {
  if (rest.empty()) return rest;
  if (name == "cp" || name == "mv") {
    size_t sp = rest.find(' ');
    if (sp == std::string::npos) return WithCwd(rest);
    return WithCwd(rest.substr(0, sp)) + " " + WithCwd(Trim(rest.substr(sp + 1)));
  }
  return WithCwd(rest);
}

// The exit status of the last guest command a stage ran, or -1: a guest's
// reply is its stdout, so its status cannot ride in the reply.
// Per thread: a background command runs the shell on its own.
thread_local int g_guest_code = -1;

// A command that is a guest: `name` for `/bin/<name>.wasm`, or a path to a
// `.wasm` in the kernel's files. Empty when it is not one.
std::string GuestCommand(const std::string& name) {
  std::string path;
  if (name.size() > 5 && name.compare(name.size() - 5, 5, ".wasm") == 0 && (name[0] == '/' || name[0] == '#')) {
    path = name;
  } else if (!name.empty() && name.find_first_of("/\\#.:") == std::string::npos) {
    path = "/bin/" + name + ".wasm";
  } else {
    return "";
  }
  return files::StatOf(files::Walk(path)).kind == files::Kind::kFile ? path : "";
}

std::string RunOne(const std::vector<std::string>& argv, const std::string& incoming,
                   bool have_in) {
  if (argv.empty()) return "sh: syntax";
  const std::string& name = argv[0];
  std::string rest;
  for (size_t i = 1; i < argv.size(); ++i) {
    if (!rest.empty()) rest.push_back(' ');
    rest += argv[i];
  }
  if (name == "cd") {
    std::string path = rest.empty() ? "/" : rest;
    std::string got = Fd1("chdir", path);
    if (got == "walk miss") return got;
    return "";
  }
  if (name == "pwd") return jit_fd("cwd");
  // `touch <path>...`: a file that is not there is made empty; one that is
  // has its time set to now (include/gocos/files.hpp).
  if (name == "touch") {
    if (argv.size() < 2) return "walk miss";
    for (size_t i = 1; i < argv.size(); ++i) {
      std::string walked = jit_walk(argv[i]);
      files::Stat st = files::StatOf(walked);
      if (st.kind == files::Kind::kNone) {
        if (!files::Write(walked, "")) return "create miss";
      } else if (!files::SetTime(walked, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count())) {
        return "walk miss";
      }
    }
    return "";
  }
  if (name == "export") {
    std::string key = rest;
    std::string val;
    size_t eq = rest.find('=');
    size_t sp = rest.find(' ');
    if (eq != std::string::npos && (sp == std::string::npos || eq < sp)) {
      key = rest.substr(0, eq);
      val = rest.substr(eq + 1);
    } else if (sp != std::string::npos) {
      key = rest.substr(0, sp);
      val = Trim(rest.substr(sp + 1));
    }
    if (key.empty()) return "env miss";
    return jit_env((std::string("set") + '\x1f' + key + '\x1f' + val).c_str()) ==
                   "set " + key
               ? ""
               : "env miss";
  }
  if (name == "unset") {
    if (rest.empty()) return "env miss";
    std::string got = jit_env((std::string("unset") + '\x1f' + rest).c_str());
    if (got.rfind("unset ", 0) != 0) return got;
    return "";
  }
  if (name == "bind") {
    if (rest.empty()) return jit_fd("bind");
    std::string flag;
    if (rest.rfind("-b ", 0) == 0 || rest.rfind("-a ", 0) == 0) {
      flag = rest.substr(0, 2);
      rest = Trim(rest.substr(2));
    }
    size_t sp = rest.find(' ');
    if (sp == std::string::npos) return "walk miss";
    std::string dev = rest.substr(0, sp);
    std::string at = Trim(rest.substr(sp + 1));
    if (flag.empty()) return Fd2("bind", dev, at);
    return jit_fd((std::string("bind") + '\x1f' + flag + '\x1f' + dev + '\x1f' + at).c_str());
  }
  if (name == "date") return Slurp("#t");
  if (name == "uname") {
    std::string hopped = hv::nix("uname", "");
    if (hopped.rfind("error:", 0) != 0) return hopped;
    std::string ver = hv::k32("RtlGetVersion", "");
    std::string node = hv::k32("GetComputerNameW", "");
    if (ver.rfind("error:", 0) == 0) return hopped;
    if (node.rfind("error:", 0) == 0) node = "localhost";
#if defined(_WIN64)
    const char* mach = "x86_64";
#else
    const char* mach = "x86";
#endif
    return std::string("Windows_NT ") + node + " " + ver + " " + ver + " " + mach;
  }
  if (name == "svc") {
    if (rest.empty() || rest == "list") return jit_svc("");
    size_t sp = rest.find(' ');
    std::string verb = sp == std::string::npos ? rest : rest.substr(0, sp);
    std::string tail = sp == std::string::npos ? std::string() : Trim(rest.substr(sp + 1));
    auto word = [](std::string& s) {
      size_t sp2 = s.find(' ');
      std::string w = sp2 == std::string::npos ? s : s.substr(0, sp2);
      s = sp2 == std::string::npos ? std::string() : Trim(s.substr(sp2 + 1));
      return w;
    };
    if (verb == "rc" || verb == "sched") {
      std::string svc_name = word(tail);
      std::string mid = word(tail);
      if (svc_name.empty() || mid.empty() || tail.empty()) return "svc miss";
      return jit_svc((verb + '\x1f' + svc_name + '\x1f' + mid + '\x1f' + tail).c_str());
    }
    if (verb == "daemon") {
      std::string svc_name = word(tail);
      if (svc_name.empty() || tail.empty()) return "svc miss";
      return jit_svc((std::string("daemon") + '\x1f' + svc_name + '\x1f' + tail).c_str());
    }
    if (verb == "start" || verb == "stop") {
      if (tail.empty()) return jit_svc(verb.c_str());
      return jit_svc((verb + '\x1f' + tail).c_str());
    }
    if (verb == "tick") return jit_svc("tick");
    if (verb == "init") return jit_svc("init");
    return "svc miss";
  }
  if (name == "net") {
    if (rest.empty() || rest == "link") return jit_net("");
    size_t sp = rest.find(' ');
    std::string verb = sp == std::string::npos ? rest : rest.substr(0, sp);
    std::string tail = sp == std::string::npos ? std::string() : Trim(rest.substr(sp + 1));
    if (verb == "put") {
      size_t sp2 = tail.find(' ');
      if (sp2 == std::string::npos) return "net miss";
      return jit_net((std::string("put") + '\x1f' + tail.substr(0, sp2) + '\x1f' +
                      Trim(tail.substr(sp2 + 1)))
                         .c_str());
    }
    if (verb == "query" && !tail.empty())
      return jit_net((std::string("query") + '\x1f' + tail).c_str());
    if (verb == "serve") return jit_net("serve");
    if (verb == "open" && !tail.empty())
      return jit_net((std::string("open") + '\x1f' + tail).c_str());
    if (verb == "send" || verb == "recv") {
      size_t sp2 = tail.find(' ');
      std::string channel = sp2 == std::string::npos ? tail : tail.substr(0, sp2);
      std::string body = sp2 == std::string::npos ? std::string() : Trim(tail.substr(sp2 + 1));
      if (channel.empty()) return "net miss";
      if (verb == "recv") return jit_net((std::string("recv") + '\x1f' + channel).c_str());
      if (body.empty()) return "net miss";
      return jit_net((std::string("send") + '\x1f' + channel + '\x1f' + body).c_str());
    }
    return "net miss";
  }
  if (name == "react") {
    if (rest.empty()) return jit_react("");
    std::string args;
    std::string cur = rest;
    while (!cur.empty()) {
      size_t sp = cur.find(' ');
      std::string word = sp == std::string::npos ? cur : cur.substr(0, sp);
      cur = sp == std::string::npos ? std::string() : Trim(cur.substr(sp + 1));
      if (!args.empty()) args.push_back('\x1f');
      args += word;
    }
    return jit_react(args.c_str());
  }
  if (name == "ctr") {
    if (rest.empty() || rest == "list") return jit_ctr("");
    size_t sp = rest.find(' ');
    std::string verb = sp == std::string::npos ? rest : rest.substr(0, sp);
    std::string tail = sp == std::string::npos ? std::string() : Trim(rest.substr(sp + 1));
    if (verb == "load") {
      size_t sp2 = tail.find(' ');
      std::string cname = sp2 == std::string::npos ? tail : tail.substr(0, sp2);
      std::string body = sp2 == std::string::npos ? std::string() : Trim(tail.substr(sp2 + 1));
      std::string args = std::string("load") + '\x1f' + cname;
      if (!body.empty()) {
        args.push_back('\x1f');
        args += body;
      }
      return jit_ctr(args.c_str());
    }
    if (verb == "open") {
      if (tail.empty()) return "ctr miss";
      size_t sp2 = tail.find(' ');
      std::string args = std::string("open") + '\x1f' + tail.substr(0, sp2);
      if (sp2 != std::string::npos) {
        args.push_back('\x1f');
        args += Trim(tail.substr(sp2 + 1));
      }
      return jit_ctr(args.c_str());
    }
    if (verb == "run") {
      std::string args = "run";
      size_t i = 0;
      while (i < tail.size()) {
        while (i < tail.size() && tail[i] == ' ') ++i;
        if (i >= tail.size()) break;
        size_t j = tail.find(' ', i);
        if (j == std::string::npos) j = tail.size();
        args.push_back('\x1f');
        args.append(tail, i, j - i);
        i = j;
      }
      return jit_ctr(args.c_str());
    }
    if (verb == "install") {
      size_t sp2 = tail.find(' ');
      if (sp2 == std::string::npos) return "ctr miss";
      return jit_ctr((std::string("install") + '\x1f' + tail.substr(0, sp2) + '\x1f' +
                      Trim(tail.substr(sp2 + 1)))
                         .c_str());
    }
    if (verb == "save") {
      size_t sp2 = tail.find(' ');
      if (sp2 == std::string::npos) return "ctr miss";
      return jit_ctr((std::string("save") + '\x1f' + tail.substr(0, sp2) + '\x1f' +
                      Trim(tail.substr(sp2 + 1)))
                         .c_str());
    }
    if (verb == "read") {
      size_t sp2 = tail.find(' ');
      if (sp2 == std::string::npos) return "ctr miss";
      return jit_ctr((std::string("read") + '\x1f' + tail.substr(0, sp2) + '\x1f' +
                      Trim(tail.substr(sp2 + 1)))
                         .c_str());
    }
    if (verb == "eval") {
      size_t sp2 = tail.find(' ');
      if (sp2 == std::string::npos) return "ctr miss";
      return jit_ctr((std::string("eval") + '\x1f' + tail.substr(0, sp2) + '\x1f' +
                      Trim(tail.substr(sp2 + 1)))
                         .c_str());
    }
    if (verb == "call") {
      std::string args = "call";
      size_t i = 0;
      while (i < tail.size()) {
        while (i < tail.size() && tail[i] == ' ') ++i;
        if (i >= tail.size()) break;
        size_t j = tail.find(' ', i);
        if (j == std::string::npos) j = tail.size();
        args.push_back('\x1f');
        args.append(tail, i, j - i);
        i = j;
      }
      if (args == "call") return "ctr miss";
      return jit_ctr(args.c_str());
    }
    return "ctr miss";
  }
  if (name == "jobs") return jit_jobs("");
  if (name == "status") return jit_status("");
  if (name == "kill") {
    if (rest.empty()) return "job miss";
    return jit_jobs((std::string("kill") + '\x1f' + rest).c_str());
  }
  if (name == "wait") {
    if (rest.empty()) return jit_jobs("wait");
    return jit_jobs((std::string("wait") + '\x1f' + rest).c_str());
  }
  // A guest in /bin: stdin is what the stage was given, the reply its
  // stdout.
  std::string guest = GuestCommand(name);
  if (!guest.empty()) {
    std::string out;
    int code = 1;
    GuestIo io;
    if (have_in) io.in = incoming;
    std::string refused = jit_ctr_exec(guest, argv, io, &out, &code);
    if (!refused.empty()) return refused;
    g_guest_code = code;
    return out;
  }
  if (have_in && rest.empty() && name == "cat") return incoming;
  if (have_in && rest.empty() && name == "wc") {
    std::string stored = Store("/tmp/sh-stdin", incoming);
    if (!stored.empty()) return stored;
    rest = "/tmp/sh-stdin";
  } else if (TakesPath(name)) {
    rest = PathArgs(name, rest);
  }
  std::string spec = std::string("run") + '\x1f' + name;
  if (!rest.empty()) {
    spec.push_back(' ');
    spec += rest;
  }
  std::string reply = jit_bbx(spec.c_str());
  if (reply.rfind("ENOSYS:applet=", 0) != 0) return reply;
  std::string cmd = "cmd.exe /c " + name;
  if (!rest.empty()) {
    cmd.push_back(' ');
    cmd += rest;
  }
  GocProc launched = goc_launch(cmd.c_str(), true);
  if (!launched.error.empty()) return launched.error;
  return launched.output;
}

struct Stage {
  std::vector<std::string> argv;
  std::string in;
  std::string out;
  bool append = false;
};

Stage ParseStage(const std::string& text) {
  Stage stage;
  std::vector<std::string> tok = Tokens(text);
  for (size_t i = 0; i < tok.size(); ++i) {
    if (tok[i] == "<" || tok[i] == ">" || tok[i] == ">>") {
      if (i + 1 >= tok.size()) {
        stage.argv.clear();
        stage.argv.push_back("sh:");
        stage.argv.push_back("syntax");
        return stage;
      }
      if (tok[i] == "<") stage.in = tok[++i];
      else {
        stage.append = tok[i] == ">>";
        stage.out = tok[++i];
      }
    } else {
      stage.argv.push_back(tok[i]);
    }
  }
  return stage;
}

int CodeOf(const std::string& reply);

// A pipeline with a guest in it: every stage runs at once, each on its
// own thread under the kernel lock, joined by streams
// (include/gocos/stream.hpp). A guest reads its stream as it fills and
// writes the next one as it goes, so `yes | head 3` ends: when head does,
// yes's next write is a broken pipe. Any other stage reads its input to
// the end, runs, and writes its reply. `<` and `>` are the stage's own, as
// in the one-at-a-time path; a stage writing to `>` gives the next stage
// an empty stream. The status is the last stage's.
std::string Streamed(const std::vector<Stage>& stages) {
  size_t n = stages.size();
  std::vector<std::shared_ptr<Stream>> links;
  for (size_t i = 0; i + 1 < n; ++i) links.push_back(std::make_shared<Stream>());
  struct Ran {
    std::string guest;  // its image, when the stage is a guest
    std::string in;     // its `<` file
    std::string out;    // what it wrote, when that was not a stream
    int code = 0;
  };
  std::vector<Ran> ran(n);
  for (size_t i = 0; i < n; ++i) {
    if (stages[i].argv.empty()) return "sh: syntax";
    ran[i].guest = GuestCommand(stages[i].argv[0]);
    if (!stages[i].in.empty()) {
      ran[i].in = Slurp(stages[i].in);
      if (ran[i].in == "walk miss" || ran[i].in == "create miss" || ran[i].in == "fd miss") return ran[i].in;
    }
  }
  // The stages are the caller's job: a kill of it reaches every one.
  std::atomic<bool>* kill = kernel_kill_flag();
  std::vector<std::thread> threads;
  for (size_t i = 0; i < n; ++i) {
    std::shared_ptr<Stream> in = i > 0 ? links[i - 1] : nullptr;
    std::shared_ptr<Stream> out = i + 1 < n ? links[i] : nullptr;
    if (out && !stages[i].out.empty()) {
      out->CloseWrite();
      out = nullptr;
    }
    threads.emplace_back([&stages, &ran, i, in, out, kill] {
      kernel_set_kill(kill);
      const Stage& stage = stages[i];
      Ran& r = ran[i];
      bool from_file = !stage.in.empty();
      if (!r.guest.empty()) {
        KernelLock lock;
        GuestIo io;
        if (from_file) io.in = r.in;
        else io.in_stream = in;
        io.out_stream = out;
        std::string refused = jit_ctr_exec(r.guest, stage.argv, io, &r.out, &r.code);
        if (!refused.empty()) {
          r.out = refused;
          r.code = 1;
        }
      } else {
        std::string input = r.in;
        if (!from_file && in) input = in->ReadAll();
        KernelLock lock;
        r.out = RunOne(stage.argv, input, from_file || in != nullptr);
        r.code = CodeOf(r.out);
      }
      if (in) in->CloseRead();
      if (out) {
        if (!r.out.empty()) out->Write(r.out);
        r.out.clear();
        out->CloseWrite();
      }
      kernel_set_kill(nullptr);
    });
  }
  kernel_unlocked([&threads] {
    for (std::thread& t : threads) t.join();
  });
  for (size_t i = 0; i < n; ++i) {
    if (stages[i].out.empty()) continue;
    std::string err = stages[i].append ? Append(stages[i].out, ran[i].out) : Store(stages[i].out, ran[i].out);
    if (!err.empty()) return err;
    ran[i].out.clear();
  }
  g_guest_code = ran.back().code;
  return ran.back().out;
}

int CodeOf(const std::string& reply) {
  if (reply == "false=1" || reply == "walk miss" || reply == "sh: syntax" || reply == "env miss" ||
      reply == "job miss" || reply == "create miss" || reply == "fd miss")
    return 1;
  if (reply.rfind("error:", 0) == 0 || reply.rfind("ENOSYS:", 0) == 0) return 1;
  if (reply.rfind("ctr pid=", 0) == 0) {
    size_t at = reply.find(" exit=");
    if (at != std::string::npos) return std::atoi(reply.c_str() + at + 6);
  }
  if (reply.rfind("job ", 0) == 0) {
    size_t at = reply.rfind("exit=");
    if (at != std::string::npos) return std::atoi(reply.c_str() + at + 5);
  }
  return 0;
}

std::string Done(const std::string& reply, bool keep) {
  if (!keep) jit_status((std::string("set") + '\x1f' + std::to_string(CodeOf(reply))).c_str());
  return reply;
}

}  // namespace

std::string ExecLine(const std::string& line) {
  std::vector<std::string> head = Tokens(line);
  // An svc line keeps its `&`: it is part of a daemon's body.
  if (!head.empty() && head.back() == "&" && head.front() != "svc") {
    head.pop_back();
    if (head.empty()) return "sh: syntax";
    // A command line with a guest in it runs as a kernel job, pipeline and
    // all, not under cmd.exe.
    std::string body = Trim(line.substr(0, line.rfind('&')));
    bool guest = false;
    for (size_t at = 0, bar; at <= body.size(); at = bar + 1) {
      bar = body.find('|', at);
      if (bar == std::string::npos) bar = body.size();
      Stage stage = ParseStage(body.substr(at, bar - at));
      if (!stage.argv.empty() && !GuestCommand(stage.argv[0]).empty()) guest = true;
    }
    if (guest) {
      return jit_ctr_start_command(body, [body](int* code) {
        g_guest_code = -1;
        std::string out = ExecLine(body);
        *code = g_guest_code >= 0 ? g_guest_code : CodeOf(out);
        return out;
      });
    }
    // A guest in the background runs on the kernel's own thread, not cmd.exe.
    if (head.size() >= 4 && head[0] == "ctr" && head[1] == "run") {
      std::string args = "start";
      for (size_t i = 2; i < head.size(); ++i) {
        args.push_back('\x1f');
        args += head[i];
      }
      return jit_ctr(args.c_str());
    }
    std::string cmd;
    for (const std::string& tok : head) {
      if (tok == "|" || tok == ">" || tok == ">>" || tok == "<" || tok == ";") return "sh: syntax";
      if (!cmd.empty()) cmd.push_back(' ');
      cmd += tok;
    }
    GocProc launched = goc_launch((std::string("cmd.exe /c ") + cmd).c_str(), false);
    if (!launched.error.empty()) return launched.error;
    return "job " + std::to_string(launched.job) + " pid=" + std::to_string(launched.pid);
  }
  std::vector<std::string> parts;
  std::string cur;
  for (char c : line) {
    if (c == '|') {
      parts.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  parts.push_back(cur);

  if (parts.size() > 1) {
    std::vector<Stage> stages;
    bool guest = false;
    for (const std::string& part : parts) {
      stages.push_back(ParseStage(part));
      const Stage& stage = stages.back();
      if (stage.argv.size() == 2 && stage.argv[0] == "sh:" && stage.argv[1] == "syntax") return "sh: syntax";
      if (!stage.argv.empty() && !GuestCommand(stage.argv[0]).empty()) guest = true;
    }
    if (guest) return Streamed(stages);
  }

  std::string incoming;
  bool have_in = false;
  std::string produced;
  for (size_t i = 0; i < parts.size(); ++i) {
    Stage stage = ParseStage(parts[i]);
    if (stage.argv.size() == 2 && stage.argv[0] == "sh:" && stage.argv[1] == "syntax")
      return "sh: syntax";
    if (!stage.in.empty()) {
      incoming = Slurp(stage.in);
      if (incoming == "walk miss" || incoming == "create miss" || incoming == "fd miss")
        return incoming;
      have_in = true;
    }
    g_guest_code = -1;
    produced = RunOne(stage.argv, incoming, have_in);
    if (!stage.out.empty()) {
      std::string err = stage.append ? Append(stage.out, produced) : Store(stage.out, produced);
      if (!err.empty()) return err;
      produced.clear();
    }
    if (i + 1 < parts.size()) {
      incoming = ThroughPipe(produced);
      have_in = true;
    }
  }
  return produced;
}

bool NameStart(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool NameChar(char c) { return NameStart(c) || (c >= '0' && c <= '9'); }

std::string Value(const std::string& name) {
  if (name == "?" || name == "ERRORLEVEL") return jit_status("");
  std::string got = jit_env((std::string("get") + '\x1f' + name).c_str());
  if (got == "env miss") return "";
  return got;
}

std::string Expand(const std::string& line) {
  std::string out;
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '$' && i + 1 < line.size()) {
      if (line[i + 1] == '?') {
        out += Value("?");
        ++i;
        continue;
      }
      if (line[i + 1] == '{') {
        size_t end = line.find('}', i + 2);
        if (end != std::string::npos) {
          out += Value(line.substr(i + 2, end - (i + 2)));
          i = end;
          continue;
        }
      } else if (NameStart(line[i + 1])) {
        size_t j = i + 1;
        while (j < line.size() && NameChar(line[j])) ++j;
        out += Value(line.substr(i + 1, j - (i + 1)));
        i = j - 1;
        continue;
      }
    } else if (line[i] == '%') {
      size_t end = line.find('%', i + 1);
      if (end != std::string::npos && end > i + 1) {
        std::string name = line.substr(i + 1, end - (i + 1));
        bool ok = NameStart(name[0]);
        for (size_t k = 1; ok && k < name.size(); ++k) ok = NameChar(name[k]);
        if (ok) {
          out += Value(name);
          i = end;
          continue;
        }
      }
    }
    out.push_back(line[i]);
  }
  return out;
}

struct Step {
  std::string cmd;
  char link = 0;
};

std::vector<Step> Chain(const std::string& line) {
  std::vector<Step> steps;
  std::string cur;
  char link = 0;
  for (size_t i = 0; i < line.size(); ++i) {
    if (i + 1 < line.size() && line[i] == '&' && line[i + 1] == '&') {
      steps.push_back(Step{cur, link});
      cur.clear();
      link = '&';
      ++i;
    } else if (i + 1 < line.size() && line[i] == '|' && line[i + 1] == '|') {
      steps.push_back(Step{cur, link});
      cur.clear();
      link = '|';
      ++i;
    } else {
      cur.push_back(line[i]);
    }
  }
  steps.push_back(Step{cur, link});
  return steps;
}

std::string jit_sh(const char* args) {
  if (!plan9_local()) return plan9_hop("sh", args != nullptr ? args : "");
  std::string line = Trim(args != nullptr ? args : "");
  if (line.empty()) return Done("", false);
  std::vector<std::string> seq;
  std::string cur;
  for (char c : line) {
    if (c == ';') {
      seq.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  seq.push_back(cur);

  std::string all;
  int code = 0;
  bool keep = false;
  bool any = false;
  for (const std::string& part : seq) {
    std::string one_line = Trim(part);
    if (one_line.empty()) continue;
    any = true;
    for (const Step& step : Chain(one_line)) {
      std::string cmd = Trim(step.cmd);
      if (cmd.empty()) return Done("sh: syntax", false);
      if (step.link == '&' && code != 0) continue;
      if (step.link == '|' && code == 0) continue;
      cmd = Expand(cmd);
      std::vector<std::string> head = Tokens(cmd);
      g_guest_code = -1;
      std::string one = ExecLine(cmd);
      if (!head.empty() && head[0] == "status") keep = true;
      else {
        code = g_guest_code >= 0 ? g_guest_code : CodeOf(one);
        jit_status((std::string("set") + '\x1f' + std::to_string(code)).c_str());
      }
      if (!one.empty()) {
        if (!all.empty()) all.push_back('\n');
        all += one;
      }
    }
  }
  if (!any) return Done("", false);
  if (!keep) jit_status((std::string("set") + '\x1f' + std::to_string(code)).c_str());
  return all;
}

}  // namespace wasmgocos
