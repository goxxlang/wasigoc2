#ifndef WASMNIX_INCLUDE_NIX_WSL_HPP_
#define WASMNIX_INCLUDE_NIX_WSL_HPP_

// WSL. Distributions, the distro filesystem, /mnt drives, wslpath,
// interop, .wslconfig, and /etc/wsl.conf. One machine.

#include "nix/catalog.h"
#include "nix/dispatch.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace wasmnix {

inline bool eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline std::string err_msg(const char* msg) { return std::string("error: ") + msg; }

inline void split1f(const char* a, std::string* left, std::string* right) {
  const char* p = a ? std::strchr(a, '\x1f') : nullptr;
  if (!p) {
    *left = a ? a : "";
    right->clear();
    return;
  }
  left->assign(a, p);
  *right = p + 1;
}

inline std::vector<std::string> split_ws(const std::string& s) {
  std::vector<std::string> v;
  std::string cur;
  bool q = false;
  for (char c : s) {
    if (c == '"') {
      q = !q;
      continue;
    }
    if (!q && (c == ' ' || c == '\t')) {
      if (!cur.empty()) {
        v.push_back(cur);
        cur.clear();
      }
      continue;
    }
    cur.push_back(c);
  }
  if (!cur.empty()) v.push_back(cur);
  return v;
}

struct FsEnt {
  bool dir = false;
  std::string data;
};

struct Distro {
  std::string name;
  int version = 2;
  bool running = false;
  std::string user = "root";
  unsigned uid = 0;
  std::string location;
  bool interop = true;
  bool append_windows_path = true;
  bool drive_mounting = true;
  bool systemd = false;
  bool sparse = false;
  std::uint64_t vhd_bytes = 1024ull * 1024ull * 1024ull;
  std::string boot_command;
  std::string cwd = "/root";
  std::map<std::string, std::string> env;
  std::map<std::string, FsEnt> fs;
  unsigned next_pid = 1;
};

struct Engine {
  std::string default_name;
  int default_version = 2;
  std::string networking = "nat";
  bool localhost_forwarding = true;
  bool gui = true;
  bool nested_virt = false;
  bool dns_tunneling = true;
  bool firewall = true;
  std::string memory = "50%";
  unsigned processors = 8;
  std::string swap = "25%";
  std::string vm_id;
  std::vector<std::string> mounts;
  std::vector<Distro> distros;
  std::map<std::string, std::string> blobs;
  std::map<char, std::map<std::string, FsEnt>> drives;
  unsigned vm_seq = 1;
};

inline Engine& wsl() {
  static Engine e;
  return e;
}

inline const char* kOnline[] = {
    "Ubuntu",
    "Ubuntu-24.04",
    "Ubuntu-22.04",
    "Debian",
    "kali-linux",
    "openSUSE-Tumbleweed",
    "SUSE-Linux-Enterprise-15-SP6",
    "OracleLinux_9_5",
    "AlmaLinux-9",
};

inline Distro* find_distro(const std::string& name) {
  for (Distro& d : wsl().distros) {
    if (d.name == name) return &d;
  }
  return nullptr;
}

inline Distro* default_distro() {
  if (wsl().default_name.empty()) return nullptr;
  return find_distro(wsl().default_name);
}

inline std::string norm_unix(const std::string& base, const std::string& in) {
  std::string raw = in;
  if (raw.empty()) raw = base;
  if (raw[0] != '/') raw = base + "/" + raw;
  std::vector<std::string> parts;
  std::string cur;
  for (size_t i = 0; i <= raw.size(); ++i) {
    if (i == raw.size() || raw[i] == '/') {
      if (cur == "..") {
        if (!parts.empty()) parts.pop_back();
      } else if (!cur.empty() && cur != ".") {
        parts.push_back(cur);
      }
      cur.clear();
    } else {
      cur.push_back(raw[i]);
    }
  }
  std::string out = "/";
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out += "/";
    out += parts[i];
  }
  if (out.empty()) out = "/";
  return out;
}

inline bool is_drive_path(const std::string& p) {
  return p.size() >= 6 && p.compare(0, 5, "/mnt/") == 0 &&
         std::isalpha(static_cast<unsigned char>(p[5])) && (p.size() == 6 || p[6] == '/');
}

