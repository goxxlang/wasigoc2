// The vCPU inside a wasigocvm module. Same symbols native TurboSpace
// exports for the process partition. There is no WinHvPlatform.lib here:
// the partition is the WinHvPlatform DLL catalog (wasmwin32_call).

#include "win32/dispatch.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kVmGpaBase = 0x200000;
constexpr uint64_t kVmBytes = 2 * 1024 * 1024;

struct Mapped {
  uint64_t gpa = 0;
  uint32_t bytes = 0;
};

struct Vcpu {
  bool attached = false;
  std::string part;
  uint64_t bump = kVmGpaBase + 4096;
  uint32_t k32_exits = 0;
  uint32_t io_exits = 0;
  std::vector<Mapped> maps;
};

Vcpu& Cpu() {
  static Vcpu cpu;
  return cpu;
}

bool Call(const char* api, const std::string& args, std::string* reply) {
  char buf[4096];
  buf[0] = 0;
  int rc = wasmwin32_call(api, args.c_str(), buf, sizeof(buf));
  if (reply != nullptr) *reply = buf;
  return rc == 0;
}

std::string Field(const std::string& s, unsigned n) {
  unsigned i = 0;
  size_t at = 0;
  while (i < n) {
    at = s.find('\x1f', at);
    if (at == std::string::npos) return {};
    ++at;
    ++i;
  }
  size_t end = s.find('\x1f', at);
  if (end == std::string::npos) return s.substr(at);
  return s.substr(at, end - at);
}

bool Attach() {
  Vcpu& cpu = Cpu();
  if (cpu.attached) return true;
  std::string cap;
  if (!Call("WHvGetCapability", "", &cap) || cap != "1") return false;
  std::string part;
  if (!Call("WHvCreatePartition", "", &part) || part.empty()) return false;
  part = Field(part, 0);
  Call("WHvSetPartitionProperty", part + "\x1f" "ProcessorCount" "\x1f" "1", nullptr);
  if (!Call("WHvSetupPartition", part, nullptr)) return false;

  // The OUT stub has to stay mapped for every later hypercall. A
  // temporary buffer is gone before the guest runs.
  static uint8_t stub[4096];
  std::memset(stub, 0, sizeof(stub));
  stub[0] = 0xE6;
  stub[1] = 0x01;
  stub[2] = 0xC3;
  std::string map = part + "\x1f" + std::to_string(kVmGpaBase) + "\x1f" "4096" "\x1f" "7" "\x1f" +
                    std::to_string(reinterpret_cast<uintptr_t>(stub));
  if (!Call("WHvMapGpaRange", map, nullptr)) return false;
  if (!Call("WHvCreateVirtualProcessor", part + "\x1f" "0", nullptr)) return false;

  cpu.part = part;
  cpu.attached = true;
  cpu.bump = kVmGpaBase + 4096;
  return true;
}

}  // namespace

extern "C" int32_t wasmturbo_vcpu_attach(char* out, unsigned cap) {
  if (!Attach()) {
    if (out != nullptr && cap != 0) out[0] = 0;
    return -1;
  }
  if (out != nullptr && cap != 0) {
    std::snprintf(out, cap, "vp=0\x1f" "base=%llu\x1f" "bytes=%llu",
                  static_cast<unsigned long long>(kVmGpaBase),
                  static_cast<unsigned long long>(kVmBytes));
  }
  return 0;
}

extern "C" int32_t wasmturbo_vcpu_map(uint32_t bytes, char* out, unsigned cap) {
  if (!Attach() || bytes == 0) return -1;
  uint32_t pages = (bytes + 4095u) / 4096u;
  uint64_t need = static_cast<uint64_t>(pages) * 4096u;
  Vcpu& cpu = Cpu();
  if (cpu.bump < kVmGpaBase + 4096 || cpu.bump + need > kVmGpaBase + kVmBytes) return -1;
  std::vector<uint8_t> page(static_cast<size_t>(need), 0);
  std::string map = cpu.part + "\x1f" + std::to_string(cpu.bump) + "\x1f" + std::to_string(need) +
                    "\x1f" "7" "\x1f" + std::to_string(reinterpret_cast<uintptr_t>(page.data()));
  if (!Call("WHvMapGpaRange", map, nullptr)) return -1;
  Mapped slot;
  slot.gpa = cpu.bump;
  slot.bytes = static_cast<uint32_t>(need);
  cpu.maps.push_back(slot);
  if (out != nullptr && cap != 0)
    std::snprintf(out, cap, "%llu", static_cast<unsigned long long>(slot.gpa));
  cpu.bump += need;
  return 0;
}

extern "C" int32_t wasmturbo_vcpu_unmap(const char* gpa_text) {
  if (gpa_text == nullptr || gpa_text[0] == 0 || !Attach()) return -1;
  uint64_t gpa = std::strtoull(gpa_text, nullptr, 10);
  Vcpu& cpu = Cpu();
  for (auto it = cpu.maps.begin(); it != cpu.maps.end(); ++it) {
    if (it->gpa != gpa) continue;
    Call("WHvUnmapGpaRange", cpu.part + "\x1f" + std::to_string(gpa), nullptr);
    cpu.maps.erase(it);
    return 0;
  }
  return -1;
}

extern "C" int32_t wasmturbo_catalog_call(int32_t catalog, const char* api, const char* args,
                                          char* out, unsigned cap);

// Port 1 is the hypercall. The reply is the catalog for that exit
// (Win32 or nix), including names this module owns, such as the console.
int32_t ExitAndCall(int32_t catalog, const char* api, const char* args, char* out, unsigned cap) {
  if (api == nullptr || out == nullptr || cap == 0) return -1;
  if (catalog != 1 && catalog != 2) catalog = 1;
  static int depth = 0;
  if (depth > 0) return wasmturbo_catalog_call(catalog, api, args != nullptr ? args : "", out, cap);
  struct Guard {
    int* n;
    ~Guard() { --(*n); }
  } guard{&depth};
  ++depth;
  if (!Attach()) return -1;
  Vcpu& cpu = Cpu();
  if (!Call("WHvSetVirtualProcessorRegisters",
            cpu.part + "\x1f" "0" "\x1f" "Rip" "\x1f" + std::to_string(kVmGpaBase), nullptr))
    return -1;
  std::string exit;
  if (!Call("WHvRunVirtualProcessor", cpu.part + "\x1f" "0", &exit)) return -1;
  if (Field(exit, 0) != "2" || Field(exit, 3) != "1") return -1;
  cpu.io_exits++;
  cpu.k32_exits++;
  return wasmturbo_catalog_call(catalog, api, args != nullptr ? args : "", out, cap);
}

extern "C" int32_t wasmturbo_vcpu_syscall(const char* api, const char* args, char* out,
                                          unsigned cap) {
  return ExitAndCall(1, api, args, out, cap);
}

extern "C" int32_t wasmturbo_vcpu_hypercall(int32_t catalog, const char* api, const char* args,
                                            char* out, unsigned cap) {
  return ExitAndCall(catalog, api, args, out, cap);
}

extern "C" uint32_t wasmturbo_vcpu_k32_exits(void) { return Cpu().k32_exits; }

extern "C" uint32_t wasmturbo_vcpu_io_exits(void) { return Cpu().io_exits; }

extern "C" uint32_t wasmturbo_vcpu_cache_gia(void) { return 0; }

