#include "gocos/dispatch.h"

#include <cassert>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  int rc = wasmgocos_call("Present", "", buf, sizeof(buf));
  if (std::strstr(buf, "k32 hop required")) return 0;
  assert(rc == 0);
  assert(std::strstr(buf, "gocdesk") != nullptr);

  assert(wasmgocos_call("GpuInit", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmgocos_call("FlushUI", "", buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "FlushUI") != nullptr ||
         std::strstr(buf, "gocdesk") != nullptr);

  assert(wasmgocos_call("IFrame", "", buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "iframe") != nullptr);

  rc = wasmgocos_call("CreateWindow", "STATIC", buf, sizeof(buf));
  if (std::strstr(buf, "k32 hop required") == nullptr) {
    assert(rc == 0);
    assert(std::strlen(buf) > 0);
  }

  rc = wasmgocos_call("BuildId", "", buf, sizeof(buf));
  assert(rc == 0);
  assert(std::strstr(buf, "gocsys") != nullptr);
  return 0;
}
