#include "gocos/peinterp.hpp"

#include "reactos/catalog.h"
#include "reactos/dispatch.h"
#include "win32/catalog.h"
#include "win32/dispatch.h"
#include "win32/metadata.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace wasmgocos {
namespace {

constexpr uint32_t kStackBase = 0x02000000u;
constexpr uint32_t kStackSize = 0x10000u;  // 64 KiB
constexpr uint32_t kSentinel = 0xDEAD0000u;

// EFLAGS bits.
constexpr uint32_t CF = 0x1, PF = 0x4, ZF = 0x40, SF = 0x80, OF = 0x800;

struct Cpu {
  uint32_t r[8] = {0};  // eax ecx edx ebx esp ebp esi edi
  uint32_t eip = 0;
  uint32_t flags = 0;
  std::vector<uint8_t> image;  // at kPeImageBase
  std::vector<uint8_t> stack;  // at kStackBase
  bool fault = false;
  std::string last_reply;

  uint8_t* at(uint32_t va, uint32_t n) {
    if (va >= kPeImageBase && static_cast<uint64_t>(va) + n <= kPeImageBase + image.size())
      return image.data() + (va - kPeImageBase);
    if (va >= kStackBase && static_cast<uint64_t>(va) + n <= kStackBase + stack.size())
      return stack.data() + (va - kStackBase);
    return nullptr;
  }
  uint32_t rd(uint32_t va, int n) {
    uint8_t* p = at(va, n);
    if (!p) {
      fault = true;
      return 0;
    }
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i);
    return v;
  }
  void wr(uint32_t va, uint32_t v, int n) {
    uint8_t* p = at(va, n);
    if (!p) {
      fault = true;
      return;
    }
    for (int i = 0; i < n; ++i) p[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xff);
  }
  uint8_t code(uint32_t va) { return static_cast<uint8_t>(rd(va, 1)); }
  void push(uint32_t v) {
    r[4] -= 4;
    wr(r[4], v, 4);
  }
  uint32_t pop() {
    uint32_t v = rd(r[4], 4);
    r[4] += 4;
    return v;
  }
};

void set_reg(Cpu& c, int i, uint32_t v, int w) {
  if (w == 4) {
    c.r[i] = v;
  } else if (w == 1) {
    // 8-bit: al..bl are r[0..3] low byte; ah..bh (i>=4) are high byte of r[i-4].
    if (i < 4)
      c.r[i] = (c.r[i] & 0xffffff00u) | (v & 0xff);
    else
      c.r[i - 4] = (c.r[i - 4] & 0xffff00ffu) | ((v & 0xff) << 8);
  } else {  // 16-bit
    c.r[i] = (c.r[i] & 0xffff0000u) | (v & 0xffff);
  }
}

uint32_t get_reg(Cpu& c, int i, int w) {
  if (w == 4) return c.r[i];
  if (w == 1) return i < 4 ? (c.r[i] & 0xff) : ((c.r[i - 4] >> 8) & 0xff);
  return c.r[i] & 0xffff;
}

uint32_t msb(int w) { return w == 1 ? 0x80u : w == 2 ? 0x8000u : 0x80000000u; }
uint32_t mask(int w) { return w == 1 ? 0xffu : w == 2 ? 0xffffu : 0xffffffffu; }

void set_szp(Cpu& c, uint32_t res, int w) {
  c.flags &= ~(ZF | SF | PF);
  if ((res & mask(w)) == 0) c.flags |= ZF;
  if (res & msb(w)) c.flags |= SF;
  uint8_t lb = res & 0xff;
  int bits = 0;
  for (int i = 0; i < 8; ++i) bits += (lb >> i) & 1;
  if (!(bits & 1)) c.flags |= PF;
}

// Arithmetic flags. kind: 0=add, 1=sub/cmp, 2=logic.
uint32_t alu(Cpu& c, uint32_t a, uint32_t b, int kind, int w) {
  uint32_t res;
  c.flags &= ~(CF | OF);
  if (kind == 0) {
    res = (a + b) & mask(w);
    if ((a & mask(w)) + (b & mask(w)) > mask(w)) c.flags |= CF;
    if (((a ^ res) & (b ^ res)) & msb(w)) c.flags |= OF;
  } else if (kind == 1) {
    res = (a - b) & mask(w);
    if ((a & mask(w)) < (b & mask(w))) c.flags |= CF;
    if (((a ^ b) & (a ^ res)) & msb(w)) c.flags |= OF;
  } else {
    res = a & b;  // caller supplies the logic result via b when needed
  }
  set_szp(c, res, w);
  return res;
}

