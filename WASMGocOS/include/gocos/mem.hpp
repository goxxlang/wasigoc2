#ifndef WASMGOCOS_INCLUDE_GOCOS_MEM_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_MEM_HPP_

// gocvm memory model. GocKrnl never mallocs. Guest pages are named
// through hypervision:
//   k32  VirtualAlloc / VirtualFree / NtAllocateVirtualMemory  (EPT vmem)
//   nix  mmap / munmap
// The working-set table below is kernel BSS (PFN analog), not a heap.

enum { kGocPages = 128 };

struct GocPage {
  char addr[64];
  unsigned n;
  int used;
};

inline GocPage* goc_pages() {
  static GocPage t[kGocPages]{};
  return t;
}

inline int goc_page_slot(const char* addr) {
  if (!addr || !addr[0]) return -1;
  GocPage* t = goc_pages();
  for (int i = 0; i < kGocPages; ++i) {
    if (t[i].used && eq(t[i].addr, addr)) return i;
  }
  return -1;
}

inline void goc_page_add(const std::string& addr, unsigned n) {
  if (addr.empty() || addr.rfind("error:", 0) == 0) return;
  GocPage* t = goc_pages();
  for (int i = 0; i < kGocPages; ++i) {
    if (!t[i].used) {
      t[i].used = 1;
      t[i].n = n ? n : 4096;
      copy_field(t[i].addr, sizeof(t[i].addr), addr);
      return;
    }
  }
}

inline void goc_page_drop(const char* addr) {
  int i = goc_page_slot(addr);
  if (i < 0) return;
  goc_pages()[i] = GocPage{};
}

inline std::string goc_alloc(const char* a) {
  unsigned n = a && a[0] ? static_cast<unsigned>(std::strtoul(a, nullptr, 10)) : 0;
  if (!n) n = 4096;
  std::string p = hv::k32("VirtualAlloc", std::to_string(n).c_str());
  if (p.rfind("error:", 0) == 0) {
    std::string nt = join1f("-1", std::to_string(n).c_str());
    p = hv::k32("NtAllocateVirtualMemory", nt.c_str());
  }
  goc_page_add(p, n);
  return p;
}

inline std::string goc_free(const char* a) {
  if (!a || !a[0]) return err_msg("GocKrnl.Free: empty address");
  std::string r = hv::k32("VirtualFree", a);
  if (r.rfind("error:", 0) == 0) r = hv::k32("NtFreeVirtualMemory", a);
  goc_page_drop(a);
  return r;
}

inline std::string goc_protect(const char* a) {
  return hv::k32("VirtualProtect", a ? a : "");
}

inline std::string goc_query(const char* a) {
  int i = goc_page_slot(a);
  if (i >= 0) return std::to_string(goc_pages()[i].n);
  return hv::k32("VirtualQuery", a ? a : "");
}

inline std::string goc_map(const char* a) {
  return hv::k32("NtMapViewOfSection", a ? a : "");
}

inline std::string goc_unmap(const char* a) {
  return hv::k32("NtUnmapViewOfSection", a ? a : "");
}

inline std::string goc_alloc_nix(const char* a) {
  unsigned n = a && a[0] ? static_cast<unsigned>(std::strtoul(a, nullptr, 10)) : 0;
  if (!n) n = 4096;
  std::string p = hv::nix("mmap", std::to_string(n).c_str());
  goc_page_add(p, n);
  return p;
}

inline std::string goc_free_nix(const char* a) {
  if (!a || !a[0]) return err_msg("GocNix.Free: empty address");
  std::string r = hv::nix("munmap", a);
  goc_page_drop(a);
  return r;
}

#endif  // WASMGOCOS_INCLUDE_GOCOS_MEM_HPP_
