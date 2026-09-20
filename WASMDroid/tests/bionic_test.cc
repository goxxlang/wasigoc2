#include "droid/catalog.h"
#include "droid/dispatch.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  int n = 0;
  const WasmDroidApi* cat = wasmdroid_catalog(&n);
  assert(n >= 1500);
  bool saw_pid = false, saw_binder = false, saw_pkvm = false, saw_kmalloc = false;
  bool saw_prop = false, saw_avf = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].name, "getpid") == 0) saw_pid = true;
    if (std::strcmp(cat[i].name, "BINDER_WRITE_READ") == 0) saw_binder = true;
    if (std::strcmp(cat[i].name, "PKVM_CREATE_VM") == 0) saw_pkvm = true;
    if (std::strcmp(cat[i].name, "kmalloc") == 0) saw_kmalloc = true;
    if (std::strcmp(cat[i].name, "__system_property_get") == 0) saw_prop = true;
    if (std::strcmp(cat[i].name, "VirtualizationService") == 0) saw_avf = true;
  }
  assert(saw_pid);
  assert(saw_binder);
  assert(saw_pkvm);
  assert(saw_kmalloc);
  assert(saw_prop);
  assert(saw_avf);

  assert(wasmdroid_call("getpid", "", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) != 0);

  assert(wasmdroid_call("uname", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmdroid_call("getcwd", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmdroid_call("android_get_device_api_level", "", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) >= 34);

  assert(wasmdroid_call("malloc", "32", buf, sizeof(buf)) == 0);
  std::string hp = buf;
  assert(wasmdroid_call("free", hp.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("__system_property_set", "ro.test.wasmdroid\x1f" "1", buf,
                        sizeof(buf)) == 0);
  assert(wasmdroid_call("__system_property_get", "ro.test.wasmdroid", buf, sizeof(buf)) ==
         0);
  assert(std::strcmp(buf, "1") == 0);

  assert(wasmdroid_call("no_such_android_api", "", buf, sizeof(buf)) != 0);
  std::printf("bionic_test ok catalog=%d\n", n);
  return 0;
}