bool cond(Cpu& c, unsigned cc) {
  bool zf = c.flags & ZF, cf = c.flags & CF, sf = c.flags & SF, of = c.flags & OF, pf = c.flags & PF;
  switch (cc & 0xf) {
    case 0x0: return of;
    case 0x1: return !of;
    case 0x2: return cf;
    case 0x3: return !cf;
    case 0x4: return zf;
    case 0x5: return !zf;
    case 0x6: return cf || zf;
    case 0x7: return !(cf || zf);
    case 0x8: return sf;
    case 0x9: return !sf;
    case 0xa: return pf;
    case 0xb: return !pf;
    case 0xc: return sf != of;
    case 0xd: return sf == of;
    case 0xe: return zf || (sf != of);
    case 0xf: return !zf && (sf == of);
  }
  return false;
}

struct Ea {
  bool mem;
  uint32_t addr;  // when mem
  int reg;        // rm register when !mem
  int rfield;     // ModRM reg field
  int len;        // bytes consumed from the ModRM byte onward
};

Ea modrm(Cpu& c, uint32_t p) {
  Ea e{};
  uint8_t m = c.code(p);
  unsigned mod = m >> 6, reg = (m >> 3) & 7, rm = m & 7;
  e.rfield = static_cast<int>(reg);
  e.len = 1;
  if (mod == 3) {
    e.mem = false;
    e.reg = static_cast<int>(rm);
    return e;
  }
  e.mem = true;
  uint32_t base = 0;
  if (rm == 4) {  // SIB
    uint8_t sib = c.code(p + e.len);
    e.len++;
    unsigned scale = 1u << (sib >> 6), index = (sib >> 3) & 7, sbase = sib & 7;
    if (index != 4) base += c.r[index] * scale;
    if (sbase == 5 && mod == 0) {
      base += c.rd(p + e.len, 4);
      e.len += 4;
    } else {
      base += c.r[sbase];
    }
  } else if (rm == 5 && mod == 0) {
    e.addr = c.rd(p + e.len, 4);
    e.len += 4;
    return e;
  } else {
    base = c.r[rm];
  }
  if (mod == 1) {
    base += static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(c.code(p + e.len))));
    e.len++;
  } else if (mod == 2) {
    base += c.rd(p + e.len, 4);
    e.len += 4;
  }
  e.addr = base;
  return e;
}

uint32_t load_ea(Cpu& c, const Ea& e, int w) {
  return e.mem ? c.rd(e.addr, w) : get_reg(c, e.reg, w);
}
void store_ea(Cpu& c, const Ea& e, uint32_t v, int w) {
  if (e.mem)
    c.wr(e.addr, v, w);
  else
    set_reg(c, e.reg, v, w);
}

// A catalog thunk (not real guest memory)?
bool is_thunk(Cpu& c, uint32_t t) {
  if (t < kPeThunkHostBase) return false;
  if (c.at(t, 1)) return false;  // real mapped memory
  if (t >= kPeThunkReactBase) return true;
  int n = 0;
  wasmwin32_catalog(&n);
  return t < kPeThunkHostBase + static_cast<uint32_t>(n);
}

// The catalog name a band resolves to (React vs host).
const char* band_name(uint32_t thunk) {
  if (thunk >= kPeThunkReactBase) {
    int n = 0;
    const WasmReactApi* r = wasmreact_catalog(&n);
    unsigned i = thunk - kPeThunkReactBase;
    return static_cast<int>(i) < n ? r[i].name : nullptr;
  }
  int n = 0;
  const WasmWin32Api* w = wasmwin32_catalog(&n);
  unsigned i = thunk - kPeThunkHostBase;
  return static_cast<int>(i) < n ? w[i].name : nullptr;
}

