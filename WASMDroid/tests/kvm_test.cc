#include "droid/catalog.h"
#include "droid/dispatch.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

static void hex_of(const unsigned char* p, size_t n, std::string* out) {
  static const char* h = "0123456789abcdef";
  out->assign(n * 2, '\0');
  for (size_t i = 0; i < n; ++i) {
    (*out)[i * 2] = h[p[i] >> 4];
    (*out)[i * 2 + 1] = h[p[i] & 0xf];
  }
}

int main() {
  char buf[4096];
  int n = 0;
  const WasmDroidApi* cat = wasmdroid_catalog(&n);
  bool saw_run = false, saw_pkvm = false, saw_gunyah = false, saw_avf = false;
  bool saw_vfio = false, saw_vhost = false, saw_mshv = false, saw_nested = false;
  bool saw_guest_memfd = false, saw_tdx = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].name, "KVM_RUN") == 0) saw_run = true;
    if (std::strcmp(cat[i].name, "PKVM_CREATE_VM") == 0) saw_pkvm = true;
    if (std::strcmp(cat[i].name, "GUNYAH_CREATE_VM") == 0) saw_gunyah = true;
    if (std::strcmp(cat[i].name, "AVF_CREATE_VM") == 0) saw_avf = true;
    if (std::strcmp(cat[i].name, "VFIO_IOMMU_MAP_DMA") == 0) saw_vfio = true;
    if (std::strcmp(cat[i].name, "VHOST_SET_VRING_ADDR") == 0) saw_vhost = true;
    if (std::strcmp(cat[i].name, "MSHV_CREATE_PARTITION") == 0) saw_mshv = true;
    if (std::strcmp(cat[i].name, "KVM_GET_NESTED_STATE") == 0) saw_nested = true;
    if (std::strcmp(cat[i].name, "KVM_CREATE_GUEST_MEMFD") == 0) saw_guest_memfd = true;
    if (std::strcmp(cat[i].name, "KVM_EXIT_TDX") == 0) saw_tdx = true;
  }
  assert(saw_run);
  assert(saw_pkvm);
  assert(saw_gunyah);
  assert(saw_avf);
  assert(saw_vfio);
  assert(saw_vhost);
  assert(saw_mshv);
  assert(saw_nested);
  assert(saw_guest_memfd);
  assert(saw_tdx);

  assert(wasmdroid_call("KVM_GET_API_VERSION", "", buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "12") == 0);

  assert(wasmdroid_call("KVM_CREATE_VM", "", buf, sizeof(buf)) == 0);
  std::string vm = buf;
  std::string cv = vm + "\x1f" "0";
  assert(wasmdroid_call("KVM_CREATE_VCPU", cv.c_str(), buf, sizeof(buf)) == 0);
  std::string vp = buf;

  unsigned char hlt = 0xF4;
  std::string hex;
  hex_of(&hlt, 1, &hex);
  std::string map = vm + "\x1f" "0" "\x1f" "4096" "\x1f" + hex;
  assert(wasmdroid_call("KVM_SET_USER_MEMORY_REGION", map.c_str(), buf, sizeof(buf)) == 0);
  std::string setrip = vp + "\x1f" "rip" "\x1f" "0";
  assert(wasmdroid_call("KVM_SET_REGS", setrip.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("KVM_RUN", vp.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "hlt") != nullptr);

  unsigned char outb[] = {0xE6, 0x42};
  hex_of(outb, 2, &hex);
  map = vm + "\x1f" "4096" "\x1f" "4096" "\x1f" + hex;
  assert(wasmdroid_call("KVM_SET_USER_MEMORY_REGION2", map.c_str(), buf, sizeof(buf)) == 0);
  setrip = vp + "\x1f" "rip" "\x1f" "4096";
  assert(wasmdroid_call("KVM_SET_REGS", setrip.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("KVM_RUN", vp.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strstr(buf, "io") != nullptr);
  assert(wasmdroid_call("KvmEmulateIo", vp.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("KVM_CREATE_IRQCHIP", vm.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("KVM_IRQFD", vm.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("KVM_IOEVENTFD", vm.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("KVM_GET_NESTED_STATE", vp.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("KVM_CREATE_GUEST_MEMFD", "4096", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("MSHV_CREATE_PARTITION", "", buf, sizeof(buf)) == 0);
  std::string part = buf;
  std::string mvp = part + "\x1f" "0";
  assert(wasmdroid_call("MSHV_CREATE_VP", mvp.c_str(), buf, sizeof(buf)) == 0);
  std::string mshv_vp = buf;
  assert(wasmdroid_call("MSHV_RUN_VP", mshv_vp.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("VFIO_GET_API_VERSION", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("VFIO_SET_IOMMU", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("VHOST_SET_OWNER", "", buf, sizeof(buf)) == 0);
  std::string vh = buf;
  assert(wasmdroid_call("VHOST_SET_VRING_ADDR", vh.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("PKVM_CREATE_VM", "", buf, sizeof(buf)) == 0);
  std::string pvm = buf;
  assert(wasmdroid_call("PKVM_CREATE_VCPU", (pvm + "\x1f" "0").c_str(), buf, sizeof(buf)) ==
         0);
  assert(wasmdroid_call("GUNYAH_CREATE_VM", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("AVF_CREATE_VM", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("VirtualizationService", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("KVM_CAP_ARM_PROTECTED_VM", "", buf, sizeof(buf)) == 0);

  std::printf("kvm_test ok catalog=%d\n", n);
  return 0;
}
