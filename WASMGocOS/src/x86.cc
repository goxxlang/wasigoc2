#include "gocos/x86.hpp"

#include <cstdio>
#include <cstring>

// x86-64 / IA-32 engine for the vCPU (include/gocos/x86.hpp). One
// instruction at a time: prefixes, REX, opcode, ModRM/SIB (RIP-relative in
// 64-bit code), then the operation on the guest's registers and memory.
// Memory is linear -> (CR3 page walk when CR0.PG) -> guest physical ->
// host through the caller's map. A guest fault is thrown as Fault and
// delivered through the guest IDT by Run.

namespace wasmgocos {
namespace x86 {
namespace {

constexpr uint64_t CF = 1u << 0, PF = 1u << 2, AF = 1u << 4, ZF = 1u << 6, SF = 1u << 7,
                   TF = 1u << 8, IF = 1u << 9, DF = 1u << 10, OF = 1u << 11, IOPL = 3u << 12,
                   NT = 1u << 14, RF = 1u << 16, VM = 1u << 17, AC = 1u << 18;
constexpr uint64_t kArith = CF | PF | AF | ZF | SF | OF;
// RFLAGS bits POPF/SAHF may change at CPL 0 (not VM/RF/VIF/VIP).
constexpr uint64_t kPopfMask = 0x003F7FD5u & ~((1u << 16) | (1u << 17) | (1u << 19) | (1u << 20));
// RFLAGS bits IRET restores at CPL 0.
constexpr uint64_t kIretMask = kPopfMask | RF;

constexpr uint64_t kCr0Ts = 1u << 3, kCr0Wp = 1u << 16, kCr0Pg = 1ull << 31;
constexpr uint64_t kEferSce = 1u << 0, kEferLme = 1u << 8, kEferLma = 1u << 10, kEferNxe = 1u << 11;

enum : uint8_t {
  kDE = 0, kDB = 1, kBP = 3, kUD = 6, kNM = 7, kDF = 8, kTS = 10, kNP = 11, kSS = 12, kGP = 13, kPF = 14,
};

struct Fault {
  uint8_t vector;
  bool has_code;
  uint32_t code;
  uint64_t addr;
  bool trap;         // RIP pushed is the next instruction (INT3, INT n)
  uint64_t next;     // that next RIP
  const char* why;
  bool soft = false;      // INT n / INT3: the gate's DPL must admit the CPL
  bool external = false;  // an injected interrupt, not raised by an instruction
};

// Linear page -> host page, with what the walk allowed. A hit needs every
// bit the access asks for; a write only caches once the walk has set the
// PTE's dirty bit. Flushed by Run, CR0/CR3/CR4 writes, and INVLPG.
enum : uint8_t { kTlbRead = 1, kTlbWrite = 2, kTlbFetch = 4, kTlbUser = 8 };
struct TlbEntry {
  uint64_t vpn = ~0ull;
  uint8_t* host = nullptr;
  uint8_t perm = 0;
};
constexpr int kTlbSize = 1024;
TlbEntry g_tlb[kTlbSize];

void TlbFlush() {
  for (TlbEntry& e : g_tlb) e.vpn = ~0ull;
}

// ---- decoded-instruction cache ------------------------------------------
//
// Decoding the same instruction on every pass is most of a tree-walking
// interpreter's cost. This memoizes the decode of register-form and
// control-flow instructions (the bulk of what a boot executes), keyed by
// the linear RIP. It is coherent by construction: a hit re-compares the
// instruction's bytes against the current code page, so self-modifying
// code, a page remapped under the same address, and a switch between long
// and 32-bit mode all simply miss and re-decode -- no write-tracking, no
// flush on CR3. Instructions with a register-dependent effective address
// (a memory operand) are not cached; they fall back to the full decoder.
enum FastH : uint8_t {
  FH_NONE = 0, FH_ALU_RR, FH_ALU_AI, FH_GRP1_RR, FH_TEST_RR, FH_MOV_RR,
  FH_MOV_RI, FH_MOVI_RR, FH_SHIFT_RR, FH_INCDEC_RR, FH_PUSH_R, FH_POP_R,
  FH_JCC, FH_JMP, FH_CALL, FH_RET, FH_MOVX_RR, FH_XCHG_A, FH_NOP,
  FH_MOVSXD_RR, FH_CWD, FH_CBW, FH_CMOV_RR, FH_SETCC_RR, FH_FLAG,
};

struct DecEntry {
  uint64_t tag = ~0ull;  // the linear RIP this decode is for
  uint8_t valid = 0;
  uint8_t mode = 0;   // 1 in 64-bit code, 0 in 32-bit: the one thing not in the bytes
  uint8_t len = 0;    // instruction length
  uint8_t h = FH_NONE;
  uint8_t w = 0;      // operand width in bytes
  uint8_t kind = 0;   // ALU/shift kind, or Group-1/Group-5 sub-op
  uint8_t reg = 0;    // ModRM.reg with REX.R (or the embedded register)
  uint8_t rm = 0;     // ModRM.rm with REX.B
  uint8_t cc = 0;     // condition code (Jcc/CMOVcc/SETcc)
  uint8_t cnt = 0;    // shift count source: 0 = 1, 1 = imm, 2 = CL
  uint8_t sub = 0;    // movzx/movsx source width, or a flag op selector
  uint8_t sign = 0;   // sign-extend (movsx / movsxd)
  uint8_t rexp = 0;   // a REX prefix was present (AH/AL vs SPL selection)
  uint64_t imm = 0;
  int64_t rel = 0;
  uint8_t b[15] = {};  // the instruction's bytes, re-checked on every hit
};

constexpr int kDecBits = 14;
constexpr int kDecSize = 1 << kDecBits;
DecEntry g_dec[kDecSize];

inline uint32_t DecHash(uint64_t rip) {
  uint64_t x = rip * 0x9E3779B97F4A7C15ull;
  return static_cast<uint32_t>(x >> (64 - kDecBits)) & (kDecSize - 1);
}

// 8-byte segment descriptor fields.
uint64_t DescBase(uint64_t d) { return ((d >> 16) & 0xffffff) | (((d >> 56) & 0xff) << 24); }
uint32_t DescLimit(uint64_t d) {
  uint32_t l = static_cast<uint32_t>((d & 0xffff) | (((d >> 48) & 0xf) << 16));
  return (d & (1ull << 55)) ? (l << 12) | 0xfff : l;
}
int DescDpl(uint64_t d) { return static_cast<int>((d >> 45) & 3); }
bool DescLong(uint64_t d) { return (d >> 53) & 1; }

[[noreturn]] void Raise(uint8_t v, const char* why, uint64_t addr = 0, bool has_code = false,
                        uint32_t code = 0) {
  throw Fault{v, has_code, code, addr, false, 0, why};
}

uint64_t Mask(int w) { return w == 8 ? ~0ull : (1ull << (w * 8)) - 1; }
uint64_t Msb(int w) { return 1ull << (w * 8 - 1); }
int64_t Sx(uint64_t v, int w) {
  if (w == 8) return static_cast<int64_t>(v);
  uint64_t m = Msb(w);
  v &= Mask(w);
  return static_cast<int64_t>((v ^ m) - m);
}
bool Parity(uint64_t v) {
  unsigned b = static_cast<unsigned>(v & 0xff);
  b ^= b >> 4;
  b ^= b >> 2;
  b ^= b >> 1;
  return !(b & 1);
}

// A message for an opcode the engine does not implement. One per run is
// enough: the run stops on it.
char g_why[96];
const char* Unimplemented(const uint8_t* b, int n) {
  int at = std::snprintf(g_why, sizeof(g_why), "unimplemented:");
  for (int i = 0; i < n && at < static_cast<int>(sizeof(g_why)) - 4; ++i)
    at += std::snprintf(g_why + at, sizeof(g_why) - at, " %02x", b[i]);
  return g_why;
}

class Cpu {
 public:
  Cpu(State& s, const Memory& m, ExitInfo& info) : s_(s), m_(m), info_(info) {}

  // One instruction. True when it ends the run with an exit.
  bool Step();
  // Delivers `f` through the guest IDT. False when the guest cannot take it.
  bool Deliver(const Fault& f);

 private:
  struct Ea {
    bool mem = false;
    int reg = 0;
    uint64_t off = 0;
    bool riprel = false;
    int seg = -1;
    int end = 0;  // instruction offset just after ModRM/SIB/disp
    int imm = 0;  // immediate bytes that follow (RIP-relative is from the end)
  };

  // ---- privilege
  int Cpl() const { return s_.cs & 3; }
  int Iopl() const { return static_cast<int>((s_.rflags >> 12) & 3); }
  // System instructions: #GP(0) outside ring 0.
  void Priv() {
    if (Cpl() != 0) Raise(kGP, "privileged instruction at CPL 3", 0, true, 0);
  }
  // IN/OUT/CLI/STI: #GP(0) when the CPL is above IOPL.
  void IoPriv() {
    if (Cpl() > Iopl()) Raise(kGP, "I/O privilege", 0, true, 0);
  }
  // The 8-byte GDT/LDT entry `sel` names (a supervisor access).
  uint64_t Descriptor(uint16_t sel) {
    bool ldt = sel & 4;
    uint64_t base = ldt ? s_.ldtr_base : s_.gdtr_base;
    uint32_t limit = ldt ? s_.ldtr_limit : s_.gdtr_limit;
    uint32_t off = sel & ~7u;
    if (off + 7 > limit) Raise(kGP, "selector outside the descriptor table", 0, true, sel & ~3u);
    return SysRd(base + off, 8);
  }
  // A code selector's CS.L; 64-bit when the table has no such entry.
  bool CodeLong(uint16_t sel) {
    if ((sel & ~3u) == 0) return s_.code64;
    return DescLong(Descriptor(sel));
  }
  // MOV/POP to a data segment register. FS and GS take the base the
  // descriptor names (0 for a null selector), as a load does on Intel.
  void LoadSeg(int sr, uint16_t sel);
  uint64_t SysRd(uint64_t la, int w) {
    bool was = sys_;
    sys_ = true;
    uint64_t v = Rd(la, w);
    sys_ = was;
    return v;
  }
  void SysWr(uint64_t la, uint64_t v, int w) {
    bool was = sys_;
    sys_ = true;
    Wr(la, v, w);
    sys_ = was;
  }

  // ---- memory
  // Linear -> guest physical. `perm` receives the TlbEntry bits the walk
  // allows. Checks U/S, R/W (CR0.WP), and NX; sets accessed and dirty.
  uint64_t Translate(uint64_t la, bool write, bool fetch, uint8_t* perm);
  // Host bytes for `la` (the rest of its page), through the TLB.
  uint8_t* Lookup(uint64_t la, bool write, bool fetch, uint64_t n);
  void Access(uint64_t la, void* buf, uint64_t n, bool write, bool fetch = false);
  uint64_t Rd(uint64_t la, int w) {
    uint64_t v = 0;
    Access(la, &v, static_cast<uint64_t>(w), false);
    return v;
  }
  void Wr(uint64_t la, uint64_t v, int w) { Access(la, &v, static_cast<uint64_t>(w), true); }
  void Rd128(uint64_t la, Xmm* x) {
    uint8_t b[16];
    Access(la, b, 16, false);
    std::memcpy(&x->lo, b, 8);
    std::memcpy(&x->hi, b + 8, 8);
  }
  void Wr128(uint64_t la, const Xmm& x) {
    uint8_t b[16];
    std::memcpy(b, &x.lo, 8);
    std::memcpy(b + 8, &x.hi, 8);
    Access(la, b, 16, true);
  }
  uint64_t Wrap(uint64_t a) const { return s_.code64 ? a : (a & 0xffffffffull); }

  // ---- instruction bytes
  uint8_t Ib(int i);
  uint64_t Imm(int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v |= static_cast<uint64_t>(Ib(p_ + i)) << (8 * i);
    p_ += n;
    return v;
  }
  int64_t Simm(int n) { return Sx(Imm(n), n); }

  // ---- operands
  Ea ModRM();
  uint64_t Addr(const Ea& e) {
    uint64_t off = e.off;
    if (e.riprel) off += ip0_ + static_cast<uint64_t>(e.end + e.imm);
    if (adsize_ == 4) off &= 0xffffffffull;
    uint64_t base = e.seg == 4 ? s_.fs_base : e.seg == 5 ? s_.gs_base : 0;
    return Wrap(base + off);
  }
  uint64_t GetR(int r, int w) const {
    if (w == 1 && !rexp_ && r >= 4 && r < 8) return (s_.gpr[r - 4] >> 8) & 0xff;
    return s_.gpr[r] & Mask(w);
  }
  void SetR(int r, int w, uint64_t v) {
    if (w == 8) {
      s_.gpr[r] = v;
    } else if (w == 4) {
      s_.gpr[r] = v & 0xffffffffull;
    } else if (w == 2) {
      s_.gpr[r] = (s_.gpr[r] & ~0xffffull) | (v & 0xffff);
    } else if (!rexp_ && r >= 4 && r < 8) {
      s_.gpr[r - 4] = (s_.gpr[r - 4] & ~0xff00ull) | ((v & 0xff) << 8);
    } else {
      s_.gpr[r] = (s_.gpr[r] & ~0xffull) | (v & 0xff);
    }
  }
  uint64_t Load(const Ea& e, int w) { return e.mem ? Rd(Addr(e), w) : GetR(e.reg, w); }
  void Store(const Ea& e, int w, uint64_t v) {
    if (e.mem)
      Wr(Addr(e), v, w);
    else
      SetR(e.reg, w, v);
  }
  Xmm LoadX(const Ea& e) {
    Xmm x;
    if (e.mem)
      Rd128(Addr(e), &x);
    else
      x = s_.xmm[e.reg];
    return x;
  }
  Xmm LoadXAligned(const Ea& e) {
    if (e.mem && (Addr(e) & 15)) Raise(kGP, "misaligned 16-byte access", Addr(e), true, 0);
    return LoadX(e);
  }

