// JitDll + the no-vCPU React fallback. jit_dll maps a PE and links its
// imports to ~/WASMReact's kernel32. On a VM with no hypervisor, the
// kernel installs two hooks that WASMWin32's software WHv (k32_whp_*)
// calls to reach WASMReact for an import the host catalog does not serve.
// This test drives that dispatch the way a React-band IAT thunk does.
#include "gocos/dispatch.h"
#include "gocos/jitrt.hpp"
#include "gocos/peinterp.hpp"
#include "wasmpe/loader.hpp"
#include "win32/dispatch.h"
#include "win32/metadata.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
void put32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
  for (int i = 0; i < 4; ++i) v[at + i] = static_cast<uint8_t>((x >> (8 * i)) & 0xff);
}
}  // namespace

int main() {
  char boot[4096];
  assert(wasmgocos_call("Boot", "", boot, sizeof(boot)) == 0);
  char react[4096];
  assert(wasmgocos_call("React", "Boot", react, sizeof(react)) == 0);
  assert(std::strstr(react, "state=up") != nullptr);

  // The kernel32 base API WASMReact answers is linkable; a bogus name is
  // not. This is the classification jit_dll and k32_pe_resolve link through.
  assert(wasmgocos::jit_react_export_index("CreateFileW") >= 0);
  assert(wasmgocos::jit_react_export_index("GetTickCount64") >= 0);
  assert(wasmgocos::jit_react_export_index("NtCreateFile") >= 0);  // ntdll -> ntoskrnl
  assert(wasmgocos::jit_react_export_index("NoSuchApi_ZZZ") < 0);

  // The kernel installed the hooks WASMWin32's software WHv uses when it
  // hits a React-band thunk while stepping a mapped PE with no vCPU.
  K32ReactIndex react_index = k32_get_react_index();
  K32ReactCall react_call = k32_get_react_call();
  assert(react_index != nullptr);
  assert(react_call != nullptr);
  assert(react_index("GetTickCount64") >= 0);
  assert(react_index("NoSuchApi_ZZZ") < 0);

  // Dispatch a kernel32 call the way a React-band IAT thunk resolves under
  // the software WHv: name -> index -> wasmreact_call. No hypervisor.
  int idx = react_index("GetTickCount64");
  char reply[256];
  reply[0] = 0;
  int rc = react_call(static_cast<unsigned>(idx), "", reply, sizeof(reply));
  assert(rc == 0);
  assert(reply[0] != 0);  // a tick count from WASMReact's own clock

  // GetVersion the same way, then a bad index is refused, not a crash.
  idx = react_index("GetVersion");
  assert(idx >= 0);
  assert(react_call(static_cast<unsigned>(idx), "", reply, sizeof(reply)) == 0);
  assert(react_call(0xffffffu, "", reply, sizeof(reply)) != 0);

  // JitDll on the built-in import-free DLL: it maps and reports.
  char out[8192];
  assert(wasmgocos_call("JitDll", "minimal", out, sizeof(out)) == 0);
  assert(std::strstr(out, "backend=react") != nullptr);
  assert(std::strstr(out, "imports=0") != nullptr);
  assert(std::strstr(out, "unresolved=0") != nullptr);

  // The portable interpreter (no vCPU, runs on Android) executes real
  // machine code: sum 1..10 in a loop -> eax == 55.
  //   31 C0            xor eax,eax
  //   B9 0A 00 00 00   mov ecx,10
  //   01 C8            add eax,ecx     <- loop
  //   49               dec ecx
  //   75 FB            jnz loop (-5)
  //   C3               ret
  {
    std::vector<uint8_t> prog = {0x31, 0xC0, 0xB9, 0x0A, 0x00, 0x00, 0x00,
                                 0x01, 0xC8, 0x49, 0x75, 0xFB, 0xC3};
    wasmgocos::PeInterpResult r = wasmgocos::pe_interp_run(prog.data(), prog.size(), 0, nullptr, 0);
    assert(!r.fault);
    assert(r.eax == 55);
  }

  // Marshalling is driven by win32metadata (per-parameter types), not a
  // per-API thunk: the JIT reads these signatures and marshals generically.
  {
    namespace md = wasmwin32meta;
    md::Sig s;
    assert(md::lookup("GetTickCount64", &s) && s.n == 0 && md::ret64(s.ret));
    assert(md::lookup("Sleep", &s) && s.n == 1 && s.p[0].type == md::U32);
    assert(md::lookup("CloseHandle", &s) && s.n == 1 && s.p[0].type == md::Handle);
    assert(md::lookup("GetSystemTimeAsFileTime", &s) && s.n == 1 && s.p[0].out);
    assert(md::lookup("CreateFileW", &s) && s.n == 7 && s.p[0].type == md::LpcWStr);
    assert(!md::lookup("NoSuchApi_ZZZ", &s));
  }

  // __stdcall dispatch driven by that metadata: GetTickCount64 takes 0 args
  // returns 64 bits in edx:eax. No vCPU.
  //   B8 <band>  mov eax,reactband
  //   FF D0      call eax
  //   C3         ret
  {
    int tick_idx = wasmgocos::jit_react_export_index("GetTickCount64");
    assert(tick_idx >= 0);
    uint32_t band = wasmgocos::kPeThunkReactBase + static_cast<uint32_t>(tick_idx);
    std::vector<uint8_t> prog(64, 0);
    size_t i = 0;
    prog[i++] = 0xB8; put32(prog, i, band); i += 4;  // mov eax,band
    prog[i++] = 0xFF; prog[i++] = 0xD0;              // call eax
    prog[i++] = 0xC3;                                // ret
    wasmgocos::PeInterpResult r = wasmgocos::pe_interp_run(prog.data(), prog.size(), 0, nullptr, 0);
    assert(!r.fault);
    assert(!r.last_reply.empty());
    uint64_t tick = std::strtoull(r.last_reply.c_str(), nullptr, 10);
    assert(tick > 0);
    assert(r.eax == static_cast<uint32_t>(tick));  // low 32 in eax (edx:eax)
  }

  // __stdcall with an argument + callee stack cleanup: Sleep(100). If the
  // callee did not pop its 1 dword arg, the final RET would pop 100 (an
  // unmapped address) and fault. fault==0 proves the stdcall cleanup.
  //   6A 64      push 100
  //   B8 <band>  mov eax,sleepband
  //   FF D0      call eax
  //   C3         ret
  {
    int sleep_idx = wasmgocos::jit_react_export_index("Sleep");
    assert(sleep_idx >= 0);
    uint32_t band = wasmgocos::kPeThunkReactBase + static_cast<uint32_t>(sleep_idx);
    std::vector<uint8_t> prog(64, 0);
    size_t i = 0;
    prog[i++] = 0x6A; prog[i++] = 0x64;              // push 100
    prog[i++] = 0xB8; put32(prog, i, band); i += 4;  // mov eax,band
    prog[i++] = 0xFF; prog[i++] = 0xD0;              // call eax
    prog[i++] = 0xC3;                                // ret
    wasmgocos::PeInterpResult r = wasmgocos::pe_interp_run(prog.data(), prog.size(), 0, nullptr, 0);
    assert(!r.fault);                            // stdcall cleaned the arg
    assert(r.last_reply == "ms=100");            // WASMReact saw the arg
  }

  // Typed file I/O, fully metadata-marshalled through the interpreter, no
  // vCPU: CreateFileW(L"\\Device\\Null", GENERIC_WRITE, ...) then
  // WriteFile(h, "hello", 5, ...). The access mask and the sized buffer are
  // marshalled from win32metadata (dwDesiredAccess; lpBuffer sized by param 2).
  {
    int cfw = wasmgocos::jit_react_export_index("CreateFileW");
    int wf = wasmgocos::jit_react_export_index("WriteFile");
    assert(cfw >= 0 && wf >= 0);
    uint32_t cfw_band = wasmgocos::kPeThunkReactBase + static_cast<uint32_t>(cfw);
    uint32_t wf_band = wasmgocos::kPeThunkReactBase + static_cast<uint32_t>(wf);
    uint32_t path_va = wasmgocos::kPeImageBase + 0x100;
    uint32_t buf_va = wasmgocos::kPeImageBase + 0x140;

    std::vector<uint8_t> prog(0x200, 0);
    size_t pi = 0x100;  // UTF-16LE L"\\Device\\Null"
    for (const char* p = "\\Device\\Null"; *p; ++p) {
      prog[pi++] = static_cast<uint8_t>(*p);
      prog[pi++] = 0;
    }
    std::memcpy(&prog[0x140], "hello", 5);

    size_t i = 0;
    auto p8 = [&](uint8_t v) { prog[i++] = 0x6A; prog[i++] = v; };            // push imm8
    auto p32 = [&](uint32_t v) { prog[i++] = 0x68; put32(prog, i, v); i += 4; };  // push imm32
    // CreateFileW(path, GENERIC_WRITE, share, sa, disp, flags, template)
    p8(0); p8(0); p8(0); p8(0); p8(0);   // template, flags, disp, sa, share
    p32(0x40000000);                     // dwDesiredAccess = GENERIC_WRITE
    p32(path_va);                        // lpFileName
    prog[i++] = 0xB8; put32(prog, i, cfw_band); i += 4;  // mov eax,band
    prog[i++] = 0xFF; prog[i++] = 0xD0;                  // call eax  -> eax=handle
    prog[i++] = 0x89; prog[i++] = 0xC3;                  // mov ebx,eax
    // WriteFile(handle, buf, 5, lpWritten, overlapped)
    p8(0); p8(0);                        // overlapped, lpNumberOfBytesWritten
    p8(5);                               // nNumberOfBytesToWrite
    p32(buf_va);                         // lpBuffer
    prog[i++] = 0x53;                    // push ebx (hFile)
    prog[i++] = 0xB8; put32(prog, i, wf_band); i += 4;   // mov eax,band
    prog[i++] = 0xFF; prog[i++] = 0xD0;                  // call eax
    prog[i++] = 0xC3;                                    // ret

    wasmgocos::PeInterpResult r = wasmgocos::pe_interp_run(prog.data(), prog.size(), 0, nullptr, 0);
    assert(!r.fault);
    assert(r.last_reply == "bytes=5");   // 5 buffer bytes marshalled to the kernel
  }

  // CreateEventW in win32metadata order (lpEventAttributes, bManualReset,
  // bInitialState, lpName), marshalled through the interpreter. bManualReset
  // is param 1 and bInitialState param 2 — the handler now reads them there,
  // so producer and consumer share the metadata wire order. (LpcWStr reading
  // is covered by the CreateFileW path; here lpName is null — an unnamed
  // event, since a bare name is not a rooted object path in this kernel.)
  {
    int cew = wasmgocos::jit_react_export_index("CreateEventW");
    assert(cew >= 0);
    uint32_t band = wasmgocos::kPeThunkReactBase + static_cast<uint32_t>(cew);
    std::vector<uint8_t> prog(64, 0);
    size_t i = 0;
    prog[i++] = 0x6A; prog[i++] = 0x00;              // push lpName = 0
    prog[i++] = 0x6A; prog[i++] = 0x00;              // push bInitialState = 0
    prog[i++] = 0x6A; prog[i++] = 0x01;              // push bManualReset = 1
    prog[i++] = 0x6A; prog[i++] = 0x00;              // push lpEventAttributes = 0
    prog[i++] = 0xB8; put32(prog, i, band); i += 4;  // mov eax,band
    prog[i++] = 0xFF; prog[i++] = 0xD0;              // call eax
    prog[i++] = 0xC3;                                // ret
    wasmgocos::PeInterpResult r = wasmgocos::pe_interp_run(prog.data(), prog.size(), 0, nullptr, 0);
    assert(!r.fault);
    assert(r.eax != 0);  // a real event handle
  }

  // End to end: map a real PE and run its entry on the interpreter through
  // the JitDll catalog API. Build a PE (WASMPELoader's minimal DLL, whose
  // entry is RVA 0x1000 / file offset 0x200) carrying the sum-1..10 loop.
  {
    std::vector<unsigned char> pe = wasmpe::minimal_dll();
    const unsigned char code[] = {0x31, 0xC0, 0xB9, 0x0A, 0x00, 0x00, 0x00,
                                  0x01, 0xC8, 0x49, 0x75, 0xFB, 0xC3};
    std::memcpy(&pe[0x200], code, sizeof(code));
    const char* fixture = "jitdll_run_fixture.bin";
    {
      std::ofstream o(fixture, std::ios::binary);
      o.write(reinterpret_cast<const char*>(pe.data()), static_cast<std::streamsize>(pe.size()));
    }
    char rr[512];
    int rc2 = wasmgocos_call("JitDll", (std::string("run") + '\x1f' + fixture).c_str(), rr,
                             sizeof(rr));
    std::remove(fixture);
    assert(rc2 == 0);
    assert(std::strstr(rr, "eax=55") != nullptr);
    assert(std::strstr(rr, "fault=0") != nullptr);
  }

  std::printf("dll_test: ok\n");
  return 0;
}
