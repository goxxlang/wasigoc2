#ifndef WASMGOCOS_INCLUDE_GOCOS_PEINTERP_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_PEINTERP_HPP_

// Portable software x86 (IA-32) interpreter for the no-vCPU path. Unlike
// ~/WASMWin32's K32Whp (native only, tied to WHv emulation), this depends
// on nothing but a flat byte image, so it runs in the wasm / Android
// build where there is no hypervisor. ~/WASMGocOS's jit_dll maps a PE and
// links its IAT to catalog thunk bands; this executes the mapped code and
// dispatches a band call to its catalog: the host band to wasmwin32_call
// (kernel32 in-process on wasm), the React band to wasmreact_call
// (~/WASMReact's own kernel32). It is a real decoder — arithmetic,
// branches, loops, a stack, call/ret — not a straight-line stub runner.

#include <cstddef>
#include <cstdint>
#include <string>

namespace wasmgocos {

// The thunk bands jit_dll / k32_pe_resolve write into an IAT slot.
constexpr uint32_t kPeThunkHostBase = 0x10000u;
constexpr uint32_t kPeThunkReactBase = 0x40000000u;

// The virtual address a mapped image is placed at (RVA r -> base + r).
constexpr uint32_t kPeImageBase = 0x00400000u;

struct PeInterpResult {
  uint32_t eax = 0;    // return value in eax
  long steps = 0;      // instructions retired, or -1 on a fault
  bool fault = false;  // an opcode the decoder does not implement, or bad memory
  std::string last_reply;  // the reply of the last catalog band call
};

// Run `len` bytes of 32-bit code as a flat image at kPeImageBase. `regs4`
// (eax, ecx, edx, ebx) seeds those registers and, when non-null, receives
// them back. Execution starts at `entry_rva`, on a fresh stack, and stops
// at RET to the top frame, HLT/INT3, the budget, or a fault.
PeInterpResult pe_interp_run(const uint8_t* code, size_t len, uint32_t entry_rva,
                             const uint32_t* regs4, unsigned budget);

}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_PEINTERP_HPP_
