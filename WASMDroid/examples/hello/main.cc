#include "droid/catalog.h"
#include "droid/dispatch.h"

#include <cstdio>
#include <cstring>

int main() {
  char buf[4096];
  int n = 0;
  const WasmDroidApi* cat = wasmdroid_catalog(&n);
  std::printf("catalog %d apis\n", n);
  const char* last_ns = "";
  int shown = 0;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].ns, last_ns) != 0) {
      std::printf("  [%s]\n", cat[i].ns);
      last_ns = cat[i].ns;
    }
    if (shown < 30 || std::strcmp(cat[i].ns, "Android.Binder") == 0 ||
        std::strcmp(cat[i].ns, "Android.pKVM") == 0 ||
        std::strcmp(cat[i].ns, "Android.AVF") == 0) {
      if (shown < 50) {
        std::printf("    %s %s %s\n", cat[i].ns, cat[i].lib, cat[i].name);
        shown++;
      }
    }
  }
  if (n > shown) std::printf("  ... %d more\n", n - shown);

  if (wasmdroid_call("getpid", "", buf, sizeof(buf)) != 0) {
    std::printf("getpid failed: %s\n", buf);
    return 1;
  }
  std::printf("pid %s\n", buf);
  wasmdroid_call("uname", "", buf, sizeof(buf));
  std::printf("uname %s\n", buf);
  wasmdroid_call("android_get_device_api_level", "", buf, sizeof(buf));
  std::printf("api %s\n", buf);
  wasmdroid_call("BINDER_VERSION", "", buf, sizeof(buf));
  std::printf("binder %s\n", buf);
  wasmdroid_call("KVM_GET_API_VERSION", "", buf, sizeof(buf));
  std::printf("kvm %s\n", buf);
  return 0;
}