inline char drive_letter(const std::string& p) {
  char c = p[5];
  if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return c;
}

inline std::string drive_rest(const std::string& p) {
  if (p.size() <= 6) return "/";
  return p.substr(6);
}

inline std::map<std::string, FsEnt>* fs_of(Distro& d, const std::string& path, std::string* key) {
  if (d.drive_mounting && is_drive_path(path)) {
    *key = drive_rest(path);
    if (key->empty()) *key = "/";
    return &wsl().drives[drive_letter(path)];
  }
  *key = path.empty() ? "/" : path;
  return &d.fs;
}

inline void ensure_dir(std::map<std::string, FsEnt>& fs, const std::string& path) {
  fs[path].dir = true;
}

inline void seed_distro(Distro& d) {
  ensure_dir(d.fs, "/");
  ensure_dir(d.fs, "/etc");
  ensure_dir(d.fs, "/root");
  ensure_dir(d.fs, "/home");
  ensure_dir(d.fs, "/tmp");
  ensure_dir(d.fs, "/mnt");
  ensure_dir(d.fs, "/usr");
  ensure_dir(d.fs, "/bin");
  ensure_dir(d.fs, "/run");
  ensure_dir(d.fs, "/run/WSL");
  d.fs["/etc/os-release"] = FsEnt{false, "NAME=\"" + d.name + "\"\nID=wsl\n"};
  d.fs["/etc/wsl.conf"] = FsEnt{
      false,
      "[user]\ndefault=" + d.user +
          "\n[boot]\nsystemd=false\n[interop]\nenabled=true\nappendWindowsPath=true\n"
          "[automount]\nenabled=true\n"};
  d.fs["/init"] = FsEnt{false, "wsl"};
  d.env["WSL_DISTRO_NAME"] = d.name;
  d.env["WSL_INTEROP"] = "/run/WSL/interop";
  d.env["WSLENV"] = "";
  d.env["NAME"] = d.name;
  if (wsl().gui) {
    d.env["DISPLAY"] = ":0";
    d.env["WAYLAND_DISPLAY"] = "wayland-0";
    d.env["PULSE_SERVER"] = "unix:/mnt/wslg/PulseServer";
  }
  d.cwd = "/root";
}

inline void start_distro(Distro& d) {
  if (d.running) return;
  d.running = true;
  if (wsl().vm_id.empty()) wsl().vm_id = "vm-" + std::to_string(wsl().vm_seq++);
}

inline std::string parent_dir(const std::string& path) {
  if (path == "/") return "/";
  auto s = path.rfind('/');
  if (s == std::string::npos || s == 0) return "/";
  return path.substr(0, s);
}

inline std::string base_name(const std::string& path) {
  auto s = path.rfind('/');
  if (s == std::string::npos) return path;
  return path.substr(s + 1);
}

inline std::string win_from_unix(const Distro& d, const std::string& unix) {
  std::string p = norm_unix("/", unix);
  if (is_drive_path(p)) {
    std::string rest = drive_rest(p);
    if (!rest.empty() && rest[0] == '/') rest.erase(rest.begin());
    std::string out;
    out.push_back(static_cast<char>(drive_letter(p) - 'a' + 'A'));
    out += ":\\";
    for (size_t i = 0; i < rest.size(); ++i) {
      if (rest[i] == '/')
        out.push_back('\\');
      else
        out.push_back(rest[i]);
    }
    while (!out.empty() && out.back() == '\\' && out.size() > 3) out.pop_back();
    return out;
  }
  std::string out = "\\\\wsl$\\" + d.name;
  if (p != "/") out += p;
  for (char& c : out) {
    if (c == '/') c = '\\';
  }
  return out;
}

inline std::string unix_from_win(const std::string& win) {
  if (win.size() >= 2 && win[1] == ':') {
    char letter = win[0];
    if (letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter - 'A' + 'a');
    std::string rest = win.size() > 2 ? win.substr(2) : "";
    if (!rest.empty() && (rest[0] == '\\' || rest[0] == '/')) rest.erase(rest.begin());
    std::string out = std::string("/mnt/") + letter;
    if (!rest.empty()) out += "/";
    for (char c : rest) out.push_back(c == '\\' ? '/' : c);
    return out;
  }
  const std::string pfx = "\\\\wsl$\\";
  if (win.rfind(pfx, 0) == 0 || win.rfind("//wsl$/", 0) == 0) {
    std::string t = win.substr(win[0] == '/' ? 8 : 7);
    auto sl = t.find_first_of("\\/");
    std::string rel = sl == std::string::npos ? "" : t.substr(sl + 1);
    std::string out = "/";
    for (char c : rel) out.push_back(c == '\\' ? '/' : c);
    return out.empty() ? "/" : out;
  }
  return err_msg("path: not a Windows path");
}

