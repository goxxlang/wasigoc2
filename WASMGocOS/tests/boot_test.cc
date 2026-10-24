#include "gocos/catalog.h"
#include "gocos/dispatch.h"

#include <cassert>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  int n = 0;
  const WasmGocOSApi* cat = wasmgocos_catalog(&n);
  assert(n >= 40);
  bool saw_boot = false;
  bool saw_krnl = false;
  bool saw_sys = false;
  bool saw_nix = false;
  bool saw_desk = false;
  bool saw_shell = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].name, "Boot") == 0) saw_boot = true;
    if (std::strcmp(cat[i].name, "CreateProcess") == 0) saw_krnl = true;
    if (std::strcmp(cat[i].name, "LoadModule") == 0) saw_sys = true;
    if (std::strcmp(cat[i].name, "Launch") == 0) saw_nix = true;
    if (std::strcmp(cat[i].name, "Present") == 0) saw_desk = true;
    if (std::strcmp(cat[i].name, "Cmd") == 0) saw_shell = true;
    if (std::strcmp(cat[i].lib, "gocdesk") == 0) saw_desk = true;
  }
  assert(saw_boot);
  assert(saw_krnl);
  assert(saw_sys);
  assert(saw_nix);
  assert(saw_desk);
  assert(saw_shell);

  assert(wasmgocos_call("Boot", "", buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "krnl=") != nullptr);
  assert(std::strstr(buf, "nix=") != nullptr);
  assert(std::strstr(buf, "sys=") != nullptr);
  assert(std::strstr(buf, "ns=plan9") != nullptr);
  assert(std::strstr(buf, "desk=plan9") != nullptr);
  assert(std::strstr(buf, "os=gocos") != nullptr);
  assert(std::strstr(buf, "session=") != nullptr);
  assert(std::strstr(buf, "station=") != nullptr);
  assert(std::strstr(buf, "hv=") != nullptr);

  int sm = wasmgocos_call("Session", "", buf, sizeof(buf));
  if (std::strstr(buf, "k32 hop required") == nullptr) {
    assert(sm == 0);
    assert(std::strlen(buf) > 0);
  }

  int st = wasmgocos_call("Station", "", buf, sizeof(buf));
  if (std::strstr(buf, "k32 hop required") == nullptr) {
    assert(st == 0);
  }

  assert(wasmgocos_call("no_such_gocos_api", "", buf, sizeof(buf)) != 0);
  assert(std::strstr(buf, "unknown api") != nullptr ||
         std::strstr(buf, "k32 hop required") != nullptr ||
         std::strstr(buf, "nix hop required") != nullptr);
  return 0;
}
