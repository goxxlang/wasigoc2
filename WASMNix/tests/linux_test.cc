#include "nix/catalog.h"
#include "nix/dispatch.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  int n = 0;
  const WasmNixApi* cat = wasmnix_catalog(&n);
  assert(n >= 800);
  bool saw_pid = false;
  bool saw_uname = false;
  bool saw_wslpath = false;
  bool saw_nix = false;
  bool saw_kvm = false;
  bool saw_io_uring = false;
  bool saw_kmalloc = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].name, "getpid") == 0) saw_pid = true;
    if (std::strcmp(cat[i].name, "uname") == 0) saw_uname = true;
    if (std::strcmp(cat[i].name, "WslPath") == 0) saw_wslpath = true;
    if (std::strcmp(cat[i].name, "NixVersion") == 0) saw_nix = true;
    if (std::strcmp(cat[i].name, "KVM_CREATE_VM") == 0) saw_kvm = true;
    if (std::strcmp(cat[i].name, "KVM_SET_USER_MEMORY_REGION2") == 0) saw_kvm = true;
    if (std::strcmp(cat[i].name, "io_uring_setup") == 0) saw_io_uring = true;
    if (std::strcmp(cat[i].name, "kmalloc") == 0) saw_kmalloc = true;
  }
  assert(saw_pid);
  assert(saw_uname);
  assert(saw_wslpath);
  assert(saw_nix);
  assert(saw_kvm);
  assert(saw_io_uring);
  assert(saw_kmalloc);

  assert(wasmnix_call("getpid", "", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) != 0);

  assert(wasmnix_call("uname", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmnix_call("getcwd", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmnix_call("getuid", "", buf, sizeof(buf)) == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmnix_call("sysconf", "pagesize", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) != 0);

  {
    const char* dir = "/tmp/wasmnix_linux_test";
    wasmnix_call("rmdir", dir, buf, sizeof(buf));
    assert(wasmnix_call("mkdir", dir, buf, sizeof(buf)) == 0);
    assert(wasmnix_call("stat", dir, buf, sizeof(buf)) == 0);
    assert(std::strlen(buf) > 0);
    assert(wasmnix_call("access", dir, buf, sizeof(buf)) == 0);
    assert(wasmnix_call("rmdir", dir, buf, sizeof(buf)) == 0);
  }

  assert(wasmnix_call("malloc", "32", buf, sizeof(buf)) == 0);
  std::string hp = buf;
  assert(std::strtoull(buf, nullptr, 10) != 0);
  assert(wasmnix_call("free", hp.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmnix_call("strlen", "linux", buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) == 5);

  int rc = wasmnix_call("getenv", "PATH", buf, sizeof(buf));
  if (rc != 0) {
    rc = wasmnix_call("getenv", "HOME", buf, sizeof(buf));
  }
  assert(rc == 0);
  assert(std::strlen(buf) > 0);

  assert(wasmnix_call("no_such_linux_api", "", buf, sizeof(buf)) != 0);
  assert(std::strstr(buf, "unknown api") != nullptr);

  std::printf("linux_test ok catalog=%d pid path\n", n);
  return 0;
}
