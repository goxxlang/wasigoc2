#ifndef WASIGO_SYSROOT_WINHVPLATFORM_H_
#define WASIGO_SYSROOT_WINHVPLATFORM_H_

#include <windows.h>

typedef void VOID;

#include <cstdint>
#include <cstring>
#include <vector>

typedef void* WHV_PARTITION_HANDLE;

typedef enum WHV_REGISTER_NAME {
  WHvX64RegisterRax = 0,
  WHvX64RegisterRcx = 1,
  WHvX64RegisterRdx = 2,
  WHvX64RegisterRbx = 3,
  WHvX64RegisterRsp = 4,
  WHvX64RegisterRbp = 5,
  WHvX64RegisterRsi = 6,
  WHvX64RegisterRdi = 7,
  WHvX64RegisterR8 = 8,
  WHvX64RegisterR9 = 9,
  WHvX64RegisterR10 = 10,
  WHvX64RegisterR11 = 11,
  WHvX64RegisterR12 = 12,
  WHvX64RegisterR13 = 13,
  WHvX64RegisterR14 = 14,
  WHvX64RegisterR15 = 15,
  WHvX64RegisterRip = 16,
  WHvX64RegisterRflags = 17,
  WHvX64RegisterXmm0 = 18,
  WHvX64RegisterXmm1 = 19,
  WHvX64RegisterCr0 = 20,
  WHvX64RegisterCr3 = 21,
  WHvX64RegisterCr4 = 22,
  WHvX64RegisterEfer = 23,
  WHvX64RegisterCs = 24,
  WHvX64RegisterSs = 25,
  WHvX64RegisterDs = 26,
  WHvX64RegisterEs = 27,
  WHvX64RegisterGdtr = 28,
  WHvX64RegisterIdtr = 29,
  WHvX64RegisterFs = 30,
  WHvX64RegisterGs = 31,
  WHvX64RegisterCr2 = 32,
  WHvX64RegisterXmm2 = 33,
  WHvX64RegisterXmm3 = 34,
  WHvX64RegisterXmm4 = 35,
  WHvX64RegisterXmm5 = 36,
  WHvX64RegisterXmm6 = 37,
  WHvX64RegisterXmm7 = 38,
  WHvX64RegisterXmm8 = 39,
  WHvX64RegisterXmm9 = 40,
  WHvX64RegisterXmm10 = 41,
  WHvX64RegisterXmm11 = 42,
  WHvX64RegisterXmm12 = 43,
  WHvX64RegisterXmm13 = 44,
  WHvX64RegisterXmm14 = 45,
  WHvX64RegisterXmm15 = 46,
  // System state a guest kernel owns: task and LDT registers, debug
  // registers, the SYSCALL/SWAPGS MSRs, and the x87 file FXSAVE carries.
  WHvX64RegisterTr = 47,
  WHvX64RegisterLdtr = 48,
  WHvX64RegisterCr8 = 49,
  WHvX64RegisterDr0 = 50,
  WHvX64RegisterDr1 = 51,
  WHvX64RegisterDr2 = 52,
  WHvX64RegisterDr3 = 53,
  WHvX64RegisterDr6 = 54,
  WHvX64RegisterDr7 = 55,
  WHvX64RegisterKernelGsBase = 56,
  WHvX64RegisterStar = 57,
  WHvX64RegisterLstar = 58,
  WHvX64RegisterCstar = 59,
  WHvX64RegisterSfmask = 60,
  WHvX64RegisterSysenterCs = 61,
  WHvX64RegisterSysenterEip = 62,
  WHvX64RegisterSysenterEsp = 63,
  WHvX64RegisterPat = 64,
  WHvX64RegisterTscAux = 65,
  WHvX64RegisterXCr0 = 66,
  WHvX64RegisterApicBase = 67,
  WHvX64RegisterTsc = 68,
  // Event injection: WHV_X64_PENDING_INTERRUPTION_REGISTER's layout.
  // Bit 0 pending, bits 1-3 type (0 external), bits 16-31 vector. The
  // processor clears it when it delivers the interrupt.
  WHvRegisterPendingInterruption = 69,
  // Bit 0: interrupt shadow (the instruction after STI or MOV SS).
  WHvRegisterInterruptState = 70,
  WHvX64RegisterFpMmx0 = 71,
  WHvX64RegisterFpMmx1 = 72,
  WHvX64RegisterFpMmx2 = 73,
  WHvX64RegisterFpMmx3 = 74,
  WHvX64RegisterFpMmx4 = 75,
  WHvX64RegisterFpMmx5 = 76,
  WHvX64RegisterFpMmx6 = 77,
  WHvX64RegisterFpMmx7 = 78,
  // Low64: FCW (0-15), FSW (16-31), abridged FTW (32-39), FOP (48-63).
  // High64: last x87 instruction pointer.
  WHvX64RegisterFpControlStatus = 79,
  // Low64: last x87 data pointer. High64: MXCSR (0-31), MXCSR mask (32-63).
  WHvX64RegisterXmmControlStatus = 80,
  WHvRegisterCount = 81
} WHV_REGISTER_NAME;

