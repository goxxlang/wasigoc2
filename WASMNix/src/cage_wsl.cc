// In-cage WSL Linux hop. wslpath is the DrvFs map. uname / pwd / hostname
// are the cage's own answers. An arbitrary command is refused: the
// distro process is created by the caller, not by a host shell.
#include "nix/cage.h"
#include "nix/drvfs.h"

#include <cstring>
#include <string>

#if defined(_WIN32) && !defined(__wasi__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <stdlib.h>
namespace std {
using ::malloc;
using ::calloc;
using ::realloc;
using ::free;
using ::strtol;
using ::strtoll;
using ::strtoul;
using ::strtoull;
}
#include "nix/posix_host.hpp"
#endif

namespace {

int Fill(char* out, unsigned cap, const std::string& s) {
  if (!out || cap == 0) return 0;
  unsigned n = static_cast<unsigned>(s.size());
  if (n + 1 > cap) n = cap - 1;
  std::memcpy(out, s.data(), n);
  out[n] = 0;
  return s.rfind("error:", 0) == 0 ? -1 : 0;
}

bool Eq(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }

#if defined(_WIN32) && !defined(__wasi__)
std::string UnameText() {
  char name[256];
  DWORD n = sizeof(name);
  if (!GetComputerNameA(name, &n) || !name[0]) return "Windows";
  return std::string("Windows ") + name;
}

std::string PwdText() {
  char buf[MAX_PATH];
  DWORD n = GetCurrentDirectoryA(MAX_PATH, buf);
  if (!n || n >= MAX_PATH) return "error: pwd";
  return buf;
}

std::string HostText() {
  char name[256];
  DWORD n = sizeof(name);
  if (!GetComputerNameA(name, &n) || !name[0]) return "error: hostname";
  return name;
}
#endif

}  // namespace

extern "C" int wasmnix_cage_call(const char* api, const char* args, char* out, unsigned cap) {
  const char* a = args ? args : "";
#if !defined(_WIN32) || defined(__wasi__)
  return Fill(out, cap, wasmnix::posix_call(api ? api : "", a));
#else
  std::string reply;
  if (Eq(api, "uname")) {
    reply = UnameText();
  } else if (Eq(api, "pwd")) {
    reply = PwdText();
  } else if (Eq(api, "hostname")) {
    reply = HostText();
  } else if (Eq(api, "echo")) {
    reply = a;
  } else if (Eq(api, "true")) {
    reply = "ok";
  } else if (Eq(api, "getuid")) {
    reply = "0";
  } else if (Eq(api, "wslpath")) {
    std::string flags, path;
    const char* cut = std::strchr(a, '\x1f');
    if (!cut) {
      path = a;
    } else {
      flags.assign(a, cut);
      path = cut + 1;
    }
    if (path.empty()) {
      path = flags;
      flags.clear();
    }
    reply = wasmnix::drvfs_translate(flags, path);
    if (reply.empty()) reply = PwdText();
  } else {
    reply = "error: no exec in the cage";
  }
  return Fill(out, cap, reply);
#endif
}