  // ---- flags
  uint64_t& F() { return s_.rflags; }
  void SetSZP(uint64_t r, int w) {
    F() &= ~(SF | ZF | PF);
    r &= Mask(w);
    if (!r) F() |= ZF;
    if (r & Msb(w)) F() |= SF;
    if (Parity(r)) F() |= PF;
  }
  uint64_t Add(uint64_t a, uint64_t b, int w, uint64_t c = 0) {
    a &= Mask(w);
    b &= Mask(w);
    uint64_t r = (a + b + c) & Mask(w);
    bool cf = w == 8 ? (r < a || (c && r == a)) : ((a + b + c) >> (w * 8)) != 0;
    F() &= ~kArith;
    if (cf) F() |= CF;
    if ((a ^ r) & (b ^ r) & Msb(w)) F() |= OF;
    if ((a ^ b ^ r) & 0x10) F() |= AF;
    SetSZP(r, w);
    return r;
  }
  uint64_t Sub(uint64_t a, uint64_t b, int w, uint64_t c = 0) {
    a &= Mask(w);
    b &= Mask(w);
    uint64_t r = (a - b - c) & Mask(w);
    bool cf = b > a || (c && b == a);
    F() &= ~kArith;
    if (cf) F() |= CF;
    if ((a ^ b) & (a ^ r) & Msb(w)) F() |= OF;
    if ((a ^ b ^ r) & 0x10) F() |= AF;
    SetSZP(r, w);
    return r;
  }
  uint64_t Logic(uint64_t r, int w) {
    F() &= ~kArith;
    SetSZP(r, w);
    return r & Mask(w);
  }
  // kind: 0 add, 1 or, 2 adc, 3 sbb, 4 and, 5 sub, 6 xor, 7 cmp
  uint64_t Alu(int kind, uint64_t a, uint64_t b, int w) {
    switch (kind) {
      case 0: return Add(a, b, w);
      case 1: return Logic(a | b, w);
      case 2: return Add(a, b, w, (F() & CF) ? 1 : 0);
      case 3: return Sub(a, b, w, (F() & CF) ? 1 : 0);
      case 4: return Logic(a & b, w);
      case 5:
      case 7: return Sub(a, b, w);
      default: return Logic(a ^ b, w);
    }
  }
  bool Cond(unsigned cc) const {
    uint64_t f = s_.rflags;
    bool zf = f & ZF, cf = f & CF, sf = f & SF, of = f & OF, pf = f & PF;
    bool r;
    switch ((cc >> 1) & 7) {
      case 0: r = of; break;
      case 1: r = cf; break;
      case 2: r = zf; break;
      case 3: r = cf || zf; break;
      case 4: r = sf; break;
      case 5: r = pf; break;
      case 6: r = sf != of; break;
      default: r = zf || (sf != of); break;
    }
    return (cc & 1) ? !r : r;
  }
  uint64_t Shift(int kind, uint64_t a, unsigned cnt, int w);
  void MulDiv(int kind, const Ea& e, int w);

  // ---- stack and control
  int StackW() const { return s_.code64 ? (opsize_ == 2 ? 2 : 8) : (opsize_ == 2 ? 2 : 4); }
  void Push(uint64_t v, int w) {
    uint64_t sp = Wrap(s_.gpr[RSP] - static_cast<uint64_t>(w));
    Wr(sp, v, w);
    s_.gpr[RSP] = sp;
  }
  uint64_t Pop(int w) {
    uint64_t v = Rd(Wrap(s_.gpr[RSP]), w);
    s_.gpr[RSP] = Wrap(s_.gpr[RSP] + static_cast<uint64_t>(w));
    return v;
  }
  uint64_t NextIp() const { return Wrap(ip0_ + static_cast<uint64_t>(p_)); }
  bool Next() {
    s_.rip = NextIp();
    return false;
  }
  bool Jump(uint64_t target) {
    s_.rip = Wrap(target);
    return false;
  }
  bool ExitHere(Exit why) {
    info_.reason = why;
    info_.len = static_cast<uint8_t>(p_);
    s_.rip = ip0_;
    return true;
  }
  [[noreturn]] void Ud() { Raise(kUD, Unimplemented(ib_, p_ < nib_ ? p_ + 1 : nib_)); }
  [[noreturn]] void Trap(uint8_t v, const char* why) {
    throw Fault{v, false, 0, 0, true, NextIp(), why, true, false};
  }

  bool StringOp(uint8_t op);
  bool TwoByte();
  bool Group0F00();
  bool Group0F01();
  bool MovCrDr(uint8_t op2);
  bool Msr(bool write);
  bool FxSave(const Ea& e, bool save);
  bool Iret();
  bool FarRet(bool imm);
  bool Sse(uint8_t op2);
  bool X87(uint8_t op);
  bool DeliverLong(const Fault& f);

  // Decoded-instruction cache. DecodeFast parses a register-form or
  // control-flow instruction into `d` (no state change) and returns true, or
  // returns false for anything it does not handle (fall back to the full
  // decoder). ExecFast runs a decoded entry, reusing the same helpers as the
  // full path. StepFull is the original decode-and-execute path.
  void ResetDecode();
  bool DecodeFast(DecEntry& d);
  bool ExecFast(const DecEntry& d);
  bool StepFull();

 public:
  // Delivers `f`; a fault raised while delivering it becomes #DF, and one
  // while delivering #DF shuts the processor down.
  enum class Taken { kDelivered, kNoIdt, kShutdown };
  Taken Take(Fault f);

 private:

