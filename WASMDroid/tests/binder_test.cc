#include "droid/dispatch.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  assert(wasmdroid_call("BINDER_VERSION", "", buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "8") == 0);

  assert(wasmdroid_call("AServiceManager_addService", "wasmdroid.test", buf, sizeof(buf)) ==
         0);
  std::string h = buf;
  assert(wasmdroid_call("AServiceManager_checkService", "wasmdroid.test", buf,
                        sizeof(buf)) == 0);
  assert(std::strcmp(buf, h.c_str()) == 0);
  assert(wasmdroid_call("AServiceManager_getService", "wasmdroid.test", buf, sizeof(buf)) ==
         0);

  std::string tx = std::string("wasmdroid.test") + "\x1f" "1" "\x1f" "ping";
  assert(wasmdroid_call("BINDER_WRITE_READ", tx.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "BR_REPLY") != nullptr);

  assert(wasmdroid_call("AParcel_create", "", buf, sizeof(buf)) == 0);
  std::string p = buf;
  std::string wr = p + "\x1f" "hello";
  assert(wasmdroid_call("AParcel_writeString", wr.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("AParcel_readString", p.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "hello") == 0);

  assert(wasmdroid_call("ashmem_create_region", "buf\x1f" "4096", buf, sizeof(buf)) == 0);
  std::string ash = buf;
  assert(wasmdroid_call("ASHMEM_GET_SIZE", ash.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) == 4096);

  assert(wasmdroid_call("hw_get_module", "gralloc", buf, sizeof(buf)) == 0);
  std::printf("binder_test ok\n");
  return 0;
}
