// The software partition's virtual processor (go++'s sysroot
// WinHvPlatform.h, the wasm build) executes guest code on the x86 engine
// (src/x86.cc). Registers move between the partition's register file and
// the engine; guest physical memory is the partition's mapped ranges; and
// each engine exit becomes the WHv exit the VMM (TurboSpace's
// hv-partition, or its Linux VM) services, with RIP on the instruction and
// its length.

#include <WinHvPlatform.h>

#include <chrono>
#include <cstdint>
#include <cstring>

#include "gocos/x86.hpp"

namespace {

namespace x86 = wasmgocos::x86;

uint8_t* Gpa(void* ctx, uint64_t gpa, uint64_t n) {
  auto* whp = static_cast<WasigoWhp*>(ctx);
  for (const WasigoWhpMap& m : whp->maps) {
    if (m.host && gpa >= m.gpa && gpa - m.gpa + n <= m.bytes)
      return static_cast<uint8_t*>(m.host) + (gpa - m.gpa);
  }
  return nullptr;
}

const WHV_REGISTER_NAME kXmm[16] = {
    WHvX64RegisterXmm0,  WHvX64RegisterXmm1,  WHvX64RegisterXmm2,  WHvX64RegisterXmm3,
    WHvX64RegisterXmm4,  WHvX64RegisterXmm5,  WHvX64RegisterXmm6,  WHvX64RegisterXmm7,
    WHvX64RegisterXmm8,  WHvX64RegisterXmm9,  WHvX64RegisterXmm10, WHvX64RegisterXmm11,
    WHvX64RegisterXmm12, WHvX64RegisterXmm13, WHvX64RegisterXmm14, WHvX64RegisterXmm15,
};

// DR0-DR3, DR6, DR7 (DR4/DR5 alias DR6/DR7 and have no register of their own).
const struct {
  WHV_REGISTER_NAME name;
  int index;
} kDr[6] = {
    {WHvX64RegisterDr0, 0}, {WHvX64RegisterDr1, 1}, {WHvX64RegisterDr2, 2},
    {WHvX64RegisterDr3, 3}, {WHvX64RegisterDr6, 6}, {WHvX64RegisterDr7, 7},
};

// Engine fields that are one 64-bit register each.
const struct {
  WHV_REGISTER_NAME name;
  uint64_t x86::State::*field;
} kPlain[] = {
    {WHvX64RegisterCr0, &x86::State::cr0},
    {WHvX64RegisterCr2, &x86::State::cr2},
    {WHvX64RegisterCr3, &x86::State::cr3},
    {WHvX64RegisterCr4, &x86::State::cr4},
    {WHvX64RegisterCr8, &x86::State::cr8},
    {WHvX64RegisterEfer, &x86::State::efer},
    {WHvX64RegisterKernelGsBase, &x86::State::kernel_gs_base},
    {WHvX64RegisterStar, &x86::State::star},
    {WHvX64RegisterLstar, &x86::State::lstar},
    {WHvX64RegisterCstar, &x86::State::cstar},
    {WHvX64RegisterSfmask, &x86::State::sfmask},
    {WHvX64RegisterSysenterCs, &x86::State::sysenter_cs},
    {WHvX64RegisterSysenterEip, &x86::State::sysenter_eip},
    {WHvX64RegisterSysenterEsp, &x86::State::sysenter_esp},
    {WHvX64RegisterTscAux, &x86::State::tsc_aux},
    {WHvX64RegisterApicBase, &x86::State::apic_base},
    {WHvX64RegisterTsc, &x86::State::tsc},
};

// Registers whose reset value is not zero: a register file nobody wrote
// keeps the engine's reset value for them.
const struct {
  WHV_REGISTER_NAME name;
  uint64_t x86::State::*field;
} kResetNonZero[] = {
    {WHvX64RegisterPat, &x86::State::pat},
    {WHvX64RegisterXCr0, &x86::State::xcr0},
};

void LoadSeg(const WHV_X64_SEGMENT_REGISTER& r, uint16_t* sel, uint64_t* base, uint32_t* limit) {
  *sel = r.Selector;
  *base = r.Base;
  *limit = r.Limit;
}

void StoreSeg(uint16_t sel, uint64_t base, uint32_t limit, WHV_X64_SEGMENT_REGISTER* r) {
  r->Selector = sel;
  r->Base = base;
  r->Limit = limit;
}

void Load(const WasigoWhp& w, x86::State* s) {
  for (int i = 0; i < 16; ++i) s->gpr[i] = w.reg[i].Reg64;
  s->rip = w.reg[WHvX64RegisterRip].Reg64;
  s->rflags = w.reg[WHvX64RegisterRflags].Reg64 | 2;
  for (const auto& p : kPlain) s->*p.field = w.reg[p.name].Reg64;
  for (const auto& p : kResetNonZero) {
    if (w.reg[p.name].Reg64) s->*p.field = w.reg[p.name].Reg64;
  }
  const WHV_X64_SEGMENT_REGISTER& cs = w.reg[WHvX64RegisterCs].Segment;
  // CS.L selects 64-bit code; a partition that never set CS runs 64-bit
  // code when long mode is active (EFER.LMA).
  s->code64 = cs.Selector || cs.Long ? cs.Long != 0 : (s->efer & (1ull << 10)) != 0;
  s->cs = cs.Selector ? cs.Selector : 0x08;
  s->ss = w.reg[WHvX64RegisterSs].Segment.Selector;
  s->ds = w.reg[WHvX64RegisterDs].Segment.Selector;
  s->es = w.reg[WHvX64RegisterEs].Segment.Selector;
  s->fs = w.reg[WHvX64RegisterFs].Segment.Selector;
  s->gs = w.reg[WHvX64RegisterGs].Segment.Selector;
  s->fs_base = w.reg[WHvX64RegisterFs].Segment.Base;
  s->gs_base = w.reg[WHvX64RegisterGs].Segment.Base;
  s->idtr_base = w.reg[WHvX64RegisterIdtr].Table.Base;
  s->idtr_limit = w.reg[WHvX64RegisterIdtr].Table.Limit;
  s->gdtr_base = w.reg[WHvX64RegisterGdtr].Table.Base;
  s->gdtr_limit = w.reg[WHvX64RegisterGdtr].Table.Limit;
  LoadSeg(w.reg[WHvX64RegisterTr].Segment, &s->tr, &s->tr_base, &s->tr_limit);
  LoadSeg(w.reg[WHvX64RegisterLdtr].Segment, &s->ldtr, &s->ldtr_base, &s->ldtr_limit);
  for (const auto& d : kDr) {
    if (w.reg[d.name].Reg64 || d.index < 4) s->dr[d.index] = w.reg[d.name].Reg64;
  }
  for (int i = 0; i < 16; ++i) {
    s->xmm[i].lo = w.reg[kXmm[i]].Reg128.Low64;
    s->xmm[i].hi = w.reg[kXmm[i]].Reg128.High64;
  }
  for (int i = 0; i < 8; ++i) {
    const WHV_REGISTER_VALUE& st = w.reg[WHvX64RegisterFpMmx0 + i];
    s->st[i].lo = st.Reg128.Low64;
    s->st[i].hi = st.Reg128.High64;
  }
  const WHV_REGISTER_VALUE& fp = w.reg[WHvX64RegisterFpControlStatus];
  if (fp.Reg128.Low64 || fp.Reg128.High64) {
    s->fcw = static_cast<uint16_t>(fp.Reg128.Low64);
    s->fsw = static_cast<uint16_t>(fp.Reg128.Low64 >> 16);
    s->ftw = static_cast<uint8_t>(fp.Reg128.Low64 >> 32);
    s->fop = static_cast<uint16_t>(fp.Reg128.Low64 >> 48);
    s->fip = fp.Reg128.High64;
  }
  const WHV_REGISTER_VALUE& xs = w.reg[WHvX64RegisterXmmControlStatus];
  s->fdp = xs.Reg128.Low64;
  if (xs.Reg128.High64) s->mxcsr = static_cast<uint32_t>(xs.Reg128.High64);
  // Event injection, WHV_X64_PENDING_INTERRUPTION_REGISTER-shaped: bit 0
  // pending, bits 1-3 type (3 = hardware exception), bit 4 error code
  // valid, bits 16-31 vector, bits 32-63 error code.
  uint64_t pending = w.reg[WHvRegisterPendingInterruption].Reg64;
  if (pending & 1) {
    s->pending_vector = static_cast<int>((pending >> 16) & 0xff);
    s->pending_exception = ((pending >> 1) & 7) == 3;
    s->pending_code = (pending & (1ull << 4)) ? static_cast<int64_t>(pending >> 32) : -1;
  }
  s->int_shadow = w.reg[WHvRegisterInterruptState].Reg64 & 1;
  if (w.tsc_virtual) s->tsc_per_insn = w.tsc_per_insn;
}

void Store(const x86::State& s, WasigoWhp* w) {
  for (int i = 0; i < 16; ++i) w->reg[i].Reg64 = s.gpr[i];
  w->reg[WHvX64RegisterRip].Reg64 = s.rip;
  w->reg[WHvX64RegisterRflags].Reg64 = s.rflags;
  for (const auto& p : kPlain) w->reg[p.name].Reg64 = s.*p.field;
  for (const auto& p : kResetNonZero) w->reg[p.name].Reg64 = s.*p.field;
  WHV_X64_SEGMENT_REGISTER& cs = w->reg[WHvX64RegisterCs].Segment;
  cs.Selector = s.cs;
  cs.Long = s.code64 ? 1 : 0;
  cs.DescriptorPrivilegeLevel = s.cs & 3;
  w->reg[WHvX64RegisterSs].Segment.Selector = s.ss;
  w->reg[WHvX64RegisterDs].Segment.Selector = s.ds;
  w->reg[WHvX64RegisterEs].Segment.Selector = s.es;
  w->reg[WHvX64RegisterFs].Segment.Selector = s.fs;
  w->reg[WHvX64RegisterGs].Segment.Selector = s.gs;
  w->reg[WHvX64RegisterFs].Segment.Base = s.fs_base;
  w->reg[WHvX64RegisterGs].Segment.Base = s.gs_base;
  w->reg[WHvX64RegisterIdtr].Table.Base = s.idtr_base;
  w->reg[WHvX64RegisterIdtr].Table.Limit = s.idtr_limit;
  w->reg[WHvX64RegisterGdtr].Table.Base = s.gdtr_base;
  w->reg[WHvX64RegisterGdtr].Table.Limit = s.gdtr_limit;
  StoreSeg(s.tr, s.tr_base, s.tr_limit, &w->reg[WHvX64RegisterTr].Segment);
  StoreSeg(s.ldtr, s.ldtr_base, s.ldtr_limit, &w->reg[WHvX64RegisterLdtr].Segment);
  for (const auto& d : kDr) w->reg[d.name].Reg64 = s.dr[d.index];
  for (int i = 0; i < 16; ++i) {
    w->reg[kXmm[i]].Reg128.Low64 = s.xmm[i].lo;
    w->reg[kXmm[i]].Reg128.High64 = s.xmm[i].hi;
  }
  for (int i = 0; i < 8; ++i) {
    WHV_REGISTER_VALUE& st = w->reg[WHvX64RegisterFpMmx0 + i];
    st.Reg128.Low64 = s.st[i].lo;
    st.Reg128.High64 = s.st[i].hi;
  }
  WHV_REGISTER_VALUE& fp = w->reg[WHvX64RegisterFpControlStatus];
  fp.Reg128.Low64 = static_cast<uint64_t>(s.fcw) | (static_cast<uint64_t>(s.fsw) << 16) |
                    (static_cast<uint64_t>(s.ftw) << 32) | (static_cast<uint64_t>(s.fop) << 48);
  fp.Reg128.High64 = s.fip;
  WHV_REGISTER_VALUE& xs = w->reg[WHvX64RegisterXmmControlStatus];
  xs.Reg128.Low64 = s.fdp;
  xs.Reg128.High64 = static_cast<uint64_t>(s.mxcsr) | (0xffffull << 32);
  uint64_t pending = 0;
  if (s.pending_vector >= 0) {
    pending = 1 | (static_cast<uint64_t>(s.pending_exception ? 3 : 0) << 1) |
              (static_cast<uint64_t>(s.pending_vector & 0xff) << 16);
    if (s.pending_code >= 0) pending |= (1ull << 4) | (static_cast<uint64_t>(s.pending_code) << 32);
  }
  w->reg[WHvRegisterPendingInterruption].Reg64 = pending;
  w->reg[WHvRegisterInterruptState].Reg64 = s.int_shadow ? 1 : 0;
}

}  // namespace

