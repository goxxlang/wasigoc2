#ifndef WASMPE_INCLUDE_WASMPE_LOADER_HPP_
#define WASMPE_INCLUDE_WASMPE_LOADER_HPP_

// WASMPELoader: map MZ/PE32/PE32+ into linear memory for wasigocvm /
// WASMWin32 LoadLibrary. Headers, sections, relocs, exports (including
// forwards), import / delay-load IAT, bound imports, resources, TLS
// callback RVAs, exception / debug / load-config / security directories
// as data. AddressOfEntryPoint / TLS RVAs are for the WASMWin32 MainDLL
// hop (DllMain via WHvRunVirtualProcessor). This mapper does not JIT x86.
// WinVerifyTrust / Authenticode is not invented here.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace wasmpe {

enum Dir {
  kExport = 0,
  kImport = 1,
  kResource = 2,
  kException = 3,
  kSecurity = 4,
  kReloc = 5,
  kDebug = 6,
  kArchitecture = 7,
  kGlobalPtr = 8,
  kTls = 9,
  kLoadConfig = 10,
  kBoundImport = 11,
  kIat = 12,
  kDelay = 13,
  kCom = 14
};

struct Res {
  std::string type;
  std::string name;
  unsigned rva = 0;
  unsigned size = 0;
  unsigned lang = 0;
};

struct Section {
  char name[9]{};
  unsigned va = 0;
  unsigned vsz = 0;
  unsigned raw = 0;
  unsigned rawsz = 0;
  unsigned chars = 0;
};

struct Image {
  void* base = nullptr;
  size_t size = 0;
  int pe64 = 0;
  unsigned machine = 0;
  unsigned entry_rva = 0;
  unsigned checksum = 0;
  unsigned subsystem = 0;
  unsigned dll_chars = 0;
  unsigned nt_off = 0;
  int dll = 0;
  uint32_t dirs_rva[16]{};
  uint32_t dirs_sz[16]{};
  std::map<std::string, unsigned> exports;
  std::map<std::string, std::string> forwards;
  std::vector<Res> resources;
  std::vector<unsigned> tls_callbacks;
  std::vector<Section> sections;
  std::vector<std::string> imports;
  std::vector<std::string> delay_imports;
  std::vector<std::string> bound_imports;
  unsigned exception_count = 0;
  unsigned debug_type = 0;
  unsigned load_config_sz = 0;
  std::string err;
};

using Resolve = unsigned long long (*)(const char* dll, const char* name, void* ctx);

inline uint16_t u16(const unsigned char* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<unsigned>(p[1]) << 8));
}
inline uint32_t u32(const unsigned char* p) {
  return p[0] | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}
inline uint64_t u64(const unsigned char* p) {
  return static_cast<uint64_t>(u32(p)) | (static_cast<uint64_t>(u32(p + 4)) << 32);
}
inline void w32(unsigned char* p, uint32_t v) {
  p[0] = static_cast<unsigned char>(v);
  p[1] = static_cast<unsigned char>(v >> 8);
  p[2] = static_cast<unsigned char>(v >> 16);
  p[3] = static_cast<unsigned char>(v >> 24);
}
inline void w64(unsigned char* p, uint64_t v) {
  w32(p, static_cast<uint32_t>(v));
  w32(p + 4, static_cast<uint32_t>(v >> 32));
}

inline void unmap(Image* m) {
  if (!m) return;
  if (m->base) std::free(m->base);
  *m = Image{};
}

inline const unsigned char* rva_ptr(const Image& m, unsigned rva, unsigned need = 1) {
  if (!m.base || !rva || static_cast<size_t>(rva) + need > m.size) return nullptr;
  return static_cast<const unsigned char*>(m.base) + rva;
}

inline unsigned export_rva(const Image& m, const char* name) {
  if (!name || !name[0]) return 0;
  auto it = m.exports.find(name);
  return it == m.exports.end() ? 0 : it->second;
}