inline std::string list_dir(Distro& d, const std::string& path) {
  std::string key;
  auto* fs = fs_of(d, path, &key);
  if (!fs->count(key) || !fs->at(key).dir) return err_msg("ls: not a directory");
  std::string prefix = key == "/" ? "/" : key + "/";
  std::string out;
  for (const auto& kv : *fs) {
    if (kv.first == key) continue;
    if (kv.first.rfind(prefix, 0) != 0) continue;
    std::string rest = kv.first.substr(prefix.size());
    if (rest.empty() || rest.find('/') != std::string::npos) continue;
    if (!out.empty()) out.push_back('\n');
    out += rest;
  }
  return out;
}

inline std::string exec_line(Distro& d, const std::string& line) {
  std::string cmd = line;
  while (!cmd.empty() && cmd[0] == ' ') cmd.erase(cmd.begin());
  if (cmd.empty()) return "";
  auto argv = split_ws(cmd);
  if (argv.empty()) return "";
  if (argv[0] == "sh" && argv.size() >= 3 && argv[1] == "-c") {
    std::string rest;
    auto p = cmd.find("-c");
    rest = cmd.substr(p + 2);
    while (!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
    if (!rest.empty() && rest.front() == '"' && rest.back() == '"') rest = rest.substr(1, rest.size() - 2);
    return exec_line(d, rest);
  }
  unsigned pid = d.next_pid++;
  if (argv[0] == "true" || argv[0] == ":") return "";
  if (argv[0] == "false") return err_msg("exit 1");
  if (argv[0] == "pwd") return d.cwd;
  if (argv[0] == "whoami") return d.user;
  if (argv[0] == "hostname") return d.name;
  if (argv[0] == "echo") {
    std::string o;
    for (size_t i = 1; i < argv.size(); ++i) {
      if (i > 1) o.push_back(' ');
      if (argv[i] == "$$")
        o += std::to_string(pid);
      else
        o += argv[i];
    }
    return o;
  }
  if (argv[0] == "id") {
    return "uid=" + std::to_string(d.uid) + "(" + d.user + ") gid=" + std::to_string(d.uid);
  }
  if (argv[0] == "uname") {
    std::string rel = d.version == 1 ? "microsoft-standard-WSL1" : "microsoft-standard-WSL2";
    if (argv.size() == 1 || argv[1] == "-a" || argv[1] == "-s") {
      if (argv.size() > 1 && argv[1] == "-s") return "Linux";
      return "Linux " + d.name + " " + rel + " #1 SMP x86_64 GNU/Linux";
    }
    if (argv[1] == "-r") return rel;
    if (argv[1] == "-n") return d.name;
    return "Linux";
  }
  if (argv[0] == "cd") {
    std::string to = argv.size() > 1 ? norm_unix(d.cwd, argv[1]) : "/root";
    std::string key;
    auto* fs = fs_of(d, to, &key);
    if (!fs->count(key) || !fs->at(key).dir) return err_msg("cd: no such directory");
    d.cwd = to;
    return "";
  }
  if (argv[0] == "ls") {
    std::string p = argv.size() > 1 ? norm_unix(d.cwd, argv[1]) : d.cwd;
    return list_dir(d, p);
  }
  if (argv[0] == "cat") {
    if (argv.size() < 2) return err_msg("cat: missing path");
    std::string p = norm_unix(d.cwd, argv[1]);
    std::string key;
    auto* fs = fs_of(d, p, &key);
    auto it = fs->find(key);
    if (it == fs->end() || it->second.dir) return err_msg("cat: no such file");
    return it->second.data;
  }
  if (argv[0] == "mkdir") {
    if (argv.size() < 2) return err_msg("mkdir: missing path");
    std::string p = norm_unix(d.cwd, argv.back());
    std::string key;
    auto* fs = fs_of(d, p, &key);
    ensure_dir(*fs, key);
    ensure_dir(*fs, parent_dir(key));
    return "";
  }
  if (argv[0] == "rm" || argv[0] == "rmdir") {
    if (argv.size() < 2) return err_msg("rm: missing path");
    std::string p = norm_unix(d.cwd, argv.back());
    std::string key;
    auto* fs = fs_of(d, p, &key);
    fs->erase(key);
    return "";
  }
  if (argv[0] == "touch") {
    if (argv.size() < 2) return err_msg("touch: missing path");
    std::string p = norm_unix(d.cwd, argv[1]);
    std::string key;
    auto* fs = fs_of(d, p, &key);
    if (!fs->count(key)) fs->emplace(key, FsEnt{false, ""});
    return "";
  }
  if (argv[0] == "cp" || argv[0] == "mv") {
    if (argv.size() < 3) return err_msg("cp: missing path");
    std::string src = norm_unix(d.cwd, argv[1]);
    std::string dst = norm_unix(d.cwd, argv[2]);
    std::string sk, dk;
    auto* sfs = fs_of(d, src, &sk);
    auto* dfs = fs_of(d, dst, &dk);
    auto it = sfs->find(sk);
    if (it == sfs->end()) return err_msg("cp: no such file");
    (*dfs)[dk] = it->second;
    if (argv[0] == "mv") sfs->erase(sk);
    return "";
  }
  if (argv[0] == "printenv" || argv[0] == "env") {
    if (argv[0] == "printenv" && argv.size() > 1) {
      auto it = d.env.find(argv[1]);
      if (it == d.env.end()) return err_msg("printenv: not found");
      return it->second;
    }
    std::string o;
    for (const auto& kv : d.env) {
      if (!o.empty()) o.push_back('\n');
      o += kv.first + "=" + kv.second;
    }
    return o;
  }
  if (argv[0] == "export" && argv.size() > 1) {
    auto eqp = argv[1].find('=');
    if (eqp == std::string::npos) return err_msg("export: NAME=value");
    d.env[argv[1].substr(0, eqp)] = argv[1].substr(eqp + 1);
    return "";
  }
  if (argv[0] == "test" && argv.size() >= 3 && argv[1] == "-e") {
    std::string p = norm_unix(d.cwd, argv[2]);
    std::string key;
    auto* fs = fs_of(d, p, &key);
    if (!fs->count(key)) return err_msg("exit 1");
    return "";
  }
  if (argv[0] == "wslpath") {
    std::string flag = "-a";
    std::string path;
    for (size_t i = 1; i < argv.size(); ++i) {
      if (argv[i] == "-w" || argv[i] == "-u" || argv[i] == "-m")
        flag = argv[i];
      else
        path = argv[i];
    }
    if (flag == "-u") return unix_from_win(path);
    std::string unix = path.find(':') != std::string::npos || path.rfind("\\\\", 0) == 0
                           ? unix_from_win(path)
                           : norm_unix(d.cwd, path);
    if (unix.rfind("error:", 0) == 0) return unix;
    std::string win = win_from_unix(d, unix);
    if (flag == "-m") {
      for (char& c : win)
        if (c == '\\') c = '/';
    }
    return win;
  }
  if (argv[0] == "wslinfo") {
    std::string flag = argv.size() > 1 ? argv[1] : "--wsl-version";
    if (flag == "--networking-mode") return wsl().networking;
    if (flag == "--wsl-version") return std::to_string(d.version);
    if (flag == "--vm-id") return wsl().vm_id.empty() ? err_msg("wslinfo: distro stopped") : wsl().vm_id;
    if (flag == "--nthreads") return std::to_string(wsl().processors);
    if (flag == "--ms-wsl") return "1";
    return err_msg("wslinfo: unknown flag");
  }
  if (argv[0].size() > 4 && argv[0].compare(argv[0].size() - 4, 4, ".exe") == 0) {
    if (!d.interop) return err_msg("interop disabled");
    return std::string("win32 ") + cmd;
  }
  return err_msg("exec: not found");
}

inline std::string row_distro(const Distro& d) {
  std::string name = d.name;
  if (d.name == wsl().default_name) name = "*" + name;
  return name + "\x1f" + (d.running ? "Running" : "Stopped") + "\x1f" + std::to_string(d.version);
}

inline std::string wsl_install(const std::string& spec) {
  std::string name = spec.empty() ? "Ubuntu" : spec;
  auto cut = name.find('\x1f');
  if (cut != std::string::npos) name = name.substr(0, cut);
  if (find_distro(name)) return err_msg("install: already registered");
  Distro d;
  d.name = name;
  d.version = wsl().default_version;
  d.location = "C:\\WSL\\" + name;
  d.running = true;
  seed_distro(d);
  start_distro(d);
  wsl().distros.push_back(std::move(d));
  if (wsl().default_name.empty()) wsl().default_name = name;
  return "ok";
}

inline std::string require_distro(const std::string& name, Distro** out) {
  Distro* d = name.empty() ? default_distro() : find_distro(name);
  if (!d) return err_msg(name.empty() ? "no distribution" : "distribution not found");
  *out = d;
  return "";
}

inline std::string serialize(const Distro& d) {
  std::string o = "V\x1f" + std::to_string(d.version) + "\x1f" + d.user + "\n";
  for (const auto& kv : d.fs) {
    o += kv.second.dir ? "D\x1f" : "F\x1f";
    o += kv.first;
    o += "\x1f";
    o += kv.second.data;
    o += "\n";
  }
  return o;
}

inline void apply_blob(Distro& d, const std::string& blob) {
  d.fs.clear();
  size_t i = 0;
  while (i < blob.size()) {
    auto nl = blob.find('\n', i);
    if (nl == std::string::npos) nl = blob.size();
    std::string line = blob.substr(i, nl - i);
    i = nl + 1;
    if (line.size() < 2) continue;
    std::string a, b;
    split1f(line.c_str() + 2, &a, &b);
    if (line[0] == 'V') {
      d.version = std::atoi(a.c_str());
      if (!b.empty()) d.user = b;
    } else if (line[0] == 'D') {
      d.fs[a] = FsEnt{true, ""};
    } else if (line[0] == 'F') {
      d.fs[a] = FsEnt{false, b};
    }
  }
}

inline std::string wsl_call(const char* api, const char* args) {
  if (!api || !api[0]) return err_msg("wsl needs an API name");
  const char* a = args ? args : "";
  std::string name = api;
  if (name == "WslList") name = "List";
  if (name == "WslExec") name = "Exec";
  if (name == "WslIsDistributionRegistered") name = "IsDistributionRegistered";
  if (name == "WslShutdown") name = "Shutdown";
  if (name == "WslTerminate") name = "Terminate";
  if (name == "WslStatus") name = "Status";
  if (name == "WslVersion") name = "Version";
  if (name == "WslSetVersion") name = "SetVersion";
  if (name == "WslExport") name = "Export";
  if (name == "WslImport") name = "Import";
  if (name == "WslUnregister") name = "Unregister";
  if (name == "WslPath" || name == "wslpath") name = "Path";
  if (name == "WslLaunchWin32") name = "LaunchWin32";
  if (name == "WslInfo" || name == "wslinfo") name = "Info";
  if (name == "WslVar" || name == "wslvar") name = "Var";

  if (name == "ListOnline") {
    std::string o;
    for (const char* n : kOnline) {
      if (!o.empty()) o.push_back('\n');
      o += n;
    }
    return o;
  }
  if (name == "List" || name == "ListRunning" || name == "ListQuiet") {
    if (std::strcmp(a, "online") == 0) return wsl_call("ListOnline", "");
    std::string o;
    for (const Distro& d : wsl().distros) {
      if (name == "ListRunning" && !d.running) continue;
      if (!o.empty()) o.push_back('\n');
      if (name == "ListQuiet")
        o += d.name;
      else
        o += row_distro(d);
    }
    return o;
  }
  if (name == "Install") return wsl_install(a);
  if (name == "SetDefault") {
    if (!find_distro(a)) return err_msg("set-default: not found");
    wsl().default_name = a;
    return "ok";
  }
  if (name == "SetDefaultUser") {
    std::string dist, user;
    split1f(a, &dist, &user);
    Distro* d = nullptr;
    std::string e = require_distro(dist, &d);
    if (!e.empty()) return e;
    if (user.empty()) return err_msg("set-default-user: missing user");
    d->user = user;
    d->uid = user == "root" ? 0 : 1000;
    d->cwd = user == "root" ? "/root" : "/home/" + user;
    ensure_dir(d->fs, d->cwd);
    return "ok";
  }
  if (name == "SetVersion") {
    std::string dist, ver;
    split1f(a, &dist, &ver);
    Distro* d = nullptr;
    std::string e = require_distro(dist, &d);
    if (!e.empty()) return e;
    int v = std::atoi(ver.c_str());
    if (v != 1 && v != 2) return err_msg("set-version: 1 or 2");
    d->running = false;
    d->version = v;
    return "ok";
  }
  if (name == "Terminate") {
    Distro* d = nullptr;
    std::string e = require_distro(a, &d);
    if (!e.empty()) return e;
    d->running = false;
    return "ok";
  }
  if (name == "Shutdown") {
    for (Distro& d : wsl().distros) d.running = false;
    wsl().vm_id.clear();
    return "ok";
  }
  if (name == "Unregister") {
    auto& v = wsl().distros;
    for (auto it = v.begin(); it != v.end(); ++it) {
      if (it->name == a) {
        if (wsl().default_name == a) wsl().default_name.clear();
        v.erase(it);
        if (wsl().default_name.empty() && !v.empty()) wsl().default_name = v[0].name;
        return "ok";
      }
    }
    return err_msg("unregister: not found");
  }
  if (name == "IsDistributionRegistered") return find_distro(a) ? "1" : "0";
  if (name == "Export") {
    std::string dist, file;
    split1f(a, &dist, &file);
    Distro* d = nullptr;
    std::string e = require_distro(dist, &d);
    if (!e.empty()) return e;
    if (file.empty()) return err_msg("export: missing file");
    wsl().blobs[file] = serialize(*d);
    return "ok";
  }
  if (name == "Import" || name == "ImportInPlace") {
    std::string dist, rest, loc, file;
    split1f(a, &dist, &rest);
    split1f(rest.c_str(), &loc, &file);
    if (dist.empty()) return err_msg("import: missing name");
    if (find_distro(dist)) return err_msg("import: already registered");
    if (name == "Import" && (file.empty() || !wsl().blobs.count(file)))
      return err_msg("import: no such file");
    Distro d;
    d.name = dist;
    d.location = loc.empty() ? "C:\\WSL\\" + dist : loc;
    d.version = wsl().default_version;
    seed_distro(d);
    if (name == "Import") apply_blob(d, wsl().blobs[file]);
    d.name = dist;
    wsl().distros.push_back(std::move(d));
    if (wsl().default_name.empty()) wsl().default_name = dist;
    return "ok";
  }
  if (name == "Mount") {
    if (!a[0]) {
      std::string o;
      for (const std::string& m : wsl().mounts) {
        if (!o.empty()) o.push_back('\n');
        o += m;
      }
      return o;
    }
    wsl().mounts.push_back(a);
    return "ok";
  }
  if (name == "Unmount") {
    auto& m = wsl().mounts;
    if (!a[0]) {
      m.clear();
      return "ok";
    }
    for (auto it = m.begin(); it != m.end(); ++it) {
      if (*it == a) {
        m.erase(it);
        return "ok";
      }
    }
    return err_msg("unmount: not mounted");
  }
  if (name == "Status") {
    std::string o = "Default Distribution: " +
                    (wsl().default_name.empty() ? std::string("(none)") : wsl().default_name);
    o += "\nDefault Version: " + std::to_string(wsl().default_version);
    o += "\nNetworking mode: " + wsl().networking;
    o += "\nLocalhost forwarding: " + std::string(wsl().localhost_forwarding ? "1" : "0");
    o += "\nWSLg: " + std::string(wsl().gui ? "1" : "0");
    o += "\nNested virtualization: " + std::string(wsl().nested_virt ? "1" : "0");
    o += "\nDNS tunneling: " + std::string(wsl().dns_tunneling ? "1" : "0");
    o += "\nFirewall: " + std::string(wsl().firewall ? "1" : "0");
    o += "\nMemory: " + wsl().memory;
    o += "\nProcessors: " + std::to_string(wsl().processors);
    o += "\nSwap: " + wsl().swap;
    if (!wsl().vm_id.empty()) o += "\nVM: " + wsl().vm_id;
    return o;
  }
  if (name == "Version") return std::to_string(wsl().default_version);
  if (name == "Update") return "ok";
  if (name == "Config") {
    std::string key, val;
    split1f(a, &key, &val);
    if (key.empty()) return wsl_call("Status", "");
    if (key == "networkingMode") {
      if (val != "nat" && val != "mirrored" && val != "virtioproxy" && val != "none")
        return err_msg("config: networkingMode");
      wsl().networking = val;
    } else if (key == "memory")
      wsl().memory = val;
    else if (key == "processors")
      wsl().processors = static_cast<unsigned>(std::strtoul(val.c_str(), nullptr, 10));
    else if (key == "swap")
      wsl().swap = val;
    else if (key == "localhostForwarding")
      wsl().localhost_forwarding = val != "false" && val != "0";
    else if (key == "guiApplications")
      wsl().gui = val != "false" && val != "0";
    else if (key == "nestedVirtualization")
      wsl().nested_virt = val == "true" || val == "1";
    else if (key == "dnsTunneling")
      wsl().dns_tunneling = val != "false" && val != "0";
    else if (key == "firewall")
      wsl().firewall = val != "false" && val != "0";
    else if (key == "defaultVersion") {
      int v = std::atoi(val.c_str());
      if (v != 1 && v != 2) return err_msg("config: defaultVersion");
      wsl().default_version = v;
    } else
      return err_msg("config: unknown key");
    return "ok";
  }
  if (name == "GetConfiguration") {
    Distro* d = nullptr;
    std::string e = require_distro(a, &d);
    if (!e.empty()) return e;
    return std::to_string(d->version) + "\x1f" + std::to_string(d->uid) + "\x1f" +
           (d->interop ? "1" : "0") + "\x1f" + (d->append_windows_path ? "1" : "0") + "\x1f" +
           (d->drive_mounting ? "1" : "0") + "\x1f" + (d->systemd ? "1" : "0") + "\x1f" +
           (d->running ? "Running" : "Stopped") + "\x1f" + d->user + "\x1f" + d->location;
  }
  if (name == "Configure") {
    std::string dist, rest;
    split1f(a, &dist, &rest);
    Distro* d = nullptr;
    std::string e = require_distro(dist, &d);
    if (!e.empty()) return e;
    std::string uid, flags;
    split1f(rest.c_str(), &uid, &flags);
    if (!uid.empty()) d->uid = static_cast<unsigned>(std::strtoul(uid.c_str(), nullptr, 10));
    if (flags.find("interop=0") != std::string::npos) d->interop = false;
    if (flags.find("interop=1") != std::string::npos) d->interop = true;
    if (flags.find("append=0") != std::string::npos) d->append_windows_path = false;
    if (flags.find("append=1") != std::string::npos) d->append_windows_path = true;
    if (flags.find("drvfs=0") != std::string::npos) d->drive_mounting = false;
    if (flags.find("drvfs=1") != std::string::npos) d->drive_mounting = true;
    if (flags.find("systemd=1") != std::string::npos) d->systemd = true;
    if (flags.find("systemd=0") != std::string::npos) d->systemd = false;
    if (flags.find("sparse=1") != std::string::npos) d->sparse = true;
    if (flags.find("sparse=0") != std::string::npos) d->sparse = false;
    auto boot = flags.find("boot=");
    if (boot != std::string::npos) d->boot_command = flags.substr(boot + 5);
    return "ok";
  }
  if (name == "Manage") {
    std::string dist, rest, op, val;
    split1f(a, &dist, &rest);
    split1f(rest.c_str(), &op, &val);
    Distro* d = nullptr;
    std::string e = require_distro(dist, &d);
    if (!e.empty()) return e;
    if (op == "set-sparse") {
      d->sparse = val == "true" || val == "1";
      return "ok";
    }
    if (op == "move") {
      if (val.empty()) return err_msg("manage: missing location");
      d->location = val;
      return "ok";
    }
    if (op == "resize") {
      d->vhd_bytes = std::strtoull(val.c_str(), nullptr, 10);
      return "ok";
    }
    return err_msg("manage: set-sparse, move, or resize");
  }
  if (name == "Info") {
    Distro* d = default_distro();
    std::string flag = a[0] ? a : "--wsl-version";
    if (flag == "--networking-mode") return wsl().networking;
    if (flag == "--nthreads") return std::to_string(wsl().processors);
    if (flag == "--ms-wsl") return "1";
    if (!d) return err_msg("info: no distribution");
    if (flag == "--wsl-version") return std::to_string(d->version);
    if (flag == "--vm-id") {
      if (!d->running) return err_msg("info: distro stopped");
      return wsl().vm_id;
    }
    return err_msg("info: unknown flag");
  }
  if (name == "Var") {
    Distro* d = default_distro();
    if (!d) return err_msg("var: no distribution");
    if (!a[0]) return err_msg("var: missing name");
    auto it = d->env.find(a);
    if (it == d->env.end()) return err_msg("var: not found");
    return it->second;
  }
  if (name == "Path") {
    std::string flag, path;
    split1f(a, &flag, &path);
    if (path.empty()) {
      path = flag;
      flag = "-w";
    }
    Distro* d = default_distro();
    if (!d) return err_msg("path: no distribution");
    std::string unix = (path.find(':') != std::string::npos || path.rfind("\\\\", 0) == 0)
                           ? unix_from_win(path)
                           : norm_unix("/", path);
    if (unix.rfind("error:", 0) == 0) return unix;
    if (flag == "-u") return unix;
    std::string win = win_from_unix(*d, unix);
    if (flag == "-m") {
      for (char& c : win)
        if (c == '\\') c = '/';
    }
    return win;
  }
  if (name == "Read") {
    std::string dist, path;
    split1f(a, &dist, &path);
    Distro* d = nullptr;
    std::string e = require_distro(dist, &d);
    if (!e.empty()) return e;
    std::string p = norm_unix(d->cwd, path);
    std::string key;
    auto* fs = fs_of(*d, p, &key);
    auto it = fs->find(key);
    if (it == fs->end() || it->second.dir) return err_msg("read: no such file");
    return it->second.data;
  }
  if (name == "Write") {
    std::string dist, rest, path, data;
    split1f(a, &dist, &rest);
    split1f(rest.c_str(), &path, &data);
    Distro* d = nullptr;
    std::string e = require_distro(dist, &d);
    if (!e.empty()) return e;
    std::string p = norm_unix(d->cwd, path);
    std::string key;
    auto* fs = fs_of(*d, p, &key);
    ensure_dir(*fs, parent_dir(key));
    (*fs)[key] = FsEnt{false, data};
    return "ok";
  }
  if (name == "Exec" || name == "Launch") {
    std::string dist, cmd;
    split1f(a, &dist, &cmd);
    Distro* d = nullptr;
    if (!cmd.empty() && find_distro(dist)) {
      d = find_distro(dist);
    } else {
      cmd = a;
      std::string e = require_distro("", &d);
      if (!e.empty()) return e;
    }
    start_distro(*d);
    if (!d->boot_command.empty() && d->next_pid == 1) exec_line(*d, d->boot_command);
    return exec_line(*d, cmd);
  }
  if (name == "LaunchWin32") {
    Distro* d = nullptr;
    std::string e = require_distro("", &d);
    if (!e.empty()) return e;
    if (!d->interop) return err_msg("interop disabled");
    start_distro(*d);
    return std::string("win32 ") + a;
  }
  if (name == "Help") {
    return "List\nListOnline\nListRunning\nInstall\nSetDefault\nSetDefaultUser\n"
           "SetVersion\nExport\nImport\nImportInPlace\nUnregister\nTerminate\nShutdown\n"
           "Status\nVersion\nUpdate\nMount\nUnmount\nConfig\nGetConfiguration\nConfigure\n"
           "Manage\nInfo\nVar\nPath\nRead\nWrite\nExec\nLaunch\nLaunchWin32";
  }
  return std::string("error: unknown api ") + api;
}

inline int fill_out(char* out, unsigned cap, const std::string& s) {
  if (!out || cap == 0) return s.rfind("error:", 0) == 0 ? -1 : 0;
  unsigned n = static_cast<unsigned>(s.size());
  if (n + 1 > cap) n = cap - 1;
  std::memcpy(out, s.data(), n);
  out[n] = 0;
  return s.rfind("error:", 0) == 0 ? -1 : 0;
}

}  // namespace wasmnix

#endif  // WASMNIX_INCLUDE_NIX_WSL_HPP_
