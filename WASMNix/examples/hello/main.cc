#include "nix/catalog.h"
#include "nix/dispatch.h"

#include <cstdio>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  int n = 0;
  const WasmNixApi* cat = wasmnix_catalog(&n);
  std::printf("catalog %d apis\n", n);
  const char* last_ns = "";
  int shown = 0;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].ns, last_ns) != 0) {
      std::printf("  [%s]\n", cat[i].ns);
      last_ns = cat[i].ns;
    }
    if (shown < 40 || std::strcmp(cat[i].ns, "WSL") == 0 ||
        std::strcmp(cat[i].ns, "Nix") == 0) {
      std::printf("    %s %s %s\n", cat[i].ns, cat[i].lib, cat[i].name);
      shown++;
    }
  }
  if (n > shown) std::printf("  ... %d more\n", n - shown);

  if (wasmnix_call("getpid", "", buf, sizeof(buf)) != 0) {
    std::printf("getpid failed: %s\n", buf);
    return 1;
  }
  std::printf("pid %s\n", buf);

  wasmnix_call("uname", "", buf, sizeof(buf));
  std::printf("uname %s\n", buf);

  int wsl = wasmnix_call("WslIsWsl", "", buf, sizeof(buf));
  std::printf("wsl rc=%d %s\n", wsl, buf);

  int nix = wasmnix_call("NixVersion", "", buf, sizeof(buf));
  std::printf("nix rc=%d %s\n", nix, buf);
  return 0;
}