inline std::string cstr(const unsigned char* img, uint32_t rva, uint32_t cap, uint32_t maxn = 127) {
  if (!img || !rva || rva >= cap) return {};
  char b[128]{};
  size_t k = 0;
  while (k + 1 < sizeof(b) && k < maxn && rva + k < cap && img[rva + k]) {
    b[k] = static_cast<char>(img[rva + k]);
    ++k;
  }
  return b;
}

inline bool map(const void* file, size_t n, Image* out, Resolve resolve, void* ctx) {
  if (out) *out = Image{};
  if (!out) return false;
  if (!file || n < 64) {
    out->err = "short";
    return false;
  }
  const unsigned char* f = static_cast<const unsigned char*>(file);
  if (f[0] != 'M' || f[1] != 'Z') {
    out->err = "not MZ";
    return false;
  }
  uint32_t lfanew = u32(f + 0x3c);
  if (lfanew + 24 > n) {
    out->err = "bad e_lfanew";
    return false;
  }
  if (std::memcmp(f + lfanew, "PE\0\0", 4) != 0) {
    out->err = "not PE";
    return false;
  }
  const unsigned char* fh = f + lfanew + 4;
  uint16_t machine = u16(fh);
  uint16_t nsec = u16(fh + 2);
  uint16_t optsz = u16(fh + 16);
  uint16_t chars = u16(fh + 18);
  const unsigned char* oh = fh + 20;
  if (oh + optsz > f + n || optsz < 2) {
    out->err = "bad optional";
    return false;
  }
  uint16_t magic = u16(oh);
  int pe64 = magic == 0x20b;
  if (magic != 0x10b && magic != 0x20b) {
    out->err = "bad magic";
    return false;
  }
  uint32_t entry = u32(oh + 16);
  uint32_t size_image = u32(oh + 56);
  uint32_t size_headers = u32(oh + 60);
  uint32_t checksum = u32(oh + 64);
  uint16_t subsystem = u16(oh + 68);
  uint16_t dll_chars = u16(oh + 70);
  uint64_t image_base = pe64 ? u64(oh + 24) : u32(oh + 28);
  uint32_t dd_off = pe64 ? 112 : 96;
  uint32_t ndd = (optsz > dd_off) ? u32(oh + (pe64 ? 108 : 92)) : 0;
  if (ndd > 16) ndd = 16;
  if (!size_image || size_image > (64u << 20)) {
    out->err = "SizeOfImage";
    return false;
  }
  unsigned char* img = static_cast<unsigned char*>(std::calloc(1, size_image));
  if (!img) {
    out->err = "oom";
    return false;
  }
  uint32_t hdrn = size_headers && size_headers <= n ? size_headers : (uint32_t)(lfanew + 24 + optsz);
  if (hdrn > size_image) hdrn = size_image;
  if (hdrn > n) hdrn = (uint32_t)n;
  std::memcpy(img, f, hdrn);
  const unsigned char* sec = oh + optsz;
  for (uint16_t i = 0; i < nsec; ++i) {
    if (sec + 40 > f + n) break;
    Section s{};
    std::memcpy(s.name, sec, 8);
    s.vsz = u32(sec + 8);
    s.va = u32(sec + 12);
    s.rawsz = u32(sec + 16);
    s.raw = u32(sec + 20);
    s.chars = u32(sec + 36);
    out->sections.push_back(s);
    uint32_t copy = s.rawsz < s.vsz ? s.rawsz : s.vsz;
    if (!s.vsz) copy = s.rawsz;
    if (s.va < size_image && s.raw < n && copy) {
      uint32_t room = size_image - s.va;
      if (copy > room) copy = room;
      if (s.raw + copy > n) copy = (uint32_t)(n - s.raw);
      std::memcpy(img + s.va, f + s.raw, copy);
    }
    sec += 40;
  }
  auto dir = [&](unsigned idx, uint32_t* rva, uint32_t* sz) {
    *rva = 0;
    *sz = 0;
    if (idx >= ndd) return;
    const unsigned char* d = oh + dd_off + idx * 8;
    if (d + 8 > oh + optsz) return;
    *rva = u32(d);
    *sz = u32(d + 4);
    if (idx < 16) {
      out->dirs_rva[idx] = *rva;
      out->dirs_sz[idx] = *sz;
    }
  };
  auto inimg = [&](uint32_t rva, uint32_t need) -> unsigned char* {
    if (!rva || rva + need > size_image) return nullptr;
    return img + rva;
  };

  for (unsigned i = 0; i < ndd && i < 16; ++i) {
    uint32_t r = 0, z = 0;
    dir(i, &r, &z);
  }

  uint32_t rel_rva = out->dirs_rva[kReloc], rel_sz = out->dirs_sz[kReloc];
  uintptr_t mapped = reinterpret_cast<uintptr_t>(img);
  int64_t delta = static_cast<int64_t>(mapped) - static_cast<int64_t>(image_base);
  if (delta && rel_rva && rel_sz) {
    unsigned char* rel = inimg(rel_rva, rel_sz);
    if (rel) {
      size_t off = 0;
      while (off + 8 <= rel_sz) {
        uint32_t page = u32(rel + off);
        uint32_t block = u32(rel + off + 4);
        if (block < 8) break;
        unsigned count = (block - 8) / 2;
        for (unsigned i = 0; i < count; ++i) {
          uint16_t e = u16(rel + off + 8 + i * 2);
          unsigned type = e >> 12;
          unsigned ro = e & 0xfff;
          uint32_t at = page + ro;
          if (at + 8 > size_image) continue;
          if (type == 0) continue;
          if (type == 1) {
            uint16_t v = u16(img + at);
            w32(img + at, (u32(img + at) & 0xffff0000u) |
                              static_cast<uint16_t>(static_cast<int32_t>(v) + (delta >> 16)));
          } else if (type == 2) {
            uint16_t v = u16(img + at);
            w32(img + at, (u32(img + at) & 0xffff0000u) |
                              static_cast<uint16_t>(static_cast<int32_t>(v) + (delta & 0xffff)));
          } else if (type == 3) {
            uint32_t v = u32(img + at);
            w32(img + at, static_cast<uint32_t>(static_cast<int64_t>(v) + delta));
          } else if (type == 4) {
            ++i;
          } else if (type == 10) {
            uint64_t v = u64(img + at);
            w64(img + at, static_cast<uint64_t>(static_cast<int64_t>(v) + delta));
          }
        }
        off += block;
      }
    }
  }

  uint32_t exp_rva = out->dirs_rva[kExport], exp_sz = out->dirs_sz[kExport];
  if (exp_rva && exp_sz >= 40) {
    unsigned char* exp = inimg(exp_rva, 40);
    if (exp) {
      uint32_t nn = u32(exp + 24);
      uint32_t names = u32(exp + 32);
      uint32_t ords = u32(exp + 36);
      uint32_t fns = u32(exp + 28);
      for (uint32_t i = 0; i < nn && i < 4096; ++i) {
        unsigned char* np = inimg(names + i * 4, 4);
        unsigned char* op = inimg(ords + i * 2, 2);
        if (!np || !op) break;
        uint32_t nrva = u32(np);
        uint16_t ord = u16(op);
        uint32_t frva = 0;
        unsigned char* fp = inimg(fns + static_cast<uint32_t>(ord) * 4, 4);
        if (fp) frva = u32(fp);
        std::string name = cstr(img, nrva, size_image);
        if (name.empty()) continue;
        if (frva >= exp_rva && frva < exp_rva + exp_sz)
          out->forwards[name] = cstr(img, frva, size_image);
        else
          out->exports[name] = frva;
      }
    }
  }

  auto read_imports = [&](uint32_t desc_rva, uint32_t name_off, uint32_t iat_off, uint32_t oft_off,
                          unsigned desc_sz, std::vector<std::string>* dlls, int delay) {
    if (!desc_rva) return;
    for (uint32_t i = 0; i < 256; ++i) {
      unsigned char* desc = inimg(desc_rva + i * desc_sz, desc_sz);
      if (!desc) break;
      uint32_t name_rva = u32(desc + name_off);
      uint32_t iat = u32(desc + iat_off);
      uint32_t oft = oft_off != 0xffffffffu ? u32(desc + oft_off) : 0;
      if (!name_rva && !iat) break;
      std::string dll = cstr(img, name_rva, size_image);
      if (!dll.empty()) dlls->push_back(dll);
      if (!resolve) continue;
      uint32_t thunk = oft ? oft : iat;
      unsigned step = pe64 ? 8u : 4u;
      for (uint32_t t = 0; t < 1024; ++t) {
        unsigned char* th = inimg(thunk + t * step, step);
        unsigned char* ia = inimg(iat + t * step, step);
        if (!th || !ia) break;
        uint64_t tv = pe64 ? u64(th) : u32(th);
        if (!tv) break;
        char nbuf[128]{};
        const char* iname = nbuf;
        uint64_t ordinal_flag = pe64 ? (1ull << 63) : (1ull << 31);
        if (tv & ordinal_flag) {
          std::snprintf(nbuf, sizeof(nbuf), "#%u", static_cast<unsigned>(tv & 0xffff));
        } else {
          uint32_t hint_rva = delay ? static_cast<uint32_t>(tv) : static_cast<uint32_t>(tv & 0x7fffffff);
          unsigned char* hint = inimg(hint_rva + 2, 1);
          if (hint) {
            size_t k = 0;
            while (k + 1 < sizeof(nbuf) && hint_rva + 2 + k < size_image && hint[k]) {
              nbuf[k] = static_cast<char>(hint[k]);
              ++k;
            }
          }
        }
        unsigned long long addr = resolve(dll.c_str(), iname, ctx);
        if (pe64) w64(ia, addr);
        else w32(ia, static_cast<uint32_t>(addr));
      }
    }
  };
  read_imports(out->dirs_rva[kImport], 12, 16, 0, 20, &out->imports, 0);
  read_imports(out->dirs_rva[kDelay], 4, 12, 16, 32, &out->delay_imports, 1);

  uint32_t bound_rva = out->dirs_rva[kBoundImport], bound_sz = out->dirs_sz[kBoundImport];
  if (bound_rva && bound_sz >= 8) {
    unsigned char* b = inimg(bound_rva, bound_sz);
    if (b) {
      size_t off = 0;
      while (off + 8 <= bound_sz) {
        uint32_t ts = u32(b + off);
        uint16_t name_off = u16(b + off + 4);
        uint16_t nfwd = u16(b + off + 6);
        if (!ts && !name_off) break;
        if (name_off && name_off < bound_sz)
          out->bound_imports.push_back(cstr(b, name_off, bound_sz));
        off += 8u + static_cast<size_t>(nfwd) * 8u;
      }
    }
  }

  uint32_t rsrc_rva = out->dirs_rva[kResource], rsrc_sz = out->dirs_sz[kResource];
  if (rsrc_rva && rsrc_sz >= 16) {
    auto res_at = [&](uint32_t off, uint32_t need) -> unsigned char* {
      if (off + need > rsrc_sz) return nullptr;
      return inimg(rsrc_rva + off, need);
    };
    auto res_label = [&](uint32_t id) -> std::string {
      if (id & 0x80000000u) {
        unsigned char* s = res_at(id & 0x7fffffffu, 2);
        if (!s) return {};
        uint16_t ln = u16(s);
        std::string o;
        for (uint16_t i = 0; i < ln && i < 64; ++i) {
          unsigned char* wp = res_at((id & 0x7fffffffu) + 2 + i * 2, 2);
          if (!wp) break;
          unsigned char c = wp[0];
          if (c) o.push_back(static_cast<char>(c));
        }
        return o;
      }
      return std::to_string(id);
    };
    std::function<void(uint32_t, int, std::string, std::string)> walk;
    walk = [&](uint32_t off, int depth, std::string type, std::string name) {
      if (depth > 4) return;
      unsigned char* d = res_at(off, 16);
      if (!d) return;
      uint32_t nent = static_cast<uint32_t>(u16(d + 12)) + u16(d + 14);
      if (nent > 512) nent = 512;
      for (uint32_t i = 0; i < nent; ++i) {
        unsigned char* e = res_at(off + 16 + i * 8, 8);
        if (!e) break;
        uint32_t id = u32(e);
        uint32_t nxt = u32(e + 4);
        std::string label = res_label(id);
        std::string t = type, nm = name;
        if (depth == 0) t = label;
        else if (depth == 1) nm = label;
        if (nxt & 0x80000000u)
          walk(nxt & 0x7fffffffu, depth + 1, t, nm);
        else {
          unsigned char* de = res_at(nxt, 16);
          if (!de) continue;
          Res r;
          r.type = t;
          r.name = nm;
          r.rva = u32(de);
          r.size = u32(de + 4);
          r.lang = static_cast<unsigned>(std::strtoul(label.c_str(), nullptr, 10));
          if (r.rva && r.rva < size_image) out->resources.push_back(r);
        }
      }
    };
    walk(0, 0, {}, {});
  }

  uint32_t tls_rva = out->dirs_rva[kTls];
  if (tls_rva) {
    unsigned char* td = inimg(tls_rva, pe64 ? 40u : 24u);
    if (td) {
      uint64_t cb = pe64 ? u64(td + 24) : u32(td + 12);
      uint32_t off = 0;
      if (cb >= mapped && cb < mapped + size_image)
        off = static_cast<uint32_t>(cb - mapped);
      else if (cb >= image_base && cb < image_base + size_image)
        off = static_cast<uint32_t>(cb - image_base);
      else if (cb && cb < size_image)
        off = static_cast<uint32_t>(cb);
      unsigned step = pe64 ? 8u : 4u;
      for (unsigned i = 0; off && i < 64; ++i) {
        unsigned char* p = inimg(off + i * step, step);
        if (!p) break;
        uint64_t fn = pe64 ? u64(p) : u32(p);
        if (!fn) break;
        unsigned rva = (fn >= mapped && fn < mapped + size_image)
                           ? static_cast<unsigned>(fn - mapped)
                           : static_cast<unsigned>(fn);
        out->tls_callbacks.push_back(rva);
      }
    }
  }

  if (out->dirs_sz[kException])
    out->exception_count = out->dirs_sz[kException] / (pe64 ? 12u : 20u);
  if (out->dirs_rva[kDebug] && out->dirs_sz[kDebug] >= 28) {
    unsigned char* d = inimg(out->dirs_rva[kDebug], 28);
    if (d) out->debug_type = u32(d + 12);
  }
  if (out->dirs_rva[kLoadConfig] && out->dirs_sz[kLoadConfig] >= 4) {
    unsigned char* lc = inimg(out->dirs_rva[kLoadConfig], 4);
    if (lc) out->load_config_sz = u32(lc);
  }

  out->base = img;
  out->size = size_image;
  out->pe64 = pe64;
  out->machine = machine;
  out->entry_rva = entry;
  out->checksum = checksum;
  out->subsystem = subsystem;
  out->dll_chars = dll_chars;
  out->nt_off = lfanew;
  out->dll = (chars & 0x2000) ? 1 : 0;
  return true;
}