  State& s_;
  const Memory& m_;
  ExitInfo& info_;
  uint64_t ip0_ = 0;
  uint8_t ib_[16] = {};
  int nib_ = 0;
  int p_ = 0;
  int opsize_ = 4;
  int adsize_ = 8;
  int seg_ = -1;
  uint8_t rex_ = 0;
  bool rexp_ = false;
  bool pf66_ = false, pfF2_ = false, pfF3_ = false;
  int reg_ = 0;  // ModRM reg field, with REX.R
  bool sys_ = false;  // an implicit supervisor access (descriptor tables, TSS)
};

uint64_t Cpu::Translate(uint64_t la, bool write, bool fetch, uint8_t* perm) {
  if (!(s_.cr0 & kCr0Pg)) {
    *perm = kTlbRead | kTlbWrite | kTlbFetch | kTlbUser;
    return la;
  }
  if (!s_.code64 && !(s_.efer & kEferLma)) Raise(kGP, "32-bit paging is not modeled", la);
  bool user = Cpl() == 3 && !sys_;
  bool nxe = s_.efer & kEferNxe;
  if (s_.code64 && static_cast<uint64_t>(static_cast<int64_t>(la << 16) >> 16) != la)
    Raise(kGP, "non-canonical address", la, true, 0);
  uint32_t code = (write ? 2u : 0u) | (user ? 4u : 0u) | (fetch && nxe ? 16u : 0u);
  uint64_t table = s_.cr3 & 0x000ffffffffff000ull;
  static const int kShift[4] = {39, 30, 21, 12};
  uint8_t* ent[4] = {};
  int levels = 0;
  bool rw = true, us = true, nx = false;
  uint64_t pa = 0;
  for (int level = 0; level < 4; ++level) {
    uint64_t slot = table + ((la >> kShift[level]) & 0x1ff) * 8;
    uint8_t* h = m_.gpa(m_.ctx, slot, 8);
    if (h == nullptr) {
      s_.cr2 = la;
      Raise(kPF, "page table outside guest memory", la, true, code);
    }
    uint64_t e;
    std::memcpy(&e, h, 8);
    if (!(e & 1)) {
      s_.cr2 = la;
      Raise(kPF, "page not present", la, true, code);
    }
    ent[levels++] = h;
    rw = rw && (e & 2);
    us = us && (e & 4);
    if (nxe && (e >> 63)) nx = true;
    if (level == 1 && (e & 0x80)) {
      pa = (e & 0x000fffffc0000000ull) | (la & 0x3fffffffull);
      break;
    }
    if (level == 2 && (e & 0x80)) {
      pa = (e & 0x000fffffffe00000ull) | (la & 0x1fffffull);
      break;
    }
    table = e & 0x000ffffffffff000ull;
    if (level == 3) pa = table | (la & 0xfff);
  }
  bool write_ok = rw || (!user && !(s_.cr0 & kCr0Wp));
  if ((user && !us) || (write && !write_ok) || (fetch && nx)) {
    s_.cr2 = la;
    Raise(kPF, "page protection", la, true, code | 1u);
  }
  // Accessed on every level; dirty on the leaf for a write.
  for (int i = 0; i < levels; ++i) {
    uint64_t e;
    std::memcpy(&e, ent[i], 8);
    uint64_t want = e | 0x20 | ((write && i == levels - 1) ? 0x40 : 0);
    if (want != e) std::memcpy(ent[i], &want, 8);
  }
  *perm = static_cast<uint8_t>(kTlbRead | (write && rw ? kTlbWrite : 0) | (nx ? 0 : kTlbFetch) |
                               (us ? kTlbUser : 0));
  return pa;
}

uint8_t* Cpu::Lookup(uint64_t la, bool write, bool fetch, uint64_t n) {
  uint64_t vpn = la >> 12;
  uint8_t need = write ? kTlbWrite : fetch ? kTlbFetch : kTlbRead;
  if (Cpl() == 3 && !sys_) need |= kTlbUser;
  TlbEntry& t = g_tlb[vpn & (kTlbSize - 1)];
  if (t.vpn == vpn && (t.perm & need) == need) return t.host + (la & 0xfff);
  uint8_t perm = 0;
  uint64_t pa = Translate(la, write, fetch, &perm);
  uint8_t* page = m_.gpa(m_.ctx, pa & ~0xfffull, 0x1000);
  if (page != nullptr) {
    // A user-mode walk may not grant what a supervisor one would; cache
    // what this walk proved.
    t.vpn = vpn;
    t.host = page;
    t.perm = perm;
    return page + (la & 0xfff);
  }
  uint8_t* h = m_.gpa(m_.ctx, pa, n);
  if (h == nullptr) {
    s_.cr2 = la;
    Raise(kPF, "guest physical page not mapped", la, true,
          1u | (write ? 2u : 0u) | (fetch ? 16u : 0u));
  }
  return h;
}

void Cpu::Access(uint64_t la, void* buf, uint64_t n, bool write, bool fetch) {
  auto* b = static_cast<uint8_t*>(buf);
  while (n) {
    uint64_t chunk = 0x1000 - (la & 0xfff);
    if (chunk > n) chunk = n;
    uint8_t* h = Lookup(la, write, fetch, chunk);
    if (write)
      std::memcpy(h, b, chunk);
    else
      std::memcpy(b, h, chunk);
    la = Wrap(la + chunk);
    b += chunk;
    n -= chunk;
  }
}

void Cpu::LoadSeg(int sr, uint16_t sel) {
  uint64_t base = 0;
  if ((sel & ~3u) != 0 && (sr == 4 || sr == 5)) base = DescBase(Descriptor(sel));
  switch (sr) {
    case 0: s_.es = sel; break;
    case 2:
      s_.ss = sel;
      s_.int_shadow = true;
      break;
    case 3: s_.ds = sel; break;
    case 4:
      s_.fs = sel;
      s_.fs_base = base;
      break;
    case 5:
      s_.gs = sel;
      s_.gs_base = base;
      break;
    default: Ud();
  }
}

uint8_t Cpu::Ib(int i) {
  if (i >= 15) Raise(kGP, "instruction longer than 15 bytes");
  while (i >= nib_) {
    uint64_t la = Wrap(ip0_ + static_cast<uint64_t>(nib_));
    uint64_t chunk = 0x1000 - (la & 0xfff);
    if (chunk > static_cast<uint64_t>(15 - nib_)) chunk = static_cast<uint64_t>(15 - nib_);
    // The tail of an instruction may sit on a page that is not mapped;
    // only bytes the decoder asks for must be there.
    if (i >= nib_ + static_cast<int>(chunk)) {
      Access(la, ib_ + nib_, chunk, false, true);
      nib_ += static_cast<int>(chunk);
      continue;
    }
    Access(la, ib_ + nib_, static_cast<uint64_t>(i - nib_ + 1), false, true);
    nib_ = i + 1;
  }
  return ib_[i];
}

Cpu::Ea Cpu::ModRM() {
  Ea e;
  e.seg = seg_;
  uint8_t m = Ib(p_++);
  int mod = m >> 6, rm = m & 7;
  reg_ = ((m >> 3) & 7) | ((rex_ & 4) ? 8 : 0);
  if (mod == 3) {
    e.reg = rm | ((rex_ & 1) ? 8 : 0);
    e.end = p_;
    return e;
  }
  e.mem = true;
  uint64_t a = 0;
  if (rm == 4) {
    uint8_t sib = Ib(p_++);
    int sc = sib >> 6, raw_idx = (sib >> 3) & 7, bs = sib & 7;
    int idx = raw_idx | ((rex_ & 2) ? 8 : 0);
    if (!(raw_idx == 4 && !(rex_ & 2))) a += s_.gpr[idx] << sc;
    if (bs == 5 && mod == 0)
      a += static_cast<uint64_t>(Simm(4));
    else
      a += s_.gpr[bs | ((rex_ & 1) ? 8 : 0)];
  } else if (rm == 5 && mod == 0) {
    int64_t d = Simm(4);
    e.off = static_cast<uint64_t>(d);
    e.riprel = s_.code64;
    e.end = p_;
    return e;
  } else {
    a = s_.gpr[rm | ((rex_ & 1) ? 8 : 0)];
  }
  if (mod == 1)
    a += static_cast<uint64_t>(Simm(1));
  else if (mod == 2)
    a += static_cast<uint64_t>(Simm(4));
  e.off = a;
  e.end = p_;
  return e;
}

// kind: 0 rol, 1 ror, 2 rcl, 3 rcr, 4 shl, 5 shr, 6 sal(shl), 7 sar
uint64_t Cpu::Shift(int kind, uint64_t a, unsigned cnt, int w) {
  int bits = w * 8;
  cnt &= (w == 8) ? 63 : 31;
  a &= Mask(w);
  if (cnt == 0) return a;
  uint64_t r = a;
  switch (kind) {
    case 0: {  // rol
      unsigned c = cnt % static_cast<unsigned>(bits);
      r = c ? ((a << c) | (a >> (bits - c))) & Mask(w) : a;
      F() = (F() & ~CF) | ((r & 1) ? CF : 0);
      if (cnt == 1) F() = (F() & ~OF) | ((((r & Msb(w)) != 0) ^ ((F() & CF) != 0)) ? OF : 0);
      return r;
    }
    case 1: {  // ror
      unsigned c = cnt % static_cast<unsigned>(bits);
      r = c ? ((a >> c) | (a << (bits - c))) & Mask(w) : a;
      F() = (F() & ~CF) | ((r & Msb(w)) ? CF : 0);
      if (cnt == 1) F() = (F() & ~OF) | ((((r >> (bits - 1)) ^ (r >> (bits - 2))) & 1) ? OF : 0);
      return r;
    }
    case 2:
    case 3: {  // rcl / rcr: through carry, one bit at a time
      unsigned c = cnt % static_cast<unsigned>(bits + 1);
      bool cf = F() & CF;
      for (unsigned i = 0; i < c; ++i) {
        if (kind == 2) {
          bool out = r & Msb(w);
          r = ((r << 1) | (cf ? 1 : 0)) & Mask(w);
          cf = out;
        } else {
          bool out = r & 1;
          r = (r >> 1) | (cf ? Msb(w) : 0);
          cf = out;
        }
      }
      F() = (F() & ~CF) | (cf ? CF : 0);
      if (cnt == 1) {
        bool of = kind == 2 ? (((r & Msb(w)) != 0) ^ cf) : (((r >> (bits - 1)) ^ (r >> (bits - 2))) & 1);
        F() = (F() & ~OF) | (of ? OF : 0);
      }
      return r;
    }
    case 4:
    case 6: {  // shl
      bool cf = cnt <= static_cast<unsigned>(bits) ? ((a >> (bits - cnt)) & 1) : false;
      r = cnt >= static_cast<unsigned>(bits) ? 0 : (a << cnt) & Mask(w);
      F() &= ~(CF | OF | AF);
      if (cf) F() |= CF;
      if (((r & Msb(w)) != 0) ^ cf) F() |= OF;
      SetSZP(r, w);
      return r;
    }
    case 5: {  // shr
      bool cf = (a >> (cnt - 1)) & 1;
      r = cnt >= static_cast<unsigned>(bits) ? 0 : a >> cnt;
      F() &= ~(CF | OF | AF);
      if (cf) F() |= CF;
      if (a & Msb(w)) F() |= OF;
      SetSZP(r, w);
      return r;
    }
    default: {  // sar
      int64_t sa = Sx(a, w);
      unsigned c = cnt >= static_cast<unsigned>(bits) ? static_cast<unsigned>(bits - 1) : cnt;
      bool cf = (static_cast<uint64_t>(sa >> (c - 1))) & 1;
      if (cnt >= static_cast<unsigned>(bits)) cf = sa < 0;
      r = static_cast<uint64_t>(sa >> c) & Mask(w);
      F() &= ~(CF | OF | AF);
      if (cf) F() |= CF;
      SetSZP(r, w);
      return r;
    }
  }
}

// F6/F7 /4../7: mul, imul, div, idiv on rDX:rAX.
void Cpu::MulDiv(int kind, const Ea& e, int w) {
  uint64_t src = Load(e, w);
  int bits = w * 8;
  if (kind == 4 || kind == 5) {
    uint64_t lo, hi;
    bool over;
    if (kind == 4) {
      unsigned __int128 p = static_cast<unsigned __int128>(GetR(RAX, w)) * src;
      lo = static_cast<uint64_t>(p) & Mask(w);
      hi = static_cast<uint64_t>(p >> bits) & Mask(w);
      over = hi != 0;
    } else {
      __int128 p = static_cast<__int128>(Sx(GetR(RAX, w), w)) * Sx(src, w);
      lo = static_cast<uint64_t>(p) & Mask(w);
      hi = static_cast<uint64_t>(p >> bits) & Mask(w);
      over = p != static_cast<__int128>(Sx(lo, w));
    }
    if (w == 1) {
      SetR(RAX, 2, (hi << 8) | lo);
    } else {
      SetR(RAX, w, lo);
      SetR(RDX, w, hi);
    }
    F() &= ~(CF | OF);
    if (over) F() |= CF | OF;
    SetSZP(lo, w);  // undefined on hardware; kept deterministic
    return;
  }
  if (src == 0) Raise(kDE, "divide by zero");
  unsigned __int128 num;
  if (w == 1)
    num = GetR(RAX, 2);
  else
    num = (static_cast<unsigned __int128>(GetR(RDX, w)) << bits) | GetR(RAX, w);
  uint64_t q, r;
  if (kind == 6) {
    unsigned __int128 qq = num / src;
    if (qq > Mask(w)) Raise(kDE, "divide overflow");
    q = static_cast<uint64_t>(qq);
    r = static_cast<uint64_t>(num % src);
  } else {
    __int128 sn;
    if (w == 8)
      sn = static_cast<__int128>(num);
    else if (w == 1)
      sn = Sx(static_cast<uint64_t>(num), 2);
    else
      sn = Sx(static_cast<uint64_t>(num), w * 2);
    __int128 sd = Sx(src, w);
    __int128 qq = sn / sd;
    __int128 lim = static_cast<__int128>(1) << (bits - 1);
    if (qq >= lim || qq < -lim) Raise(kDE, "divide overflow");
    q = static_cast<uint64_t>(qq) & Mask(w);
    r = static_cast<uint64_t>(sn % sd) & Mask(w);
  }
  if (w == 1) {
    SetR(RAX, 2, (r << 8) | q);
  } else {
    SetR(RAX, w, q);
    SetR(RDX, w, r);
  }
}

bool Cpu::StringOp(uint8_t op) {
  int w = (op & 1) ? opsize_ : 1;
  int64_t step = (F() & DF) ? -w : w;
  bool rep = pfF3_ || pfF2_;
  auto count = [&]() -> uint64_t { return adsize_ == 4 ? (s_.gpr[RCX] & 0xffffffffull) : s_.gpr[RCX]; };
  auto setcount = [&](uint64_t v) {
    if (adsize_ == 4)
      s_.gpr[RCX] = v & 0xffffffffull;
    else
      s_.gpr[RCX] = v;
  };
  auto adv = [&](int r) {
    uint64_t v = s_.gpr[r] + static_cast<uint64_t>(step);
    s_.gpr[r] = adsize_ == 4 ? (v & 0xffffffffull) : v;
  };
  auto src = [&]() -> uint64_t {
    uint64_t base = seg_ == 4 ? s_.fs_base : seg_ == 5 ? s_.gs_base : 0;
    uint64_t si = adsize_ == 4 ? (s_.gpr[RSI] & 0xffffffffull) : s_.gpr[RSI];
    return Wrap(base + si);
  };
  auto dst = [&]() -> uint64_t {
    return Wrap(adsize_ == 4 ? (s_.gpr[RDI] & 0xffffffffull) : s_.gpr[RDI]);
  };
  uint8_t base = op & 0xFE;
  bool cmp = base == 0xA6 || base == 0xAE;
  for (;;) {
    if (rep && count() == 0) break;
    switch (base) {
      case 0xA4: Wr(dst(), Rd(src(), w), w); adv(RSI); adv(RDI); break;           // movs
      case 0xA6: Sub(Rd(src(), w), Rd(dst(), w), w); adv(RSI); adv(RDI); break;  // cmps
      case 0xAA: Wr(dst(), GetR(RAX, w), w); adv(RDI); break;                    // stos
      case 0xAC: SetR(RAX, w, Rd(src(), w)); adv(RSI); break;                    // lods
      default: Sub(GetR(RAX, w), Rd(dst(), w), w); adv(RDI); break;              // scas
    }
    if (!rep) break;
    setcount(count() - 1);
    if (cmp) {
      bool zf = F() & ZF;
      if (pfF3_ && !zf) break;  // repe
      if (pfF2_ && zf) break;   // repne
    }
  }
  return Next();
}

bool Cpu::X87(uint8_t op) {
  // Control and status words: the CRT's startup and the kernel's FPU probe.
  if (s_.cr0 & kCr0Ts) Raise(kNM, "x87 with CR0.TS set");
  uint8_t m = Ib(p_);
  if (op == 0xDB && m == 0xE3) {  // fninit
    p_++;
    s_.fcw = 0x037f;
    s_.fsw = 0;
    s_.ftw = 0;
    s_.fop = 0;
    s_.fip = s_.fdp = 0;
    return Next();
  }
  if (op == 0xDB && m == 0xE2) {  // fnclex
    p_++;
    s_.fsw &= 0x7f00;
    return Next();
  }
  if (op == 0xDF && m == 0xE0) {  // fnstsw ax
    p_++;
    SetR(RAX, 2, s_.fsw);
    return Next();
  }
  if (op == 0xD9 && (m >> 6) != 3 && ((m >> 3) & 7) == 5) {  // fldcw m16
    Ea e = ModRM();
    s_.fcw = static_cast<uint16_t>(Load(e, 2));
    return Next();
  }
  if (op == 0xD9 && (m >> 6) != 3 && ((m >> 3) & 7) == 7) {  // fnstcw m16
    Ea e = ModRM();
    Store(e, 2, s_.fcw);
    return Next();
  }
  if (op == 0xDD && (m >> 6) != 3 && ((m >> 3) & 7) == 7) {  // fnstsw m16
    Ea e = ModRM();
    Store(e, 2, s_.fsw);
    return Next();
  }
  Raise(kNM, Unimplemented(ib_, p_ + 1));
}

// Runs a cached decode. Every handler here reuses the same primitives as the
// full interpreter (Alu/Shift/GetR/SetR/Push/Pop/Jump/Next), so the fast path
// cannot compute a different result than the full path for the same bytes.
bool Cpu::ExecFast(const DecEntry& d) {
  p_ = d.len;          // NextIp()/Push(NextIp()) read p_
  rexp_ = d.rexp != 0;  // AH/AL vs SPL selection in GetR/SetR
  switch (d.h) {
    case FH_ALU_RR: {
      bool to_rm = d.sub != 0;
      uint64_t a = to_rm ? GetR(d.rm, d.w) : GetR(d.reg, d.w);
      uint64_t b = to_rm ? GetR(d.reg, d.w) : GetR(d.rm, d.w);
      uint64_t r = Alu(d.kind, a, b, d.w);
      if (d.kind != 7) {
        if (to_rm) SetR(d.rm, d.w, r);
        else SetR(d.reg, d.w, r);
      }
      return Next();
    }
    case FH_ALU_AI: {
      uint64_t r = Alu(d.kind, GetR(RAX, d.w), d.imm, d.w);
      if (d.kind != 7) SetR(RAX, d.w, r);
      return Next();
    }
    case FH_GRP1_RR: {
      uint64_t r = Alu(d.kind, GetR(d.rm, d.w), d.imm, d.w);
      if (d.kind != 7) SetR(d.rm, d.w, r);
      return Next();
    }
    case FH_TEST_RR:
      Logic(GetR(d.rm, d.w) & GetR(d.reg, d.w), d.w);
      return Next();
    case FH_MOV_RR:
      if (d.sub) SetR(d.rm, d.w, GetR(d.reg, d.w));
      else SetR(d.reg, d.w, GetR(d.rm, d.w));
      return Next();
    case FH_MOV_RI:
      SetR(d.reg, d.w, d.imm);
      return Next();
    case FH_MOVI_RR:
      SetR(d.rm, d.w, d.imm);
      return Next();
    case FH_SHIFT_RR: {
      unsigned cnt = d.cnt == 1 ? static_cast<unsigned>(d.imm)
                     : d.cnt == 0 ? 1u
                                  : static_cast<unsigned>(s_.gpr[RCX] & 0xff);
      SetR(d.rm, d.w, Shift(d.kind, GetR(d.rm, d.w), cnt, d.w));
      return Next();
    }
    case FH_INCDEC_RR: {
      uint64_t cf = F() & CF;
      uint64_t a = GetR(d.rm, d.w);
      uint64_t r = d.kind == 0 ? Add(a, 1, d.w) : Sub(a, 1, d.w);
      F() = (F() & ~CF) | cf;
      SetR(d.rm, d.w, r);
      return Next();
    }
    case FH_PUSH_R:
      Push(s_.gpr[d.rm] & Mask(d.w), d.w);
      return Next();
    case FH_POP_R: {
      uint64_t v = Pop(d.w);
      SetR(d.rm, d.w == 2 ? 2 : 8, v);
      if (!s_.code64 && d.w == 4) s_.gpr[d.rm] = v & 0xffffffffull;
      return Next();
    }
    case FH_JCC:
      return Cond(d.cc) ? Jump(NextIp() + static_cast<uint64_t>(d.rel)) : Next();
    case FH_JMP:
      return Jump(NextIp() + static_cast<uint64_t>(d.rel));
    case FH_CALL: {
      uint64_t next = NextIp();
      Push(next, s_.code64 ? 8 : 4);
      return Jump(next + static_cast<uint64_t>(d.rel));
    }
    case FH_RET:
      return Jump(Pop(d.w));
    case FH_MOVX_RR: {
      uint64_t v = GetR(d.rm, d.sub);
      if (d.sign) v = static_cast<uint64_t>(Sx(v, d.sub));
      SetR(d.reg, d.w, v);
      return Next();
    }
    case FH_XCHG_A: {
      uint64_t t = GetR(d.rm, d.w);
      SetR(d.rm, d.w, GetR(RAX, d.w));
      SetR(RAX, d.w, t);
      return Next();
    }
    case FH_NOP:
      return Next();
    case FH_MOVSXD_RR:
      if (d.w == 8) SetR(d.reg, 8, static_cast<uint64_t>(Sx(GetR(d.rm, 4), 4)));
      else SetR(d.reg, d.w, GetR(d.rm, d.w));
      return Next();
    case FH_CWD:
      SetR(RDX, d.w, (GetR(RAX, d.w) & Msb(d.w)) ? Mask(d.w) : 0);
      return Next();
    case FH_CBW:
      SetR(RAX, d.w, static_cast<uint64_t>(Sx(GetR(RAX, d.w / 2), d.w / 2)));
      return Next();
    case FH_CMOV_RR: {
      uint64_t v = GetR(d.rm, d.w);
      if (Cond(d.cc)) SetR(d.reg, d.w, v);
      else if (d.w == 4) SetR(d.reg, 4, GetR(d.reg, 4));
      return Next();
    }
    case FH_SETCC_RR:
      SetR(d.rm, 1, Cond(d.cc) ? 1 : 0);
      return Next();
    case FH_FLAG:
      switch (d.sub) {
        case 0: F() ^= CF; break;
        case 1: F() &= ~CF; break;
        case 2: F() |= CF; break;
        case 3: F() &= ~DF; break;
        default: F() |= DF; break;
      }
      return Next();
    default:
      return Next();
  }
}

// Decodes a register-form or control-flow instruction into `d` (no CPU state
// change), or returns false for anything with a memory operand or outside the
// cached set, to be handled by the full decoder. The parsing mirrors StepFull.
bool Cpu::DecodeFast(DecEntry& d) {
  for (;;) {
    uint8_t b = Ib(p_);
    if (b == 0x66) {
      pf66_ = true;
      opsize_ = 2;
    } else if (b == 0x67) {
      adsize_ = s_.code64 ? 4 : 2;
    } else if (b == 0xF2) {
      pfF2_ = true;
    } else if (b == 0xF3) {
      pfF3_ = true;
    } else if (b == 0xF0 || b == 0x26 || b == 0x2E || b == 0x36 || b == 0x3E) {
      // lock / ES CS SS DS overrides: no effect on a register-form op
    } else if (b == 0x64) {
      seg_ = 4;
    } else if (b == 0x65) {
      seg_ = 5;
    } else {
      break;
    }
    p_++;
  }
  if (adsize_ == 2) return false;
  if (s_.code64 && (Ib(p_) & 0xF0) == 0x40) {
    rex_ = Ib(p_++);
    rexp_ = true;
    if (rex_ & 8) opsize_ = 8;
  }
  if (pfF2_ || pfF3_) return false;  // rep / SSE selectors: not the plain path
  uint8_t op = Ib(p_++);
  d.rexp = rexp_ ? 1 : 0;
  d.h = FH_NONE;

  auto regf = [&](uint8_t m) -> uint8_t {
    return static_cast<uint8_t>(((m >> 3) & 7) | ((rex_ & 4) ? 8 : 0));
  };
  auto rmf = [&](uint8_t m) -> uint8_t {
    return static_cast<uint8_t>((m & 7) | ((rex_ & 1) ? 8 : 0));
  };

  // ALU grid: add/or/adc/sbb/and/sub/xor/cmp
  if (op < 0x40 && (op & 7) < 6) {
    int form = op & 7;
    d.kind = static_cast<uint8_t>(op >> 3);
    d.w = static_cast<uint8_t>((form & 1) ? opsize_ : 1);
    if (form < 4) {
      uint8_t m = Ib(p_);
      if ((m >> 6) != 3) return false;
      p_++;
      d.reg = regf(m);
      d.rm = rmf(m);
      d.sub = (form & 2) ? 0 : 1;  // to_rm on forms 0/1
      d.h = FH_ALU_RR;
    } else {
      d.imm = d.w == 8 ? static_cast<uint64_t>(Simm(4)) : Imm(d.w);
      d.h = FH_ALU_AI;
    }
  } else if (op >= 0x50 && op <= 0x57) {
    d.h = FH_PUSH_R;
    d.rm = static_cast<uint8_t>((op & 7) | ((rex_ & 1) ? 8 : 0));
    d.w = static_cast<uint8_t>(StackW());
  } else if (op >= 0x58 && op <= 0x5F) {
    d.h = FH_POP_R;
    d.rm = static_cast<uint8_t>((op & 7) | ((rex_ & 1) ? 8 : 0));
    d.w = static_cast<uint8_t>(StackW());
  } else if (op >= 0x70 && op <= 0x7F) {
    d.h = FH_JCC;
    d.cc = static_cast<uint8_t>(op & 0xf);
    d.rel = Simm(1);
  } else if (op >= 0x91 && op <= 0x97) {
    d.h = FH_XCHG_A;
    d.rm = static_cast<uint8_t>((op & 7) | ((rex_ & 1) ? 8 : 0));
    d.w = static_cast<uint8_t>(opsize_);
  } else if (op >= 0xB0 && op <= 0xB7) {
    d.h = FH_MOV_RI;
    d.reg = static_cast<uint8_t>((op & 7) | ((rex_ & 1) ? 8 : 0));
    d.w = 1;
    d.imm = Imm(1);
  } else if (op >= 0xB8 && op <= 0xBF) {
    d.h = FH_MOV_RI;
    d.reg = static_cast<uint8_t>((op & 7) | ((rex_ & 1) ? 8 : 0));
    d.w = static_cast<uint8_t>(opsize_);
    d.imm = Imm(opsize_);
  } else {
    switch (op) {
      case 0x63: {  // movsxd
        if (!s_.code64) return false;
        uint8_t m = Ib(p_);
        if ((m >> 6) != 3) return false;
        p_++;
        d.reg = regf(m);
        d.rm = rmf(m);
        d.w = static_cast<uint8_t>(opsize_);
        d.h = FH_MOVSXD_RR;
        break;
      }
      case 0x80: case 0x81: case 0x83: {  // group 1
        int w = op == 0x80 ? 1 : opsize_;
        uint8_t m = Ib(p_);
        if ((m >> 6) != 3) return false;
        p_++;
        d.kind = static_cast<uint8_t>((m >> 3) & 7);
        d.rm = rmf(m);
        d.w = static_cast<uint8_t>(w);
        d.imm = op == 0x80 ? Imm(1) : static_cast<uint64_t>(Simm(op == 0x81 ? (w == 2 ? 2 : 4) : 1));
        d.h = FH_GRP1_RR;
        break;
      }
      case 0x84: case 0x85: {  // test
        int w = op == 0x84 ? 1 : opsize_;
        uint8_t m = Ib(p_);
        if ((m >> 6) != 3) return false;
        p_++;
        d.reg = regf(m);
        d.rm = rmf(m);
        d.w = static_cast<uint8_t>(w);
        d.h = FH_TEST_RR;
        break;
      }
      case 0x88: case 0x89: case 0x8A: case 0x8B: {  // mov
        uint8_t m = Ib(p_);
        if ((m >> 6) != 3) return false;
        p_++;
        d.reg = regf(m);
        d.rm = rmf(m);
        d.w = static_cast<uint8_t>((op & 1) ? opsize_ : 1);
        d.sub = op < 0x8A ? 1 : 0;  // 1: store to rm; 0: to reg
        d.h = FH_MOV_RR;
        break;
      }
      case 0x90:
        if (rex_ & 1) {
          d.h = FH_XCHG_A;
          d.rm = 8;
          d.w = static_cast<uint8_t>(opsize_);
        } else {
          d.h = FH_NOP;
        }
        break;
      case 0x98:
        d.h = FH_CBW;
        d.w = static_cast<uint8_t>(opsize_);
        break;
      case 0x99:
        d.h = FH_CWD;
        d.w = static_cast<uint8_t>(opsize_);
        break;
      case 0xC3:
        d.h = FH_RET;
        d.w = static_cast<uint8_t>(StackW());
        break;
      case 0xC6: case 0xC7: {  // mov rm, imm
        int w = op == 0xC6 ? 1 : opsize_;
        uint8_t m = Ib(p_);
        if ((m >> 6) != 3 || ((m >> 3) & 7) != 0) return false;
        p_++;
        d.rm = rmf(m);
        d.w = static_cast<uint8_t>(w);
        int iw = w == 1 ? 1 : (w == 2 ? 2 : 4);
        d.imm = w == 8 ? static_cast<uint64_t>(Simm(4)) : Imm(iw);
        d.h = FH_MOVI_RR;
        break;
      }
      case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3: {  // shifts
        int w = (op & 1) ? opsize_ : 1;
        uint8_t m = Ib(p_);
        if ((m >> 6) != 3) return false;
        p_++;
        d.kind = static_cast<uint8_t>((m >> 3) & 7);
        d.rm = rmf(m);
        d.w = static_cast<uint8_t>(w);
        if (op <= 0xC1) {
          d.cnt = 1;
          d.imm = Imm(1);
        } else if (op <= 0xD1) {
          d.cnt = 0;
        } else {
          d.cnt = 2;
        }
        d.h = FH_SHIFT_RR;
        break;
      }
      case 0xE8:
        d.h = FH_CALL;
        d.rel = Simm(4);
        break;
      case 0xE9:
        d.h = FH_JMP;
        d.rel = Simm(opsize_ == 2 ? 2 : 4);
        break;
      case 0xEB:
        d.h = FH_JMP;
        d.rel = Simm(1);
        break;
      case 0xF5:
        d.h = FH_FLAG;
        d.sub = 0;
        break;
      case 0xF8:
        d.h = FH_FLAG;
        d.sub = 1;
        break;
      case 0xF9:
        d.h = FH_FLAG;
        d.sub = 2;
        break;
      case 0xFC:
        d.h = FH_FLAG;
        d.sub = 3;
        break;
      case 0xFD:
        d.h = FH_FLAG;
        d.sub = 4;
        break;
      case 0xFE: case 0xFF: {  // group 4/5: only inc/dec reg here
        int w = op == 0xFE ? 1 : opsize_;
        uint8_t m = Ib(p_);
        if ((m >> 6) != 3) return false;
        int kind = (m >> 3) & 7;
        if (kind > 1) return false;  // call/jmp/push Ev: full path
        p_++;
        d.kind = static_cast<uint8_t>(kind);
        d.rm = rmf(m);
        d.w = static_cast<uint8_t>(w);
        d.h = FH_INCDEC_RR;
        break;
      }
      case 0x0F: {
        uint8_t op2 = Ib(p_++);
        if (op2 >= 0x80 && op2 <= 0x8F) {
          d.h = FH_JCC;
          d.cc = static_cast<uint8_t>(op2 & 0xf);
          d.rel = Simm(opsize_ == 2 ? 2 : 4);
        } else if (op2 >= 0x40 && op2 <= 0x4F) {
          uint8_t m = Ib(p_);
          if ((m >> 6) != 3) return false;
          p_++;
          d.h = FH_CMOV_RR;
          d.cc = static_cast<uint8_t>(op2 & 0xf);
          d.reg = regf(m);
          d.rm = rmf(m);
          d.w = static_cast<uint8_t>(opsize_);
        } else if (op2 >= 0x90 && op2 <= 0x9F) {
          uint8_t m = Ib(p_);
          if ((m >> 6) != 3) return false;
          p_++;
          d.h = FH_SETCC_RR;
          d.cc = static_cast<uint8_t>(op2 & 0xf);
          d.rm = rmf(m);
        } else if (op2 == 0xB6 || op2 == 0xB7 || op2 == 0xBE || op2 == 0xBF) {
          uint8_t m = Ib(p_);
          if ((m >> 6) != 3) return false;
          p_++;
          d.h = FH_MOVX_RR;
          d.reg = regf(m);
          d.rm = rmf(m);
          d.sub = (op2 == 0xB6 || op2 == 0xBE) ? 1 : 2;
          d.sign = op2 >= 0xBE ? 1 : 0;
          d.w = static_cast<uint8_t>(opsize_);
        } else {
          return false;
        }
        break;
      }
      default:
        return false;
    }
  }
  if (d.h == FH_NONE) return false;
  if (p_ > 15) return false;
  d.len = static_cast<uint8_t>(p_);
  return true;
}

// Resets the per-instruction decode state (not ip0_/ib_/nib_, which Step
// sets once per instruction).
void Cpu::ResetDecode() {
  p_ = 0;
  rex_ = 0;
  rexp_ = false;
  opsize_ = 4;
  adsize_ = s_.code64 ? 8 : 4;
  seg_ = -1;
  pf66_ = pfF2_ = pfF3_ = false;
}

bool Cpu::Step() {
  ip0_ = s_.rip;
  nib_ = 0;
  // Prefetch this instruction's bytes from the current code page in one go,
  // so decoding reads ib_ instead of walking the TLB a byte at a time. Only
  // the current page is read (never faulting on an unmapped next page for a
  // short instruction); a byte past it is still fetched lazily by Ib for the
  // rare page-crossing instruction. A fault here is the same #PF the first
  // byte fetch would raise, at the same RIP.
  {
    uint64_t chunk = 0x1000 - (ip0_ & 0xfff);
    if (chunk > 15) chunk = 15;
    Access(ip0_, ib_, chunk, false, true);
    nib_ = static_cast<int>(chunk);
  }
  // A cached decode for this RIP, still matching the code bytes and mode.
  uint8_t mode = s_.code64 ? 1 : 0;
  DecEntry& e = g_dec[DecHash(ip0_)];
  if (e.valid && e.tag == ip0_ && e.mode == mode && e.len && e.len <= nib_ &&
      std::memcmp(e.b, ib_, e.len) == 0) {
    return ExecFast(e);
  }
  // Decode afresh; cache it when it is a register-form or control-flow op.
  ResetDecode();
  DecEntry nd;
  if (DecodeFast(nd)) {
    nd.tag = ip0_;
    nd.mode = mode;
    nd.valid = 1;
    for (int i = 0; i < nd.len && i < 15; ++i) nd.b[i] = ib_[i];
    e = nd;
    return ExecFast(e);
  }
  ResetDecode();
  return StepFull();
}

bool Cpu::StepFull() {
  for (;;) {
    uint8_t b = Ib(p_);
    if (b == 0x66) {
      pf66_ = true;
      opsize_ = 2;
    } else if (b == 0x67) {
      adsize_ = s_.code64 ? 4 : 2;
    } else if (b == 0xF2) {
      pfF2_ = true;
      pfF3_ = false;
    } else if (b == 0xF3) {
      pfF3_ = true;
      pfF2_ = false;
    } else if (b == 0xF0) {
      // lock: one processor, every access is already atomic
    } else if (b == 0x64) {
      seg_ = 4;
    } else if (b == 0x65) {
      seg_ = 5;
    } else if (b == 0x26 || b == 0x2E || b == 0x36 || b == 0x3E) {
      // ES/CS/SS/DS: flat, base 0
    } else {
      break;
    }
    p_++;
  }
  if (adsize_ == 2) Raise(kUD, "16-bit addressing is not modeled");
  if (s_.code64 && (Ib(p_) & 0xF0) == 0x40) {
    rex_ = Ib(p_++);
    rexp_ = true;
    if (rex_ & 8) opsize_ = 8;
  }
  uint8_t op = Ib(p_++);

  // ALU grid: add or adc sbb and sub xor cmp
  if (op < 0x40 && (op & 7) < 6) {
    int kind = op >> 3, form = op & 7;
    int w = (form & 1) ? opsize_ : 1;
    if (form < 4) {
      Ea e = ModRM();
      bool to_rm = !(form & 2);
      uint64_t a = to_rm ? Load(e, w) : GetR(reg_, w);
      uint64_t b = to_rm ? GetR(reg_, w) : Load(e, w);
      uint64_t r = Alu(kind, a, b, w);
      if (kind != 7) {
        if (to_rm)
          Store(e, w, r);
        else
          SetR(reg_, w, r);
      }
    } else {
      uint64_t imm = w == 8 ? static_cast<uint64_t>(Simm(4)) : Imm(w);
      uint64_t r = Alu(kind, GetR(RAX, w), imm, w);
      if (kind != 7) SetR(RAX, w, r);
    }
    return Next();
  }
  if (op >= 0x40 && op <= 0x4F) {  // inc/dec r (32-bit code; REX in 64-bit)
    int r = op & 7, w = opsize_;
    uint64_t cf = F() & CF;
    uint64_t v = op < 0x48 ? Add(GetR(r, w), 1, w) : Sub(GetR(r, w), 1, w);
    F() = (F() & ~CF) | cf;
    SetR(r, w, v);
    return Next();
  }
  if (op >= 0x50 && op <= 0x57) {
    int r = (op & 7) | ((rex_ & 1) ? 8 : 0);
    int w = StackW();
    Push(s_.gpr[r] & Mask(w), w);
    return Next();
  }
  if (op >= 0x58 && op <= 0x5F) {
    int r = (op & 7) | ((rex_ & 1) ? 8 : 0);
    int w = StackW();
    uint64_t v = Pop(w);
    SetR(r, w == 2 ? 2 : 8, v);
    if (!s_.code64 && w == 4) s_.gpr[r] = v & 0xffffffffull;
    return Next();
  }
  switch (op) {
    case 0x63: {  // movsxd (64-bit code)
      if (!s_.code64) Ud();
      Ea e = ModRM();
      if (opsize_ == 8)
        SetR(reg_, 8, static_cast<uint64_t>(Sx(Load(e, 4), 4)));
      else
        SetR(reg_, opsize_, Load(e, opsize_));
      return Next();
    }
    case 0x68:
    case 0x6A: {  // push imm
      int w = StackW();
      int64_t v = op == 0x6A ? Simm(1) : Simm(w == 2 ? 2 : 4);
      Push(static_cast<uint64_t>(v) & Mask(w), w);
      return Next();
    }
    case 0x69:
    case 0x6B: {  // imul Gv, Ev, imm
      Ea e = ModRM();
      int w = opsize_;
      e.imm = op == 0x6B ? 1 : (w == 2 ? 2 : 4);
      int64_t a = Sx(Load(e, w), w);
      int64_t b = Simm(e.imm);
      __int128 p = static_cast<__int128>(a) * b;
      uint64_t lo = static_cast<uint64_t>(p) & Mask(w);
      SetR(reg_, w, lo);
      F() &= ~(CF | OF);
      if (p != static_cast<__int128>(Sx(lo, w))) F() |= CF | OF;
      SetSZP(lo, w);
      return Next();
    }
    case 0x80:
    case 0x81:
    case 0x83: {  // group 1
      int w = op == 0x80 ? 1 : opsize_;
      Ea e = ModRM();
      int kind = reg_ & 7;
      e.imm = op == 0x81 ? (w == 2 ? 2 : 4) : 1;
      uint64_t a = Load(e, w);
      uint64_t imm = op == 0x80 ? Imm(1) : static_cast<uint64_t>(Simm(e.imm));
      uint64_t r = Alu(kind, a, imm, w);
      if (kind != 7) Store(e, w, r);
      return Next();
    }
    case 0x84:
    case 0x85: {
      int w = op == 0x84 ? 1 : opsize_;
      Ea e = ModRM();
      Logic(Load(e, w) & GetR(reg_, w), w);
      return Next();
    }
    case 0x86:
    case 0x87: {  // xchg
      int w = op == 0x86 ? 1 : opsize_;
      Ea e = ModRM();
      uint64_t a = Load(e, w), b = GetR(reg_, w);
      Store(e, w, b);
      SetR(reg_, w, a);
      return Next();
    }
    case 0x88:
    case 0x89: {
      int w = op == 0x88 ? 1 : opsize_;
      Ea e = ModRM();
      Store(e, w, GetR(reg_, w));
      return Next();
    }
    case 0x8A:
    case 0x8B: {
      int w = op == 0x8A ? 1 : opsize_;
      Ea e = ModRM();
      SetR(reg_, w, Load(e, w));
      return Next();
    }
    case 0x8C: {  // mov Ev, Sreg
      Ea e = ModRM();
      int sr = reg_ & 7;
      if (sr > 5) Ud();
      const uint16_t sels[6] = {s_.es, s_.cs, s_.ss, s_.ds, s_.fs, s_.gs};
      Store(e, e.mem ? 2 : opsize_, sels[sr]);
      return Next();
    }
    case 0x8E: {  // mov Sreg, Ew
      Ea e = ModRM();
      int sr = reg_ & 7;
      if (sr == 1) Ud();
      LoadSeg(sr, static_cast<uint16_t>(Load(e, 2)));
      return Next();
    }
    case 0x8D: {  // lea
      Ea e = ModRM();
      if (!e.mem) Ud();
      uint64_t off = e.off;
      if (e.riprel) off += ip0_ + static_cast<uint64_t>(e.end);
      if (adsize_ == 4) off &= 0xffffffffull;
      SetR(reg_, opsize_, off);
      return Next();
    }
    case 0x8F: {  // pop Ev
      int w = StackW();
      uint64_t v = Pop(w);
      Ea e = ModRM();
      if ((reg_ & 7) != 0) Ud();
      Store(e, w, v);
      return Next();
    }
    case 0x90:
      if (rex_ & 1) {  // xchg r8, rax
        uint64_t t = GetR(8, opsize_);
        SetR(8, opsize_, GetR(RAX, opsize_));
        SetR(RAX, opsize_, t);
      }
      return Next();  // nop / pause
    case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: {
      int r = (op & 7) | ((rex_ & 1) ? 8 : 0);
      uint64_t t = GetR(r, opsize_);
      SetR(r, opsize_, GetR(RAX, opsize_));
      SetR(RAX, opsize_, t);
      return Next();
    }
    case 0x98:  // cbw / cwde / cdqe
      SetR(RAX, opsize_, static_cast<uint64_t>(Sx(GetR(RAX, opsize_ / 2), opsize_ / 2)));
      return Next();
    case 0x99:  // cwd / cdq / cqo
      SetR(RDX, opsize_, (GetR(RAX, opsize_) & Msb(opsize_)) ? Mask(opsize_) : 0);
      return Next();
    case 0x9B:  // fwait
      return Next();
    case 0x9C: {  // pushf
      int w = StackW();
      Push(F() & 0x00FCFFFFull & Mask(w), w);
      return Next();
    }
    case 0x9D: {  // popf
      int w = StackW();
      uint64_t v = Pop(w);
      uint64_t mask = kPopfMask & Mask(w);
      if (Cpl() > 0) mask &= ~IOPL;
      if (Cpl() > Iopl()) mask &= ~IF;
      F() = (F() & ~mask) | (v & mask) | 2;
      return Next();
    }
    case 0x9E:  // sahf
      F() = (F() & ~0xD5ull) | (GetR(RAX, 2) >> 8 & 0xD5) | 2;
      return Next();
    case 0x9F:  // lahf
      s_.gpr[RAX] = (s_.gpr[RAX] & ~0xff00ull) | ((F() & 0xD7) | 2) << 8;
      return Next();
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: {  // mov moffs
      int w = (op & 1) ? opsize_ : 1;
      uint64_t off = Imm(adsize_);
      uint64_t base = seg_ == 4 ? s_.fs_base : seg_ == 5 ? s_.gs_base : 0;
      uint64_t la = Wrap(base + off);
      if (op < 0xA2)
        SetR(RAX, w, Rd(la, w));
      else
        Wr(la, GetR(RAX, w), w);
      return Next();
    }
    case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xAA: case 0xAB:
    case 0xAC: case 0xAD: case 0xAE: case 0xAF:
      return StringOp(op);
    case 0xA8:
      Logic(GetR(RAX, 1) & Imm(1), 1);
      return Next();
    case 0xA9: {
      int w = opsize_;
      uint64_t imm = w == 8 ? static_cast<uint64_t>(Simm(4)) : Imm(w);
      Logic(GetR(RAX, w) & imm, w);
      return Next();
    }
    case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3: {
      int w = (op & 1) ? opsize_ : 1;
      Ea e = ModRM();
      unsigned cnt;
      if (op <= 0xC1) {
        e.imm = 1;
        uint64_t a = Load(e, w);
        cnt = static_cast<unsigned>(Imm(1));
        Store(e, w, Shift(reg_ & 7, a, cnt, w));
        return Next();
      }
      cnt = op <= 0xD1 ? 1u : static_cast<unsigned>(s_.gpr[RCX] & 0xff);
      Store(e, w, Shift(reg_ & 7, Load(e, w), cnt, w));
      return Next();
    }
    case 0xC2: {  // ret imm16
      uint64_t n = Imm(2);
      uint64_t t = Pop(StackW());
      s_.gpr[RSP] = Wrap(s_.gpr[RSP] + n);
      return Jump(t);
    }
    case 0xC3:
      return Jump(Pop(StackW()));
    case 0xC6:
    case 0xC7: {
      int w = op == 0xC6 ? 1 : opsize_;
      Ea e = ModRM();
      if ((reg_ & 7) != 0) Ud();
      e.imm = w == 1 ? 1 : (w == 2 ? 2 : 4);
      uint64_t imm = w == 8 ? static_cast<uint64_t>(Simm(4)) : Imm(e.imm);
      Store(e, w, imm);
      return Next();
    }
    case 0xC8: {  // enter imm16, 0
      uint64_t size = Imm(2);
      if (Imm(1) != 0) Ud();
      int w = StackW();
      Push(s_.gpr[RBP] & Mask(w), w);
      s_.gpr[RBP] = s_.gpr[RSP];
      s_.gpr[RSP] = Wrap(s_.gpr[RSP] - size);
      return Next();
    }
    case 0xC9: {  // leave
      s_.gpr[RSP] = s_.gpr[RBP];
      int w = StackW();
      SetR(RBP, w == 2 ? 2 : 8, Pop(w));
      return Next();
    }
    case 0xCA:
    case 0xCB:
      return FarRet(op == 0xCA);
    case 0xCC:
      Trap(kBP, "int3");
    case 0xCD: {
      uint8_t v = static_cast<uint8_t>(Imm(1));
      Trap(v, "int n");
    }
    case 0xCF:
      return Iret();
    case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
      return X87(op);
    case 0xE0: case 0xE1: case 0xE2: {  // loopne / loope / loop
      int64_t rel = Simm(1);
      uint64_t c = (adsize_ == 4 ? (s_.gpr[RCX] - 1) & 0xffffffffull : s_.gpr[RCX] - 1);
      s_.gpr[RCX] = adsize_ == 4 ? c : c;
      bool go = c != 0;
      if (op == 0xE0) go = go && !(F() & ZF);
      if (op == 0xE1) go = go && (F() & ZF);
      return go ? Jump(NextIp() + static_cast<uint64_t>(rel)) : Next();
    }
    case 0xE3: {  // jrcxz
      int64_t rel = Simm(1);
      uint64_t c = adsize_ == 4 ? (s_.gpr[RCX] & 0xffffffffull) : s_.gpr[RCX];
      return c == 0 ? Jump(NextIp() + static_cast<uint64_t>(rel)) : Next();
    }
    case 0xE4: case 0xE5: case 0xE6: case 0xE7: case 0xEC: case 0xED: case 0xEE: case 0xEF: {
      IoPriv();
      info_.port = op >= 0xEC ? static_cast<uint16_t>(s_.gpr[RDX] & 0xffff)
                              : static_cast<uint16_t>(Imm(1));
      info_.size = static_cast<uint8_t>((op & 1) ? (opsize_ == 2 ? 2 : 4) : 1);
      info_.write = (op & 2) != 0;
      return ExitHere(Exit::kIo);
    }
    case 0xE8: {  // call rel32
      int64_t rel = Simm(4);
      uint64_t next = NextIp();
      Push(next, s_.code64 ? 8 : 4);
      return Jump(next + static_cast<uint64_t>(rel));
    }
    case 0xE9: {
      int64_t rel = Simm(opsize_ == 2 ? 2 : 4);
      return Jump(NextIp() + static_cast<uint64_t>(rel));
    }
    case 0xEB: {
      int64_t rel = Simm(1);
      return Jump(NextIp() + static_cast<uint64_t>(rel));
    }
    case 0xF4:
      Priv();
      return ExitHere(Exit::kHalt);
    case 0xF5: F() ^= CF; return Next();
    case 0xF8: F() &= ~CF; return Next();
    case 0xF9: F() |= CF; return Next();
    case 0xFA:
      IoPriv();
      F() &= ~IF;
      return Next();
    case 0xFB:
      IoPriv();
      if (!(F() & IF)) s_.int_shadow = true;
      F() |= IF;
      return Next();
    case 0xFC: F() &= ~DF; return Next();
    case 0xFD: F() |= DF; return Next();
    case 0xF6:
    case 0xF7: {  // group 3
      int w = op == 0xF6 ? 1 : opsize_;
      Ea e = ModRM();
      int kind = reg_ & 7;
      if (kind <= 1) {  // test imm
        e.imm = w == 1 ? 1 : (w == 2 ? 2 : 4);
        uint64_t a = Load(e, w);
        uint64_t imm = w == 8 ? static_cast<uint64_t>(Simm(4)) : Imm(e.imm);
        Logic(a & imm, w);
      } else if (kind == 2) {
        Store(e, w, ~Load(e, w) & Mask(w));
      } else if (kind == 3) {
        uint64_t a = Load(e, w);
        uint64_t r = Sub(0, a, w);
        Store(e, w, r);
      } else {
        MulDiv(kind, e, w);
      }
      return Next();
    }
    case 0xFE:
    case 0xFF: {  // groups 4 and 5
      int w = op == 0xFE ? 1 : opsize_;
      Ea e = ModRM();
      int kind = reg_ & 7;
      if (kind == 0 || kind == 1) {
        uint64_t cf = F() & CF;
        uint64_t a = Load(e, w);
        uint64_t r = kind == 0 ? Add(a, 1, w) : Sub(a, 1, w);
        F() = (F() & ~CF) | cf;
        Store(e, w, r);
        return Next();
      }
      if (op == 0xFE) Ud();
      int sw = s_.code64 ? 8 : 4;
      if (kind == 2) {  // call Ev
        uint64_t t = Load(e, sw);
        Push(NextIp(), sw);
        return Jump(t);
      }
      if (kind == 4) return Jump(Load(e, sw));  // jmp Ev
      if (kind == 6) {                          // push Ev
        int pw = StackW();
        Push(Load(e, pw), pw);
        return Next();
      }
      Ud();
    }
    case 0x0F:
      return TwoByte();
    default:
      break;
  }
  if (op >= 0x70 && op <= 0x7F) {  // jcc rel8
    int64_t rel = Simm(1);
    return Cond(op & 0xf) ? Jump(NextIp() + static_cast<uint64_t>(rel)) : Next();
  }
  if (op >= 0xB0 && op <= 0xB7) {
    int r = (op & 7) | ((rex_ & 1) ? 8 : 0);
    SetR(r, 1, Imm(1));
    return Next();
  }
  if (op >= 0xB8 && op <= 0xBF) {
    int r = (op & 7) | ((rex_ & 1) ? 8 : 0);
    SetR(r, opsize_, Imm(opsize_));
    return Next();
  }
  Ud();
}

bool Cpu::TwoByte() {
  uint8_t op2 = Ib(p_++);
  switch (op2) {
    case 0x00:
      return Group0F00();
    case 0x01:
      return Group0F01();
    case 0x05: {  // syscall
      if (!(s_.efer & kEferSce) || !s_.code64) Raise(kUD, "syscall: EFER.SCE clear");
      s_.gpr[RCX] = NextIp();
      s_.gpr[R11] = F() & ~RF;
      F() = (F() & ~(s_.sfmask | RF)) | 2;
      s_.cs = static_cast<uint16_t>((s_.star >> 32) & 0xfffc);
      s_.ss = static_cast<uint16_t>(((s_.star >> 32) + 8) & 0xfffc);
      s_.code64 = true;
      return Jump(s_.lstar);
    }
    case 0x06:  // clts
      Priv();
      s_.cr0 &= ~kCr0Ts;
      return Next();
    case 0x07: {  // sysret
      if (!(s_.efer & kEferSce) || !s_.code64) Raise(kUD, "sysret: EFER.SCE clear");
      Priv();
      uint16_t base = static_cast<uint16_t>(s_.star >> 48);
      F() = (s_.gpr[R11] & 0x3C7FD7ull) | 2;
      s_.ss = static_cast<uint16_t>(((base + 8) & 0xfffc) | 3);
      if (opsize_ == 8) {
        s_.cs = static_cast<uint16_t>(((base + 16) & 0xfffc) | 3);
        s_.code64 = true;
        return Jump(s_.gpr[RCX]);
      }
      s_.cs = static_cast<uint16_t>((base & 0xfffc) | 3);
      s_.code64 = false;
      return Jump(s_.gpr[RCX] & 0xffffffffull);
    }
    case 0x08:
    case 0x09:  // invd / wbinvd: no caches to write back
      Priv();
      return Next();
    case 0x0B:
      Raise(kUD, "ud2");
    case 0xB9:
    case 0xFF:  // ud1 / ud0: the guest's BUG/WARN traps decode the operand themselves
      Raise(kUD, op2 == 0xB9 ? "ud1" : "ud0");
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
      return MovCrDr(op2);
    case 0x77:  // emms
      return Next();
    case 0xA0:
    case 0xA8: {  // push fs / push gs
      int w = StackW();
      Push(op2 == 0xA0 ? s_.fs : s_.gs, w);
      return Next();
    }
    case 0xA1:
    case 0xA9: {  // pop fs / pop gs
      int w = StackW();
      LoadSeg(op2 == 0xA1 ? 4 : 5, static_cast<uint16_t>(Pop(w)));
      return Next();
    }
    case 0xC3: {  // movnti
      Ea e = ModRM();
      if (!e.mem) Ud();
      Store(e, opsize_ == 8 ? 8 : 4, GetR(reg_, opsize_ == 8 ? 8 : 4));
      return Next();
    }
    case 0xC7: {  // cmpxchg8b / cmpxchg16b
      Ea e = ModRM();
      if (!e.mem || (reg_ & 7) != 1) Ud();
      if (opsize_ == 8) {
        uint64_t a = Addr(e);
        if (a & 15) Raise(kGP, "cmpxchg16b misaligned", a, true, 0);
        Xmm cur;
        Rd128(a, &cur);
        if (cur.lo == s_.gpr[RAX] && cur.hi == s_.gpr[RDX]) {
          Xmm nv{s_.gpr[RBX], s_.gpr[RCX]};
          Wr128(a, nv);
          F() |= ZF;
        } else {
          s_.gpr[RAX] = cur.lo;
          s_.gpr[RDX] = cur.hi;
          F() &= ~ZF;
        }
        return Next();
      }
      uint64_t cur = Load(e, 8);
      uint64_t want = (GetR(RDX, 4) << 32) | GetR(RAX, 4);
      if (cur == want) {
        Store(e, 8, (GetR(RCX, 4) << 32) | GetR(RBX, 4));
        F() |= ZF;
      } else {
        SetR(RAX, 4, cur & 0xffffffffull);
        SetR(RDX, 4, cur >> 32);
        F() &= ~ZF;
      }
      return Next();
    }
    case 0x0D:
    case 0x18:
    case 0x1E:
    case 0x1F: {  // prefetch, endbr, nop Ev
      ModRM();
      return Next();
    }
    case 0x30:
    case 0x32:
      return Msr(op2 == 0x30);
    case 0x31:
      if (s_.tsc_per_insn) {
        SetR(RAX, 4, s_.tsc & 0xffffffffull);
        SetR(RDX, 4, s_.tsc >> 32);
        return Next();
      }
      return ExitHere(Exit::kRdtsc);
    case 0xA2:
      return ExitHere(Exit::kCpuid);
    case 0xA3: case 0xAB: case 0xB3: case 0xBB: case 0xBA: {  // bt bts btr btc
      Ea e = ModRM();
      int w = opsize_;
      int kind;
      int64_t bit;
      if (op2 == 0xBA) {
        kind = reg_ & 7;
        if (kind < 4) Ud();
        e.imm = 1;
        bit = static_cast<int64_t>(Imm(1) & static_cast<uint64_t>(w * 8 - 1));
      } else {
        kind = op2 == 0xA3 ? 4 : op2 == 0xAB ? 5 : op2 == 0xB3 ? 6 : 7;
        bit = Sx(GetR(reg_, w), w);
      }
      if (e.mem && op2 != 0xBA) {
        // A register bit offset reaches past the operand, in operand units.
        int64_t bits = w * 8;
        int64_t unit = bit >= 0 ? bit / bits : -((-bit + bits - 1) / bits);
        e.off += static_cast<uint64_t>(unit * w);
        bit -= unit * bits;
      } else {
        bit &= w * 8 - 1;
      }
      uint64_t v = Load(e, w);
      bool was = (v >> bit) & 1;
      F() = (F() & ~CF) | (was ? CF : 0);
      if (kind == 5) Store(e, w, v | (1ull << bit));
      if (kind == 6) Store(e, w, v & ~(1ull << bit));
      if (kind == 7) Store(e, w, v ^ (1ull << bit));
      return Next();
    }
    case 0xA4: case 0xA5: case 0xAC: case 0xAD: {  // shld / shrd
      Ea e = ModRM();
      int w = opsize_;
      bool imm = op2 == 0xA4 || op2 == 0xAC;
      if (imm) e.imm = 1;
      uint64_t dst = Load(e, w), src = GetR(reg_, w);
      unsigned cnt = imm ? static_cast<unsigned>(Imm(1)) : static_cast<unsigned>(s_.gpr[RCX] & 0xff);
      cnt &= w == 8 ? 63 : 31;
      if (cnt) {
        int bits = w * 8;
        uint64_t r;
        bool cf;
        if (op2 <= 0xA5) {
          r = ((dst << cnt) | (cnt < static_cast<unsigned>(bits) ? src >> (bits - cnt) : 0)) & Mask(w);
          cf = (dst >> (bits - cnt)) & 1;
        } else {
          r = ((dst >> cnt) | (cnt < static_cast<unsigned>(bits) ? src << (bits - cnt) : 0)) & Mask(w);
          cf = (dst >> (cnt - 1)) & 1;
        }
        F() &= ~(CF | OF);
        if (cf) F() |= CF;
        if ((r ^ dst) & Msb(w)) F() |= OF;
        SetSZP(r, w);
        Store(e, w, r);
      }
      return Next();
    }
    case 0xAE: {
      uint8_t m = Ib(p_);
      if ((m >> 6) == 3) {  // lfence / mfence / sfence
        p_++;
        return Next();
      }
      Ea e = ModRM();
      int k = reg_ & 7;
      if (k == 0 || k == 1) return FxSave(e, k == 0);
      if (k == 7 || k == 6) return Next();  // clflush / clwb: no caches
      if (k == 2) {  // ldmxcsr
        s_.mxcsr = static_cast<uint32_t>(Load(e, 4));
        return Next();
      }
      if (k == 3) {  // stmxcsr
        Store(e, 4, s_.mxcsr);
        return Next();
      }
      Ud();
    }
    case 0xAF: {  // imul Gv, Ev
      Ea e = ModRM();
      int w = opsize_;
      __int128 p = static_cast<__int128>(Sx(GetR(reg_, w), w)) * Sx(Load(e, w), w);
      uint64_t lo = static_cast<uint64_t>(p) & Mask(w);
      SetR(reg_, w, lo);
      F() &= ~(CF | OF);
      if (p != static_cast<__int128>(Sx(lo, w))) F() |= CF | OF;
      SetSZP(lo, w);
      return Next();
    }
    case 0xB0:
    case 0xB1: {  // cmpxchg
      int w = op2 == 0xB0 ? 1 : opsize_;
      Ea e = ModRM();
      uint64_t d = Load(e, w);
      Sub(GetR(RAX, w), d, w);
      if (F() & ZF)
        Store(e, w, GetR(reg_, w));
      else
        SetR(RAX, w, d);
      return Next();
    }
    case 0xB6: case 0xB7: case 0xBE: case 0xBF: {
      int sw = (op2 == 0xB6 || op2 == 0xBE) ? 1 : 2;
      Ea e = ModRM();
      uint64_t v = Load(e, sw);
      if (op2 >= 0xBE) v = static_cast<uint64_t>(Sx(v, sw));
      SetR(reg_, opsize_, v);
      return Next();
    }
    case 0xBC:
    case 0xBD: {  // bsf / bsr, tzcnt / lzcnt
      Ea e = ModRM();
      int w = opsize_;
      uint64_t v = Load(e, w);
      int bits = w * 8;
      if (pfF3_) {
        int n = 0;
        if (op2 == 0xBC) {
          while (n < bits && !((v >> n) & 1)) ++n;
        } else {
          while (n < bits && !((v >> (bits - 1 - n)) & 1)) ++n;
        }
        F() &= ~(CF | ZF);
        if (v == 0) F() |= CF;
        if (n == 0) F() |= ZF;
        SetR(reg_, w, static_cast<uint64_t>(n));
        return Next();
      }
      if (v == 0) {
        F() |= ZF;
        return Next();
      }
      F() &= ~ZF;
      int n = 0;
      if (op2 == 0xBC) {
        while (!((v >> n) & 1)) ++n;
      } else {
        n = bits - 1;
        while (!((v >> n) & 1)) --n;
      }
      SetR(reg_, w, static_cast<uint64_t>(n));
      return Next();
    }
    case 0xC0:
    case 0xC1: {  // xadd
      int w = op2 == 0xC0 ? 1 : opsize_;
      Ea e = ModRM();
      uint64_t d = Load(e, w), s = GetR(reg_, w);
      uint64_t r = Add(d, s, w);
      Store(e, w, r);
      SetR(reg_, w, d);
      return Next();
    }
    default:
      break;
  }
  if (op2 >= 0x40 && op2 <= 0x4F) {  // cmovcc
    Ea e = ModRM();
    uint64_t v = Load(e, opsize_);
    if (Cond(op2 & 0xf))
      SetR(reg_, opsize_, v);
    else if (opsize_ == 4)
      SetR(reg_, 4, GetR(reg_, 4));  // a 32-bit cmov clears the upper half either way
    return Next();
  }
  if (op2 >= 0x80 && op2 <= 0x8F) {
    int64_t rel = Simm(opsize_ == 2 ? 2 : 4);
    return Cond(op2 & 0xf) ? Jump(NextIp() + static_cast<uint64_t>(rel)) : Next();
  }
  if (op2 >= 0x90 && op2 <= 0x9F) {
    Ea e = ModRM();
    Store(e, 1, Cond(op2 & 0xf) ? 1 : 0);
    return Next();
  }
  if (op2 >= 0xC8 && op2 <= 0xCF) {  // bswap
    int r = (op2 & 7) | ((rex_ & 1) ? 8 : 0);
    if (opsize_ == 8) {
      s_.gpr[r] = __builtin_bswap64(s_.gpr[r]);
    } else {
      SetR(r, 4, __builtin_bswap32(static_cast<uint32_t>(s_.gpr[r])));
    }
    return Next();
  }
  return Sse(op2);
}

// SSE integer and move forms the CRT and compiled code use. Lanes are
// little-endian bytes of the 128-bit register.
bool Cpu::Sse(uint8_t op2) {
  auto bytes = [](const Xmm& x, uint8_t* b) {
    std::memcpy(b, &x.lo, 8);
    std::memcpy(b + 8, &x.hi, 8);
  };
  auto pack = [](const uint8_t* b) {
    Xmm x;
    std::memcpy(&x.lo, b, 8);
    std::memcpy(&x.hi, b + 8, 8);
    return x;
  };
  Ea e = ModRM();
  int xr = reg_;
  Xmm& d = s_.xmm[xr];
  switch (op2) {
    case 0x10:
    case 0x11: {  // movups/movupd, movss, movsd
      bool store = op2 == 0x11;
      if (pfF3_ || pfF2_) {
        int w = pfF3_ ? 4 : 8;
        if (store) {
          if (e.mem) {
            Wr(Addr(e), d.lo & Mask(w), w);
          } else {
            Xmm& t = s_.xmm[e.reg];
            t.lo = (t.lo & ~Mask(w)) | (d.lo & Mask(w));
          }
        } else if (e.mem) {
          d.lo = Rd(Addr(e), w);
          d.hi = 0;
        } else {
          d.lo = (d.lo & ~Mask(w)) | (s_.xmm[e.reg].lo & Mask(w));
        }
        return Next();
      }
      if (store) {
        if (e.mem)
          Wr128(Addr(e), d);
        else
          s_.xmm[e.reg] = d;
      } else {
        d = LoadX(e);
      }
      return Next();
    }
    case 0x12:
    case 0x13: {  // movlps/movlpd, movhlps
      if (op2 == 0x13) {
        if (!e.mem) Ud();
        Wr(Addr(e), d.lo, 8);
      } else if (e.mem) {
        d.lo = Rd(Addr(e), 8);
      } else {
        d.lo = s_.xmm[e.reg].hi;
      }
      return Next();
    }
    case 0x14:
    case 0x15: {  // unpcklps/pd, unpckhps/pd
      Xmm s = LoadX(e);
      if (pf66_) {
        Xmm r;
        r.lo = op2 == 0x14 ? d.lo : d.hi;
        r.hi = op2 == 0x14 ? s.lo : s.hi;
        d = r;
      } else {
        uint64_t a = op2 == 0x14 ? d.lo : d.hi, b = op2 == 0x14 ? s.lo : s.hi;
        Xmm r;
        r.lo = (a & 0xffffffffull) | (b << 32);
        r.hi = (a >> 32) | (b & 0xffffffff00000000ull);
        d = r;
      }
      return Next();
    }
    case 0x16:
    case 0x17: {  // movhps/movhpd, movlhps
      if (op2 == 0x17) {
        if (!e.mem) Ud();
        Wr(Addr(e), d.hi, 8);
      } else if (e.mem) {
        d.hi = Rd(Addr(e), 8);
      } else {
        d.hi = s_.xmm[e.reg].lo;
      }
      return Next();
    }
    case 0x28:
      d = LoadXAligned(e);
      return Next();
    case 0x29:
    case 0x2B:
      if (e.mem) {
        if (Addr(e) & 15) Raise(kGP, "misaligned 16-byte store", Addr(e), true, 0);
        Wr128(Addr(e), d);
      } else {
        s_.xmm[e.reg] = d;
      }
      return Next();
    case 0x54: case 0x55: case 0x56: case 0x57:
    case 0xDB: case 0xDF: case 0xEB: case 0xEF: {  // and andn or xor (ps/pd/p)
      if (op2 >= 0xD0 && !pf66_) Ud();
      Xmm s = LoadX(e);
      if (op2 == 0x54 || op2 == 0xDB) { d.lo &= s.lo; d.hi &= s.hi; }
      if (op2 == 0x55 || op2 == 0xDF) { d.lo = ~d.lo & s.lo; d.hi = ~d.hi & s.hi; }
      if (op2 == 0x56 || op2 == 0xEB) { d.lo |= s.lo; d.hi |= s.hi; }
      if (op2 == 0x57 || op2 == 0xEF) { d.lo ^= s.lo; d.hi ^= s.hi; }
      return Next();
    }
    case 0xC6: {  // shufps / shufpd
      e.imm = 1;
      Xmm s = LoadX(e);
      unsigned imm = static_cast<unsigned>(Imm(1));
      if (pf66_) {
        Xmm r;
        r.lo = (imm & 1) ? d.hi : d.lo;
        r.hi = (imm & 2) ? s.hi : s.lo;
        d = r;
      } else {
        uint32_t a[4], b[4], r[4];
        std::memcpy(a, &d, 16);
        std::memcpy(b, &s, 16);
        r[0] = a[imm & 3];
        r[1] = a[(imm >> 2) & 3];
        r[2] = b[(imm >> 4) & 3];
        r[3] = b[(imm >> 6) & 3];
        std::memcpy(&d, r, 16);
      }
      return Next();
    }
    default:
      break;
  }
  if (!pf66_ && !pfF3_ && !pfF2_) Ud();  // MMX forms are not modeled
  switch (op2) {
    case 0x60: case 0x61: case 0x62: case 0x6C:
    case 0x68: case 0x69: case 0x6A: case 0x6D: {  // punpckl/h bw wd dq qdq
      if (!pf66_) Ud();
      Xmm s = LoadX(e);
      uint8_t a[16], b[16], r[16];
      bytes(d, a);
      bytes(s, b);
      bool high = (op2 >= 0x68 && op2 <= 0x6A) || op2 == 0x6D;
      int size = (op2 & 3) == 0 ? 1 : (op2 & 3) == 1 ? 2 : (op2 & 3) == 2 ? 4 : 8;
      if (op2 == 0x6C || op2 == 0x6D) size = 8;
      int from = high ? 8 : 0;
      for (int i = 0, o = 0; o < 16; i += size) {
        std::memcpy(r + o, a + from + i, static_cast<size_t>(size));
        o += size;
        std::memcpy(r + o, b + from + i, static_cast<size_t>(size));
        o += size;
      }
      d = pack(r);
      return Next();
    }
    case 0x64: case 0x65: case 0x66: case 0x74: case 0x75: case 0x76: {  // pcmpgt / pcmpeq
      if (!pf66_) Ud();
      Xmm s = LoadX(e);
      uint8_t a[16], b[16], r[16];
      bytes(d, a);
      bytes(s, b);
      int size = 1 << ((op2 & 3));
      bool eq = op2 >= 0x74;
      for (int i = 0; i < 16; i += size) {
        uint64_t x = 0, y = 0;
        std::memcpy(&x, a + i, static_cast<size_t>(size));
        std::memcpy(&y, b + i, static_cast<size_t>(size));
        bool t = eq ? x == y : Sx(x, size) > Sx(y, size);
        std::memset(r + i, t ? 0xff : 0, static_cast<size_t>(size));
      }
      d = pack(r);
      return Next();
    }
    case 0x6E:  // movd / movq xmm, r/m
      if (!pf66_) Ud();
      d.lo = Load(e, opsize_ == 8 ? 8 : 4);
      d.hi = 0;
      return Next();
    case 0x7E:
      if (pfF3_) {  // movq xmm, xmm/m64
        uint64_t v = e.mem ? Rd(Addr(e), 8) : s_.xmm[e.reg].lo;
        d.lo = v;
        d.hi = 0;
        return Next();
      }
      if (!pf66_) Ud();
      Store(e, opsize_ == 8 ? 8 : 4, opsize_ == 8 ? d.lo : (d.lo & 0xffffffffull));
      return Next();
    case 0x6F:  // movdqa / movdqu load
      if (pf66_)
        d = LoadXAligned(e);
      else if (pfF3_)
        d = LoadX(e);
      else
        Ud();
      return Next();
    case 0x7F:  // movdqa / movdqu store
      if (!pf66_ && !pfF3_) Ud();
      if (e.mem) {
        if (pf66_ && (Addr(e) & 15)) Raise(kGP, "misaligned 16-byte store", Addr(e), true, 0);
        Wr128(Addr(e), d);
      } else {
        s_.xmm[e.reg] = d;
      }
      return Next();
    case 0x70: {  // pshufd / pshuflw / pshufhw
      e.imm = 1;
      Xmm s = LoadX(e);
      unsigned imm = static_cast<unsigned>(Imm(1));
      if (pf66_) {
        uint32_t a[4], r[4];
        std::memcpy(a, &s, 16);
        for (int i = 0; i < 4; ++i) r[i] = a[(imm >> (2 * i)) & 3];
        std::memcpy(&d, r, 16);
      } else {
        uint16_t a[8], r[8];
        std::memcpy(a, &s, 16);
        std::memcpy(r, a, 16);
        int base = pfF2_ ? 0 : 4;
        for (int i = 0; i < 4; ++i) r[base + i] = a[base + ((imm >> (2 * i)) & 3)];
        std::memcpy(&d, r, 16);
      }
      return Next();
    }
    case 0x71: case 0x72: case 0x73: {  // shifts by immediate
      if (!pf66_ || e.mem) Ud();
      e.imm = 1;
      unsigned cnt = static_cast<unsigned>(Imm(1));
      Xmm& t = s_.xmm[e.reg];
      int k = reg_ & 7;
      if (op2 == 0x73 && (k == 3 || k == 7)) {  // psrldq / pslldq: bytes
        uint8_t a[16], r[16] = {};
        bytes(t, a);
        unsigned n = cnt > 16 ? 16 : cnt;
        for (int i = 0; i < 16; ++i) {
          int from = k == 3 ? i + static_cast<int>(n) : i - static_cast<int>(n);
          r[i] = (from >= 0 && from < 16) ? a[from] : 0;
        }
        t = pack(r);
        return Next();
      }
      int size = op2 == 0x71 ? 2 : op2 == 0x72 ? 4 : 8;
      uint8_t a[16];
      bytes(t, a);
      for (int i = 0; i < 16; i += size) {
        uint64_t v = 0;
        std::memcpy(&v, a + i, static_cast<size_t>(size));
        int bits = size * 8;
        if (k == 2)
          v = cnt >= static_cast<unsigned>(bits) ? 0 : v >> cnt;
        else if (k == 6)
          v = cnt >= static_cast<unsigned>(bits) ? 0 : (v << cnt) & Mask(size);
        else if (k == 4 && size != 8)
          v = static_cast<uint64_t>(Sx(v, size) >> (cnt >= static_cast<unsigned>(bits) ? bits - 1 : static_cast<int>(cnt))) & Mask(size);
        else
          Ud();
        std::memcpy(a + i, &v, static_cast<size_t>(size));
      }
      t = pack(a);
      return Next();
    }
    case 0xC4: {  // pinsrw xmm, r32/m16, imm8
      if (!pf66_) Ud();
      e.imm = 1;
      uint64_t v = Load(e, 2);
      unsigned sel = static_cast<unsigned>(Imm(1)) & 7;
      uint16_t w[8];
      std::memcpy(w, &d, 16);
      w[sel] = static_cast<uint16_t>(v);
      std::memcpy(&d, w, 16);
      return Next();
    }
    case 0xC5: {  // pextrw r32, xmm, imm8
      if (!pf66_ || e.mem) Ud();
      e.imm = 1;
      unsigned sel = static_cast<unsigned>(Imm(1)) & 7;
      uint16_t w[8];
      std::memcpy(w, &s_.xmm[e.reg], 16);
      SetR(reg_, 4, w[sel]);
      return Next();
    }
    case 0xD6:  // movq xmm/m64, xmm
      if (!pf66_) Ud();
      if (e.mem) {
        Wr(Addr(e), d.lo, 8);
      } else {
        s_.xmm[e.reg].lo = d.lo;
        s_.xmm[e.reg].hi = 0;
      }
      return Next();
    case 0xD7: {  // pmovmskb r32, xmm
      if (!pf66_ || e.mem) Ud();
      uint8_t a[16];
      bytes(s_.xmm[e.reg], a);
      uint32_t m = 0;
      for (int i = 0; i < 16; ++i) m |= static_cast<uint32_t>(a[i] >> 7) << i;
      SetR(reg_, 4, m);
      return Next();
    }
    case 0xDA: case 0xDE: {  // pminub / pmaxub
      if (!pf66_) Ud();
      Xmm s = LoadX(e);
      uint8_t a[16], b[16];
      bytes(d, a);
      bytes(s, b);
      for (int i = 0; i < 16; ++i) a[i] = op2 == 0xDA ? (a[i] < b[i] ? a[i] : b[i]) : (a[i] > b[i] ? a[i] : b[i]);
      d = pack(a);
      return Next();
    }
    case 0xD4: case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD: case 0xFE: {
      if (!pf66_) Ud();
      Xmm s = LoadX(e);
      uint8_t a[16], b[16];
      bytes(d, a);
      bytes(s, b);
      int size;
      bool sub;
      switch (op2) {
        case 0xD4: size = 8; sub = false; break;  // paddq
        case 0xF8: size = 1; sub = true; break;   // psubb
        case 0xF9: size = 2; sub = true; break;
        case 0xFA: size = 4; sub = true; break;
        case 0xFB: size = 8; sub = true; break;
        case 0xFC: size = 1; sub = false; break;  // paddb
        case 0xFD: size = 2; sub = false; break;
        default: size = 4; sub = false; break;    // paddd
      }
      for (int i = 0; i < 16; i += size) {
        uint64_t x = 0, y = 0;
        std::memcpy(&x, a + i, static_cast<size_t>(size));
        std::memcpy(&y, b + i, static_cast<size_t>(size));
        uint64_t r = (sub ? x - y : x + y) & Mask(size);
        std::memcpy(a + i, &r, static_cast<size_t>(size));
      }
      d = pack(a);
      return Next();
    }
    default:
      Ud();
  }
}

bool Cpu::Group0F00() {
  Ea e = ModRM();
  int k = reg_ & 7;
  switch (k) {
    case 0:  // sldt
      Store(e, e.mem ? 2 : opsize_, s_.ldtr);
      return Next();
    case 1:  // str
      Store(e, e.mem ? 2 : opsize_, s_.tr);
      return Next();
    case 2:
    case 3: {  // lldt / ltr: a 16-byte system descriptor in the GDT
      Priv();
      uint16_t sel = static_cast<uint16_t>(Load(e, 2));
      if ((sel & ~3u) == 0) {
        if (k == 3) Raise(kGP, "ltr with a null selector", 0, true, 0);
        s_.ldtr = 0;
        s_.ldtr_base = 0;
        s_.ldtr_limit = 0;
        return Next();
      }
      if (sel & 4) Raise(kGP, "system descriptor in the LDT", 0, true, sel & ~3u);
      uint64_t lo = Descriptor(sel);
      uint64_t hi = s_.code64 ? SysRd(s_.gdtr_base + (sel & ~7u) + 8, 8) : 0;
      uint64_t base = DescBase(lo) | ((hi & 0xffffffffull) << 32);
      int type = static_cast<int>((lo >> 40) & 0xf);
      if (!((lo >> 47) & 1)) Raise(kNP, "descriptor not present", 0, true, sel & ~3u);
      if (k == 2) {
        if (type != 2) Raise(kGP, "lldt: not an LDT descriptor", 0, true, sel & ~3u);
        s_.ldtr = sel;
        s_.ldtr_base = base;
        s_.ldtr_limit = DescLimit(lo);
        return Next();
      }
      if (type != 9 && type != 1) Raise(kGP, "ltr: not an available TSS", 0, true, sel & ~3u);
      s_.tr = sel;
      s_.tr_base = base;
      s_.tr_limit = DescLimit(lo);
      SysWr(s_.gdtr_base + (sel & ~7u), lo | (2ull << 40), 8);  // busy
      return Next();
    }
    case 4:
    case 5: {  // verr / verw
      uint16_t sel = static_cast<uint16_t>(Load(e, 2));
      bool ok = false;
      if ((sel & ~3u) != 0 && !(sel & 4) && (sel | 7u) <= s_.gdtr_limit) {
        uint64_t d = Descriptor(sel);
        bool user_seg = (d >> 44) & 1, code = (d >> 43) & 1, rw = (d >> 41) & 1;
        ok = user_seg && (k == 4 ? (!code || rw) : (!code && rw));
      }
      F() = (F() & ~ZF) | (ok ? ZF : 0);
      return Next();
    }
    default:
      Ud();
  }
}

bool Cpu::Group0F01() {
  uint8_t m = Ib(p_);
  if ((m >> 6) == 3) {
    p_++;
    switch (m) {
      case 0xC1:
      case 0xD9:  // vmcall / vmmcall
        return ExitHere(Exit::kHypercall);
      case 0xCA:  // clac
        Priv();
        F() &= ~AC;
        return Next();
      case 0xCB:  // stac
        Priv();
        F() |= AC;
        return Next();
      case 0xD0: {  // xgetbv
        if ((s_.gpr[RCX] & 0xffffffffull) != 0) Raise(kGP, "xgetbv ecx", 0, true, 0);
        SetR(RAX, 4, s_.xcr0 & 0xffffffffull);
        SetR(RDX, 4, s_.xcr0 >> 32);
        return Next();
      }
      case 0xD1: {  // xsetbv: x87 and SSE state only
        Priv();
        uint64_t v = (GetR(RDX, 4) << 32) | GetR(RAX, 4);
        if ((s_.gpr[RCX] & 0xffffffffull) != 0 || !(v & 1) || (v & ~3ull))
          Raise(kGP, "xsetbv", 0, true, 0);
        s_.xcr0 = v;
        return Next();
      }
      case 0xF8: {  // swapgs
        if (!s_.code64) Ud();
        Priv();
        uint64_t t = s_.gs_base;
        s_.gs_base = s_.kernel_gs_base;
        s_.kernel_gs_base = t;
        return Next();
      }
      case 0xF9:  // rdtscp
        if (s_.tsc_per_insn) {
          SetR(RAX, 4, s_.tsc & 0xffffffffull);
          SetR(RDX, 4, s_.tsc >> 32);
          SetR(RCX, 4, s_.tsc_aux & 0xffffffffull);
          return Next();
        }
        info_.rdtscp = true;
        return ExitHere(Exit::kRdtsc);
      default:
        Ud();
    }
  }
  Ea e = ModRM();
  int k = reg_ & 7;
  int bw = s_.code64 ? 8 : 4;
  switch (k) {
    case 0:
    case 1: {  // sgdt / sidt
      uint64_t a = Addr(e);
      Wr(a, k == 0 ? s_.gdtr_limit : s_.idtr_limit, 2);
      Wr(Wrap(a + 2), k == 0 ? s_.gdtr_base : s_.idtr_base, bw);
      return Next();
    }
    case 2:
    case 3: {  // lgdt / lidt
      Priv();
      uint64_t a = Addr(e);
      uint16_t limit = static_cast<uint16_t>(Rd(a, 2));
      uint64_t base = Rd(Wrap(a + 2), bw);
      if (k == 2) {
        s_.gdtr_limit = limit;
        s_.gdtr_base = base;
      } else {
        s_.idtr_limit = limit;
        s_.idtr_base = base;
      }
      return Next();
    }
    case 4:  // smsw
      Store(e, 2, s_.cr0 & 0xffff);
      return Next();
    case 6: {  // lmsw: MP, EM, TS; PE can be set, not cleared
      Priv();
      uint64_t v = Load(e, 2);
      s_.cr0 = (s_.cr0 & ~0xeull) | (v & 0xf);
      return Next();
    }
    case 7:  // invlpg
      Priv();
      TlbFlush();
      return Next();
    default:
      Ud();
  }
}

bool Cpu::MovCrDr(uint8_t op2) {
  Priv();
  // The ModRM always names registers here, whatever its mod field says.
  uint8_t m = Ib(p_++);
  int n = ((m >> 3) & 7) | ((rex_ & 4) ? 8 : 0);
  int r = (m & 7) | ((rex_ & 1) ? 8 : 0);
  int w = s_.code64 ? 8 : 4;
  bool to_reg = op2 == 0x20 || op2 == 0x21;
  if (op2 == 0x21 || op2 == 0x23) {
    if (n > 7) Ud();
    int i = n == 4 ? 6 : n == 5 ? 7 : n;
    if (to_reg)
      SetR(r, w, s_.dr[i]);
    else
      s_.dr[i] = GetR(r, w);
    return Next();
  }
  if (to_reg) {
    uint64_t v;
    switch (n) {
      case 0: v = s_.cr0; break;
      case 2: v = s_.cr2; break;
      case 3: v = s_.cr3; break;
      case 4: v = s_.cr4; break;
      case 8: v = s_.cr8; break;
      default: Ud();
    }
    SetR(r, w, v);
    return Next();
  }
  uint64_t v = GetR(r, w);
  switch (n) {
    case 0:
      s_.cr0 = v | 0x10;  // ET is hardwired
      if ((s_.cr0 & kCr0Pg) && (s_.efer & kEferLme))
        s_.efer |= kEferLma;
      else if (!(s_.cr0 & kCr0Pg))
        s_.efer &= ~kEferLma;
      TlbFlush();
      break;
    case 2: s_.cr2 = v; break;
    case 3:
      s_.cr3 = v & ~(1ull << 63);
      TlbFlush();
      break;
    case 4:
      s_.cr4 = v;
      TlbFlush();
      break;
    case 8: s_.cr8 = v & 0xf; break;
    default: Ud();
  }
  return Next();
}

uint64_t* MsrSlot(State& s, uint32_t msr) {
  switch (msr) {
    case 0xC0000080: return &s.efer;
    case 0xC0000081: return &s.star;
    case 0xC0000082: return &s.lstar;
    case 0xC0000083: return &s.cstar;
    case 0xC0000084: return &s.sfmask;
    case 0xC0000100: return &s.fs_base;
    case 0xC0000101: return &s.gs_base;
    case 0xC0000102: return &s.kernel_gs_base;
    case 0xC0000103: return &s.tsc_aux;
    case 0x277: return &s.pat;
    case 0x174: return &s.sysenter_cs;
    case 0x175: return &s.sysenter_esp;
    case 0x176: return &s.sysenter_eip;
    default: return nullptr;
  }
}

bool Cpu::Msr(bool write) {
  Priv();
  uint32_t msr = static_cast<uint32_t>(GetR(RCX, 4));
  bool tsc = msr == 0x10 && s_.tsc_per_insn;
  uint64_t* slot = MsrSlot(s_, msr);
  if (!slot && !tsc) {
    info_.write = write;
    return ExitHere(Exit::kMsr);
  }
  if (write) {
    uint64_t v = (GetR(RDX, 4) << 32) | GetR(RAX, 4);
    if (tsc) {
      s_.tsc = v;
    } else if (msr == 0xC0000080) {
      s_.efer = (v & ~kEferLma) | (s_.efer & kEferLma);
      TlbFlush();  // NXE changes what a walk allows
    } else {
      *slot = v;
    }
  } else {
    uint64_t v = tsc ? s_.tsc : *slot;
    SetR(RAX, 4, v & 0xffffffffull);
    SetR(RDX, 4, v >> 32);
  }
  return Next();
}

bool Cpu::FxSave(const Ea& e, bool save) {
  if (!e.mem) Ud();
  if (s_.cr0 & kCr0Ts) Raise(kNM, "fxsave with CR0.TS set");
  uint64_t a = Addr(e);
  if (a & 15) Raise(kGP, "fxsave area misaligned", a, true, 0);
  // Bytes 0-415: control/status, x87 registers, XMM registers. The
  // reserved and software-available tail is left alone.
  uint8_t img[416];
  auto put = [&](int at, uint64_t v, int n) { std::memcpy(img + at, &v, static_cast<size_t>(n)); };
  auto get = [&](int at, int n) {
    uint64_t v = 0;
    std::memcpy(&v, img + at, static_cast<size_t>(n));
    return v;
  };
  int nxmm = s_.code64 ? 16 : 8;
  if (save) {
    std::memset(img, 0, sizeof(img));
    put(0, s_.fcw, 2);
    put(2, s_.fsw, 2);
    put(4, s_.ftw, 1);
    put(6, s_.fop, 2);
    if (opsize_ == 8) {
      put(8, s_.fip, 8);
      put(16, s_.fdp, 8);
    } else {
      put(8, s_.fip & 0xffffffffull, 4);
      put(16, s_.fdp & 0xffffffffull, 4);
    }
    put(24, s_.mxcsr, 4);
    put(28, 0xffff, 4);  // MXCSR_MASK
    for (int i = 0; i < 8; ++i) {
      put(32 + 16 * i, s_.st[i].lo, 8);
      put(40 + 16 * i, s_.st[i].hi & 0xffff, 8);
    }
    for (int i = 0; i < nxmm; ++i) {
      put(160 + 16 * i, s_.xmm[i].lo, 8);
      put(168 + 16 * i, s_.xmm[i].hi, 8);
    }
    Access(a, img, sizeof(img), true);
    return Next();
  }
  Access(a, img, sizeof(img), false);
  uint32_t mx = static_cast<uint32_t>(get(24, 4));
  if (mx & ~0xffffu) Raise(kGP, "fxrstor: reserved MXCSR bits", 0, true, 0);
  s_.fcw = static_cast<uint16_t>(get(0, 2));
  s_.fsw = static_cast<uint16_t>(get(2, 2));
  s_.ftw = static_cast<uint8_t>(get(4, 1));
  s_.fop = static_cast<uint16_t>(get(6, 2));
  s_.fip = opsize_ == 8 ? get(8, 8) : get(8, 4);
  s_.fdp = opsize_ == 8 ? get(16, 8) : get(16, 4);
  s_.mxcsr = mx;
  for (int i = 0; i < 8; ++i) {
    s_.st[i].lo = get(32 + 16 * i, 8);
    s_.st[i].hi = get(40 + 16 * i, 2);
  }
  for (int i = 0; i < nxmm; ++i) {
    s_.xmm[i].lo = get(160 + 16 * i, 8);
    s_.xmm[i].hi = get(168 + 16 * i, 8);
  }
  return Next();
}

bool Cpu::Iret() {
  int w = opsize_;
  int old_cpl = Cpl();
  uint64_t rip = Pop(w);
  uint16_t cs = static_cast<uint16_t>(Pop(w));
  uint64_t fl = Pop(w);
  int new_cpl = cs & 3;
  // 64-bit mode always pops SS:RSP; protected mode only across rings.
  bool stack = s_.code64 || new_cpl > old_cpl;
  uint64_t rsp = 0;
  uint16_t ss = s_.ss;
  if (stack) {
    rsp = Pop(w);
    ss = static_cast<uint16_t>(Pop(w));
  }
  if (new_cpl < old_cpl) Raise(kGP, "iret to an inner ring", 0, true, cs & ~3u);
  bool long_code = CodeLong(cs);
  uint64_t mask = kIretMask & Mask(w);
  if (old_cpl > 0) mask &= ~IOPL;
  if (old_cpl > Iopl()) mask &= ~IF;
  F() = (F() & ~mask) | (fl & mask) | 2;
  s_.cs = cs;
  s_.code64 = long_code;
  if (stack) {
    s_.gpr[RSP] = w == 8 ? rsp : rsp & Mask(w);
    s_.ss = ss;
  }
  return Jump(rip);
}

bool Cpu::FarRet(bool imm) {
  uint64_t n = imm ? Imm(2) : 0;
  int w = opsize_;
  uint64_t rip = Pop(w);
  uint16_t cs = static_cast<uint16_t>(Pop(w));
  if ((cs & 3) != Cpl()) Raise(kGP, "far return across rings", 0, true, cs & ~3u);
  bool long_code = CodeLong(cs);
  s_.gpr[RSP] = Wrap(s_.gpr[RSP] + n);
  s_.cs = cs;
  s_.code64 = long_code;
  return Jump(rip);
}

// Long-mode delivery: a 16-byte gate, the ring the target code segment
// names, RSP0-2 or an IST stack from the TSS, and the five-quadword frame.
bool Cpu::DeliverLong(const Fault& f) {
  if (s_.idtr_limit == 0 || static_cast<uint32_t>(f.vector) * 16 + 15 > s_.idtr_limit) return false;
  uint8_t g[16];
  {
    bool was = sys_;
    sys_ = true;
    Access(s_.idtr_base + static_cast<uint64_t>(f.vector) * 16, g, 16, false);
    sys_ = was;
  }
  if (!(g[5] & 0x80)) return false;  // not present
  int type = g[5] & 0x0f;
  if (type != 0x0e && type != 0x0f) return false;
  if (f.soft && ((g[5] >> 5) & 3) < Cpl())
    throw Fault{kGP, true, static_cast<uint32_t>(f.vector) * 8 + 2, 0, false, 0, "gate DPL below CPL"};
  uint16_t sel = static_cast<uint16_t>(g[2] | (g[3] << 8));
  int ist = g[4] & 7;
  uint64_t target = static_cast<uint64_t>(g[0] | (g[1] << 8)) |
                    (static_cast<uint64_t>(g[6] | (g[7] << 8)) << 16);
  uint32_t hi;
  std::memcpy(&hi, g + 8, 4);
  target |= static_cast<uint64_t>(hi) << 32;

  int old_cpl = Cpl();
  int new_cpl = old_cpl;
  bool long_code = true;
  // A partition that never loaded a GDT keeps its ring and 64-bit code.
  if (s_.gdtr_limit != 0 && (sel & ~3u) != 0) {
    uint64_t d = Descriptor(sel);
    new_cpl = DescDpl(d);
    long_code = DescLong(d);
  }
  if (new_cpl > old_cpl) throw Fault{kGP, true, sel & ~3u, 0, false, 0, "gate to an outer ring"};
  uint64_t rsp = s_.gpr[RSP];
  if (ist)
    rsp = SysRd(s_.tr_base + 36 + static_cast<uint64_t>(ist - 1) * 8, 8);
  else if (new_cpl < old_cpl)
    rsp = SysRd(s_.tr_base + 4 + static_cast<uint64_t>(new_cpl) * 8, 8);

  uint64_t rip = f.trap ? f.next : ip0_;
  uint64_t old_rsp = s_.gpr[RSP], old_fl = F();
  uint16_t old_cs = s_.cs, old_ss = s_.ss;
  bool old_code64 = s_.code64;
  s_.cs = static_cast<uint16_t>((sel & ~3u) | static_cast<unsigned>(new_cpl));
  s_.code64 = long_code;
  s_.gpr[RSP] = rsp & ~0xfull;
  try {
    Push(old_ss, 8);
    Push(old_rsp, 8);
    Push(old_fl, 8);
    Push(old_cs, 8);
    Push(rip, 8);
    if (f.has_code) Push(f.code, 8);
  } catch (const Fault&) {
    s_.cs = old_cs;
    s_.code64 = old_code64;
    s_.gpr[RSP] = old_rsp;
    throw;
  }
  if (new_cpl < old_cpl) s_.ss = static_cast<uint16_t>(new_cpl);  // null, RPL = new CPL
  F() &= ~(TF | NT | RF | VM);
  if (type == 0x0e) F() &= ~IF;  // interrupt gate
  s_.rip = target;
  return true;
}

Cpu::Taken Cpu::Take(Fault f) {
  for (;;) {
    try {
      return Deliver(f) ? Taken::kDelivered : Taken::kNoIdt;
    } catch (const Fault& nested) {
      if (f.vector == kDF) return Taken::kShutdown;
      f = Fault{kDF, true, 0, nested.addr, false, 0, nested.why};
    }
  }
}

bool Cpu::Deliver(const Fault& f) {
  if (s_.code64 || (s_.efer & kEferLma)) return DeliverLong(f);
  bool long_mode = s_.code64;
  unsigned gate = long_mode ? 16u : 8u;
  if (s_.idtr_limit == 0 || static_cast<uint64_t>(f.vector) * gate + gate - 1 > s_.idtr_limit)
    return false;
  uint8_t g[16] = {};
  try {
    Access(s_.idtr_base + static_cast<uint64_t>(f.vector) * gate, g, gate, false);
  } catch (const Fault&) {
    return false;
  }
  if (!(g[5] & 0x80)) return false;  // not present
  uint64_t target = static_cast<uint64_t>(g[0] | (g[1] << 8)) |
                    (static_cast<uint64_t>(g[6] | (g[7] << 8)) << 16);
  if (long_mode) {
    uint32_t hi;
    std::memcpy(&hi, g + 8, 4);
    target |= static_cast<uint64_t>(hi) << 32;
  }
  uint64_t rip = f.trap ? f.next : ip0_;
  try {
    if (long_mode) {
      uint64_t old_sp = s_.gpr[RSP];
      s_.gpr[RSP] &= ~0xfull;
      Push(s_.ss, 8);
      Push(old_sp, 8);
      Push(F(), 8);
      Push(s_.cs, 8);
      Push(rip, 8);
      if (f.has_code) Push(f.code, 8);
    } else {
      Push(F(), 4);
      Push(s_.cs, 4);
      Push(rip, 4);
      if (f.has_code) Push(f.code, 4);
    }
  } catch (const Fault&) {
    return false;  // a fault while delivering one: the guest cannot take it
  }
  if ((g[5] & 0x0f) == 0x0e) F() &= ~IF;  // interrupt gate
  s_.rip = target;
  return true;
}

uint8_t g_last_vector = 0;
uint64_t g_last_rip = 0;
const char* g_last_why = nullptr;

}  // namespace

const char* LastFault(uint8_t* vector, uint64_t* rip) {
  if (vector) *vector = g_last_vector;
  if (rip) *rip = g_last_rip;
  return g_last_why;
}

bool OwnsMsr(uint32_t msr) {
  State probe;
  return MsrSlot(probe, msr) != nullptr;
}

Exit Run(State* s, const Memory& m, uint64_t budget, ExitInfo* info, uint64_t* retired) {
  ExitInfo local;
  ExitInfo& out = info ? *info : local;
  out = ExitInfo{};
  // Host pointers in the TLB are the caller's mapping of this run.
  TlbFlush();
  Cpu cpu(*s, m, out);
  uint64_t n = 0;
  // Takes `f`; false stops the run with `out` filled in.
  auto take = [&](const Fault& f) {
    g_last_vector = f.vector;
    g_last_rip = s->rip;
    g_last_why = f.why;
    Cpu::Taken t = cpu.Take(f);
    if (t == Cpu::Taken::kDelivered) return true;
    out.reason = t == Cpu::Taken::kShutdown ? Exit::kShutdown : Exit::kException;
    out.vector = f.vector;
    out.addr = f.addr;
    out.why = t == Cpu::Taken::kShutdown ? "triple fault" : f.why;
    return false;
  };
  // The try region is entered once per fault, not once per instruction:
  // setting up a wasm-EH landing pad on every instruction was pure overhead
  // on a hot loop that almost never throws. A caught Fault is delivered and
  // the inner loop re-entered at the next instruction.
  while (n < budget) {
    try {
      while (n < budget) {
        if (s->pending_vector >= 0 &&
            (s->pending_exception || ((s->rflags & IF) && !s->int_shadow))) {
          bool exception = s->pending_exception;
          Fault f{static_cast<uint8_t>(s->pending_vector), exception && s->pending_code >= 0,
                  static_cast<uint32_t>(s->pending_code), 0, true, s->rip,
                  exception ? "injected exception" : "external interrupt", false, !exception};
          s->pending_vector = -1;
          s->pending_exception = false;
          s->pending_code = -1;
          if (!take(f)) {
            if (retired) *retired = n;
            return out.reason;
          }
        }
        // The shadow covers one instruction; STI or MOV SS in it raises it again.
        s->int_shadow = false;
        if (cpu.Step()) {
          if (retired) *retired = n;
          return out.reason;
        }
        ++n;
        s->tsc += s->tsc_per_insn;
      }
    } catch (const Fault& f) {
      if (!take(f)) {
        if (retired) *retired = n;
        return out.reason;
      }
    }
  }
  out.reason = Exit::kBudget;
  if (retired) *retired = n;
  return out.reason;
}

}  // namespace x86
}  // namespace wasmgocos
