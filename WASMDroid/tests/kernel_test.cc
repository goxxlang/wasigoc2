#include "droid/catalog.h"
#include "droid/dispatch.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
  char buf[4096];
  int n = 0;
  const WasmDroidApi* cat = wasmdroid_catalog(&n);
  bool saw_kmalloc = false, saw_kallsyms = false, saw_guest = false, saw_binder = false;
  bool saw_irq = false, saw_bpf = false, saw_tun = false, saw_ptrace = false;
  bool saw_prctl = false, saw_uring = false, saw_sysctl = false;
  for (int i = 0; i < n; i++) {
    if (std::strcmp(cat[i].name, "kmalloc") == 0) saw_kmalloc = true;
    if (std::strcmp(cat[i].name, "kallsyms_lookup_name") == 0) saw_kallsyms = true;
    if (std::strcmp(cat[i].name, "kvm_read_guest") == 0) saw_guest = true;
    if (std::strcmp(cat[i].name, "binder_transaction") == 0) saw_binder = true;
    if (std::strcmp(cat[i].name, "request_irq") == 0) saw_irq = true;
    if (std::strcmp(cat[i].name, "BPF_PROG_LOAD") == 0) saw_bpf = true;
    if (std::strcmp(cat[i].name, "TUNSETIFF") == 0) saw_tun = true;
    if (std::strcmp(cat[i].name, "PTRACE_ATTACH") == 0) saw_ptrace = true;
    if (std::strcmp(cat[i].name, "PR_SET_NO_NEW_PRIVS") == 0) saw_prctl = true;
    if (std::strcmp(cat[i].name, "IORING_OP_READ") == 0) saw_uring = true;
    if (std::strcmp(cat[i].ns, "Android.Sysctl") == 0) saw_sysctl = true;
  }
  assert(saw_kmalloc);
  assert(saw_kallsyms);
  assert(saw_guest);
  assert(saw_binder);
  assert(saw_irq);
  assert(saw_bpf);
  assert(saw_tun);
  assert(saw_ptrace);
  assert(saw_prctl);
  assert(saw_uring);
  assert(saw_sysctl);

  assert(wasmdroid_call("kmalloc", "64", buf, sizeof(buf)) == 0);
  std::string p = buf;
  assert(std::strtoull(buf, nullptr, 10) != 0);
  assert(wasmdroid_call("ksize", p.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strtoul(buf, nullptr, 10) >= 64);
  assert(wasmdroid_call("kfree", p.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("kmem_cache_create", "objs\x1f" "32", buf, sizeof(buf)) == 0);
  std::string cache = buf;
  assert(wasmdroid_call("kmem_cache_alloc", cache.c_str(), buf, sizeof(buf)) == 0);
  std::string obj = buf;
  std::string fr = cache + "\x1f" + obj;
  assert(wasmdroid_call("kmem_cache_free", fr.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("kmem_cache_destroy", cache.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("kallsyms_lookup_name", "kmalloc", buf, sizeof(buf)) == 0);
  assert(std::strtoull(buf, nullptr, 10) != 0);

  assert(wasmdroid_call("request_irq", "16\x1f" "handler", buf, sizeof(buf)) == 0);
  std::string irq = buf;
  assert(wasmdroid_call("free_irq", irq.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("kthread_run", "droid", buf, sizeof(buf)) == 0);
  std::string th = buf;
  assert(wasmdroid_call("kthread_stop", th.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("spin_lock", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("rcu_read_lock", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("schedule_work", "", buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("KVM_CREATE_VM", "", buf, sizeof(buf)) == 0);
  std::string vm = buf;
  std::string map = vm + "\x1f" "0" "\x1f" "4096" "\x1f" "f4";
  assert(wasmdroid_call("KVM_SET_USER_MEMORY_REGION", map.c_str(), buf, sizeof(buf)) == 0);
  std::string rd = vm + "\x1f" "0" "\x1f" "1";
  assert(wasmdroid_call("kvm_read_guest", rd.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "f4") == 0);
  std::string wr = vm + "\x1f" "0" "\x1f" "90";
  assert(wasmdroid_call("kvm_write_guest", wr.c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("kvm_read_guest", rd.c_str(), buf, sizeof(buf)) == 0);
  assert(std::strcmp(buf, "90") == 0);
  assert(wasmdroid_call("gfn_to_hva", (vm + "\x1f" "1").c_str(), buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("kvm_vcpu_kick", vm.c_str(), buf, sizeof(buf)) == 0);

  assert(wasmdroid_call("BPF_PROG_LOAD", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("TUNSETIFF", "tun0", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("iommu_domain_alloc", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("pci_enable_device", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("PTRACE_ATTACH", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("PR_GET_NO_NEW_PRIVS", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("CLONE_NEWNET", "", buf, sizeof(buf)) == 0);
  assert(wasmdroid_call("binder_transaction", "", buf, sizeof(buf)) == 0);

  std::printf("kernel_test ok catalog=%d\n", n);
  return 0;
}
