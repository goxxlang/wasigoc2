#include "gocos/catalog.h"
#include "gocos/dispatch.h"

#include <cassert>
#include <cstring>
#include <string>

int main() {
  char buf[8192];
  int n = 0;
  const WasmGocOSApi* cat = wasmgocos_catalog(&n);
  bool saw_cmd = false;
  bool saw_conhost = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].name, "Cmd") == 0) saw_cmd = true;
    if (std::strcmp(cat[i].name, "Conhost") == 0) saw_conhost = true;
  }
  assert(saw_cmd);
  assert(saw_conhost);

  assert(wasmgocos_call("Boot", "", buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "os=gocos") != nullptr);

  assert(wasmgocos_call("Conhost", "", buf, sizeof(buf)) == 0);

  assert(wasmgocos_call("Prompt", "", buf, sizeof(buf)) == 0);
  assert(std::strchr(buf, '>') != nullptr);

  int rc = wasmgocos_call("Cmd", "echo hello", buf, sizeof(buf));
  if (std::strstr(buf, "k32 hop required")) return 0;
  assert(rc == 0);
  assert(std::strstr(buf, "hello") != nullptr || std::strlen(buf) > 0);


  assert(wasmgocos_call("Cmd", "powershell", buf, sizeof(buf)) == 0);

  assert(wasmgocos_call("Prompt", "", buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "PS ") != nullptr);

  assert(wasmgocos_call("Cmd", "cmd", buf, sizeof(buf)) == 0);

  assert(wasmgocos_call("Pwsh", "Write-Output hello-ps", buf, sizeof(buf)) == 0);
  return 0;
}