typedef struct WHV_X64_SEGMENT_REGISTER {
  UINT64 Base;
  UINT32 Limit;
  UINT16 Selector;
  UINT16 SegmentType;
  UINT16 NonSystemSegment;
  UINT16 DescriptorPrivilegeLevel;
  UINT16 Present;
  UINT16 Long;
  UINT16 Default;
  UINT16 Granularity;
} WHV_X64_SEGMENT_REGISTER;

typedef struct WHV_X64_TABLE_REGISTER {
  UINT64 Base;
  UINT16 Limit;
} WHV_X64_TABLE_REGISTER;

typedef union WHV_REGISTER_VALUE {
  struct {
    UINT64 Low64;
    UINT64 High64;
  } Reg128;
  UINT64 Reg64;
  UINT32 Reg32;
  WHV_X64_SEGMENT_REGISTER Segment;
  WHV_X64_TABLE_REGISTER Table;
} WHV_REGISTER_VALUE;

typedef enum WHV_RUN_VP_EXIT_REASON {
  WHvRunVpExitReasonNone = 0,
  WHvRunVpExitReasonX64Halt = 1,
  WHvRunVpExitReasonX64IoPortAccess = 2,
  WHvRunVpExitReasonX64Cpuid = 3,
  WHvRunVpExitReasonX64MsrAccess = 4,
  WHvRunVpExitReasonX64Rdtsc = 5,
  WHvRunVpExitReasonHypercall = 6,
  WHvRunVpExitReasonUnrecoverableException = 7,
  // The run's instruction budget (WasigoWhpSetRunBudget) ran out; the VMM
  // gets the processor back between instructions, the way
  // WHvCancelRunVirtualProcessor hands it back on the platform.
  WHvRunVpExitReasonCanceled = 8
} WHV_RUN_VP_EXIT_REASON;

typedef struct WHV_RUN_VP_EXIT_CONTEXT {
  WHV_RUN_VP_EXIT_REASON ExitReason;
  struct {
    UINT64 Rip;
    UINT32 InstructionLength;
  } VpContext;
  union {
    struct {
      UINT64 Rax;
      UINT64 DefaultResultRax;
      UINT64 DefaultResultRbx;
      UINT64 DefaultResultRcx;
      UINT64 DefaultResultRdx;
    } CpuidAccess;
    struct {
      UINT32 MsrNumber;
      UINT64 Rax;
      UINT64 Rdx;
      struct {
        UINT32 IsWrite;
      } AccessInfo;
    } MsrAccess;
    struct {
      UINT64 Tsc;
      UINT64 TscAux;
      struct {
        UINT32 IsRdtscp;
      } RdtscInfo;
    } ReadTsc;
    struct {
      UINT16 PortNumber;
      struct {
        UINT32 IsWrite;
        UINT32 AccessSize;  // 1, 2, or 4
      } AccessInfo;
      UINT64 Rax;
    } IoPortAccess;
  };
  // UnrecoverableException: the vector, and what the processor could not do.
  UINT32 ExceptionVector;
  const char* Why;
} WHV_RUN_VP_EXIT_CONTEXT;

typedef union WHV_EXTENDED_VM_EXITS {
  struct {
    UINT64 X64CpuidExit : 1;
    UINT64 X64MsrExit : 1;
    UINT64 X64RdtscExit : 1;
    UINT64 HypercallExit : 1;
  };
  UINT64 AsUINT64;
} WHV_EXTENDED_VM_EXITS;

