// wasm32 backend for WASMGocOS — same gocos_call as native.
#include "gocos/host.hpp"

extern "C" int wasmgocos_call(const char* api, const char* args, char* out,
                              unsigned cap) {
  std::string reply = wasmgocos::gocos_call(api, args);
  return wasmgocos::fill_out(out, cap, reply);
}
