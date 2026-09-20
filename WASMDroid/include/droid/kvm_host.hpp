#ifndef WASMDROID_INCLUDE_DROID_KVM_HOST_HPP_
#define WASMDROID_INCLUDE_DROID_KVM_HOST_HPP_

// In-module Linux hypervisor hop. Names follow include/uapi/linux/kvm.h
// (KVM_CREATE_VM / KVM_RUN / KVM_SET_USER_MEMORY_REGION), vfio.h,
// vhost.h, and mshv.h, pKVM, Gunyah — the Android analog of WinHvPlatform (pKVM / AVF / Gunyah) /
// WinHvEmulation. The mapper does not JIT x86: KVM_RUN decodes HLT /
// INT3 / IN/OUT on GPA and returns KVM_EXIT_*. KvmEmulateIo /
// KvmEmulateMmio are the same hop as WHvEmulatorTryIoEmulation.

#include "droid/catalog.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace wasmdroid {

enum { kKvmBase = 4096 };
enum { kKvmMax = 128 };
enum {
  kKvmVm = 1,
  kKvmVcpu = 2,
  kKvmDev = 3,
  kKvmVfio = 4,
  kKvmVhost = 5,
  kKvmMshv = 6,
  kKvmPkvm = 7,
  kKvmGunyah = 8,
  kKvmAvf = 9
};

struct KvmGpa {
  unsigned long long gpa = 0;
  unsigned long long size = 0;
  std::vector<unsigned char> bytes;
};

struct KvmObj {
  int used = 0;
  int kind = 0;
  int parent = 0;
  unsigned vpindex = 0;
  unsigned long long rip = 0;
  unsigned last_reason = 5;  // KVM_EXIT_HLT
  unsigned last_ilen = 1;
  unsigned last_port = 0;
  unsigned last_dir = 0;
  unsigned last_size = 1;
  unsigned long long last_gpa = 0;
  unsigned long long last_data = 0;
  std::vector<KvmGpa> gpa;
  std::map<unsigned, unsigned long long> regs;
  std::map<unsigned, unsigned> io_ports;
  std::map<unsigned, unsigned long long> msrs;
  std::string name;
};

inline KvmObj* kvm_tab() {
  static KvmObj t[kKvmMax];
  return t;
}

inline bool kvm_eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline void kvm_split(const char* a, std::string* l, std::string* r) {
  const char* p = a ? std::strchr(a, '\x1f') : nullptr;
  if (!p) {
    *l = a ? a : "";
    r->clear();
    return;
  }
  l->assign(a, p);
  *r = p + 1;
}

inline unsigned long long kvm_u64(const std::string& s) {
  if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    return std::strtoull(s.c_str() + 2, nullptr, 16);
  return std::strtoull(s.c_str(), nullptr, 0);
}

inline int kvm_alloc(int kind) {
  KvmObj* t = kvm_tab();
  for (int i = 0; i < kKvmMax; ++i) {
    if (!t[i].used) {
      t[i] = KvmObj{};
      t[i].used = 1;
      t[i].kind = kind;
      return kKvmBase + i;
    }
  }
  return -1;
}

inline KvmObj* kvm_at(int h) {
  int i = h - kKvmBase;
  if (i < 0 || i >= kKvmMax || !kvm_tab()[i].used) return nullptr;
  return &kvm_tab()[i];
}

inline bool kvm_close(int h) {
  KvmObj* p = kvm_at(h);
  if (!p) return false;
  if (p->kind == kKvmVm || p->kind == kKvmMshv || p->kind == kKvmPkvm ||
      p->kind == kKvmGunyah || p->kind == kKvmAvf) {
    KvmObj* t = kvm_tab();
    for (int i = 0; i < kKvmMax; ++i)
      if (t[i].used && t[i].parent == h) t[i] = KvmObj{};
  }
  *p = KvmObj{};
  return true;
}

inline KvmObj* kvm_vcpu(int vm, unsigned idx) {
  KvmObj* t = kvm_tab();
  for (int i = 0; i < kKvmMax; ++i)
    if (t[i].used && t[i].kind == kKvmVcpu && t[i].parent == vm && t[i].vpindex == idx)
      return &t[i];
  return nullptr;
}

inline KvmObj* kvm_vcpu_of(int h) {
  if (KvmObj* p = kvm_at(h)) {
    if (p->kind == kKvmVcpu) return p;
    if (p->kind == kKvmVm || p->kind == kKvmMshv || p->kind == kKvmPkvm ||
        p->kind == kKvmGunyah || p->kind == kKvmAvf) {
      if (KvmObj* v = kvm_vcpu(h, 0)) return v;
    }
  }
  return nullptr;
}

inline KvmObj* kvm_vm_of(int h) {
  KvmObj* p = kvm_at(h);
  if (!p) return nullptr;
  if (p->kind == kKvmVm || p->kind == kKvmMshv || p->kind == kKvmPkvm ||
      p->kind == kKvmGunyah || p->kind == kKvmAvf)
    return p;
  if (p->kind == kKvmVcpu) return kvm_at(p->parent);
  return nullptr;
}

