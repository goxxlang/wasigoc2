// libc backend for WASMWin32 — the wasm32 path (host_win.cc is native-only).
// Native Windows still compiles host_win.cc (kernel32 / wsl.exe).
#include "win32/wasi_host.hpp"

#include <cstring>
#include <string>

extern "C" int wasmwin32_call(const char* api, const char* args, char* out,
                              unsigned cap) {
  std::string reply = wasmwin32::wasi_call(api, args);
  return wasmwin32::fill_out(out, cap, reply);
}
