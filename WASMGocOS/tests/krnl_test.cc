#include "gocos/dispatch.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(WASMGOCOS_HAS_VCPU)
extern "C" uint32_t wasmturbo_vcpu_k32_exits(void);
#endif

int main() {
  char buf[4096];
  char boot[4096];
  int rc = wasmgocos_call("Boot", "", boot, sizeof(boot));
  if (rc != 0) std::fprintf(stderr, "Boot rc=%d reply=%s\n", rc, boot);
  assert(rc == 0);
  const bool on_vcpu = std::strstr(boot, "hv=vcpu") != nullptr;

  rc = wasmgocos_call("Alloc", "4096", buf, sizeof(buf));
  if (on_vcpu) {
    assert(rc == 0);
    unsigned long long gpa = std::strtoull(buf, nullptr, 10);
    assert(gpa >= 0x200000ull);
    assert(gpa < 0x400000ull);
    char addr[64];
    std::snprintf(addr, sizeof(addr), "%s", buf);
    assert(wasmgocos_call("Free", addr, buf, sizeof(buf)) == 0);
  } else if (std::strstr(buf, "k32 hop required") == nullptr &&
             std::strstr(buf, "nix hop required") == nullptr &&
             std::strstr(buf, "error:") == nullptr) {
    assert(rc == 0);
    char addr[64];
    std::snprintf(addr, sizeof(addr), "%s", buf);
    assert(wasmgocos_call("Free", addr, buf, sizeof(buf)) == 0);
  }

#if defined(WASMGOCOS_HAS_VCPU)
  if (on_vcpu) {
    uint32_t before = wasmturbo_vcpu_k32_exits();
    assert(before > 0);
    rc = wasmgocos_call("GetCurrentProcessId", "", buf, sizeof(buf));
    assert(rc == 0);
    assert(wasmturbo_vcpu_k32_exits() == before + 1);
    const char* krnl = std::strstr(boot, "krnl=");
    assert(krnl != nullptr);
    unsigned long pid = std::strtoul(krnl + 5, nullptr, 10);
    assert(std::strtoul(buf, nullptr, 10) == pid);
    assert(pid != 0);
  }
#endif

  rc = wasmgocos_call("CreateProcess", "true", buf, sizeof(buf));
  if (std::strstr(buf, "k32 hop required")) return 0;
  // Image "true" is not a Windows executable. A live k32 reports that.
  if (std::strstr(buf, "CreateProcessW: 2") != nullptr) return 0;
  assert(rc == 0);
  assert(std::strlen(buf) > 0);
#if defined(WASMGOCOS_HAS_JITRT)
  // The child is a background job. Reaping it closes its handles.
  assert(wasmgocos_call("Jobs", "wait", buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "exit=0") != nullptr);
#endif

  // Close takes a handle the kernel handed out, once.
  rc = wasmgocos_call("CreateEvent", "1\x1f" "0", buf, sizeof(buf));
  assert(rc == 0);
  std::string ev = buf;
  assert(wasmgocos_call("Close", ev.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmgocos_call("Close", ev.c_str(), buf, sizeof(buf)) != 0);
  return 0;
}