inline KvmGpa* kvm_gpa_at(KvmObj* vm, unsigned long long gpa) {
  if (!vm) return nullptr;
  for (auto& g : vm->gpa)
    if (gpa >= g.gpa && gpa < g.gpa + g.size) return &g;
  return nullptr;
}

inline int kvm_fetch(KvmObj* vm, unsigned long long gpa, void* dst, size_t n) {
  KvmGpa* g = kvm_gpa_at(vm, gpa);
  if (!g || !dst || !n) return 0;
  size_t off = static_cast<size_t>(gpa - g->gpa);
  if (off >= g->bytes.size()) return 0;
  if (off + n > g->bytes.size()) n = g->bytes.size() - off;
  std::memcpy(dst, g->bytes.data() + off, n);
  return static_cast<int>(n);
}

inline int kvm_store(KvmObj* vm, unsigned long long gpa, const void* src, size_t n) {
  KvmGpa* g = kvm_gpa_at(vm, gpa);
  if (!g || !src || !n) return 0;
  size_t off = static_cast<size_t>(gpa - g->gpa);
  if (off >= g->bytes.size()) return 0;
  if (off + n > g->bytes.size()) n = g->bytes.size() - off;
  std::memcpy(&g->bytes[off], src, n);
  return static_cast<int>(n);
}

inline void kvm_map_bytes(KvmObj* vm, unsigned long long gpa, const void* src, size_t n) {
  if (!vm || !n) return;
  KvmGpa g;
  g.gpa = gpa & ~4095ull;
  g.size = (static_cast<unsigned long long>(n) + 4095ull) & ~4095ull;
  if (!g.size) g.size = 4096;
  g.bytes.assign(static_cast<size_t>(g.size), 0);
  if (src) {
    size_t cpy = n < g.bytes.size() ? n : g.bytes.size();
    std::memcpy(g.bytes.data(), src, cpy);
  }
  vm->gpa.push_back(std::move(g));
}

inline std::vector<unsigned char> kvm_unhex(const std::string& s) {
  std::vector<unsigned char> b;
  auto nyb = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  size_t i = 0;
  if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) i = 2;
  if ((s.size() - i) % 2) return b;
  for (; i + 1 < s.size(); i += 2) {
    int hi = nyb(s[i]), lo = nyb(s[i + 1]);
    if (hi < 0 || lo < 0) {
      b.clear();
      return b;
    }
    b.push_back(static_cast<unsigned char>((hi << 4) | lo));
  }
  return b;
}

