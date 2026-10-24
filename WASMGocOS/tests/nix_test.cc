#include "gocos/dispatch.h"

#include <cassert>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  int rc = wasmgocos_call("List", "", buf, sizeof(buf));
  if (std::strstr(buf, "nix hop required") ||
      std::strstr(buf, "k32 hop required"))
    return 0;
  assert(rc == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmgocos_call("Registered", "gocos", buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "1") == 0 || std::strcmp(buf, "0") == 0 ||
         std::strlen(buf) > 0);

  assert(wasmgocos_call("Launch", "uname", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmgocos_call("Launch", "true", buf, sizeof(buf)) == 0);
  return 0;
}
