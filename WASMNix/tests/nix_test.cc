#include "nix/catalog.h"
#include "nix/dispatch.h"

#include <cstdio>
#include <cstring>

int main() {
  char buf[8192];
  int n = 0;
  const WasmNixApi* cat = wasmnix_catalog(&n);
  bool saw = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].ns, "Nix") == 0 &&
        std::strcmp(cat[i].name, "NixVersion") == 0)
      saw = true;
  }
  if (!saw) {
    std::printf("NixVersion missing from catalog\n");
    return 1;
  }

  int rc = wasmnix_call("NixVersion", "", buf, sizeof(buf));
  if (rc != 0) {
    std::printf("nix not on this hop rc=%d %s\n", rc, buf);
    return 0;  // skip: distro may not have nix
  }
  std::printf("nix %s\n", buf);
  return 0;
}
