#include "nix/catalog.h"
#include "nix/dispatch.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

static std::string Call(const char* api, const char* args) {
  char buf[2048];
  int rc = wasmnix_call(api, args, buf, sizeof(buf));
  assert(rc == 0);
  return buf;
}

int main() {
  int n = 0;
  const WasmNixApi* cat = wasmnix_catalog(&n);
  assert(n == 33);
  assert(std::strcmp(cat[0].ns, "WSL") == 0);

  char buf[256];
  assert(Call("ListOnline", "").find("Ubuntu") != std::string::npos);
  assert(Call("ListOnline", "").find("Debian") != std::string::npos);
  assert(Call("Install", "Ubuntu") == "ok");
  std::string list = Call("List", "");
  assert(list.find("*Ubuntu") != std::string::npos);
  assert(list.find("Running") != std::string::npos);
  assert(list.find("\x1f" "2") != std::string::npos);
  assert(Call("Exec", "uname").find("microsoft-standard-WSL2") != std::string::npos);
  assert(Call("Write", "Ubuntu\x1f" "/root/hello\x1f" "hi") == "ok");
  assert(Call("Read", "Ubuntu\x1f" "/root/hello") == "hi");
  assert(Call("Exec", "cat /root/hello") == "hi");
  assert(Call("Path", "-w\x1f" "/mnt/c/Windows/System32") == "C:\\Windows\\System32");
  assert(Call("Path", "-u\x1f" "C:\\Windows") == "/mnt/c/Windows");
  assert(Call("Write", "Ubuntu\x1f" "/mnt/c/note\x1f" "from-linux") == "ok");
  assert(Call("Read", "Ubuntu\x1f" "/mnt/c/note") == "from-linux");
  assert(Call("Config", "networkingMode\x1fmirrored") == "ok");
  assert(Call("Info", "--networking-mode") == "mirrored");
  assert(Call("SetVersion", "Ubuntu\x1f" "1") == "ok");
  assert(Call("GetConfiguration", "Ubuntu").rfind("1\x1f", 0) == 0);
  assert(Call("List", "").find("Stopped") != std::string::npos);
  assert(Call("Exec", "uname -r") == "microsoft-standard-WSL1");
  assert(Call("Install", "Debian") == "ok");
  assert(Call("SetDefault", "Debian") == "ok");
  assert(Call("List", "").find("*Debian") != std::string::npos);
  assert(Call("IsDistributionRegistered", "Ubuntu") == "1");
  assert(Call("Export", "Debian\x1f" "C:\\export\\debian.tar") == "ok");
  assert(Call("Unregister", "Debian") == "ok");
  assert(Call("Import", "Debian\x1f" "C:\\WSL\\Debian\x1f" "C:\\export\\debian.tar") == "ok");
  assert(Call("Mount", "D:\\disk.vhdx") == "ok");
  assert(Call("Mount", "").find("D:\\disk.vhdx") != std::string::npos);
  assert(Call("Manage", "Ubuntu\x1fset-sparse\x1ftrue") == "ok");
  assert(Call("LaunchWin32", "notepad.exe").find("win32 ") == 0);
  assert(Call("Shutdown", "") == "ok");
  assert(Call("Status", "").find("Networking mode: mirrored") != std::string::npos);
  assert(wasmnix_call("KVM_CREATE_VM", "", buf, sizeof(buf)) != 0);
  assert(wasmnix_call("getpid", "", buf, sizeof(buf)) != 0);
  std::cout << "wsl ok\n";
  return 0;
}
