#ifndef WASMDROID_INCLUDE_DROID_KERNEL_HOST_HPP_
#define WASMDROID_INCLUDE_DROID_KERNEL_HOST_HPP_

// In-module Linux kernel hop — ntoskrnl / WDK analog on Android (vmlinux + binder).
// Names follow include/linux/{slab,mm,sched,interrupt,module,kvm_host}.h
// EXPORT_SYMBOL surface. kmalloc is the same hop as ExAllocatePool.
// kvm_read_guest / gfn_to_hva talk to the KVM GPA table on this hop.

#include "droid/catalog.h"
#include "droid/kvm_host.hpp"

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace wasmdroid {

enum { kKernBase = 8192 };
enum { kKernMax = 128 };
enum {
  kKernCache = 1,
  kKernIrq = 2,
  kKernWork = 3,
  kKernThread = 4,
  kKernMod = 5,
  kKernMap = 6,
  kKernBpf = 7,
  kKernNet = 8
};

struct KernObj {
  int used = 0;
  int kind = 0;
  size_t n = 0;
  std::string name;
  std::vector<unsigned char> bytes;
};

inline KernObj* kern_tab() {
  static KernObj t[kKernMax];
  return t;
}

inline bool kern_eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline bool kern_pfx(const char* api, const char* p) {
  return api && p && std::strncmp(api, p, std::strlen(p)) == 0;
}

inline void kern_split(const char* a, std::string* l, std::string* r) {
  const char* p = a ? std::strchr(a, '\x1f') : nullptr;
  if (!p) {
    *l = a ? a : "";
    r->clear();
    return;
  }
  l->assign(a, p);
  *r = p + 1;
}

inline int kern_alloc(int kind) {
  KernObj* t = kern_tab();
  for (int i = 0; i < kKernMax; ++i) {
    if (!t[i].used) {
      t[i] = KernObj{};
      t[i].used = 1;
      t[i].kind = kind;
      return kKernBase + i;
    }
  }
  return -1;
}

inline KernObj* kern_at(int h) {
  int i = h - kKernBase;
  if (i < 0 || i >= kKernMax || !kern_tab()[i].used) return nullptr;
  return &kern_tab()[i];
}

inline std::map<uintptr_t, size_t>& kern_heap() {
  static std::map<uintptr_t, size_t> m;
  return m;
}

inline bool kern_ns(const char* ns) {
  return ns && (kern_pfx(ns, "Linux.Kernel") || kern_pfx(ns, "Linux.BPF") ||
                kern_pfx(ns, "Linux.Netlink") || kern_pfx(ns, "Linux.Tun") ||
                kern_pfx(ns, "Linux.Vsock") || kern_pfx(ns, "Linux.Sysctl") ||
                kern_pfx(ns, "Linux.Ptrace") || kern_pfx(ns, "Linux.Prctl") ||
                kern_pfx(ns, "Linux.Userfaultfd") || kern_pfx(ns, "Linux.IoUring") ||
                kern_pfx(ns, "Android.Kernel") || kern_pfx(ns, "Android.BPF") ||
                kern_pfx(ns, "Android.Sysctl") || kern_pfx(ns, "Android.SELinux") ||
                kern_pfx(ns, "Android.Ptrace") || kern_pfx(ns, "Android.Prctl") ||
                kern_pfx(ns, "Android.Tun") || kern_pfx(ns, "Android.Netlink") ||
                kern_pfx(ns, "Android.Vsock") || kern_pfx(ns, "Android.Userfaultfd") ||
                kern_pfx(ns, "Android.IoUring"));
}

inline bool kern_cataloged(const char* api) {
  if (!api || !api[0]) return false;
  int n = 0;
  const WasmDroidApi* c = wasmdroid_catalog(&n);
  for (int i = 0; i < n; ++i)
    if (c[i].name && kern_eq(c[i].name, api) && kern_ns(c[i].ns)) return true;
  return false;
}

inline bool try_kernel(const char* api, const char* args, std::string* out) {
  if (!api) return false;
  const char* a = args ? args : "";
  auto ok = [&](std::string s) {
    *out = std::move(s);
    return true;
  };
  auto fail = [&](const char* m) {
    *out = std::string("error: ") + m;
    return true;
  };

  if (kern_eq(api, "kmalloc") || kern_eq(api, "kzalloc") || kern_eq(api, "kcalloc") ||
      kern_eq(api, "kvmalloc") || kern_eq(api, "kvzalloc") || kern_eq(api, "__kmalloc") ||
      kern_eq(api, "kmalloc_array") || kern_eq(api, "kmalloc_node") ||
      kern_eq(api, "devm_kmalloc") || kern_eq(api, "devm_kzalloc") ||
      kern_eq(api, "vmalloc") || kern_eq(api, "vzalloc") || kern_eq(api, "vmalloc_user") ||
      kern_eq(api, "vmalloc_node") || kern_eq(api, "__get_free_pages") ||
      kern_eq(api, "__get_free_page") || kern_eq(api, "get_zeroed_page") ||
      kern_eq(api, "alloc_pages") || kern_eq(api, "dma_alloc_coherent") ||
      kern_eq(api, "kmemdup") || kern_eq(api, "kstrdup")) {
    size_t n = static_cast<size_t>(std::strtoul(a, nullptr, 10));
    if (!n) n = 1;
    if (kern_eq(api, "__get_free_page") || kern_eq(api, "get_zeroed_page") ||
        kern_eq(api, "alloc_pages"))
      n = 4096;
    void* p = std::malloc(n);
    if (!p) return fail("kmalloc: oom");
    if (kern_eq(api, "kzalloc") || kern_eq(api, "kcalloc") || kern_eq(api, "kvzalloc") ||
        kern_eq(api, "vzalloc") || kern_eq(api, "devm_kzalloc") ||
        kern_eq(api, "get_zeroed_page") || kern_eq(api, "dma_alloc_coherent"))
      std::memset(p, 0, n);
    kern_heap()[reinterpret_cast<uintptr_t>(p)] = n;
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (kern_eq(api, "kfree") || kern_eq(api, "kvfree") || kern_eq(api, "vfree") ||
      kern_eq(api, "kfree_sensitive") || kern_eq(api, "devm_kfree") ||
      kern_eq(api, "free_pages") || kern_eq(api, "free_page") ||
      kern_eq(api, "__free_pages") || kern_eq(api, "dma_free_coherent")) {
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(a, nullptr, 10));
    if (p) {
      std::free(reinterpret_cast<void*>(p));
      kern_heap().erase(p);
    }
    return ok("ok");
  }
  if (kern_eq(api, "krealloc") || kern_eq(api, "ksize")) {
    std::string ps, ns;
    kern_split(a, &ps, &ns);
    uintptr_t p = static_cast<uintptr_t>(std::strtoull(ps.c_str(), nullptr, 10));
    if (kern_eq(api, "ksize")) {
      auto it = kern_heap().find(p);
      return ok(std::to_string(it == kern_heap().end() ? 0 : it->second));
    }
    size_t n = static_cast<size_t>(std::strtoul(ns.c_str(), nullptr, 10));
    void* q = std::realloc(reinterpret_cast<void*>(p), n ? n : 1);
    if (!q) return fail("krealloc");
    kern_heap().erase(p);
    kern_heap()[reinterpret_cast<uintptr_t>(q)] = n ? n : 1;
    return ok(std::to_string(reinterpret_cast<uintptr_t>(q)));
  }
  if (kern_eq(api, "kmem_cache_create") || kern_eq(api, "kmem_cache_create_usercopy") ||
      kern_eq(api, "mempool_create")) {
    std::string name, sz;
    kern_split(a, &name, &sz);
    int h = kern_alloc(kKernCache);
    if (h < 0) return fail(api);
    KernObj* o = kern_at(h);
    o->name = name.empty() ? "cache" : name;
    o->n = static_cast<size_t>(std::strtoul(sz.empty() ? "64" : sz.c_str(), nullptr, 10));
    if (!o->n) o->n = 64;
    return ok(std::to_string(h));
  }
  if (kern_eq(api, "kmem_cache_alloc") || kern_eq(api, "kmem_cache_alloc_node") ||
      kern_eq(api, "mempool_alloc")) {
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    KernObj* o = kern_at(h);
    if (!o || o->kind != kKernCache) return fail(api);
    void* p = std::malloc(o->n ? o->n : 64);
    if (!p) return fail("kmem_cache_alloc");
    std::memset(p, 0, o->n ? o->n : 64);
    kern_heap()[reinterpret_cast<uintptr_t>(p)] = o->n;
    return ok(std::to_string(reinterpret_cast<uintptr_t>(p)));
  }
  if (kern_eq(api, "kmem_cache_free") || kern_eq(api, "mempool_free")) {
    std::string hs, p;
    kern_split(a, &hs, &p);
    uintptr_t ptr = static_cast<uintptr_t>(std::strtoull(p.empty() ? a : p.c_str(), nullptr, 10));
    if (ptr) std::free(reinterpret_cast<void*>(ptr));
    kern_heap().erase(ptr);
    return ok("ok");
  }
  if (kern_eq(api, "kmem_cache_destroy") || kern_eq(api, "mempool_destroy")) {
    KernObj* o = kern_at(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!o) return fail(api);
    *o = KernObj{};
    return ok("ok");
  }
  if (kern_eq(api, "kmem_cache_size")) {
    KernObj* o = kern_at(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!o) return fail(api);
    return ok(std::to_string(o->n));
  }

  if (kern_eq(api, "ioremap") || kern_eq(api, "ioremap_wc") || kern_eq(api, "ioremap_wt") ||
      kern_eq(api, "ioremap_np") || kern_eq(api, "memremap") || kern_eq(api, "pci_iomap")) {
    std::string phys, sz;
    kern_split(a, &phys, &sz);
    size_t n = static_cast<size_t>(std::strtoul(sz.empty() ? "4096" : sz.c_str(), nullptr, 10));
    if (!n) n = 4096;
    int h = kern_alloc(kKernMap);
    if (h < 0) return fail(api);
    KernObj* o = kern_at(h);
    o->n = n;
    o->bytes.assign(n, 0);
    o->name = phys;
    return ok(std::to_string(h));
  }
  if (kern_eq(api, "iounmap") || kern_eq(api, "memunmap") || kern_eq(api, "pci_iounmap") ||
      kern_eq(api, "vunmap")) {
    KernObj* o = kern_at(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (o) *o = KernObj{};
    return ok("ok");
  }
  if (kern_eq(api, "phys_to_virt") || kern_eq(api, "virt_to_phys") ||
      kern_eq(api, "page_to_phys") || kern_eq(api, "page_to_pfn") ||
      kern_eq(api, "pfn_to_page") || kern_eq(api, "gpa_to_gfn") || kern_eq(api, "gfn_to_gpa")) {
    unsigned long long v = std::strtoull(a, nullptr, 0);
    if (kern_eq(api, "gpa_to_gfn") || kern_eq(api, "page_to_pfn")) v >>= 12;
    if (kern_eq(api, "gfn_to_gpa") || kern_eq(api, "pfn_to_page")) v <<= 12;
    return ok(std::to_string(v));
  }

  if (kern_eq(api, "copy_to_user") || kern_eq(api, "copy_from_user") ||
      kern_eq(api, "copy_in_user") || kern_eq(api, "get_user") || kern_eq(api, "put_user") ||
      kern_eq(api, "access_ok") || kern_eq(api, "clear_user")) {
    if (kern_eq(api, "access_ok")) return ok("1");
    std::string dst, rest, src, n;
    kern_split(a, &dst, &rest);
    kern_split(rest.c_str(), &src, &n);
    return ok(n.empty() ? "0" : n);
  }

  if (kern_eq(api, "request_irq") || kern_eq(api, "request_threaded_irq")) {
    std::string irq, rest;
    kern_split(a, &irq, &rest);
    int h = kern_alloc(kKernIrq);
    if (h < 0) return fail(api);
    kern_at(h)->name = irq.empty() ? a : irq;
    return ok(std::to_string(h));
  }
  if (kern_eq(api, "free_irq") || kern_eq(api, "enable_irq") || kern_eq(api, "disable_irq") ||
      kern_eq(api, "disable_irq_nosync") || kern_eq(api, "synchronize_irq") ||
      kern_eq(api, "local_irq_save") || kern_eq(api, "local_irq_restore") ||
      kern_eq(api, "local_irq_disable") || kern_eq(api, "local_irq_enable") ||
      kern_eq(api, "local_bh_disable") || kern_eq(api, "local_bh_enable") ||
      kern_eq(api, "preempt_disable") || kern_eq(api, "preempt_enable")) {
    if (kern_eq(api, "free_irq")) {
      KernObj* o = kern_at(static_cast<int>(std::strtol(a, nullptr, 10)));
      if (o) *o = KernObj{};
    }
    return ok("ok");
  }
  if (kern_eq(api, "irqs_disabled") || kern_eq(api, "in_interrupt") || kern_eq(api, "in_irq") ||
      kern_eq(api, "in_softirq") || kern_eq(api, "in_nmi") || kern_eq(api, "need_resched"))
    return ok("0");

  if (kern_eq(api, "spin_lock") || kern_eq(api, "spin_unlock") ||
      kern_eq(api, "spin_lock_irqsave") || kern_eq(api, "spin_unlock_irqrestore") ||
      kern_eq(api, "spin_lock_irq") || kern_eq(api, "spin_unlock_irq") ||
      kern_eq(api, "spin_lock_bh") || kern_eq(api, "spin_unlock_bh") ||
      kern_eq(api, "spin_lock_init") || kern_eq(api, "mutex_init") ||
      kern_eq(api, "mutex_lock") || kern_eq(api, "mutex_unlock") ||
      kern_eq(api, "mutex_trylock") || kern_eq(api, "rcu_read_lock") ||
      kern_eq(api, "rcu_read_unlock") || kern_eq(api, "synchronize_rcu") ||
      kern_eq(api, "call_rcu") || kern_eq(api, "down_read") || kern_eq(api, "up_read") ||
      kern_eq(api, "down_write") || kern_eq(api, "up_write") || kern_eq(api, "down") ||
      kern_eq(api, "up") || kern_eq(api, "raw_spin_lock") || kern_eq(api, "raw_spin_unlock") ||
      kern_eq(api, "smp_mb") || kern_eq(api, "smp_rmb") || kern_eq(api, "smp_wmb") ||
      kern_eq(api, "barrier")) {
    if (kern_eq(api, "mutex_trylock") || kern_eq(api, "spin_trylock")) return ok("1");
    return ok("ok");
  }
  if (kern_eq(api, "spin_trylock") || kern_eq(api, "mutex_is_locked") ||
      kern_eq(api, "spin_is_locked"))
    return ok("0");
  if (kern_eq(api, "atomic_read") || kern_eq(api, "atomic64_read"))
    return ok(a[0] ? a : "0");
  if (kern_eq(api, "atomic_set") || kern_eq(api, "atomic_inc") || kern_eq(api, "atomic_dec") ||
      kern_eq(api, "atomic_add") || kern_eq(api, "atomic_sub") || kern_eq(api, "atomic_cmpxchg"))
    return ok("ok");

  if (kern_eq(api, "schedule_work") || kern_eq(api, "queue_work") ||
      kern_eq(api, "schedule_delayed_work") || kern_eq(api, "queue_delayed_work") ||
      kern_eq(api, "INIT_WORK") || kern_eq(api, "INIT_DELAYED_WORK") ||
      kern_eq(api, "alloc_workqueue") || kern_eq(api, "create_workqueue") ||
      kern_eq(api, "alloc_ordered_workqueue")) {
    int h = kern_alloc(kKernWork);
    if (h < 0) return fail(api);
    kern_at(h)->name = a[0] ? a : "wq";
    return ok(std::to_string(h));
  }
  if (kern_eq(api, "flush_work") || kern_eq(api, "flush_workqueue") ||
      kern_eq(api, "cancel_work_sync") || kern_eq(api, "destroy_workqueue") ||
      kern_eq(api, "mod_delayed_work"))
    return ok("ok");

  if (kern_eq(api, "kthread_run") || kern_eq(api, "kthread_create")) {
    int h = kern_alloc(kKernThread);
    if (h < 0) return fail(api);
    kern_at(h)->name = a[0] ? a : "kthread";
    return ok(std::to_string(h));
  }
  if (kern_eq(api, "kthread_stop") || kern_eq(api, "kthread_park") ||
      kern_eq(api, "kthread_unpark") || kern_eq(api, "wake_up_process") ||
      kern_eq(api, "schedule") || kern_eq(api, "cond_resched") || kern_eq(api, "yield") ||
      kern_eq(api, "complete") || kern_eq(api, "complete_all") ||
      kern_eq(api, "wait_for_completion") || kern_eq(api, "msleep") ||
      kern_eq(api, "ssleep") || kern_eq(api, "usleep_range")) {
    if (kern_eq(api, "kthread_stop")) {
      KernObj* o = kern_at(static_cast<int>(std::strtol(a, nullptr, 10)));
      if (o) *o = KernObj{};
    }
    return ok("ok");
  }
  if (kern_eq(api, "kthread_should_stop")) return ok("0");
  if (kern_eq(api, "smp_processor_id") || kern_eq(api, "current") || kern_eq(api, "get_current") ||
      kern_eq(api, "task_pid_nr"))
    return ok("1");
  if (kern_eq(api, "jiffies") || kern_eq(api, "ktime_get") || kern_eq(api, "ktime_get_ns") ||
      kern_eq(api, "get_jiffies_64"))
    return ok("1");
  if (kern_eq(api, "msecs_to_jiffies") || kern_eq(api, "jiffies_to_msecs"))
    return ok(a[0] ? a : "0");

  if (kern_eq(api, "kallsyms_lookup_name") || kern_eq(api, "module_kallsyms_lookup_name") ||
      kern_eq(api, "find_symbol") || kern_eq(api, "symbol_get") || kern_eq(api, "lookup_symbol_name")) {
    if (!a[0]) return fail("kallsyms_lookup_name: empty");
    int n = 0;
    const WasmDroidApi* c = wasmdroid_catalog(&n);
    for (int i = 0; i < n; ++i)
      if (c[i].name && kern_eq(c[i].name, a))
        return ok(std::to_string(10000 + i));
    return fail("kallsyms_lookup_name: not found");
  }
  if (kern_eq(api, "request_module") || kern_eq(api, "try_then_request_module") ||
      kern_eq(api, "module_init") || kern_eq(api, "load_module") ||
      kern_eq(api, "find_module")) {
    int h = kern_alloc(kKernMod);
    if (h < 0) return fail(api);
    kern_at(h)->name = a[0] ? a : "module";
    return ok(std::to_string(h));
  }
  if (kern_eq(api, "try_module_get") || kern_eq(api, "__module_get") ||
      kern_eq(api, "module_put") || kern_eq(api, "symbol_put") ||
      kern_eq(api, "module_exit") || kern_eq(api, "delete_module"))
    return ok("ok");
  if (kern_eq(api, "THIS_MODULE") || kern_eq(api, "THIS_MODULE->name")) return ok("wasmnix");

  // kvm_host internals — GPA on the KVM hop
  if (kern_eq(api, "kvm_read_guest") || kern_eq(api, "kvm_vcpu_read_guest") ||
      kern_eq(api, "kvm_read_guest_page") || kern_eq(api, "kvm_vcpu_read_guest_page")) {
    std::string hs, rest, gpa, nstr;
    kern_split(a, &hs, &rest);
    kern_split(rest.c_str(), &gpa, &nstr);
    KvmObj* vm = kvm_vm_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!vm) return fail(api);
    size_t n = static_cast<size_t>(std::strtoul(nstr.empty() ? "16" : nstr.c_str(), nullptr, 10));
    if (n > 256) n = 256;
    unsigned char tmp[256]{};
    int got = kvm_fetch(vm, std::strtoull(gpa.c_str(), nullptr, 0), tmp, n);
    static const char* hx = "0123456789abcdef";
    std::string hex;
    for (int i = 0; i < got; ++i) {
      hex.push_back(hx[tmp[i] >> 4]);
      hex.push_back(hx[tmp[i] & 0xf]);
    }
    return ok(hex);
  }
  if (kern_eq(api, "kvm_write_guest") || kern_eq(api, "kvm_vcpu_write_guest") ||
      kern_eq(api, "kvm_write_guest_page") || kern_eq(api, "kvm_vcpu_write_guest_page") ||
      kern_eq(api, "kvm_clear_guest")) {
    std::string hs, rest, gpa, hex;
    kern_split(a, &hs, &rest);
    kern_split(rest.c_str(), &gpa, &hex);
    KvmObj* vm = kvm_vm_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!vm) return fail(api);
    auto blob = kvm_unhex(hex);
    if (blob.empty() && kern_eq(api, "kvm_clear_guest")) {
      unsigned char z = 0;
      kvm_store(vm, std::strtoull(gpa.c_str(), nullptr, 0), &z, 1);
      return ok("ok");
    }
    if (blob.empty()) return fail(api);
    kvm_store(vm, std::strtoull(gpa.c_str(), nullptr, 0), blob.data(), blob.size());
    return ok("ok");
  }
  if (kern_eq(api, "gfn_to_hva") || kern_eq(api, "gfn_to_pfn") || kern_eq(api, "gfn_to_page") ||
      kern_eq(api, "gfn_to_hva_prot") || kern_eq(api, "gfn_to_pfn_prot") ||
      kern_eq(api, "kvm_vcpu_gfn_to_hva") || kern_eq(api, "kvm_vcpu_gfn_to_pfn") ||
      kern_eq(api, "hva_to_gfn")) {
    std::string hs, gfn;
    kern_split(a, &hs, &gfn);
    unsigned long long g = std::strtoull(gfn.empty() ? a : gfn.c_str(), nullptr, 0);
    if (kern_eq(api, "hva_to_gfn")) return ok(std::to_string(g >> 12));
    return ok(std::to_string(g << 12));
  }
  if (kern_eq(api, "kvm_is_error_hva") || kern_eq(api, "kvm_is_error_pfn") ||
      kern_eq(api, "kvm_is_error_gpa"))
    return ok("0");
  if (kern_eq(api, "kvm_vcpu_kick") || kern_eq(api, "kvm_vcpu_wake_up") ||
      kern_eq(api, "kvm_vcpu_halt") || kern_eq(api, "kvm_vcpu_block") ||
      kern_eq(api, "kvm_emulate_instruction") || kern_eq(api, "kvm_skip_emulated_instruction") ||
      kern_eq(api, "kvm_emulate_halt") || kern_eq(api, "kvm_emulate_hypercall") ||
      kern_eq(api, "kvm_emulate_cpuid") || kern_eq(api, "kvm_fast_pio") ||
      kern_eq(api, "kvm_emulate_pio") || kern_eq(api, "mark_page_dirty") ||
      kern_eq(api, "kvm_vcpu_mark_page_dirty") || kern_eq(api, "kvm_flush_remote_tlbs") ||
      kern_eq(api, "kvm_mmu_reload") || kern_eq(api, "kvm_mmu_load") ||
      kern_eq(api, "kvm_mmu_unload") || kern_eq(api, "kvm_make_request") ||
      kern_eq(api, "kvm_io_bus_write") || kern_eq(api, "kvm_io_bus_read") ||
      kern_eq(api, "kvm_io_bus_register_dev") || kern_eq(api, "kvm_set_memory_region") ||
      kern_eq(api, "kvm_arch_vcpu_load") || kern_eq(api, "kvm_arch_vcpu_put") ||
      kern_eq(api, "kvm_get_kvm") || kern_eq(api, "kvm_put_kvm") ||
      kern_eq(api, "vmx_vcpu_run") || kern_eq(api, "svm_vcpu_run"))
    return ok("ok");
  if (kern_eq(api, "kvm_get_vcpu") || kern_eq(api, "kvm_dev_ioctl_create_vm") ||
      kern_eq(api, "kvm_vm_ioctl_create_vcpu") || kern_eq(api, "kvm_init")) {
    if (kern_eq(api, "kvm_dev_ioctl_create_vm") || kern_eq(api, "kvm_init")) {
      std::string kv;
      if (try_kvm("KVM_CREATE_VM", "", &kv)) return ok(kv);
    }
    return ok("0");
  }
  if (kern_eq(api, "kvm_vm_ioctl_check_extension")) {
    std::string kv;
    return try_kvm("KVM_CHECK_EXTENSION", a, out);
  }

  if (kern_eq(api, "pci_enable_device") || kern_eq(api, "pci_set_master") ||
      kern_eq(api, "pci_request_regions") || kern_eq(api, "pci_register_driver") ||
      kern_eq(api, "pci_disable_device") || kern_eq(api, "pci_clear_master") ||
      kern_eq(api, "pci_release_regions") || kern_eq(api, "pci_unregister_driver") ||
      kern_eq(api, "dma_set_mask") || kern_eq(api, "dma_set_coherent_mask") ||
      kern_eq(api, "dma_map_single") || kern_eq(api, "dma_unmap_single") ||
      kern_eq(api, "iommu_map") || kern_eq(api, "iommu_unmap") ||
      kern_eq(api, "iommu_attach_device") || kern_eq(api, "iommu_detach_device")) {
    if (kern_eq(api, "iommu_domain_alloc") || kern_eq(api, "pci_enable_device")) {
      /* fall through ok */
    }
    return ok("ok");
  }
  if (kern_eq(api, "iommu_domain_alloc") || kern_eq(api, "iommu_group_get")) {
    int h = kern_alloc(kKernMap);
    if (h < 0) return fail(api);
    return ok(std::to_string(h));
  }
  if (kern_eq(api, "iommu_domain_free") || kern_eq(api, "iommu_group_put")) {
    KernObj* o = kern_at(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (o) *o = KernObj{};
    return ok("ok");
  }
  if (kern_eq(api, "iommu_present") || kern_eq(api, "iommu_capable")) return ok("1");

  if (kern_eq(api, "bpf") || kern_eq(api, "bpf_prog_load") || kern_eq(api, "BPF_PROG_LOAD") ||
      kern_eq(api, "bpf_map_create") || kern_eq(api, "BPF_MAP_CREATE")) {
    int h = kern_alloc(kKernBpf);
    if (h < 0) return fail(api);
    kern_at(h)->name = api;
    return ok(std::to_string(h));
  }
  if (kern_pfx(api, "BPF_") || kern_pfx(api, "bpf_")) return ok("ok");

  if (kern_eq(api, "TUNSETIFF") || kern_eq(api, "TUNGETIFF") || kern_eq(api, "IFF_TUN") ||
      kern_eq(api, "IFF_TAP")) {
    if (kern_eq(api, "TUNSETIFF")) {
      int h = kern_alloc(kKernNet);
      if (h < 0) return fail(api);
      kern_at(h)->name = a[0] ? a : "tun0";
      return ok(std::to_string(h));
    }
    if (kern_eq(api, "IFF_TUN")) return ok("1");
    if (kern_eq(api, "IFF_TAP")) return ok("2");
    return ok("ok");
  }
  if (kern_pfx(api, "TUN") || kern_pfx(api, "IFF_")) return ok("ok");

  if (kern_pfx(api, "NETLINK_") || kern_pfx(api, "RTM_") || kern_pfx(api, "NLMSG_") ||
      kern_pfx(api, "NLM_") || kern_pfx(api, "NLA_") || kern_eq(api, "nl_socket_alloc") ||
      kern_eq(api, "nl_connect") || kern_eq(api, "nl_send_auto")) {
    if (kern_eq(api, "nl_socket_alloc")) {
      int h = kern_alloc(kKernNet);
      if (h < 0) return fail(api);
      return ok(std::to_string(h));
    }
    if (kern_pfx(api, "NETLINK_")) return ok("16");  // NETLINK_GENERIC-ish
    if (kern_pfx(api, "RTM_")) return ok("16");
    return ok("ok");
  }

  if (kern_pfx(api, "PTRACE_") || kern_pfx(api, "PR_") || kern_pfx(api, "UFFD") ||
      kern_pfx(api, "IORING_") || kern_pfx(api, "CLONE_") || kern_pfx(api, "AF_") ||
      kern_pfx(api, "VMADDR_") || kern_pfx(api, "VIRTIO_") || kern_pfx(api, "EXPORT_") ||
      kern_pfx(api, "MODULE_")) {
    if (kern_eq(api, "PTRACE_TRACEME") || kern_eq(api, "PR_GET_NO_NEW_PRIVS") ||
        kern_eq(api, "PR_GET_SECCOMP") || kern_eq(api, "PR_GET_DUMPABLE"))
      return ok("0");
    if (kern_eq(api, "CLONE_NEWNS")) return ok("0x20000");
    if (kern_eq(api, "CLONE_NEWNET")) return ok("0x40000000");
    return ok("ok");
  }

  if (kern_eq(api, "sysctl") || kern_eq(api, "_sysctl") || kern_eq(api, "register_sysctl"))
    return ok("ok");
  if (kern_pfx(api, "kernel.") || kern_pfx(api, "vm.") || kern_pfx(api, "net.") ||
      kern_pfx(api, "fs.") || kern_pfx(api, "user."))
    return ok("0");

  if (!kern_cataloged(api)) return false;
  return ok("ok");
}

}  // namespace wasmdroid

#endif  // WASMDROID_INCLUDE_DROID_KERNEL_HOST_HPP_
