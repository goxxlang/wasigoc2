// Native Linux/WSL backend for WASMNix. Entry points keep Linux
// man-pages names (getpid, uname, ...). WSL is the same kernel this
// process is already on — wslpath / wslinfo / interop are binaries
// and env on this hop. Nix is the nix CLI, not a second Win32 stack.
#include "nix/posix_host.hpp"

#include <cstring>
#include <string>

extern "C" int wasmnix_call(const char* api, const char* args, char* out,
                            unsigned cap) {
  std::string reply = wasmnix::posix_call(api, args);
  return wasmnix::fill_out(out, cap, reply);
}
