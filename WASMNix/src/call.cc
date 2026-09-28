#include "nix/wsl.hpp"

#include <string>

extern "C" int wasmnix_call(const char* api, const char* args, char* out, unsigned cap) {
  std::string reply = wasmnix::wsl_call(api, args);
  return wasmnix::fill_out(out, cap, reply);
}
