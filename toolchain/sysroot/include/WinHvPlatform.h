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
  WHvRegisterCount = 47
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
  WHvRunVpExitReasonUnrecoverableException = 7
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
    } IoPortAccess;
  };
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
};

inline WasigoWhp& WasigoWhpState() {
  static WasigoWhp whp;
  return whp;
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

inline HRESULT WHvCreatePartition(WHV_PARTITION_HANDLE* partition) {
  if (!partition) return E_FAIL;
  WasigoWhp& w = WasigoWhpState();
  w = WasigoWhp{};
  w.live = 1;
  *partition = &w;
  return S_OK;
}

inline HRESULT WHvSetPartitionProperty(WHV_PARTITION_HANDLE, WHV_PARTITION_PROPERTY_CODE, const VOID*,
                                       UINT32) {
  return WasigoWhpState().live ? S_OK : E_FAIL;
}

inline HRESULT WHvSetupPartition(WHV_PARTITION_HANDLE) {
  return WasigoWhpState().live ? S_OK : E_FAIL;
}

inline HRESULT WHvMapGpaRange(WHV_PARTITION_HANDLE, VOID* source, UINT64 gpa, UINT64 bytes,
                              WHV_MAP_GPA_RANGE_FLAGS) {
  if (!WasigoWhpState().live || !source || bytes == 0) return E_FAIL;
  WasigoWhpMap map;
  map.gpa = gpa;
  map.bytes = bytes;
  map.host = source;
  WasigoWhpState().maps.push_back(map);
  return S_OK;
}

inline HRESULT WHvUnmapGpaRange(WHV_PARTITION_HANDLE, UINT64 gpa, UINT64) {
  WasigoWhp& w = WasigoWhpState();
  for (auto it = w.maps.begin(); it != w.maps.end(); ++it) {
    if (it->gpa == gpa) {
      w.maps.erase(it);
      break;
    }
  }
  return S_OK;
}

inline HRESULT WHvCreateVirtualProcessor(WHV_PARTITION_HANDLE, UINT32, UINT32) {
  if (!WasigoWhpState().live) return E_FAIL;
  WasigoWhpState().vp = 1;
  return S_OK;
}

inline HRESULT WHvDeleteVirtualProcessor(WHV_PARTITION_HANDLE, UINT32) {
  WasigoWhpState().vp = 0;
  return S_OK;
}

inline HRESULT WHvDeletePartition(WHV_PARTITION_HANDLE) {
  WasigoWhpState() = WasigoWhp{};
  return S_OK;
}

inline HRESULT WHvSetVirtualProcessorRegisters(WHV_PARTITION_HANDLE, UINT32,
                                               const WHV_REGISTER_NAME* names,
                                               UINT32 count, const WHV_REGISTER_VALUE* values) {
  if (!names || !values) return E_FAIL;
  WasigoWhp& w = WasigoWhpState();
  for (UINT32 i = 0; i < count; i++) {
    if (names[i] < 0 || names[i] >= WHvRegisterCount) return E_FAIL;
    w.reg[names[i]] = values[i];
  }
  return S_OK;
}

inline HRESULT WHvGetVirtualProcessorRegisters(WHV_PARTITION_HANDLE, UINT32,
                                               const WHV_REGISTER_NAME* names, UINT32 count,
                                               WHV_REGISTER_VALUE* values) {
  if (!names || !values) return E_FAIL;
  WasigoWhp& w = WasigoWhpState();
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

inline HRESULT WHvRunVirtualProcessor(WHV_PARTITION_HANDLE, UINT32, VOID* context, UINT32) {
  if (wasigo_vcpu_run) {
    if (!context || !WasigoWhpState().vp) return E_FAIL;
    return wasigo_vcpu_run(&WasigoWhpState(), static_cast<WHV_RUN_VP_EXIT_CONTEXT*>(context));
  }
  if (!context || !WasigoWhpState().vp) return E_FAIL;
  WHV_RUN_VP_EXIT_CONTEXT* exit = static_cast<WHV_RUN_VP_EXIT_CONTEXT*>(context);
  std::memset(exit, 0, sizeof(*exit));
  WasigoWhp& w = WasigoWhpState();
  for (int step = 0; step < 100000; step++) {
    UINT64 rip = w.reg[WHvX64RegisterRip].Reg64;
    uint8_t* p = WasigoWhpHost(rip);
    if (!p) {
      exit->ExitReason = WHvRunVpExitReasonX64Halt;
      exit->VpContext.Rip = rip;
      exit->VpContext.InstructionLength = 1;
      return S_OK;
    }
    uint8_t op = p[0];
    if (op == 0xC3) {
      uint8_t* sp = WasigoWhpHost(w.reg[WHvX64RegisterRsp].Reg64);
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
