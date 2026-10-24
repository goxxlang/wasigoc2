// The x86 engine (src/x86.cc) against the host CPU. Each case is machine
// code ending in RET. It runs natively (tests/x86_native.S) and in the
// engine from the same registers and memory, over random states, and the
// general registers, the flags the case defines, and memory must agree.
// Native x86-64 Windows host only.

#include "gocos/x86.hpp"
#include "gocos/x86_cases.hpp"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

extern "C" void x86_native_run(uint64_t* ctx, void* code);

namespace {

using wasmgocos::x86::Exit;
using wasmgocos::x86::ExitInfo;
using wasmgocos::x86::Memory;
using wasmgocos::x86::State;
using namespace wasmgocos::x86::cases;

uint8_t* HostGpa(void*, uint64_t gpa, uint64_t) { return reinterpret_cast<uint8_t*>(gpa); }

bool RunCase(const Case& c, uint8_t* page, std::mt19937_64& rng, std::string* err) {
  // code at page, a HLT at page+0x800 the engine's top frame returns to
  std::memset(page, 0x90, 0x1000);
  std::memcpy(page, c.code.data(), c.code.size());
  page[c.code.size()] = 0xC3;
  page[0x800] = 0xF4;
  FlushInstructionCache(GetCurrentProcess(), page, 0x1000);

  alignas(16) static uint8_t mem_native[256], mem_engine[256], seed[256];
  alignas(16) static uint8_t stack[0x4000];
  uint64_t ctx[17];
  for (int i = 0; i < 16; ++i) ctx[i] = Pick(rng);
  ctx[16] = (rng() & (CF | PF | AF | ZF | SF | OF)) | 0x202;
  for (auto& b : seed) b = static_cast<uint8_t>(rng());

  switch (c.kind) {
    case kMem:
    case kString:
      ctx[1] = rng() % 8;   // rcx
      ctx[3] = rng() % 16;  // rbx
      if (c.kind == kString) ctx[1] = rng() % 14;
      break;
    case kDivU:
      ctx[2] = 0;
      ctx[3] = (rng() & 0xffffff) | 1;
      break;
    case kDivS:
      ctx[3] = static_cast<uint64_t>(static_cast<int64_t>((rng() & 0xffff) + 2) * ((rng() & 1) ? -1 : 1));
      break;
    default:
      break;
  }
  auto place = [&](uint64_t* g, uint8_t* mem) {
    if (c.kind == kMem || c.kind == kString) {
      g[6] = reinterpret_cast<uint64_t>(mem);         // rsi
      g[7] = reinterpret_cast<uint64_t>(mem + 128);   // rdi
      if (c.kind == kMem) g[7] = reinterpret_cast<uint64_t>(mem + 64);
    }
  };

  // native
  uint64_t n[17];
  std::memcpy(n, ctx, sizeof(n));
  std::memcpy(mem_native, seed, sizeof(seed));
  place(n, mem_native);
  uint64_t in_rsi = n[6], in_rdi = n[7];
  x86_native_run(n, page);

  // engine
  std::memcpy(mem_engine, seed, sizeof(seed));
  State s;
  for (int i = 0; i < 16; ++i) s.gpr[i] = ctx[i];
  place(s.gpr, mem_engine);
  s.gpr[11] = reinterpret_cast<uint64_t>(page);  // the harness calls through r11
  uint64_t sp = reinterpret_cast<uint64_t>(stack + sizeof(stack) - 64);
  uint64_t halt = reinterpret_cast<uint64_t>(page + 0x800);
  std::memcpy(reinterpret_cast<void*>(sp), &halt, 8);
  s.gpr[4] = sp;
  s.rflags = ctx[16];
  s.rip = reinterpret_cast<uint64_t>(page);
  Memory m{nullptr, HostGpa};
  ExitInfo info;
  Exit x = wasmgocos::x86::Run(&s, m, 100000, &info);
  if (x != Exit::kHalt) {
    char b[160];
    std::snprintf(b, sizeof(b), "engine stopped: reason=%d vector=%d why=%s", static_cast<int>(x),
                  info.vector, info.why);
    *err = b;
    return false;
  }

  // compare (rsp: not loaded natively; memory pointers: rebase)
  auto rebase = [&](int i, uint64_t v) -> uint64_t {
    if (c.kind != kMem && c.kind != kString) return v;
    uint64_t nb = reinterpret_cast<uint64_t>(mem_native), eb = reinterpret_cast<uint64_t>(mem_engine);
    if (v >= eb && v <= eb + 256) return v - eb + nb;
    (void)i;
    return v;
  };
  (void)in_rsi;
  (void)in_rdi;
  for (int i = 0; i < 16; ++i) {
    if (i == 4 || i == 11) continue;
    uint64_t ev = rebase(i, s.gpr[i]);
    if (n[i] != ev) {
      char b[160];
      std::snprintf(b, sizeof(b), "gpr%d native=%016llx engine=%016llx", i,
                    static_cast<unsigned long long>(n[i]), static_cast<unsigned long long>(ev));
      *err = b;
      return false;
    }
  }
  if ((n[16] & c.flags) != (s.rflags & c.flags)) {
    char b[160];
    std::snprintf(b, sizeof(b), "flags native=%03llx engine=%03llx (mask %03llx)",
                  static_cast<unsigned long long>(n[16] & c.flags),
                  static_cast<unsigned long long>(s.rflags & c.flags),
                  static_cast<unsigned long long>(c.flags));
    *err = b;
    return false;
  }
  if (std::memcmp(mem_native, mem_engine, sizeof(mem_native)) != 0) {
    *err = "memory differs";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  auto* page = static_cast<uint8_t*>(
      VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  if (!page) return 2;
  std::mt19937_64 rng(0x5eed);
  int failed = 0, total = 0;
  for (const Case& c : Cases()) {
    int bad = 0;
    std::string first;
    for (int t = 0; t < 400; ++t) {
      std::string err;
      ++total;
      if (!RunCase(c, page, rng, &err)) {
        if (!bad) first = err;
        ++bad;
      }
    }
    if (bad) {
      ++failed;
      std::printf("FAIL %-32s %d/400  %s\n", c.name, bad, first.c_str());
    }
  }
  std::printf("%zu cases, %d runs, %d failing cases\n", Cases().size(), total, failed);
  return failed ? 1 : 0;
}
