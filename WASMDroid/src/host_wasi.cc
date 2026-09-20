// wasm32 backend for WASMDroid — same bionic_call as native Linux.
#include "droid/bionic_host.hpp"

#include <string>

extern "C" int wasmdroid_call(const char* api, const char* args, char* out,
                              unsigned cap) {
  std::string reply = wasmdroid::bionic_call(api, args);
  return wasmdroid::fill_out(out, cap, reply);
}
