#include "win32/dispatch.h"

#include <cstdio>
#include <cstring>

int main() {
  char buf[8192];
  int rc = wasmwin32_call("WslList", "", buf, sizeof(buf));
  if (rc != 0) {
    std::printf("wsl not available rc=%d %s\n", rc, buf);
    return 0;  // skip: machine may not have WSL
  }
  std::printf("wsl distros:\n%s\n", buf);

  rc = wasmwin32_call("WslExec", "uname -a", buf, sizeof(buf));
  if (rc == 0) {
    std::printf("uname %s\n", buf);
  } else {
    std::printf("WslExec rc=%d %s\n", rc, buf);
  }

  rc = wasmwin32_call("NixVersion", "", buf, sizeof(buf));
  if (rc == 0) {
    std::printf("nix %s\n", buf);
  } else {
    std::printf("nix not in default WSL distro rc=%d %s\n", rc, buf);
  }
  return 0;
}
