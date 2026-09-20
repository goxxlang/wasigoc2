// Native Linux backend for WASMDroid. Bionic-shaped names (getpid,
// __system_property_get) plus in-module Binder / KVM / kernel hops.
#include "droid/bionic_host.hpp"

#include <string>

extern "C" int wasmdroid_call(const char* api, const char* args, char* out,
                              unsigned cap) {
  std::string reply = wasmdroid::bionic_call(api, args);
  return wasmdroid::fill_out(out, cap, reply);
}
