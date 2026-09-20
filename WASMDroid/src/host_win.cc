// Native Windows backend for WASMDroid. Bionic/Binder/KVM/kernel hops
// run in-module. Optional adb is an honest extra when present.
#include "droid/bionic_host.hpp"

#include <string>

extern "C" int wasmdroid_call(const char* api, const char* args, char* out,
                              unsigned cap) {
  std::string reply = wasmdroid::bionic_call(api, args);
  return wasmdroid::fill_out(out, cap, reply);
}