typedef struct WHV_X64_MSR_EXIT_BITMAP {
  UINT64 UnhandledMsrs;
} WHV_X64_MSR_EXIT_BITMAP;

typedef union WHV_CAPABILITY {
  BOOL HypervisorPresent;
  WHV_EXTENDED_VM_EXITS ExtendedVmExits;
  UINT64 AsUINT64;
} WHV_CAPABILITY;

typedef enum WHV_CAPABILITY_CODE {
  WHvCapabilityCodeHypervisorPresent = 0,
  WHvCapabilityCodeExtendedVmExits = 1
} WHV_CAPABILITY_CODE;

typedef enum WHV_PARTITION_PROPERTY_CODE {
  WHvPartitionPropertyCodeProcessorCount = 0,
  WHvPartitionPropertyCodeExtendedVmExits = 1,
  WHvPartitionPropertyCodeX64MsrExitBitmap = 2
} WHV_PARTITION_PROPERTY_CODE;

typedef enum WHV_MAP_GPA_RANGE_FLAGS {
  WHvMapGpaRangeFlagNone = 0,
  WHvMapGpaRangeFlagRead = 1,
  WHvMapGpaRangeFlagWrite = 2,
  WHvMapGpaRangeFlagExecute = 4
} WHV_MAP_GPA_RANGE_FLAGS;

struct WasigoWhpMap {
  UINT64 gpa;
  UINT64 bytes;
  void* host;
};

struct WasigoWhp {
  int live;
  int vp;
  WHV_REGISTER_VALUE reg[WHvRegisterCount];
  std::vector<WasigoWhpMap> maps;
  // Instructions one WHvRunVirtualProcessor may retire before it returns
  // WHvRunVpExitReasonCanceled. 0 runs until the guest exits.
  UINT64 run_budget;
  // Nonzero: RDTSC reads the partition's TSC register, which advances
  // `tsc_per_insn` per retired instruction, and does not exit.
  int tsc_virtual;
  UINT64 tsc_per_insn;
  // Instructions retired by this partition's processor, all runs.
  UINT64 retired;
};

// The first partition created. Every other partition is its own
// WasigoWhp, named by its WHV_PARTITION_HANDLE.
inline WasigoWhp& WasigoWhpState() {
  static WasigoWhp whp;
  return whp;
}

inline WasigoWhp* WasigoWhpOf(WHV_PARTITION_HANDLE p) {
  return p ? static_cast<WasigoWhp*>(p) : &WasigoWhpState();
}

inline HRESULT WasigoWhpSetRunBudget(WHV_PARTITION_HANDLE p, UINT64 instructions) {
  WasigoWhp* w = WasigoWhpOf(p);
  if (!w->live) return E_FAIL;
  w->run_budget = instructions;
  return S_OK;
}

inline HRESULT WasigoWhpSetVirtualTsc(WHV_PARTITION_HANDLE p, UINT64 tsc_per_insn) {
  WasigoWhp* w = WasigoWhpOf(p);
  if (!w->live) return E_FAIL;
  w->tsc_virtual = tsc_per_insn != 0;
  w->tsc_per_insn = tsc_per_insn;
  return S_OK;
}

inline uint8_t* WasigoWhpHostIn(WasigoWhp& w, UINT64 gpa) {
  for (const WasigoWhpMap& m : w.maps) {
    if (gpa >= m.gpa && gpa < m.gpa + m.bytes && m.host)
      return static_cast<uint8_t*>(m.host) + static_cast<size_t>(gpa - m.gpa);
  }
  return nullptr;
}

inline uint8_t* WasigoWhpHost(UINT64 gpa) {
  WasigoWhp& w = WasigoWhpState();
  for (const WasigoWhpMap& m : w.maps) {
    if (gpa >= m.gpa && gpa < m.gpa + m.bytes && m.host)
      return static_cast<uint8_t*>(m.host) + static_cast<size_t>(gpa - m.gpa);
  }
  return nullptr;
}

