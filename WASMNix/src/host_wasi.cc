// libc backend for WASMNix — the wasm32 path (host_linux.cc is native).
// Native Linux still compiles host_linux.cc (libc / nix / wslpath).
#include "nix/posix_host.hpp"

#include <cstring>
#include <string>

extern "C" int wasmnix_call(const char* api, const char* args, char* out,
                            unsigned cap) {
  std::string reply = wasmnix::posix_call(api, args);
  return wasmnix::fill_out(out, cap, reply);
}
