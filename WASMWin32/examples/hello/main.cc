#include "win32/catalog.h"
#include "win32/dispatch.h"

#include <cstdio>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  int n = 0;
  const WasmWin32Api* cat = wasmwin32_catalog(&n);
  std::printf("catalog %d apis\n", n);
  for (int i = 0; i < n; i++) {
    std::printf("  %s %s %s\n", cat[i].ns, cat[i].dll, cat[i].name);
  }

  if (wasmwin32_call("GetCurrentProcessId", "", buf, sizeof(buf)) != 0) {
    std::printf("GetCurrentProcessId failed: %s\n", buf);
    return 1;
  }
  std::printf("pid %s\n", buf);

  wasmwin32_call("GetComputerNameW", "", buf, sizeof(buf));
  std::printf("computer %s\n", buf);

  wasmwin32_call("GetWindowsDirectoryW", "", buf, sizeof(buf));
  std::printf("windows %s\n", buf);

  int wsl = wasmwin32_call("WslList", "", buf, sizeof(buf));
  std::printf("wsl list rc=%d %s\n", wsl, buf);

  int nix = wasmwin32_call("NixVersion", "", buf, sizeof(buf));
  std::printf("nix rc=%d %s\n", nix, buf);
  return 0;
}