inline HRESULT WHvGetCapability(WHV_CAPABILITY_CODE code, VOID* buffer, UINT32 buffer_size,
                                UINT32* written) {
  if (!buffer || buffer_size < sizeof(WHV_CAPABILITY)) return E_FAIL;
  WHV_CAPABILITY* cap = static_cast<WHV_CAPABILITY*>(buffer);
  std::memset(cap, 0, sizeof(*cap));
  if (code == WHvCapabilityCodeHypervisorPresent) cap->HypervisorPresent = 1;
  if (code == WHvCapabilityCodeExtendedVmExits) cap->ExtendedVmExits.AsUINT64 = ~UINT64{0};
  if (written) *written = sizeof(WHV_CAPABILITY);
  return S_OK;
}

// The first partition is WasigoWhpState(); a partition created while that
// one is live is a WasigoWhp of its own.
inline HRESULT WHvCreatePartition(WHV_PARTITION_HANDLE* partition) {
  if (!partition) return E_FAIL;
  WasigoWhp* w = &WasigoWhpState();
  if (w->live) w = new WasigoWhp();
  *w = WasigoWhp{};
  w->live = 1;
  *partition = w;
  return S_OK;
}

inline HRESULT WHvSetPartitionProperty(WHV_PARTITION_HANDLE p, WHV_PARTITION_PROPERTY_CODE, const VOID*,
                                       UINT32) {
  return WasigoWhpOf(p)->live ? S_OK : E_FAIL;
}

inline HRESULT WHvSetupPartition(WHV_PARTITION_HANDLE p) {
  return WasigoWhpOf(p)->live ? S_OK : E_FAIL;
}

inline HRESULT WHvMapGpaRange(WHV_PARTITION_HANDLE p, VOID* source, UINT64 gpa, UINT64 bytes,
                              WHV_MAP_GPA_RANGE_FLAGS) {
  WasigoWhp* w = WasigoWhpOf(p);
  if (!w->live || !source || bytes == 0) return E_FAIL;
  WasigoWhpMap map;
  map.gpa = gpa;
  map.bytes = bytes;
  map.host = source;
  w->maps.push_back(map);
  return S_OK;
}

inline HRESULT WHvUnmapGpaRange(WHV_PARTITION_HANDLE p, UINT64 gpa, UINT64) {
  WasigoWhp& w = *WasigoWhpOf(p);
  for (auto it = w.maps.begin(); it != w.maps.end(); ++it) {
    if (it->gpa == gpa) {
      w.maps.erase(it);
      break;
    }
  }
  return S_OK;
}

inline HRESULT WHvCreateVirtualProcessor(WHV_PARTITION_HANDLE p, UINT32, UINT32) {
  WasigoWhp* w = WasigoWhpOf(p);
  if (!w->live) return E_FAIL;
  w->vp = 1;
  return S_OK;
}

inline HRESULT WHvDeleteVirtualProcessor(WHV_PARTITION_HANDLE p, UINT32) {
  WasigoWhpOf(p)->vp = 0;
  return S_OK;
}

inline HRESULT WHvDeletePartition(WHV_PARTITION_HANDLE p) {
  WasigoWhp* w = WasigoWhpOf(p);
  if (w == &WasigoWhpState())
    *w = WasigoWhp{};
  else
    delete w;
  return S_OK;
}

inline HRESULT WHvSetVirtualProcessorRegisters(WHV_PARTITION_HANDLE p, UINT32,
                                               const WHV_REGISTER_NAME* names,
                                               UINT32 count, const WHV_REGISTER_VALUE* values) {
  if (!names || !values) return E_FAIL;
  WasigoWhp& w = *WasigoWhpOf(p);
  for (UINT32 i = 0; i < count; i++) {
    if (names[i] < 0 || names[i] >= WHvRegisterCount) return E_FAIL;
    w.reg[names[i]] = values[i];
  }
  return S_OK;
}

inline HRESULT WHvGetVirtualProcessorRegisters(WHV_PARTITION_HANDLE p, UINT32,
                                               const WHV_REGISTER_NAME* names, UINT32 count,
                                               WHV_REGISTER_VALUE* values) {
  if (!names || !values) return E_FAIL;
  WasigoWhp& w = *WasigoWhpOf(p);
  for (UINT32 i = 0; i < count; i++) {
    if (names[i] < 0 || names[i] >= WHvRegisterCount) return E_FAIL;
    values[i] = w.reg[names[i]];
  }
  return S_OK;
}

