#include "droid/catalog.h"
#include "droid/dispatch.h"

#include <cstdio>
#include <cstring>

int main() {
  char buf[8192];
  int n = 0;
  const WasmDroidApi* cat = wasmdroid_catalog(&n);
  bool saw = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].ns, "Android.Adb") == 0 &&
        std::strcmp(cat[i].name, "AdbDevices") == 0)
      saw = true;
  }
  if (!saw) {
    std::printf("AdbDevices missing from catalog\n");
    return 1;
  }

  int rc = wasmdroid_call("AdbDevices", "", buf, sizeof(buf));
  if (rc != 0) {
    std::printf("adb not on this hop rc=%d %s\n", rc, buf);
    return 0;  // skip: host may not have adb
  }
  std::printf("adb %s\n", buf);
  return 0;
}
