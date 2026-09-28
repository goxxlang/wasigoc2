#include "droid/catalog.h"
#include "droid/dispatch.h"

#include <cstdio>

int main() {
  int n = 0;
  const WasmDroidApi* cat = wasmdroid_catalog(&n);
  std::printf("Phone Link %d\n", n);
  for (int i = 0; i < n; i++)
    std::printf("  %s %s %s\n", cat[i].ns, cat[i].lib, cat[i].name);
  char buf[256];
  if (wasmdroid_call("Aumid", "", buf, sizeof(buf)) != 0) return 1;
  std::printf("%s\n", buf);
  return 0;
}