// The instruction engine behind this partition (WASMGocOS src/x86.cc,
// through src/vcpu_whp.cc): the virtual processor executes guest code
// until an exit. Without it, the few instructions below are all it runs.
extern "C" __attribute__((weak)) HRESULT wasigo_vcpu_run(WasigoWhp* whp,
                                                       WHV_RUN_VP_EXIT_CONTEXT* exit);

inline HRESULT WHvRunVirtualProcessor(WHV_PARTITION_HANDLE partition, UINT32, VOID* context, UINT32) {
  WasigoWhp& w = *WasigoWhpOf(partition);
  if (wasigo_vcpu_run) {
    if (!context || !w.vp) return E_FAIL;
    return wasigo_vcpu_run(&w, static_cast<WHV_RUN_VP_EXIT_CONTEXT*>(context));
  }
  if (!context || !w.vp) return E_FAIL;
  WHV_RUN_VP_EXIT_CONTEXT* exit = static_cast<WHV_RUN_VP_EXIT_CONTEXT*>(context);
  std::memset(exit, 0, sizeof(*exit));
  for (int step = 0; step < 100000; step++) {
    UINT64 rip = w.reg[WHvX64RegisterRip].Reg64;
    uint8_t* p = WasigoWhpHostIn(w, rip);
    if (!p) {
      exit->ExitReason = WHvRunVpExitReasonX64Halt;
      exit->VpContext.Rip = rip;
      exit->VpContext.InstructionLength = 1;
      return S_OK;
    }
    uint8_t op = p[0];
    if (op == 0xC3) {
      uint8_t* sp = WasigoWhpHostIn(w, w.reg[WHvX64RegisterRsp].Reg64);
      if (!sp) break;
      UINT64 ret = 0;
      std::memcpy(&ret, sp, 8);
      w.reg[WHvX64RegisterRsp].Reg64 += 8;
      w.reg[WHvX64RegisterRip].Reg64 = ret;
      continue;
    }
    if (op == 0x90) {
      w.reg[WHvX64RegisterRip].Reg64 = rip + 1;
      continue;
    }
    exit->VpContext.Rip = rip;
    if (op == 0xF4) {
      exit->ExitReason = WHvRunVpExitReasonX64Halt;
      exit->VpContext.InstructionLength = 1;
      return S_OK;
    }
    if (op == 0xE6 || op == 0xE4) {
      exit->ExitReason = WHvRunVpExitReasonX64IoPortAccess;
      exit->VpContext.InstructionLength = 2;
      exit->IoPortAccess.PortNumber = p[1];
      return S_OK;
    }
    if (op == 0xEE || op == 0xEC) {
      exit->ExitReason = WHvRunVpExitReasonX64IoPortAccess;
      exit->VpContext.InstructionLength = 1;
      exit->IoPortAccess.PortNumber = static_cast<UINT16>(w.reg[WHvX64RegisterRdx].Reg64);
      return S_OK;
    }
    if (op == 0x0F && (p[1] == 0x01) && (p[2] == 0xC1 || p[2] == 0xD9)) {
      exit->ExitReason = WHvRunVpExitReasonHypercall;
      exit->VpContext.InstructionLength = 3;
      return S_OK;
    }
    if (op == 0x0F && p[1] == 0xA2) {
      exit->ExitReason = WHvRunVpExitReasonX64Cpuid;
      exit->VpContext.InstructionLength = 2;
      exit->CpuidAccess.Rax = w.reg[WHvX64RegisterRax].Reg64;
      return S_OK;
    }
    if (op == 0x0F && p[1] == 0x31) {
      exit->ExitReason = WHvRunVpExitReasonX64Rdtsc;
      exit->VpContext.InstructionLength = 2;
      return S_OK;
    }
    exit->ExitReason = WHvRunVpExitReasonX64Halt;
    exit->VpContext.InstructionLength = 1;
    return S_OK;
  }
  exit->ExitReason = WHvRunVpExitReasonUnrecoverableException;
  exit->VpContext.InstructionLength = 1;
  return S_OK;
}

#endif