// Read a NUL-terminated ANSI string from guest memory at `va`.
std::string read_cstr(Cpu& c, uint32_t va) {
  std::string s;
  if (!va) return s;
  for (int i = 0; i < 65536; ++i) {
    uint8_t* p = c.at(va + static_cast<uint32_t>(i), 1);
    if (!p || *p == 0) break;
    s.push_back(static_cast<char>(*p));
  }
  return s;
}

// Read a NUL-terminated UTF-16LE string from guest memory at `va` as UTF-8
// (BMP). This is how an LPCWSTR arg reaches the catalog's UTF-8 protocol.
std::string read_wstr(Cpu& c, uint32_t va) {
  std::string s;
  if (!va) return s;
  for (int i = 0; i < 32768; ++i) {
    uint8_t* p = c.at(va + static_cast<uint32_t>(i) * 2, 2);
    if (!p) break;
    uint16_t w = static_cast<uint16_t>(p[0] | (p[1] << 8));
    if (!w) break;
    if (w < 0x80) {
      s.push_back(static_cast<char>(w));
    } else if (w < 0x800) {
      s.push_back(static_cast<char>(0xC0 | (w >> 6)));
      s.push_back(static_cast<char>(0x80 | (w & 0x3f)));
    } else {
      s.push_back(static_cast<char>(0xE0 | (w >> 12)));
      s.push_back(static_cast<char>(0x80 | ((w >> 6) & 0x3f)));
      s.push_back(static_cast<char>(0x80 | (w & 0x3f)));
    }
  }
  return s;
}

// Dispatch a __stdcall band call to its catalog, marshalling GENERICALLY
// from the API's win32metadata signature (win32/metadata.h) — no per-API
// code. Stock code pushed args right-to-left, so arg i is at [esp + i*4]
// (we intercept before the call pushes a return address). Each parameter is
// turned into a catalog field by its metadata TYPE; the integer reply lands
// in eax (edx:eax for a 64-bit return); [out] pointers get the reply written
// back; and the callee pops the args (stdcall) by their metadata dword size.
void dispatch_thunk(Cpu& c, uint32_t thunk) {
  namespace md = wasmwin32meta;
  bool react = thunk >= kPeThunkReactBase;
  const char* name = band_name(thunk);
  md::Sig sig;
  bool known = name && md::lookup(name, &sig);

  uint32_t sp = c.r[4];

  // Pass 1: the raw dword(s) of each parameter, by metadata slot size. Done
  // first so a buffer arg can read the length from its `count` sibling.
  std::vector<uint32_t> raw(known ? sig.n : 0);
  unsigned dw = 0;
  for (unsigned i = 0; known && i < sig.n; ++i) {
    raw[i] = c.rd(sp + dw * 4, 4);
    dw += md::dwords(sig.p[i].type);
  }
  auto arg_len = [&](unsigned idx) -> uint32_t { return idx < raw.size() ? raw[idx] : 0; };

  // Pass 2: one catalog field per parameter, by its metadata type.
  std::vector<std::string> fields;
  std::vector<uint32_t> out_ptrs;
  struct OutBuf {
    uint32_t va;
    uint32_t len;
  };
  std::vector<OutBuf> out_bufs;
  unsigned d2 = 0;
  for (unsigned i = 0; known && i < sig.n; ++i) {
    md::Type t = sig.p[i].type;
    uint32_t a0 = raw[i];
    if (t == md::OutPtr) {
      out_ptrs.push_back(a0);
      fields.push_back("");
    } else if (t == md::OutBuffer) {
      out_bufs.push_back({a0, arg_len(sig.p[i].count)});
      fields.push_back("");
    } else if (t == md::Buffer) {
      uint32_t len = arg_len(sig.p[i].count);
      std::string bytes;
      for (uint32_t k = 0; k < len; ++k) {
        uint8_t* p = c.at(a0 + k, 1);
        bytes.push_back(p ? static_cast<char>(*p) : '\0');
      }
      fields.push_back(bytes);
    } else if (t == md::LpcWStr) {
      fields.push_back(read_wstr(c, a0));
    } else if (t == md::LpcStr) {
      fields.push_back(read_cstr(c, a0));
    } else if (t == md::I64 || t == md::U64) {
      uint32_t a1 = c.rd(sp + (d2 + 1) * 4, 4);
      unsigned long long v = (static_cast<unsigned long long>(a1) << 32) | a0;
      fields.push_back(std::to_string(v));
    } else {
      fields.push_back(std::to_string(a0));
    }
    d2 += md::dwords(t);
  }

  std::string joined;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i) joined.push_back('\x1f');
    joined += fields[i];
  }

  std::string reply(65536, '\0');
  int rc = -1;
  if (name) {
    if (react)
      rc = wasmreact_call(name, joined.c_str(), reply.data(), static_cast<unsigned>(reply.size()));
    else
      rc = wasmwin32_call(name, joined.c_str(), reply.data(), static_cast<unsigned>(reply.size()));
  }
  reply.resize(std::strlen(reply.c_str()));
  c.last_reply = reply;
  (void)rc;

  unsigned long long v = std::strtoull(reply.c_str(), nullptr, 0);
  c.r[0] = static_cast<uint32_t>(v);                                         // eax
  if (known && md::ret64(sig.ret)) c.r[2] = static_cast<uint32_t>(v >> 32);  // edx:eax

  for (uint32_t p : out_ptrs)
    if (p) {
      c.wr(p, static_cast<uint32_t>(v), 4);
      c.wr(p + 4, static_cast<uint32_t>(v >> 32), 4);
    }

  // An out byte buffer receives the reply's `data=<hex>` (the catalog's
  // convention for returned bytes), up to the caller's length.
  if (!out_bufs.empty()) {
    size_t at = reply.find("data=");
    if (at != std::string::npos) {
      const char* h = reply.c_str() + at + 5;
      for (const OutBuf& ob : out_bufs) {
        uint32_t k = 0;
        for (; k < ob.len && h[k * 2] && h[k * 2 + 1]; ++k) {
          auto nyb = [](char x) -> int {
            return x <= '9' ? x - '0' : (x | 0x20) - 'a' + 10;
          };
          c.wr(ob.va + k, static_cast<uint32_t>((nyb(h[k * 2]) << 4) | nyb(h[k * 2 + 1])), 1);
        }
      }
    }
  }

  c.r[4] += dw * 4;  // stdcall: callee cleans its arguments
}