extern "C" HRESULT wasigo_vcpu_run(WasigoWhp* whp, WHV_RUN_VP_EXIT_CONTEXT* exit) {
  std::memset(exit, 0, sizeof(*exit));
  x86::State s;
  Load(*whp, &s);
  x86::Memory mem{whp, Gpa};
  x86::ExitInfo info;
  // With no run budget the partition runs until the guest exits; the
  // engine's own budget only bounds one pass of its loop.
  x86::Exit x;
  for (;;) {
    uint64_t pass = whp->run_budget ? whp->run_budget : uint64_t{1} << 32;
    uint64_t done = 0;
    x = x86::Run(&s, mem, pass, &info, &done);
    whp->retired += done;
    if (x != x86::Exit::kBudget || whp->run_budget) break;
  }
  Store(s, whp);
  exit->VpContext.Rip = s.rip;
  exit->VpContext.InstructionLength = info.len;
  switch (x) {
    case x86::Exit::kBudget:
      exit->ExitReason = WHvRunVpExitReasonCanceled;
      break;
    case x86::Exit::kHalt:
      exit->ExitReason = WHvRunVpExitReasonX64Halt;
      break;
    case x86::Exit::kIo:
      exit->ExitReason = WHvRunVpExitReasonX64IoPortAccess;
      exit->IoPortAccess.PortNumber = info.port;
      exit->IoPortAccess.AccessInfo.IsWrite = info.write ? 1 : 0;
      exit->IoPortAccess.AccessInfo.AccessSize = info.size;
      exit->IoPortAccess.Rax = s.gpr[0];
      break;
    case x86::Exit::kHypercall:
      exit->ExitReason = WHvRunVpExitReasonHypercall;
      break;
    case x86::Exit::kCpuid:
      // The platform's default result is what this engine is: no vendor,
      // no feature bits it would have to stand behind. The VMM adds its
      // own leaves.
      exit->ExitReason = WHvRunVpExitReasonX64Cpuid;
      exit->CpuidAccess.Rax = s.gpr[0];
      break;
    case x86::Exit::kRdtsc:
      exit->ExitReason = WHvRunVpExitReasonX64Rdtsc;
      exit->ReadTsc.Tsc = static_cast<UINT64>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count());
      exit->ReadTsc.RdtscInfo.IsRdtscp = info.rdtscp ? 1 : 0;
      break;
    case x86::Exit::kMsr:
      exit->ExitReason = WHvRunVpExitReasonX64MsrAccess;
      exit->MsrAccess.MsrNumber = static_cast<UINT32>(s.gpr[1]);
      exit->MsrAccess.Rax = s.gpr[0] & 0xffffffffull;
      exit->MsrAccess.Rdx = s.gpr[2] & 0xffffffffull;
      exit->MsrAccess.AccessInfo.IsWrite = info.write ? 1 : 0;
      break;
    default:
      // A fault the guest has no IDT entry for, or a triple fault: the
      // processor cannot go on.
      exit->ExitReason = WHvRunVpExitReasonUnrecoverableException;
      exit->ExceptionVector = info.vector;
      exit->Why = info.why;
      break;
  }
  return S_OK;
}
