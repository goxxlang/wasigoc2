#include "nix/catalog.h"
#include "nix/dispatch.h"

#include <cstdio>

int main() {
  int n = 0;
  const WasmNixApi* cat = wasmnix_catalog(&n);
  std::printf("WSL %d\n", n);
  for (int i = 0; i < n; i++) std::printf("  %s\n", cat[i].name);
  char buf[512];
  if (wasmnix_call("Install", "Ubuntu", buf, sizeof(buf)) != 0) return 1;
  if (wasmnix_call("Exec", "uname", buf, sizeof(buf)) != 0) return 1;
  std::printf("%s\n", buf);
  return 0;
}