inline unsigned kvm_reg(const std::string& s) {
  std::string t = s;
  for (char& c : t)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  static const char* gp[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                             "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
  for (unsigned i = 0; i < 16; ++i)
    if (t == gp[i]) return i;
  if (t == "rip" || t == "eip") return 16;
  if (t == "rflags" || t == "eflags") return 17;
  if (t == "cr0") return 18;
  if (t == "cr2") return 19;
  if (t == "cr3") return 20;
  if (t == "cr4") return 21;
  if (t == "cr8") return 22;
  if (t == "efer") return 23;
  return static_cast<unsigned>(kvm_u64(s));
}

inline const char* kvm_exit_name(unsigned r) {
  switch (r) {
    case 0: return "unknown";
    case 1: return "exception";
    case 2: return "io";
    case 3: return "hypercall";
    case 4: return "debug";
    case 5: return "hlt";
    case 6: return "mmio";
    case 7: return "irq_window_open";
    case 8: return "shutdown";
    case 9: return "fail_entry";
    case 10: return "intr";
    case 16: return "nmi";
    case 17: return "internal_error";
    case 21: return "watchdog";
    case 24: return "system_event";
    case 27: return "hyperv";
    case 29: return "rdmsr";
    case 30: return "wrmsr";
    case 31: return "dirty_ring_full";
    case 33: return "bus_lock";
    case 34: return "xen";
    case 39: return "memory_fault";
    case 40: return "tdx";
    default: return "exit";
  }
}

inline std::string kvm_exit_str(unsigned reason, unsigned long long rip, const std::string& extra) {
  std::string s = std::to_string(reason);
  s += "\x1f";
  s += kvm_exit_name(reason);
  s += "\x1f";
  s += std::to_string(rip);
  if (!extra.empty()) {
    s += "\x1f";
    s += extra;
  }
  return s;
}

inline std::string kvm_decode_run(KvmObj* vm, KvmObj* vp) {
  if (!vm || !vp) return kvm_exit_str(5, 0, {});
  unsigned long long rip = vp->regs.count(16) ? vp->regs[16] : vp->rip;
  vp->rip = rip;
  unsigned char op = 0;
  int n = kvm_fetch(vm, rip, &op, 1);
  vp->last_reason = 5;
  vp->last_ilen = 1;
  vp->last_port = 0;
  vp->last_dir = 0;
  vp->last_size = 1;
  vp->last_gpa = rip;
  vp->last_data = 0;
  if (n <= 0) return kvm_exit_str(5, rip, {});
  if (op == 0xF4) {
    vp->last_reason = 5;
    return kvm_exit_str(5, rip, {});
  }
  if (op == 0xCC) {
    vp->last_reason = 1;
    return kvm_exit_str(1, rip, "3");
  }
  if (op == 0x0F) {
    unsigned char op2 = 0;
    kvm_fetch(vm, rip + 1, &op2, 1);
    if (op2 == 0x01) {
      unsigned char mod = 0;
      kvm_fetch(vm, rip + 2, &mod, 1);
      if ((mod & 0x38) == 0x30) {  // vmrun / vmenter-ish
        vp->last_reason = 3;
        vp->last_ilen = 3;
        return kvm_exit_str(3, rip, "nested");
      }
    }
    if (op2 == 0x30 || op2 == 0x32) {  // wrmsr / rdmsr
      vp->last_reason = (op2 == 0x30) ? 30u : 29u;
      vp->last_ilen = 2;
      unsigned ecx = static_cast<unsigned>(vp->regs[1] & 0xffffffffull);
      return kvm_exit_str(vp->last_reason, rip, std::to_string(ecx));
    }
  }
  unsigned long long rax = vp->regs[0];
  unsigned long long rdx = vp->regs[2];
  unsigned char imm = 0;
  if (op == 0xE6 || op == 0xE4) {
    kvm_fetch(vm, rip + 1, &imm, 1);
    vp->last_reason = 2;
    vp->last_ilen = 2;
    vp->last_port = imm;
    vp->last_dir = (op == 0xE6) ? 1u : 0u;  // 1=out, 0=in
    vp->last_data = rax & 0xffull;
    return kvm_exit_str(2, rip,
                        std::to_string(imm) + "\x1f" + (op == 0xE6 ? "out" : "in"));
  }
  if (op == 0xEE || op == 0xEC) {
    vp->last_reason = 2;
    vp->last_ilen = 1;
    vp->last_port = static_cast<unsigned>(rdx & 0xffffull);
    vp->last_dir = (op == 0xEE) ? 1u : 0u;
    vp->last_data = rax & 0xffull;
    return kvm_exit_str(2, rip, std::to_string(vp->last_port) + "\x1f" +
                                    (op == 0xEE ? "out" : "in"));
  }
  // Unmapped fetch already HLT; unknown opcode is MMIO-ish memory fault at RIP.
  vp->last_reason = 6;
  vp->last_gpa = rip;
  return kvm_exit_str(6, rip, std::to_string(rip));
}

inline std::string kvm_emulate_io(KvmObj* vm, KvmObj* vp) {
  if (!vm || !vp || vp->last_reason != 2) return {};
  if (vp->last_dir == 1)
    vm->io_ports[vp->last_port] = static_cast<unsigned>(vp->last_data);
  else {
    unsigned v = vm->io_ports[vp->last_port];
    vp->regs[0] = (vp->regs[0] & ~0xffull) | (v & 0xffu);
  }
  vp->rip += vp->last_ilen;
  vp->regs[16] = vp->rip;
  return "1";
}

inline std::string kvm_emulate_mmio(KvmObj* vm, KvmObj* vp) {
  if (!vm || !vp || vp->last_reason != 6) return {};
  unsigned n = vp->last_size ? vp->last_size : 1;
  unsigned char buf[8]{};
  if (vp->last_dir == 1) {
    unsigned long long d = vp->last_data;
    std::memcpy(buf, &d, n > 8 ? 8 : n);
    kvm_store(vm, vp->last_gpa, buf, n > 8 ? 8 : n);
  } else {
    kvm_fetch(vm, vp->last_gpa, buf, n > 8 ? 8 : n);
    unsigned long long d = 0;
    std::memcpy(&d, buf, n > 8 ? 8 : n);
    vp->regs[0] = d;
  }
  vp->rip += vp->last_ilen;
  vp->regs[16] = vp->rip;
  return "1";
}

inline bool kvm_pfx(const char* api, const char* p) {
  if (!api || !p) return false;
  size_t n = std::strlen(p);
  return std::strncmp(api, p, n) == 0;
}

inline bool kvm_cataloged(const char* api) {
  if (!api) return false;
  if (kvm_pfx(api, "KVM_") || kvm_pfx(api, "Kvm") || kvm_pfx(api, "VFIO_") ||
      kvm_pfx(api, "VHOST_") || kvm_pfx(api, "MSHV_") || kvm_pfx(api, "PKVM_") ||
      kvm_pfx(api, "GUNYAH_") || kvm_pfx(api, "GH_") || kvm_pfx(api, "AVF_") ||
      kvm_pfx(api, "IVirtual") || kvm_eq(api, "VirtualizationService")) {
    int n = 0;
    const WasmDroidApi* c = wasmdroid_catalog(&n);
    for (int i = 0; i < n; ++i)
      if (c[i].name && kvm_eq(c[i].name, api)) return true;
    return kvm_pfx(api, "KVM_") || kvm_pfx(api, "KvmEmulate") || kvm_pfx(api, "MSHV_") ||
           kvm_pfx(api, "VFIO_") || kvm_pfx(api, "VHOST_") || kvm_pfx(api, "PKVM_") ||
           kvm_pfx(api, "GUNYAH_") || kvm_pfx(api, "GH_") || kvm_pfx(api, "AVF_");
  }
  return false;
}

inline std::string kvm_cap_val(const char* a) {
  if (!a || !a[0]) return "1";
  std::string t = a;
  for (char& c : t)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  if (t.find("user_memory") != std::string::npos || t == "3" || t == "kvm_cap_user_memory")
    return "1";
  if (t.find("irqchip") != std::string::npos || t == "0") return "1";
  if (t.find("hlt") != std::string::npos || t == "1") return "1";
  if (t.find("irqfd") != std::string::npos || t.find("ioeventfd") != std::string::npos)
    return "1";
  if (t.find("nested") != std::string::npos) return "1";
  if (t.find("guest_memfd") != std::string::npos) return "1";
  if (t.find("hyperv") != std::string::npos) return "1";
  if (t.find("protected") != std::string::npos || t.find("pkvm") != std::string::npos ||
      t.find("arm_el2") != std::string::npos)
    return "1";
  if (t.find("nr_vcpus") != std::string::npos || t == "9") return "64";
  if (t.find("nr_memslots") != std::string::npos || t == "10") return "509";
  if (t.find("max_vcpus") != std::string::npos || t == "66") return "288";
  return "1";
}

// Returns true if this API is on the hypervisor hop (even on failure).
inline bool try_kvm(const char* api, const char* args, std::string* out) {
  if (!kvm_cataloged(api) && !(api && (kvm_pfx(api, "KVM_") || kvm_pfx(api, "Kvm") ||
                                       kvm_pfx(api, "VFIO_") || kvm_pfx(api, "VHOST_") ||
                                       kvm_pfx(api, "MSHV_") || kvm_pfx(api, "PKVM_") ||
                                       kvm_pfx(api, "GUNYAH_") || kvm_pfx(api, "GH_") ||
                                       kvm_pfx(api, "AVF_") || kvm_pfx(api, "IVirtual") ||
                                       kvm_eq(api, "VirtualizationService"))))
    return false;
  const char* a = args ? args : "";
  auto ok = [&](std::string s) {
    *out = std::move(s);
    return true;
  };
  auto fail = [&](const char* m) {
    *out = std::string("error: ") + m;
    return true;
  };

  if (kvm_eq(api, "KVM_GET_API_VERSION") || kvm_eq(api, "VFIO_GET_API_VERSION"))
    return ok("12");
  if (kvm_eq(api, "KVM_GET_VCPU_MMAP_SIZE")) return ok("4096");
  if (kvm_eq(api, "KVM_CHECK_EXTENSION") || kvm_pfx(api, "KVM_CAP_"))
    return ok(kvm_cap_val(kvm_eq(api, "KVM_CHECK_EXTENSION") ? a : api));

  if (kvm_eq(api, "KVM_CREATE_VM") || kvm_eq(api, "MSHV_CREATE_PARTITION") ||
      kvm_eq(api, "PKVM_CREATE_VM") || kvm_eq(api, "GUNYAH_CREATE_VM") ||
      kvm_eq(api, "GH_CREATE_VM") || kvm_eq(api, "AVF_CREATE_VM") ||
      kvm_eq(api, "IVirtualMachine_create") || kvm_eq(api, "VirtualizationService")) {
    int kind = kKvmVm;
    if (kvm_eq(api, "MSHV_CREATE_PARTITION")) kind = kKvmMshv;
    if (kvm_eq(api, "PKVM_CREATE_VM")) kind = kKvmPkvm;
    if (kvm_eq(api, "GUNYAH_CREATE_VM") || kvm_eq(api, "GH_CREATE_VM")) kind = kKvmGunyah;
    if (kvm_eq(api, "AVF_CREATE_VM") || kvm_eq(api, "IVirtualMachine_create") ||
        kvm_eq(api, "VirtualizationService"))
      kind = kKvmAvf;
    int h = kvm_alloc(kind);
    if (h < 0) return fail("KVM_CREATE_VM");
    return ok(std::to_string(h));
  }
  if (kvm_eq(api, "KVM_CREATE_VCPU") || kvm_eq(api, "MSHV_CREATE_VP") ||
      kvm_eq(api, "PKVM_CREATE_VCPU") || kvm_eq(api, "GUNYAH_CREATE_VCPU") ||
      kvm_eq(api, "AVF_CREATE_VCPU")) {
    std::string hs, idx;
    kvm_split(a, &hs, &idx);
    int vm = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    KvmObj* p = kvm_at(vm);
    if (!p || (p->kind != kKvmVm && p->kind != kKvmMshv && p->kind != kKvmPkvm &&
               p->kind != kKvmGunyah && p->kind != kKvmAvf))
      return fail(api);
    unsigned n = static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10));
    if (KvmObj* ex = kvm_vcpu(vm, n)) return ok(std::to_string(kKvmBase + (int)(ex - kvm_tab())));
    int v = kvm_alloc(kKvmVcpu);
    if (v < 0) return fail(api);
    kvm_at(v)->parent = vm;
    kvm_at(v)->vpindex = n;
    kvm_at(v)->regs[16] = 0;
    return ok(std::to_string(v));
  }

  if (kvm_eq(api, "KVM_SET_USER_MEMORY_REGION") || kvm_eq(api, "KVM_SET_USER_MEMORY_REGION2") ||
      kvm_eq(api, "MSHV_MAP_GUEST_MEMORY") || kvm_eq(api, "VFIO_IOMMU_MAP_DMA")) {
    std::string hs, rest, gpa, sz, img;
    kvm_split(a, &hs, &rest);
    kvm_split(rest.c_str(), &gpa, &rest);
    kvm_split(rest.c_str(), &sz, &img);
    KvmObj* vm = kvm_vm_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!vm) return fail(api);
    size_t n = static_cast<size_t>(kvm_u64(sz));
    std::vector<unsigned char> blob = kvm_unhex(img);
    const void* src = nullptr;
    if (!blob.empty()) {
      src = blob.data();
      if (!n) n = blob.size();
    } else if (!img.empty() && img.find_first_not_of("0123456789") == std::string::npos) {
      uintptr_t hp = static_cast<uintptr_t>(std::strtoull(img.c_str(), nullptr, 10));
      src = reinterpret_cast<const void*>(hp);
    }
    if (!n) n = 4096;
    kvm_map_bytes(vm, kvm_u64(gpa), src, n);
    if (!blob.empty() && blob.size() < n)
      kvm_store(vm, kvm_u64(gpa), blob.data(), blob.size());
    return ok("ok");
  }
  if (kvm_eq(api, "KVM_GET_DIRTY_LOG") || kvm_eq(api, "KVM_CLEAR_DIRTY_LOG") ||
      kvm_eq(api, "MSHV_UNMAP_GUEST_MEMORY") || kvm_eq(api, "VFIO_IOMMU_UNMAP_DMA")) {
    std::string hs, gpa;
    kvm_split(a, &hs, &gpa);
    KvmObj* vm = kvm_vm_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!vm) return fail(api);
    if (kvm_eq(api, "KVM_GET_DIRTY_LOG")) return ok("0");
    unsigned long long g = kvm_u64(gpa);
    for (auto it = vm->gpa.begin(); it != vm->gpa.end(); ++it) {
      if (it->gpa == g) {
        vm->gpa.erase(it);
        break;
      }
    }
    return ok("ok");
  }

  if (kvm_eq(api, "KVM_RUN") || kvm_eq(api, "MSHV_RUN_VP") || kvm_eq(api, "PKVM_RUN") ||
      kvm_eq(api, "GUNYAH_RUN") || kvm_eq(api, "AVF_RUN")) {
    std::string hs, idx;
    kvm_split(a, &hs, &idx);
    int h = static_cast<int>(std::strtol(hs.c_str(), nullptr, 10));
    KvmObj* vp = kvm_vcpu_of(h);
    if (!vp && kvm_at(h) &&
        (kvm_at(h)->kind == kKvmVm || kvm_at(h)->kind == kKvmMshv ||
         kvm_at(h)->kind == kKvmPkvm || kvm_at(h)->kind == kKvmGunyah ||
         kvm_at(h)->kind == kKvmAvf)) {
      unsigned n = static_cast<unsigned>(std::strtoul(idx.c_str(), nullptr, 10));
      vp = kvm_vcpu(h, n);
      if (!vp) {
        int v = kvm_alloc(kKvmVcpu);
        if (v < 0) return fail(api);
        vp = kvm_at(v);
        vp->parent = h;
        vp->vpindex = n;
      }
    }
    KvmObj* vm = vp ? kvm_at(vp->parent) : nullptr;
    if (!vm || !vp) return fail(api);
    return ok(kvm_decode_run(vm, vp));
  }
  if (kvm_eq(api, "KvmEmulateIo") || kvm_eq(api, "KVM_EXIT_IO")) {
    if (kvm_eq(api, "KVM_EXIT_IO") && !a[0]) return ok("2");
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    KvmObj* vp = kvm_vcpu_of(h);
    KvmObj* vm = vp ? kvm_at(vp->parent) : nullptr;
    std::string r = kvm_emulate_io(vm, vp);
    if (r.empty()) return fail(api);
    return ok(r);
  }
  if (kvm_eq(api, "KvmEmulateMmio") || kvm_eq(api, "KVM_EXIT_MMIO")) {
    if (kvm_eq(api, "KVM_EXIT_MMIO") && !a[0]) return ok("6");
    int h = static_cast<int>(std::strtol(a, nullptr, 10));
    KvmObj* vp = kvm_vcpu_of(h);
    KvmObj* vm = vp ? kvm_at(vp->parent) : nullptr;
    std::string r = kvm_emulate_mmio(vm, vp);
    if (r.empty()) return fail(api);
    return ok(r);
  }

  if (kvm_eq(api, "KVM_GET_REGS") || kvm_eq(api, "KVM_GET_SREGS") ||
      kvm_eq(api, "KVM_GET_SREGS2") || kvm_eq(api, "KVM_GET_ONE_REG") ||
      kvm_eq(api, "MSHV_GET_VP_REGISTERS") || kvm_eq(api, "KVM_GET_MP_STATE") ||
      kvm_eq(api, "KVM_GET_VCPU_EVENTS") || kvm_eq(api, "KVM_GET_FPU") ||
      kvm_eq(api, "KVM_GET_XSAVE") || kvm_eq(api, "KVM_GET_XSAVE2") ||
      kvm_eq(api, "KVM_GET_XCRS") || kvm_eq(api, "KVM_GET_LAPIC") ||
      kvm_eq(api, "KVM_GET_DEBUGREGS")) {
    std::string hs, name;
    kvm_split(a, &hs, &name);
    KvmObj* vp = kvm_vcpu_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!vp) return fail(api);
    if (kvm_eq(api, "KVM_GET_MP_STATE")) return ok("0");  // RUNNABLE
    unsigned r = name.empty() ? 16u : kvm_reg(name);
    if (r == 16) return ok(std::to_string(vp->rip));
    auto it = vp->regs.find(r);
    return ok(std::to_string(it == vp->regs.end() ? 0ull : it->second));
  }
  if (kvm_eq(api, "KVM_SET_REGS") || kvm_eq(api, "KVM_SET_SREGS") ||
      kvm_eq(api, "KVM_SET_SREGS2") || kvm_eq(api, "KVM_SET_ONE_REG") ||
      kvm_eq(api, "MSHV_SET_VP_REGISTERS") || kvm_eq(api, "KVM_SET_MP_STATE") ||
      kvm_eq(api, "KVM_SET_VCPU_EVENTS") || kvm_eq(api, "KVM_SET_FPU") ||
      kvm_eq(api, "KVM_SET_XSAVE") || kvm_eq(api, "KVM_SET_XCRS") ||
      kvm_eq(api, "KVM_SET_LAPIC") || kvm_eq(api, "KVM_SET_DEBUGREGS")) {
    std::string hs, rest, name, val;
    kvm_split(a, &hs, &rest);
    kvm_split(rest.c_str(), &name, &val);
    KvmObj* vp = kvm_vcpu_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!vp) return fail(api);
    if (val.empty()) {
      val = name;
      name = "rip";
    }
    unsigned r = kvm_reg(name);
    unsigned long long v = kvm_u64(val);
    vp->regs[r] = v;
    if (r == 16) vp->rip = v;
    return ok("ok");
  }
  if (kvm_eq(api, "KVM_TRANSLATE") || kvm_eq(api, "MSHV_VP_TRANSLATE_GVA")) {
    std::string hs, gva;
    kvm_split(a, &hs, &gva);
    if (!kvm_vcpu_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10))) &&
        !kvm_vm_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10))))
      return fail(api);
    return ok(gva.empty() ? a : gva);
  }
  if (kvm_eq(api, "KVM_INTERRUPT") || kvm_eq(api, "KVM_NMI") || kvm_eq(api, "KVM_SMI") ||
      kvm_eq(api, "MSHV_ASSERT_INTERRUPT") || kvm_eq(api, "KVM_IRQ_LINE") ||
      kvm_eq(api, "KVM_IRQ_LINE_STATUS") || kvm_eq(api, "KVM_SIGNAL_MSI")) {
    if (!kvm_at(static_cast<int>(std::strtol(a, nullptr, 10))) && a[0]) {
      std::string hs, rest;
      kvm_split(a, &hs, &rest);
      if (!kvm_at(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)))) return fail(api);
    }
    return ok("ok");
  }
  if (kvm_eq(api, "KVM_GET_MSRS") || kvm_eq(api, "KVM_SET_MSRS")) {
    std::string hs, rest, idx, val;
    kvm_split(a, &hs, &rest);
    kvm_split(rest.c_str(), &idx, &val);
    KvmObj* vp = kvm_vcpu_of(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!vp) return fail(api);
    unsigned m = static_cast<unsigned>(kvm_u64(idx));
    if (kvm_eq(api, "KVM_SET_MSRS")) {
      vp->msrs[m] = kvm_u64(val);
      return ok("ok");
    }
    return ok(std::to_string(vp->msrs[m]));
  }
  if (kvm_eq(api, "KVM_SET_CPUID") || kvm_eq(api, "KVM_SET_CPUID2") ||
      kvm_eq(api, "KVM_GET_CPUID2") || kvm_eq(api, "KVM_GET_SUPPORTED_CPUID") ||
      kvm_eq(api, "KVM_GET_EMULATED_CPUID") || kvm_eq(api, "KVM_GET_SUPPORTED_HV_CPUID"))
    return ok("1");
  if (kvm_eq(api, "KVM_CREATE_IRQCHIP") || kvm_eq(api, "KVM_CREATE_PIT") ||
      kvm_eq(api, "KVM_CREATE_PIT2") || kvm_eq(api, "KVM_SET_GSI_ROUTING") ||
      kvm_eq(api, "KVM_SET_TSS_ADDR") || kvm_eq(api, "KVM_SET_IDENTITY_MAP_ADDR") ||
      kvm_eq(api, "KVM_SET_BOOT_CPU_ID") || kvm_eq(api, "KVM_ENABLE_CAP") ||
      kvm_eq(api, "KVM_IRQFD") || kvm_eq(api, "KVM_IOEVENTFD") ||
      kvm_eq(api, "KVM_SET_GUEST_DEBUG") || kvm_eq(api, "KVM_KVMCLOCK_CTRL") ||
      kvm_eq(api, "KVM_SET_CLOCK") || kvm_eq(api, "KVM_GET_CLOCK") ||
      kvm_eq(api, "KVM_SET_TSC_KHZ") || kvm_eq(api, "KVM_REGISTER_COALESCED_MMIO") ||
      kvm_eq(api, "KVM_UNREGISTER_COALESCED_MMIO") || kvm_eq(api, "KVM_XEN_HVM_CONFIG") ||
      kvm_eq(api, "KVM_SET_MEMORY_ATTRIBUTES") || kvm_eq(api, "KVM_PRE_FAULT_MEMORY") ||
      kvm_eq(api, "MSHV_IRQFD") || kvm_eq(api, "MSHV_IOEVENTFD") ||
      kvm_eq(api, "MSHV_INSTALL_INTERCEPT") || kvm_eq(api, "MSHV_SET_PARTITION_PROPERTY") ||
      kvm_eq(api, "MSHV_GET_PARTITION_PROPERTY")) {
    std::string hs, rest;
    kvm_split(a, &hs, &rest);
    int h = static_cast<int>(std::strtol(hs.empty() ? a : hs.c_str(), nullptr, 10));
    if (a[0] && !kvm_at(h) && !kvm_vm_of(h)) return fail(api);
    if (kvm_eq(api, "KVM_GET_TSC_KHZ")) return ok("2000000");
    if (kvm_eq(api, "KVM_GET_CLOCK") || kvm_eq(api, "MSHV_GET_PARTITION_PROPERTY"))
      return ok("0");
    return ok("ok");
  }
  if (kvm_eq(api, "KVM_GET_TSC_KHZ")) return ok("2000000");
  if (kvm_eq(api, "KVM_CREATE_DEVICE") || kvm_eq(api, "KVM_SET_DEVICE_ATTR") ||
      kvm_eq(api, "KVM_GET_DEVICE_ATTR") || kvm_eq(api, "KVM_HAS_DEVICE_ATTR")) {
    if (kvm_eq(api, "KVM_CREATE_DEVICE")) {
      int h = kvm_alloc(kKvmDev);
      if (h < 0) return fail(api);
      return ok(std::to_string(h));
    }
    return ok("ok");
  }
  if (kvm_eq(api, "KVM_CREATE_GUEST_MEMFD") || kvm_eq(api, "KVM_GET_STATS_FD") ||
      kvm_eq(api, "KVM_GET_REG_LIST") || kvm_eq(api, "KVM_MEMORY_ENCRYPT_OP") ||
      kvm_eq(api, "KVM_GET_NESTED_STATE") || kvm_eq(api, "KVM_SET_NESTED_STATE") ||
      kvm_eq(api, "KVM_ARM_VCPU_INIT") || kvm_eq(api, "KVM_ARM_PREFERRED_TARGET") ||
      kvm_eq(api, "KVM_ARM_VCPU_FINALIZE")) {
    if (kvm_eq(api, "KVM_CREATE_GUEST_MEMFD") || kvm_eq(api, "KVM_GET_STATS_FD")) {
      int h = kvm_alloc(kKvmDev);
      if (h < 0) return fail(api);
      return ok(std::to_string(h));
    }
    return ok("ok");
  }

  if (kvm_pfx(api, "KVM_EXIT_")) {
    static const struct {
      const char* n;
      unsigned r;
    } kEx[] = {{"KVM_EXIT_UNKNOWN", 0},     {"KVM_EXIT_EXCEPTION", 1},
               {"KVM_EXIT_IO", 2},          {"KVM_EXIT_HYPERCALL", 3},
               {"KVM_EXIT_DEBUG", 4},       {"KVM_EXIT_HLT", 5},
               {"KVM_EXIT_MMIO", 6},        {"KVM_EXIT_IRQ_WINDOW_OPEN", 7},
               {"KVM_EXIT_SHUTDOWN", 8},    {"KVM_EXIT_FAIL_ENTRY", 9},
               {"KVM_EXIT_INTR", 10},       {"KVM_EXIT_NMI", 16},
               {"KVM_EXIT_INTERNAL_ERROR", 17}, {"KVM_EXIT_SYSTEM_EVENT", 24},
               {"KVM_EXIT_HYPERV", 27},     {"KVM_EXIT_X86_RDMSR", 29},
               {"KVM_EXIT_X86_WRMSR", 30},  {"KVM_EXIT_DIRTY_RING_FULL", 31},
               {"KVM_EXIT_X86_BUS_LOCK", 33}, {"KVM_EXIT_XEN", 34},
               {"KVM_EXIT_MEMORY_FAULT", 39}, {"KVM_EXIT_TDX", 40},
               {nullptr, 0}};
    for (int i = 0; kEx[i].n; ++i)
      if (kvm_eq(api, kEx[i].n)) return ok(std::to_string(kEx[i].r));
    return ok("0");
  }

  if (kvm_pfx(api, "VFIO_")) {
    if (kvm_eq(api, "VFIO_CHECK_EXTENSION") || kvm_eq(api, "VFIO_SET_IOMMU")) return ok("1");
    if (kvm_eq(api, "VFIO_GROUP_GET_DEVICE_FD") || kvm_eq(api, "VFIO_GROUP_SET_CONTAINER") ||
        kvm_eq(api, "VFIO_DEVICE_GET_INFO") || kvm_eq(api, "VFIO_DEVICE_RESET") ||
        kvm_eq(api, "VFIO_DEVICE_BIND_IOMMUFD")) {
      if (kvm_eq(api, "VFIO_GROUP_GET_DEVICE_FD") || kvm_eq(api, "VFIO_GROUP_SET_CONTAINER")) {
        int h = kvm_alloc(kKvmVfio);
        if (h < 0) return fail(api);
        return ok(std::to_string(h));
      }
      return ok("ok");
    }
    if (kvm_eq(api, "VFIO_DEVICE_GET_REGION_INFO") || kvm_eq(api, "VFIO_DEVICE_GET_IRQ_INFO") ||
        kvm_eq(api, "VFIO_IOMMU_GET_INFO"))
      return ok("0");
    if (kvm_eq(api, "VFIO_DEVICE_SET_IRQS") || kvm_eq(api, "VFIO_DEVICE_IOEVENTFD") ||
        kvm_eq(api, "VFIO_IOMMU_ENABLE") || kvm_eq(api, "VFIO_IOMMU_DISABLE") ||
        kvm_eq(api, "VFIO_GROUP_GET_STATUS") || kvm_eq(api, "VFIO_GROUP_UNSET_CONTAINER") ||
        kvm_eq(api, "VFIO_DEVICE_FEATURE") || kvm_eq(api, "VFIO_DEVICE_ATTACH_IOMMUFD_PT") ||
        kvm_eq(api, "VFIO_DEVICE_DETACH_IOMMUFD_PT") || kvm_eq(api, "VFIO_IOMMU_DIRTY_PAGES"))
      return ok("ok");
    return ok("ok");
  }
  if (kvm_pfx(api, "VHOST_")) {
    if (kvm_eq(api, "VHOST_GET_FEATURES") || kvm_eq(api, "VHOST_GET_BACKEND_FEATURES"))
      return ok("1");
    if (kvm_eq(api, "VHOST_SET_OWNER") || kvm_eq(api, "VHOST_RESET_OWNER") ||
        kvm_eq(api, "VHOST_SET_FEATURES") || kvm_eq(api, "VHOST_SET_MEM_TABLE") ||
        kvm_eq(api, "VHOST_SET_LOG_BASE") || kvm_eq(api, "VHOST_SET_LOG_FD") ||
        kvm_eq(api, "VHOST_SET_VRING_NUM") || kvm_eq(api, "VHOST_SET_VRING_ADDR") ||
        kvm_eq(api, "VHOST_SET_VRING_BASE") || kvm_eq(api, "VHOST_GET_VRING_BASE") ||
        kvm_eq(api, "VHOST_SET_VRING_KICK") || kvm_eq(api, "VHOST_SET_VRING_CALL") ||
        kvm_eq(api, "VHOST_SET_VRING_ERR") || kvm_eq(api, "VHOST_NET_SET_BACKEND") ||
        kvm_eq(api, "VHOST_VSOCK_SET_GUEST_CID") || kvm_eq(api, "VHOST_VSOCK_SET_RUNNING") ||
        kvm_eq(api, "VHOST_VDPA_GET_DEVICE_ID") || kvm_eq(api, "VHOST_VDPA_SET_STATUS") ||
        kvm_eq(api, "VHOST_VDPA_GET_STATUS") || kvm_eq(api, "VHOST_SET_BACKEND_FEATURES") ||
        kvm_eq(api, "VHOST_SCSI_SET_ENDPOINT") || kvm_eq(api, "VHOST_SCSI_CLEAR_ENDPOINT")) {
      if (kvm_eq(api, "VHOST_SET_OWNER")) {
        int h = kvm_alloc(kKvmVhost);
        if (h < 0) return fail(api);
        return ok(std::to_string(h));
      }
      if (kvm_eq(api, "VHOST_GET_VRING_BASE") || kvm_eq(api, "VHOST_VDPA_GET_DEVICE_ID") ||
          kvm_eq(api, "VHOST_VDPA_GET_STATUS"))
        return ok("0");
      return ok("ok");
    }
    return ok("ok");
  }
  if (kvm_pfx(api, "MSHV_")) return ok("ok");
  if (kvm_pfx(api, "KVM_")) return ok("ok");

  return false;
}

}  // namespace wasmdroid

#endif  // WASMDROID_INCLUDE_DROID_KVM_HOST_HPP_
