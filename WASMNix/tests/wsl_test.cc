#include "nix/dispatch.h"

#include <cstdio>
#include <cstring>

int main() {
  char buf[8192];
  int rc = wasmnix_call("WslIsWsl", "", buf, sizeof(buf));
  if (rc != 0) {
    std::printf("wsl probe failed rc=%d %s\n", rc, buf);
    return 0;  // skip: machine may not have WSL
  }
  std::printf("WslIsWsl %s\n", buf);

  rc = wasmnix_call("WslList", "", buf, sizeof(buf));
  if (rc == 0) {
    std::printf("wsl distro %s\n", buf);
  } else {
    std::printf("WslList rc=%d %s\n", rc, buf);
  }

  rc = wasmnix_call("WslPath", "C:\\Windows", buf, sizeof(buf));
  if (rc == 0) {
    std::printf("wslpath %s\n", buf);
  } else {
    std::printf("WslPath rc=%d %s\n", rc, buf);
  }

  rc = wasmnix_call("WslInfo", "", buf, sizeof(buf));
  if (rc == 0) {
    std::printf("wslinfo %s\n", buf);
  } else {
    std::printf("WslInfo rc=%d %s\n", rc, buf);
  }

  rc = wasmnix_call("WslExec", "uname -a", buf, sizeof(buf));
  if (rc == 0) {
    std::printf("uname %s\n", buf);
  } else {
    std::printf("WslExec rc=%d %s\n", rc, buf);
  }
  return 0;
}