// A call/jmp target: a thunk dispatches (and, for call, returns); a real
// address transfers control. Returns false to stop (fault).
bool transfer(Cpu& c, uint32_t target, bool is_call, uint32_t ret_addr) {
  if (is_thunk(c, target)) {
    dispatch_thunk(c, target);
    if (is_call) c.eip = ret_addr;  // a thunk returns to just after the call
    return !c.fault;
  }
  if (is_call) c.push(ret_addr);
  c.eip = target;
  return !c.fault;
}

// One instruction. Returns 1 to continue, 0 to stop cleanly, -1 on fault.
int step(Cpu& c) {
  uint32_t ip = c.eip;
  int w = 4;  // operand width; 0x66 flips to 2
  uint32_t q = ip;
  // Prefixes.
  for (;;) {
    uint8_t pfx = c.code(q);
    if (pfx == 0x66) {
      w = 2;
      q++;
    } else if (pfx == 0xF0 || pfx == 0xF2 || pfx == 0xF3 || pfx == 0x2E || pfx == 0x36 ||
               pfx == 0x3E || pfx == 0x26 || pfx == 0x64 || pfx == 0x65 || pfx == 0x67) {
      q++;  // lock/rep/seg/addr-size: accepted, no effect on this subset
    } else {
      break;
    }
  }
  if (c.fault) return -1;
  uint8_t op = c.code(q);
  uint32_t o = q + 1;  // first byte after opcode

  // mov r32, imm32 / mov r16, imm16
  if (op >= 0xB8 && op <= 0xBF) {
    uint32_t imm = c.rd(o, w);
    set_reg(c, op - 0xB8, imm, w);
    c.eip = o + w;
    return c.fault ? -1 : 1;
  }
  // mov r8, imm8
  if (op >= 0xB0 && op <= 0xB7) {
    set_reg(c, op - 0xB0, c.code(o), 1);
    c.eip = o + 1;
    return c.fault ? -1 : 1;
  }
  // push/pop r32
  if (op >= 0x50 && op <= 0x57) {
    c.push(c.r[op - 0x50]);
    c.eip = o;
    return c.fault ? -1 : 1;
  }
  if (op >= 0x58 && op <= 0x5F) {
    c.r[op - 0x58] = c.pop();
    c.eip = o;
    return c.fault ? -1 : 1;
  }
  if (op == 0x68) {  // push imm32
    c.push(c.rd(o, 4));
    c.eip = o + 4;
    return c.fault ? -1 : 1;
  }
  if (op == 0x6A) {  // push imm8 (sign-extended)
    c.push(static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(c.code(o)))));
    c.eip = o + 1;
    return c.fault ? -1 : 1;
  }
  // inc/dec r32 (40-4F). CF preserved.
  if (op >= 0x40 && op <= 0x4F) {
    int reg = op & 7;
    bool dec = op >= 0x48;
    uint32_t save = c.flags & CF;
    uint32_t res = alu(c, c.r[reg], 1, dec ? 1 : 0, 4);
    c.flags = (c.flags & ~CF) | save;
    c.r[reg] = res;
    c.eip = o;
    return 1;
  }
  // ALU r/m,r and r,r/m (add/or/and/sub/xor/cmp): opcodes 0x00..0x3B in
  // regular grid. We handle the 32/8-bit reg forms used by compilers.
  auto alu_kind = [](uint8_t base) -> int {
    switch (base) {
      case 0x00: return 0;  // add
      case 0x28: return 1;  // sub
      case 0x38: return 3;  // cmp (no writeback)
      case 0x20: return 4;  // and
      case 0x08: return 5;  // or
      case 0x30: return 6;  // xor
      default: return -1;
    }
  };
  auto do_logic = [&](int lk, uint32_t a, uint32_t b, int ww) -> uint32_t {
    uint32_t res = lk == 4 ? (a & b) : lk == 5 ? (a | b) : (a ^ b);
    c.flags &= ~(CF | OF);
    set_szp(c, res, ww);
    return res;
  };
  {
    uint8_t base = op & 0xF8;
    int k = alu_kind(base);
    unsigned low = op & 7;
    if (k >= 0 && (low == 0 || low == 1 || low == 2 || low == 3)) {
      int ww = (low & 1) ? w : 1;
      Ea e = modrm(c, o);
      uint32_t a, b, res;
      bool to_rm = (low & 2) == 0;  // 0/1 -> r/m,reg ; 2/3 -> reg,r/m
      uint32_t rmv = load_ea(c, e, ww);
      uint32_t rv = get_reg(c, e.rfield, ww);
      if (to_rm) {
        a = rmv;
        b = rv;
      } else {
        a = rv;
        b = rmv;
      }
      if (k == 0 || k == 1 || k == 3)
        res = alu(c, a, b, k == 3 ? 1 : k, ww);
      else
        res = do_logic(k, a, b, ww);
      if (k != 3) {  // cmp discards
        if (to_rm)
          store_ea(c, e, res, ww);
        else
          set_reg(c, e.rfield, res, ww);
      }
      c.eip = o + e.len;
      return c.fault ? -1 : 1;
    }
  }
  // ALU eax, imm (05 add,2D sub,3D cmp,25 and,0D or,35 xor)
  {
    int k = -1;
    switch (op) {
      case 0x05: k = 0; break;
      case 0x2D: k = 1; break;
      case 0x3D: k = 3; break;
      case 0x25: k = 4; break;
      case 0x0D: k = 5; break;
      case 0x35: k = 6; break;
      default: break;
    }
    if (k >= 0) {
      uint32_t imm = c.rd(o, w);
      uint32_t res = (k <= 1 || k == 3) ? alu(c, c.r[0], imm, k == 3 ? 1 : k, w)
                                        : do_logic(k, c.r[0], imm, w);
      if (k != 3) c.r[0] = res;
      c.eip = o + w;
      return c.fault ? -1 : 1;
    }
  }
  // group1: 81 /r imm32, 83 /r imm8(sx) — add/or/adc/sbb/and/sub/xor/cmp
  if (op == 0x81 || op == 0x83) {
    Ea e = modrm(c, o);
    uint32_t imm;
    int ilen;
    if (op == 0x83) {
      imm = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(c.code(o + e.len))));
      ilen = 1;
    } else {
      imm = c.rd(o + e.len, w);
      ilen = w;
    }
    uint32_t a = load_ea(c, e, w), res = 0;
    switch (e.rfield) {
      case 0: res = alu(c, a, imm, 0, w); break;                 // add
      case 1: res = do_logic(5, a, imm, w); break;            // or
      case 4: res = do_logic(4, a, imm, w); break;            // and
      case 5: res = alu(c, a, imm, 1, w); break;                 // sub
      case 6: res = do_logic(6, a, imm, w); break;            // xor
      case 7: (void)alu(c, a, imm, 1, w); res = a; break;        // cmp (discard)
      default: c.fault = true; return -1;                        // adc/sbb: unimpl
    }
    if (e.rfield != 7) store_ea(c, e, res, w);
    c.eip = o + e.len + ilen;
    return c.fault ? -1 : 1;
  }
  // mov r/m,r (89/88) and r,r/m (8B/8A)
  if (op == 0x89 || op == 0x88 || op == 0x8B || op == 0x8A) {
    int ww = (op == 0x88 || op == 0x8A) ? 1 : w;
    Ea e = modrm(c, o);
    if (op == 0x89 || op == 0x88)
      store_ea(c, e, get_reg(c, e.rfield, ww), ww);
    else
      set_reg(c, e.rfield, load_ea(c, e, ww), ww);
    c.eip = o + e.len;
    return c.fault ? -1 : 1;
  }
  // mov r/m32, imm32 (C7 /0) ; mov r/m8, imm8 (C6 /0)
  if (op == 0xC7 || op == 0xC6) {
    int ww = op == 0xC6 ? 1 : w;
    Ea e = modrm(c, o);
    uint32_t imm = op == 0xC6 ? c.code(o + e.len) : c.rd(o + e.len, ww);
    store_ea(c, e, imm, ww);
    c.eip = o + e.len + (op == 0xC6 ? 1 : ww);
    return c.fault ? -1 : 1;
  }
  // lea r32, m
  if (op == 0x8D) {
    Ea e = modrm(c, o);
    if (!e.mem) {
      c.fault = true;
      return -1;
    }
    set_reg(c, e.rfield, e.addr, w);
    c.eip = o + e.len;
    return 1;
  }
  // test r/m,r (85/84)
  if (op == 0x85 || op == 0x84) {
    int ww = op == 0x84 ? 1 : w;
    Ea e = modrm(c, o);
    (void)do_logic(4, load_ea(c, e, ww), get_reg(c, e.rfield, ww), ww);
    c.eip = o + e.len;
    return c.fault ? -1 : 1;
  }
  // test eax, imm (A9)
  if (op == 0xA9) {
    (void)do_logic(4, c.r[0], c.rd(o, w), w);
    c.eip = o + w;
    return c.fault ? -1 : 1;
  }
  // 0x0F two-byte
  if (op == 0x0F) {
    uint8_t op2 = c.code(o);
    uint32_t o2 = o + 1;
    if (op2 >= 0x80 && op2 <= 0x8F) {  // jcc rel32
      int32_t rel = static_cast<int32_t>(c.rd(o2, 4));
      uint32_t next = o2 + 4;
      c.eip = cond(c, op2 & 0xf) ? next + rel : next;
      return c.fault ? -1 : 1;
    }
    if (op2 == 0xB6 || op2 == 0xB7 || op2 == 0xBE || op2 == 0xBF) {  // movzx/movsx
      int sw = (op2 == 0xB6 || op2 == 0xBE) ? 1 : 2;
      Ea e = modrm(c, o2);
      uint32_t v = load_ea(c, e, sw);
      if (op2 == 0xBE || op2 == 0xBF) {  // sign-extend
        uint32_t m = msb(sw);
        if (v & m) v |= ~mask(sw);
      }
      set_reg(c, e.rfield, v, w);
      c.eip = o2 + e.len;
      return c.fault ? -1 : 1;
    }
    if (op2 == 0xAF) {  // imul r32, r/m32
      Ea e = modrm(c, o2);
      uint32_t res = get_reg(c, e.rfield, w) * load_ea(c, e, w);
      set_reg(c, e.rfield, res, w);
      c.eip = o2 + e.len;
      return c.fault ? -1 : 1;
    }
    c.fault = true;
    return -1;
  }
  // jcc rel8
  if (op >= 0x70 && op <= 0x7F) {
    int32_t rel = static_cast<int8_t>(c.code(o));
    uint32_t next = o + 1;
    c.eip = cond(c, op & 0xf) ? next + rel : next;
    return c.fault ? -1 : 1;
  }
  if (op == 0xEB) {  // jmp rel8
    int32_t rel = static_cast<int8_t>(c.code(o));
    c.eip = o + 1 + rel;
    return c.fault ? -1 : 1;
  }
  if (op == 0xE9) {  // jmp rel32
    int32_t rel = static_cast<int32_t>(c.rd(o, 4));
    c.eip = o + 4 + rel;
    return c.fault ? -1 : 1;
  }
  if (op == 0xE8) {  // call rel32
    int32_t rel = static_cast<int32_t>(c.rd(o, 4));
    uint32_t next = o + 4;
    return transfer(c, next + rel, true, next) ? 1 : -1;
  }
  // group5: FF /2 call r/m, /4 jmp r/m, /6 push r/m, /0 inc, /1 dec
  if (op == 0xFF) {
    Ea e = modrm(c, o);
    uint32_t next = o + e.len;
    switch (e.rfield) {
      case 0: {  // inc
        uint32_t save = c.flags & CF, res = alu(c, load_ea(c, e, w), 1, 0, w);
        c.flags = (c.flags & ~CF) | save;
        store_ea(c, e, res, w);
        c.eip = next;
        return c.fault ? -1 : 1;
      }
      case 1: {  // dec
        uint32_t save = c.flags & CF, res = alu(c, load_ea(c, e, w), 1, 1, w);
        c.flags = (c.flags & ~CF) | save;
        store_ea(c, e, res, w);
        c.eip = next;
        return c.fault ? -1 : 1;
      }
      case 2:  // call r/m
        return transfer(c, load_ea(c, e, w), true, next) ? 1 : -1;
      case 4:  // jmp r/m
        return transfer(c, load_ea(c, e, w), false, next) ? 1 : -1;
      case 6:  // push r/m
        c.push(load_ea(c, e, w));
        c.eip = next;
        return c.fault ? -1 : 1;
      default:
        c.fault = true;
        return -1;
    }
  }
  if (op == 0xC3) {  // ret
    c.eip = c.pop();
    return c.fault ? -1 : 1;
  }
  if (op == 0xC2) {  // ret imm16
    uint32_t imm = c.rd(o, 2);
    c.eip = c.pop();
    c.r[4] += imm;
    return c.fault ? -1 : 1;
  }
  if (op == 0x90) {  // nop
    c.eip = o;
    return 1;
  }
  if (op == 0xF4 || op == 0xCC) {  // hlt / int3
    c.eip = o;
    return 0;
  }
  c.fault = true;  // unimplemented opcode
  return -1;
}

}  // namespace