inline bool map_file(const char* path, Image* out, Resolve resolve, void* ctx) {
  if (out) *out = Image{};
  if (!path || !path[0] || !out) return false;
  FILE* f = std::fopen(path, "rb");
  if (!f) {
    out->err = "open";
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz < 64) {
    std::fclose(f);
    out->err = "short";
    return false;
  }
  std::vector<unsigned char> buf(static_cast<size_t>(sz));
  size_t got = std::fread(buf.data(), 1, static_cast<size_t>(sz), f);
  std::fclose(f);
  buf.resize(got);
  return map(buf.data(), buf.size(), out, resolve, ctx);
}

inline std::vector<unsigned char> minimal_dll() {
  std::vector<unsigned char> b(0x400, 0);
  b[0] = 'M';
  b[1] = 'Z';
  w32(&b[0x3c], 0x40);
  b[0x40] = 'P';
  b[0x41] = 'E';
  b[0x44] = 0x4c;
  b[0x45] = 0x01;
  b[0x46] = 0x01;
  b[0x47] = 0x00;
  b[0x54] = 0xe0;
  b[0x55] = 0x00;
  b[0x56] = 0x02;
  b[0x57] = 0x21;
  unsigned oh = 0x58;
  b[oh] = 0x0b;
  b[oh + 1] = 0x01;
  w32(&b[oh + 16], 0x1000);
  w32(&b[oh + 20], 0x1000);
  w32(&b[oh + 28], 0x10000000);
  w32(&b[oh + 32], 0x1000);
  w32(&b[oh + 36], 0x200);
  b[oh + 40] = 4;
  b[oh + 48] = 4;
  w32(&b[oh + 56], 0x2000);
  w32(&b[oh + 60], 0x200);
  b[oh + 68] = 2;
  w32(&b[oh + 92], 16);
  w32(&b[oh + 96], 0x1080);
  w32(&b[oh + 100], 0x50);
  unsigned sec = 0x58 + 0xe0;
  std::memcpy(&b[sec], ".rdata\0", 7);
  w32(&b[sec + 8], 0x200);
  w32(&b[sec + 12], 0x1000);
  w32(&b[sec + 16], 0x200);
  w32(&b[sec + 20], 0x200);
  w32(&b[sec + 36], 0x40000040);
  const char mark[] = "PEMAP";
  std::memcpy(&b[0x200], mark, 6);
  unsigned exp = 0x280;
  w32(&b[exp + 12], 0x10bc);
  w32(&b[exp + 16], 1);
  w32(&b[exp + 20], 1);
  w32(&b[exp + 24], 1);
  w32(&b[exp + 28], 0x10a8);
  w32(&b[exp + 32], 0x10ac);
  w32(&b[exp + 36], 0x10b0);
  w32(&b[0x2a8], 0x1000);
  w32(&b[0x2ac], 0x10b4);
  b[0x2b0] = 0;
  b[0x2b1] = 0;
  std::memcpy(&b[0x2b4], "PeMark", 7);
  std::memcpy(&b[0x2bc], "pemap.dll", 10);
  return b;
}

inline std::string file_version(const Image& m) {
  if (!m.base) return {};
  const unsigned char* img = static_cast<const unsigned char*>(m.base);
  for (const auto& r : m.resources) {
    if (r.type != "16") continue;
    if (!r.size || r.rva + r.size > m.size) continue;
    const unsigned char* p = img + r.rva;
    for (unsigned i = 0; i + 52 <= r.size; ++i) {
      if (u32(p + i) != 0xFEEF04BDu) continue;
      uint32_t ms = u32(p + i + 8);
      uint32_t ls = u32(p + i + 12);
      char b[64];
      std::snprintf(b, sizeof(b), "%u.%u.%u.%u", ms >> 16, ms & 0xffffu, ls >> 16, ls & 0xffffu);
      return b;
    }
  }
  return {};
}

}  // namespace wasmpe

#endif  // WASMPE_INCLUDE_WASMPE_LOADER_HPP_
