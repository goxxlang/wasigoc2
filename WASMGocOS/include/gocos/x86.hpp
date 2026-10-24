#ifndef WASMGOCOS_INCLUDE_GOCOS_X86_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_X86_HPP_

// The vCPU's instruction engine: x86-64 in long mode and IA-32 in flat
// protected mode, executing guest code in guest memory. It is what runs
// behind the software WHvRunVirtualProcessor (src/vcpu_whp.cc) where there
// is no hardware partition, and behind peinterp's 32-bit path. It stops the
// way a partition does -- an I/O port access, a hypercall (VMCALL/VMMCALL),
// CPUID, RDTSC, an MSR access, HLT -- with RIP on that instruction and its
// length reported, so the VMM services the exit and moves RIP past it.
// A guest fault is delivered through the guest's own IDT when it has one;
// otherwise the run stops with the vector.

#include <cstdint>

namespace wasmgocos {
namespace x86 {

enum Gpr : int { RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 };

struct Xmm {
  uint64_t lo = 0;
  uint64_t hi = 0;
};

struct State {
  uint64_t gpr[16] = {};
  uint64_t rip = 0;
  uint64_t rflags = 0x2;
  uint64_t fs_base = 0;
  uint64_t gs_base = 0;
  uint64_t cr0 = 0, cr2 = 0, cr3 = 0, cr4 = 0, efer = 0, cr8 = 0;
  // CS.L: 64-bit code. False is 32-bit flat protected mode.
  bool code64 = true;
  // The CPL is CS's RPL.
  uint16_t cs = 0x08, ss = 0x10, ds = 0, es = 0, fs = 0, gs = 0;
  uint64_t idtr_base = 0;
  uint16_t idtr_limit = 0;
  uint64_t gdtr_base = 0;
  uint16_t gdtr_limit = 0;
  // Task register (the 64-bit TSS: RSP0-2 and IST1-7) and LDT register.
  uint16_t tr = 0, ldtr = 0;
  uint64_t tr_base = 0, ldtr_base = 0;
  uint32_t tr_limit = 0, ldtr_limit = 0;
  uint64_t dr[8] = {0, 0, 0, 0, 0, 0, 0xffff0ff0, 0x400};
  // SYSCALL/SYSRET and SWAPGS.
  uint64_t star = 0, lstar = 0, cstar = 0, sfmask = 0, kernel_gs_base = 0;
  uint64_t sysenter_cs = 0, sysenter_esp = 0, sysenter_eip = 0;
  uint64_t pat = 0x0007040600070406ull, tsc_aux = 0, xcr0 = 1, apic_base = 0;
  Xmm xmm[16];
  uint32_t mxcsr = 0x1f80;
  // x87: control, status, abridged tag word, and ST0-ST7 as FXSAVE keeps
  // them (80 bits in each 16-byte slot).
  uint16_t fcw = 0x037f, fsw = 0, fop = 0;
  uint8_t ftw = 0;
  uint64_t fip = 0, fdp = 0;
  Xmm st[8];
  // An external interrupt waiting to be taken: its vector, or -1. The
  // engine delivers it through the IDT at the first instruction boundary
  // where RFLAGS.IF is set and no interrupt shadow is up, then clears it.
  int pending_vector = -1;
  // The pending event is a hardware exception the VMM raises (a #GP for an
  // MSR it refuses): taken regardless of RFLAGS.IF, with `pending_code`
  // pushed when it is not -1. RIP stays on the instruction that faulted.
  bool pending_exception = false;
  int64_t pending_code = -1;
  // The instruction after STI (IF was clear) or MOV SS runs before any
  // external interrupt.
  bool int_shadow = false;
  // Nonzero `tsc_per_insn`: RDTSC/RDTSCP read `tsc` without an exit, and
  // every retired instruction adds `tsc_per_insn` to it.
  uint64_t tsc = 0;
  uint64_t tsc_per_insn = 0;
};

enum class Exit : uint8_t {
  kHalt,       // HLT
  kIo,         // IN/OUT: port, size, write; OUT's value is in rax
  kHypercall,  // VMCALL / VMMCALL
  kCpuid,      // CPUID: leaf in eax, subleaf in ecx
  kRdtsc,      // RDTSC / RDTSCP
  kMsr,        // RDMSR / WRMSR: msr in ecx
  kException,  // a fault the guest has no IDT for, or could not take
  kBudget,     // the instruction budget ran out
  kShutdown,   // triple fault: a fault while delivering #DF
};

struct ExitInfo {
  Exit reason = Exit::kBudget;
  uint8_t len = 0;       // length of the instruction at rip (not retired)
  uint16_t port = 0;
  uint8_t size = 0;      // I/O access size in bytes
  bool write = false;    // OUT / WRMSR
  bool rdtscp = false;
  uint8_t vector = 0;    // kException
  uint64_t addr = 0;     // #PF linear address
  const char* why = "";  // what the engine could not do, for a report
};

// Guest physical memory: host bytes for [gpa, gpa + n), or null when that
// range is not mapped. `n` never crosses a 4KB page.
struct Memory {
  void* ctx = nullptr;
  uint8_t* (*gpa)(void* ctx, uint64_t gpa, uint64_t n) = nullptr;
};

// Architectural MSRs the engine keeps in State and answers without an exit:
// EFER, STAR, LSTAR, CSTAR, SFMASK, FS/GS base, kernel GS base, TSC_AUX,
// PAT, SYSENTER_*. Every other MSR leaves as Exit::kMsr.
bool OwnsMsr(uint32_t msr);

// Runs from s->rip until an exit or `budget` instructions have retired.
// `retired`, when given, receives the count.
Exit Run(State* s, const Memory& m, uint64_t budget, ExitInfo* info, uint64_t* retired = nullptr);

// The last guest fault the engine raised, delivered or not: its vector,
// the RIP of the faulting instruction, and what the engine could not do.
// Null before any fault.
const char* LastFault(uint8_t* vector, uint64_t* rip);

}  // namespace x86
}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_X86_HPP_