PeInterpResult pe_interp_run(const uint8_t* code, size_t len, uint32_t entry_rva,
                             const uint32_t* regs4, unsigned budget) {
  PeInterpResult out;
  if (code == nullptr || len == 0) {
    out.fault = true;
    out.steps = -1;
    return out;
  }
  Cpu c;
  c.image.assign(code, code + len);
  c.stack.assign(kStackSize, 0);
  if (regs4)
    for (int i = 0; i < 4; ++i) c.r[i] = regs4[i];
  c.r[4] = kStackBase + kStackSize - 16;  // esp near the top
  c.push(kSentinel);                      // top frame return address
  c.flags = 0x2;                          // reserved bit
  c.eip = kPeImageBase + entry_rva;

  long steps = 0;
  unsigned cap = budget ? budget : 1000000u;
  for (; static_cast<unsigned>(steps) < cap; ++steps) {
    if (c.eip == kSentinel) break;  // returned from the top frame
    int r = step(c);
    if (r < 0) {
      out.fault = true;
      out.steps = -1;
      out.eax = c.r[0];
      out.last_reply = c.last_reply;
      return out;
    }
    if (r == 0) {
      ++steps;
      break;
    }
  }
  out.eax = c.r[0];
  out.steps = steps;
  out.last_reply = c.last_reply;
  if (regs4) {
    // (caller reads via result; regs4 is input-only here)
  }
  return out;
}

}  // namespace wasmgocos
