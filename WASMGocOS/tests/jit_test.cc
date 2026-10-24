#include "gocos/dispatch.h"
#include "gocos/jitrt.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(WASMGOCOS_HAS_VCPU)
extern "C" uint32_t wasmturbo_vcpu_io_exits(void);
extern "C" uint32_t wasmturbo_vcpu_cache_gia(void);
#endif

namespace {

void Append(std::vector<uint8_t>* m, std::initializer_list<uint8_t> bytes) {
  m->insert(m->end(), bytes.begin(), bytes.end());
}

void Section(std::vector<uint8_t>* m, uint8_t id, const std::vector<uint8_t>& body) {
  m->push_back(id);
  m->push_back(static_cast<uint8_t>(body.size()));
  m->insert(m->end(), body.begin(), body.end());
}

// Same shape as the kernel's JitPid module: the body is only the import.
std::vector<uint8_t> PidModule() {
  std::vector<uint8_t> m = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  Section(&m, 1, {0x01, 0x60, 0x00, 0x01, 0x7f});
  const char* mod = "win32";
  const char* name = "GetCurrentProcessId";
  std::vector<uint8_t> imp = {0x01};
  imp.push_back(static_cast<uint8_t>(std::strlen(mod)));
  imp.insert(imp.end(), mod, mod + std::strlen(mod));
  imp.push_back(static_cast<uint8_t>(std::strlen(name)));
  imp.insert(imp.end(), name, name + std::strlen(name));
  Append(&imp, {0x00, 0x00});
  Section(&m, 2, imp);
  Section(&m, 3, {0x01, 0x00});
  Section(&m, 7, {0x01, 0x03, 'p', 'i', 'd', 0x00, 0x01});
  Section(&m, 10, {0x01, 0x04, 0x00, 0x10, 0x00, 0x0b});
  return m;
}

// A fresh path for a scratch file: the host's temp directory natively,
// the working directory in the wasm build.
std::string ScratchPath(const char* prefix) {
#if defined(_WIN32)
  char dir[MAX_PATH];
  char path[MAX_PATH];
  assert(GetTempPathA(sizeof(dir), dir) != 0);
  assert(GetTempFileNameA(dir, prefix, 0, path) != 0);
  return path;
#else
  static int n = 0;
  return std::string(prefix) + "." + std::to_string(wasmgocos::KernelPid()) + "." + std::to_string(++n) +
         ".tmp";
#endif
}

}  // namespace

int main() {
#if !defined(WASMGOCOS_HAS_JITRT)
  return 0;
#else
  char boot[4096];
  int rc = wasmgocos_call("Boot", "", boot, sizeof(boot));
  if (rc != 0) std::fprintf(stderr, "Boot rc=%d reply=%s\n", rc, boot);
  assert(rc == 0);
#if defined(WASMGOCOS_HAS_VCPU)
  if (std::strstr(boot, "hv=vcpu") == nullptr) return 0;
#endif
  char react[1024];
  rc = wasmgocos_call("React", "Boot", react, sizeof(react));
  if (rc != 0) std::fprintf(stderr, "React rc=%d reply=%s\n", rc, react);
  assert(rc == 0);
  assert(std::strstr(react, "os=reactos") != nullptr);
  assert(std::strstr(react, "state=up") != nullptr);
  rc = wasmgocos_call("React", "Processes", react, sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "Idle") != nullptr);
  assert(std::strstr(react, "System") != nullptr);
  char krnl_open[64];
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  int krnl_fd = 0;
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "Idle") != nullptr);
  assert(std::strstr(react, "\n") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Sh", "react ObDir \\", react, sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "error:") == nullptr);
  rc = wasmgocos_call("Sh", "react RegSet \\Registry\\Machine\\SYSTEM\\Select Current REG_DWORD 7",
                      react, sizeof(react));
  assert(rc == 0);
  assert(std::strcmp(react, "ok") == 0);
  rc = wasmgocos_call("Sh", "react RegQuery \\Registry\\Machine\\SYSTEM\\Select Current", react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "data=7") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/ob/Driver" + '\x1f' + "r").c_str(),
                      krnl_open, sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "Null") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Sh", "react NtCreateFile \\Device\\Null rw", react, sizeof(react));
  assert(rc == 0);
  unsigned long null_handle = std::strtoul(react, nullptr, 10);
  assert(null_handle != 0);
  rc = wasmgocos_call("Sh", ("react NtWriteFile " + std::to_string(null_handle) + " hello").c_str(),
                      react, sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "information=5") != nullptr);
  rc = wasmgocos_call("Sh", ("react NtClose " + std::to_string(null_handle)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strcmp(react, "ok") == 0);
  rc = wasmgocos_call("Sh", "react NtCreateEvent \\KernelObjects\\OsEvent notification 0", react,
                      sizeof(react));
  assert(rc == 0);
  unsigned long event_handle = std::strtoul(react, nullptr, 10);
  assert(event_handle != 0);
  rc = wasmgocos_call("Sh", ("react NtQueryEvent " + std::to_string(event_handle)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "type=notification") != nullptr);
  assert(std::strstr(react, "state=0") != nullptr);
  rc = wasmgocos_call("Sh", ("react NtSetEvent " + std::to_string(event_handle)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "previous=0") != nullptr);
  rc = wasmgocos_call("Sh", ("react NtQueryEvent " + std::to_string(event_handle)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "state=1") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/ob/KernelObjects" + '\x1f' + "r").c_str(),
                      krnl_open, sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "OsEvent") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Sh", "react NtOpenEvent \\KernelObjects\\OsEvent", react, sizeof(react));
  assert(rc == 0);
  unsigned long event_open = std::strtoul(react, nullptr, 10);
  assert(event_open != 0 && event_open != event_handle);
  rc = wasmgocos_call("Sh", ("react NtResetEvent " + std::to_string(event_handle)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "previous=1") != nullptr);
  rc = wasmgocos_call("Sh", ("react NtQueryEvent " + std::to_string(event_open)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "state=0") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/handles" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "Event") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Sh", ("react NtClose " + std::to_string(event_open)).c_str(), react, sizeof(react));
  assert(rc == 0);
  rc = wasmgocos_call("Sh", ("react NtClose " + std::to_string(event_handle)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strcmp(react, "ok") == 0);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/clock" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strtoull(react, nullptr, 10) > 0);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/cons" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "ReactOS") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Sh", "react Version", react, sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "ReactOS") != nullptr);
  assert(std::strstr(react, "NT ") != nullptr);
  rc = wasmgocos_call("Sh", "react RegEnum \\Registry\\Machine\\SYSTEM\\Select", react, sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "Current") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/ob/??" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "GLOBALROOT") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/pool" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strchr(react, '\t') != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/types" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "Event") != nullptr);
  assert(std::strstr(react, "File") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/pfn" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "pages=") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/mem" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "OsloaderHeap") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Sh", "react ObQuery \\Device\\Null", react, sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "type=Device") != nullptr);
  rc = wasmgocos_call("Sh", "react ObQuery \\SystemRoot", react, sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "target=") != nullptr);
  rc = wasmgocos_call("Fd",
                      (std::string("open") + '\x1f' + "#k/ob/?" "?/GLOBALROOT/Device" + '\x1f' + "r").c_str(),
                      krnl_open, sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "Null") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  rc = wasmgocos_call("Sh", "react NtCreateFile \\Device\\Null r", react, sizeof(react));
  assert(rc == 0);
  unsigned long info_handle = std::strtoul(react, nullptr, 10);
  assert(info_handle != 0);
  rc = wasmgocos_call("Sh", ("react NtQueryInformationFile " + std::to_string(info_handle)).c_str(),
                      react, sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "links=1") != nullptr);
  rc = wasmgocos_call("Sh", ("react NtClose " + std::to_string(info_handle)).c_str(), react, sizeof(react));
  assert(rc == 0);
  rc = wasmgocos_call("Fd", (std::string("open") + '\x1f' + "#k/aspace" + '\x1f' + "r").c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);
  assert(std::sscanf(krnl_open, "fd=%d", &krnl_fd) == 1);
  rc = wasmgocos_call("Fd", (std::string("read") + '\x1f' + std::to_string(krnl_fd)).c_str(), react,
                      sizeof(react));
  assert(rc == 0);
  assert(std::strstr(react, "kseg0=") != nullptr);
  rc = wasmgocos_call("Fd", (std::string("close") + '\x1f' + std::to_string(krnl_fd)).c_str(), krnl_open,
                      sizeof(krnl_open));
  assert(rc == 0);

  std::vector<uint8_t> wasm = PidModule();
#if defined(WASMGOCOS_HAS_VCPU)
  uint32_t before = wasmturbo_vcpu_io_exits();
  char buf[256];
  rc = wasmgocos_call("JitPid", "", buf, sizeof(buf));
  if (rc != 0) std::fprintf(stderr, "JitPid rc=%d reply=%s\n", rc, buf);
  assert(rc == 0);
  assert(wasmturbo_vcpu_io_exits() == before + 1);
  unsigned long jit_pid = std::strtoul(buf, nullptr, 10);
  assert(jit_pid != 0);

  char k32[256];
  rc = wasmgocos_call("GetCurrentProcessId", "", k32, sizeof(k32));
  assert(rc == 0);
  assert(std::strtoul(k32, nullptr, 10) == jit_pid);

  wasmgocos::JitModule* module = wasmgocos::jit_load(wasm.data(), wasm.size());
  assert(module != nullptr);
  uint32_t exits = wasmturbo_vcpu_io_exits();
  int32_t pid = 0;
  assert(wasmgocos::jit_i32(module, "pid", &pid, nullptr, 0));
  assert(wasmturbo_vcpu_io_exits() == exits + 1);
  assert(static_cast<unsigned long>(pid) == jit_pid);
  wasmgocos::jit_drop(module);
#else
  // No vCPU: JitPid has no partition to run on. The process id a guest
  // reads through gocos.Call is the kernel's own.
  char buf[256];
  rc = wasmgocos_call("JitPid", "", buf, sizeof(buf));
  assert(rc != 0 && std::strstr(buf, "no vcpu") != nullptr);
  char k32[256];
  rc = wasmgocos_call("GetCurrentProcessId", "", k32, sizeof(k32));
  assert(rc == 0);
  unsigned long jit_pid = std::strtoul(k32, nullptr, 10);
  assert(jit_pid != 0);
#endif

  char share[512];
  rc = wasmgocos_call("JitShare", "", share, sizeof(share));
  if (rc != 0) std::fprintf(stderr, "JitShare rc=%d reply=%s\n", rc, share);
  assert(rc == 0);
  assert(std::strstr(share, "peek=42") != nullptr);
  const char* cage = std::strstr(share, "cage=");
  const char* engine = std::strstr(share, "engine=");
  const char* hv = std::strstr(share, "hv=");
  const char* gia = std::strstr(share, "gia=");
  const char* peer = std::strstr(share, "peer=");
  const char* type = std::strstr(share, "type=");
  assert(cage && engine && hv && gia && peer && type);
  assert(std::strtoull(cage + 5, nullptr, 10) != 0);
  assert(std::strtoul(engine + 7, nullptr, 10) != 0);
  assert(std::strtoul(gia + 4, nullptr, 10) != 0);
  assert(std::strtoul(peer + 5, nullptr, 10) != 0);
  assert(std::strtoul(gia + 4, nullptr, 10) != std::strtoul(peer + 5, nullptr, 10));
  assert(std::strtoul(type + 5, nullptr, 10) != 0);
#if defined(WASMGOCOS_HAS_VCPU)
  assert(std::strtoul(hv + 3, nullptr, 10) != 0);
#endif

  std::vector<uint8_t> start = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  Section(&start, 1, {0x01, 0x60, 0x00, 0x00});
  Section(&start, 3, {0x01, 0x00});
  Section(&start, 7, {0x01, 0x06, '_', 's', 't', 'a', 'r', 't', 0x00, 0x00});
  Section(&start, 10, {0x01, 0x02, 0x00, 0x0b});
  wasmgocos::JitModule* entry = wasmgocos::jit_load(start.data(), start.size());
  assert(entry != nullptr);
  wasmgocos::JitNames entry_names;
  assert(wasmgocos::jit_names(entry, &entry_names));
  assert(entry_names.cage_base == std::strtoull(cage + 5, nullptr, 10));
  assert(entry_names.engine == std::strtoul(engine + 7, nullptr, 10));
  assert(wasmgocos::jit_entry(entry));
  wasmgocos::jit_drop(entry);

  {
  std::string path = ScratchPath("gwt");
  FILE* out = std::fopen(path.c_str(), "wb");
  assert(out != nullptr);
  assert(std::fwrite(wasm.data(), 1, wasm.size(), out) == wasm.size());
  std::fclose(out);
  wasmgocos::JitModule* from_file = wasmgocos::jit_load_file(path.c_str());
  std::remove(path.c_str());
  assert(from_file != nullptr);
  wasmgocos::JitNames file_names;
  assert(wasmgocos::jit_names(from_file, &file_names));
  assert(file_names.cage_base == entry_names.cage_base);
  assert(file_names.engine == entry_names.engine);
  assert(file_names.hypervisor == entry_names.hypervisor);
  wasmgocos::jit_drop(from_file);
  }

  // gocos.Call(topic, payload, out) — the strings live in the module's
  // cage memory. Topic at 0 is GetCurrentProcessId; the reply is written
  // at offset 64.
  const char* topic = "GetCurrentProcessId";
  std::vector<uint8_t> call = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  Section(&call, 1, {0x02, 0x60, 0x06, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x00,
                      0x01, 0x7f});
  std::vector<uint8_t> imp = {0x01, 0x05, 'g', 'o', 'c', 'o', 's', 0x04, 'C', 'a', 'l', 'l', 0x00, 0x00};
  Section(&call, 2, imp);
  Section(&call, 3, {0x01, 0x01});
  Section(&call, 5, {0x01, 0x00, 0x01});
  Section(&call, 7, {0x01, 0x04, 'c', 'a', 'l', 'l', 0x00, 0x01});
  Section(&call, 10, {0x01, 0x11, 0x00, 0x41, 0x00, 0x41, 0x13, 0x41, 0x00, 0x41, 0x00, 0x41, 0xC0, 0x00,
                      0x41, 0x20, 0x10, 0x00, 0x0b});
  std::vector<uint8_t> data = {0x01, 0x00, 0x41, 0x00, 0x0b, static_cast<uint8_t>(std::strlen(topic))};
  data.insert(data.end(), topic, topic + std::strlen(topic));
  Section(&call, 11, data);
  wasmgocos::JitModule* caller = wasmgocos::jit_load(call.data(), call.size());
  assert(caller != nullptr);
  int32_t call_rc = -1;
  assert(wasmgocos::jit_cage_i32(caller, "call", &call_rc, nullptr, 0));
  assert(call_rc == 0);
  wasmgocos::JitNames call_names;
  assert(wasmgocos::jit_names(caller, &call_names));
  assert(call_names.cage_base == std::strtoull(cage + 5, nullptr, 10));
  assert(call_names.memory_base != 0);
  char reply[32];
  std::memcpy(reply, reinterpret_cast<const char*>(call_names.memory_base) + 64, sizeof(reply));
  reply[sizeof(reply) - 1] = 0;
  assert(std::strtoul(reply, nullptr, 10) == jit_pid);
  wasmgocos::jit_drop(caller);

  std::string run_path = ScratchPath("gwr");
  FILE* run_out = std::fopen(run_path.c_str(), "wb");
  assert(run_out != nullptr);
  assert(std::fwrite(start.data(), 1, start.size(), run_out) == start.size());
  std::fclose(run_out);
  char ran[512];
  rc = wasmgocos_call("JitRun", run_path.c_str(), ran, sizeof(ran));
  std::remove(run_path.c_str());
  if (rc != 0) std::fprintf(stderr, "JitRun rc=%d reply=%s\n", rc, ran);
  assert(rc == 0);
  assert(std::strstr(ran, "entry=1") != nullptr);
  const char* run_cage = std::strstr(ran, "cage=");
  assert(run_cage != nullptr);
  assert(std::strtoull(run_cage + 5, nullptr, 10) == std::strtoull(cage + 5, nullptr, 10));

  char unil[512];
  rc = wasmgocos_call("JitUnil", "", unil, sizeof(unil));
  if (rc != 0) std::fprintf(stderr, "JitUnil rc=%d reply=%s\n", rc, unil);
  assert(rc == 0);
  const char* unil_cage = std::strstr(unil, "cage=");
  const char* unil_reply = std::strstr(unil, "reply=");
  assert(unil_cage != nullptr && unil_reply != nullptr);
  assert(std::strtoull(unil_cage + 5, nullptr, 10) == std::strtoull(cage + 5, nullptr, 10));
  assert(std::strtoul(unil_reply + 6, nullptr, 10) == jit_pid);

  char mss[512];
  rc = wasmgocos_call("JitMss", "", mss, sizeof(mss));
  if (rc != 0) std::fprintf(stderr, "JitMss rc=%d reply=%s\n", rc, mss);
  assert(rc == 0);
  const char* mss_cage = std::strstr(mss, "cage=");
  const char* mss_jobs = std::strstr(mss, "jobs=");
  const char* mss_tasks = std::strstr(mss, "tasks=");
  const char* mss_text = std::strstr(mss, "text=");
  assert(mss_cage != nullptr && mss_jobs != nullptr && mss_tasks != nullptr && mss_text != nullptr);
  assert(std::strtoull(mss_cage + 5, nullptr, 10) == std::strtoull(cage + 5, nullptr, 10));
  assert(std::strtoul(mss_jobs + 5, nullptr, 10) >= 1);
  assert(std::strtoul(mss_tasks + 6, nullptr, 10) >= 1);
  assert(std::strcmp(mss_text + 5, "hello from the heap") == 0);

  std::string file_path = ScratchPath("ghp");
  std::string file_args = file_path;
  file_args.push_back('\x1f');
  file_args += "hello from the heap";
  char filed[512];
  rc = wasmgocos_call("JitFile", file_args.c_str(), filed, sizeof(filed));
  std::remove(file_path.c_str());
  if (rc != 0) std::fprintf(stderr, "JitFile rc=%d reply=%s\n", rc, filed);
  assert(rc == 0);
  const char* file_cage = std::strstr(filed, "cage=");
  const char* file_text = std::strstr(filed, "text=");
  const char* file_gia = std::strstr(filed, "gia_ipt=");
  const char* file_raw = std::strstr(filed, "raw_ipt=");
  assert(file_cage != nullptr && file_text != nullptr && file_gia != nullptr && file_raw != nullptr);
  assert(std::strtoull(file_cage + 5, nullptr, 10) == std::strtoull(cage + 5, nullptr, 10));
  assert(std::strtoul(file_gia + 8, nullptr, 10) != std::strtoul(file_raw + 8, nullptr, 10));
  assert(std::strcmp(file_text + 5, "hello from the heap") == 0);

  auto fnv = [](const char* s) {
    uint32_t h = 2166136261u;
    for (const auto* p = reinterpret_cast<const unsigned char*>(s); *p != 0; ++p) {
      h ^= *p;
      h *= 16777619u;
    }
    h &= 0x7fffffffu;
    return h == 0 ? 1u : h;
  };

  char miss[128];
  rc = wasmgocos_call("Heap", "file.Heap.Exec", miss, sizeof(miss));
  assert(rc == 0);
  assert(std::strcmp(miss, "heapExec miss") == 0);

  std::string heap_path = ScratchPath("ghe");
  std::string write_args = "file.Heap.Write";
  write_args.push_back('\x1f');
  write_args += heap_path;
  write_args.push_back('\x1f');
  write_args += "hello from the heap";
  char wrote[512];
  rc = wasmgocos_call("Heap", write_args.c_str(), wrote, sizeof(wrote));
  if (rc != 0) std::fprintf(stderr, "Heap write rc=%d reply=%s\n", rc, wrote);
  assert(rc == 0);
  assert(std::strncmp(wrote, "write ", 6) == 0);
  assert(std::strtoul(wrote + 6, nullptr, 10) == fnv("file.Heap.Write"));
  assert(std::strstr(wrote, " writeFS ") != nullptr);

  std::string read_args = "file.Heap.Read";
  read_args.push_back('\x1f');
  read_args += heap_path;
  char read_reply[256];
  rc = wasmgocos_call("Heap", read_args.c_str(), read_reply, sizeof(read_reply));
  std::remove(heap_path.c_str());
  if (rc != 0) std::fprintf(stderr, "Heap read rc=%d reply=%s\n", rc, read_reply);
  assert(rc == 0);
  assert(std::strncmp(read_reply, "read ", 5) == 0);
  assert(std::strtoul(read_reply + 5, nullptr, 10) == fnv("file.Heap.Read"));
  const char* read_addr = std::strstr(read_reply, " readHeap ");
  assert(read_addr != nullptr);

  char exec_reply[256];
  rc = wasmgocos_call("Heap", "file.Heap.Exec", exec_reply, sizeof(exec_reply));
  if (rc != 0) std::fprintf(stderr, "Heap exec rc=%d reply=%s\n", rc, exec_reply);
  assert(rc == 0);
  assert(std::strncmp(exec_reply, "heapExec ", 9) == 0);
  assert(std::strtoul(exec_reply + 9, nullptr, 10) == fnv("file.Heap.Exec"));
  const char* heap_addr = read_addr + std::strlen(" readHeap ");
  assert(std::strstr(exec_reply, heap_addr) != nullptr);
  assert(std::strstr(exec_reply, " hello from the heap") != nullptr);

  char proc[512];
  rc = wasmgocos_call("ProcFs", "", proc, sizeof(proc));
  if (rc != 0) std::fprintf(stderr, "ProcFs rc=%d reply=%s\n", rc, proc);
  assert(rc == 0);
  assert(std::strstr(proc, "file.Heap.Read") != nullptr);
  assert(std::strstr(proc, "file.Heap.Exec") != nullptr);
  assert(std::strstr(proc, heap_addr) != nullptr);

  char node[64];
  std::snprintf(node, sizeof(node), "%u", fnv("file.Heap.Read"));
  char body[64];
  rc = wasmgocos_call("ProcFs", node, body, sizeof(body));
  assert(rc == 0);
  assert(std::strcmp(body, "hello from the heap") == 0);

  char by_ord[256];
  std::snprintf(node, sizeof(node), "%u", fnv("file.Heap.Exec"));
  rc = wasmgocos_call("Heap", node, by_ord, sizeof(by_ord));
  assert(rc == 0);
  assert(std::strcmp(by_ord, exec_reply) == 0);

  char table[512];
  rc = wasmgocos_call("ProcTable", "", table, sizeof(table));
  if (rc != 0) std::fprintf(stderr, "ProcTable rc=%d reply=%s\n", rc, table);
  assert(rc == 0);
  assert(std::strstr(table, " raw ") != nullptr);
  assert(std::strstr(table, " gia_match=0") != nullptr);
  assert(std::strstr(table, " gockrnl ") != nullptr);
  char pid_text[32];
  std::snprintf(pid_text, sizeof(pid_text), " %lu ", jit_pid);
  assert(std::strstr(table, pid_text) != nullptr);
  unsigned long proc_ord = std::strtoul(table, nullptr, 10);
  assert(proc_ord != 0);
  std::snprintf(node, sizeof(node), "%lu", proc_ord);
  char image[32];
  rc = wasmgocos_call("ProcTable", node, image, sizeof(image));
  assert(rc == 0);
  assert(std::strcmp(image, "gockrnl") == 0);

  char svcs[512];
  rc = wasmgocos_call("Service", "", svcs, sizeof(svcs));
  if (rc != 0) std::fprintf(stderr, "Service rc=%d reply=%s\n", rc, svcs);
  assert(rc == 0);
  assert(std::strstr(svcs, "core.pid") != nullptr);
  assert(std::strstr(svcs, "core.proc") != nullptr);
  assert(std::strstr(svcs, "core.mem") != nullptr);
  assert(std::strstr(svcs, " raw ") != nullptr);
  char pid_line[32];
  std::snprintf(pid_line, sizeof(pid_line), "%lu", jit_pid);
  char pid_reply[64];
  rc = wasmgocos_call("Service", "core.pid", pid_reply, sizeof(pid_reply));
  assert(rc == 0);
  assert(std::strcmp(pid_reply, pid_line) == 0);
  char proc_reply[64];
  rc = wasmgocos_call("Service", "core.proc", proc_reply, sizeof(proc_reply));
  assert(rc == 0);
  assert(std::strstr(proc_reply, pid_line) != nullptr);
  assert(std::strstr(proc_reply, "gockrnl") != nullptr);
  char mem_reply[64];
  rc = wasmgocos_call("Service", "core.mem", mem_reply, sizeof(mem_reply));
  assert(rc == 0);
  assert(std::strcmp(mem_reply, "core.mem") == 0);
  char svc_miss[64];
  rc = wasmgocos_call("Service", "gia core.mem", svc_miss, sizeof(svc_miss));
  assert(rc == 0);
  assert(std::strcmp(svc_miss, "ipt miss") == 0);

  auto vfs = [](const std::string& args, char* out, size_t cap) {
    return wasmgocos_call("Vfs", args.c_str(), out, cap);
  };
  char vls[256];
  rc = vfs(std::string("ls") + '\x1f' + "/", vls, sizeof(vls));
  if (rc != 0) std::fprintf(stderr, "Vfs ls rc=%d reply=%s\n", rc, vls);
  assert(rc == 0);
  assert(std::strstr(vls, "proc\n") != nullptr);
  std::string vfs_body = std::string("write") + '\x1f' + "/hello" + '\x1f' + "hello from the heap";
  char vwrite[128];
  rc = vfs(vfs_body, vwrite, sizeof(vwrite));
  assert(rc == 0);
  assert(std::strcmp(vwrite, "write /hello") == 0);
  char vread[64];
  rc = vfs(std::string("read") + '\x1f' + "/hello", vread, sizeof(vread));
  assert(rc == 0);
  assert(std::strcmp(vread, "hello from the heap") == 0);
  char vstat[128];
  rc = vfs(std::string("stat") + '\x1f' + "/hello", vstat, sizeof(vstat));
  assert(rc == 0);
  assert(std::strncmp(vstat, "file raw 0x", 11) == 0);
  char vgia[64];
  rc = vfs(std::string("gia") + '\x1f' + "/hello", vgia, sizeof(vgia));
  assert(rc == 0);
  assert(std::strcmp(vgia, "ipt miss") == 0);
  char vpid[64];
  rc = vfs(std::string("read") + '\x1f' + "/proc/core.pid", vpid, sizeof(vpid));
  assert(rc == 0);
  assert(std::strcmp(vpid, pid_line) == 0);
  char vbind[64];
  rc = vfs(std::string("bind") + '\x1f' + "/proc" + '\x1f' + "/dev", vbind, sizeof(vbind));
  assert(rc == 0);
  char vdev[64];
  rc = vfs(std::string("read") + '\x1f' + "/dev/core.pid", vdev, sizeof(vdev));
  assert(rc == 0);
  assert(std::strcmp(vdev, pid_line) == 0);

#if defined(WASMGOCOS_HAS_NET)
  auto bio = [](const std::string& args, char* out, size_t cap) {
    return wasmgocos_call("Bio", args.c_str(), out, cap);
  };
  char bsend[64];
  rc = bio(std::string("send") + '\x1f' + "core.pid", bsend, sizeof(bsend));
  if (rc != 0) std::fprintf(stderr, "Bio send rc=%d reply=%s\n", rc, bsend);
  assert(rc == 0);
  assert(std::strcmp(bsend, "sent") == 0);
  char brecv[64];
  rc = bio("recv", brecv, sizeof(brecv));
  assert(rc == 0);
  assert(std::strcmp(brecv, pid_line) == 0);
  char net_out[64];
  rc = wasmgocos_call("Net", "", net_out, sizeof(net_out));
  assert(rc == 0);
  assert(std::strstr(net_out, "net resolver=") != nullptr);
  assert(std::strstr(net_out, " kernel=") != nullptr);
  rc = wasmgocos_call("Net", (std::string("put") + '\x1f' + "n.example" + '\x1f' + "192.0.2.53").c_str(),
                      net_out, sizeof(net_out));
  assert(rc == 0);
  assert(std::strcmp(net_out, "put n.example") == 0);
  rc = wasmgocos_call("Net", (std::string("query") + '\x1f' + "n.example").c_str(), net_out,
                      sizeof(net_out));
  assert(rc == 0);
  assert(std::strcmp(net_out, "192.0.2.53") == 0);
  rc = wasmgocos_call("Sh", "net query n.example", net_out, sizeof(net_out));
  assert(rc == 0);
  assert(std::strcmp(net_out, "192.0.2.53") == 0);
  rc = wasmgocos_call("Sh", "net serve", net_out, sizeof(net_out));
  assert(rc == 0);
  assert(std::strcmp(net_out, "serve") == 0);
  rc = wasmgocos_call("Sh", "net open origin.example:8080", net_out, sizeof(net_out));
  assert(rc == 0);
  unsigned net_ch = 0;
  assert(std::sscanf(net_out, "channel %u", &net_ch) == 1);
  assert(net_ch != 0);
  rc = wasmgocos_call("Sh", ("net send " + std::to_string(net_ch) + " ping").c_str(), net_out,
                      sizeof(net_out));
  assert(rc == 0);
  rc = wasmgocos_call("Net", (std::string("recv") + '\x1f' + std::to_string(net_ch)).c_str(),
                      net_out, sizeof(net_out));
  assert(rc == 0);
  assert(std::strcmp(net_out, "ping") == 0);
  const char* cipher = "hello from the heap";
  rc = bio(std::string("send") + '\x1f' + cipher, bsend, sizeof(bsend));
  assert(rc == 0);
  assert(std::strcmp(bsend, "sent") == 0);
  rc = bio("recv", brecv, sizeof(brecv));
  assert(rc == 0);
  assert(std::strcmp(brecv, cipher) == 0);
#endif

  auto bbx = [](const std::string& args, char* out, size_t cap) {
    return wasmgocos_call("Bbx", args.c_str(), out, cap);
  };
  char table_bbx[2048];
  rc = bbx("table", table_bbx, sizeof(table_bbx));
  if (rc != 0) std::fprintf(stderr, "Bbx table rc=%d reply=%s\n", rc, table_bbx);
  assert(rc == 0);
  assert(std::strstr(table_bbx, "builtin:echo=1912602625->catalog=1912602625") != nullptr);
  assert(std::strstr(table_bbx, "wpr:getProcessInfo=520932818->catalog=520932818") != nullptr);
  char reg[256];
  rc = bbx("register", reg, sizeof(reg));
  if (rc != 0 || std::strcmp(reg,
                             "registered=16/16 echo=1912602625 spawn=1912602630 "
                             "getProcessInfo=520932818 owns_echo=1") != 0) {
    std::fprintf(stderr, "Bbx register rc=%d reply=%s\n", rc, reg);
  }
  assert(rc == 0);
  assert(std::strcmp(reg,
                     "registered=16/16 echo=1912602625 spawn=1912602630 "
                     "getProcessInfo=520932818 owns_echo=1") == 0);
  char echo_bbx[64];
  rc = bbx(std::string("run") + '\x1f' + "echo busybox", echo_bbx, sizeof(echo_bbx));
  assert(rc == 0);
  assert(std::strcmp(echo_bbx, "busybox") == 0);
  char trap_ls[512];
  rc = bbx(std::string("trap") + '\x1f' + "1912602626", trap_ls, sizeof(trap_ls));
  assert(rc == 0);
  assert(std::strncmp(trap_ls, "ls=", 3) == 0);
  assert(std::strstr(trap_ls, "proc") != nullptr);
  char trap0[64];
  rc = bbx(std::string("trap") + '\x1f' + "0", trap0, sizeof(trap0));
  assert(rc == 0);
  assert(std::strcmp(trap0, "EINVAL:ordinal=0") == 0);
  char empty_run[64];
  rc = bbx("run", empty_run, sizeof(empty_run));
  assert(rc == 0);
  assert(std::strcmp(empty_run, "EINVAL:empty") == 0);
  char pwd_bbx[32];
  rc = bbx(std::string("run") + '\x1f' + "pwd", pwd_bbx, sizeof(pwd_bbx));
  assert(rc == 0);
  assert(std::strcmp(pwd_bbx, "/") == 0);
  char true_bbx[32];
  rc = bbx(std::string("run") + '\x1f' + "true", true_bbx, sizeof(true_bbx));
  assert(rc == 0);
  assert(std::strcmp(true_bbx, "true=0") == 0);
  char ps_bbx[256];
  rc = bbx("ps", ps_bbx, sizeof(ps_bbx));
  assert(rc == 0);
  assert(std::strncmp(ps_bbx, "ps=real=", 8) == 0);
  assert(std::strstr(ps_bbx, "gockrnl") != nullptr);
  char spawn_bbx[256];
  rc = bbx("spawn", spawn_bbx, sizeof(spawn_bbx));
  if (rc != 0) std::fprintf(stderr, "Bbx spawn rc=%d reply=%s\n", rc, spawn_bbx);
  assert(rc == 0);
  assert(std::strncmp(spawn_bbx, "spawn=ok pid=", 13) == 0);
  assert(std::strstr(spawn_bbx, " running=0 exit=0 owned=1 out=[wpr-bbx]") != nullptr);
  char sh_bbx[256];
  rc = bbx(std::string("run") + '\x1f' + "sh", sh_bbx, sizeof(sh_bbx));
  assert(rc == 0);
  assert(std::strstr(sh_bbx, "out=[bbx-sh]") != nullptr);
  char mkdir_bbx[64];
  rc = bbx(std::string("run") + '\x1f' + "mkdir kit", mkdir_bbx, sizeof(mkdir_bbx));
  assert(rc == 0);
  assert(std::strcmp(mkdir_bbx, "mkdir=ok") == 0);
  char note_body[64];
  rc = vfs(std::string("write") + '\x1f' + "/kit/note" + '\x1f' + "hello from the heap", note_body,
           sizeof(note_body));
  assert(rc == 0);
  char cat_bbx[64];
  rc = bbx(std::string("run") + '\x1f' + "cat kit/note", cat_bbx, sizeof(cat_bbx));
  assert(rc == 0);
  assert(std::strcmp(cat_bbx, "hello from the heap") == 0);
  char wc_bbx[64];
  rc = bbx(std::string("run") + '\x1f' + "wc kit/note", wc_bbx, sizeof(wc_bbx));
  assert(rc == 0);
  assert(std::strcmp(wc_bbx, "wc=[lines=0 words=4 bytes=19]") == 0);
  char cp_bbx[64];
  rc = bbx(std::string("run") + '\x1f' + "cp kit/note kit/copy", cp_bbx, sizeof(cp_bbx));
  assert(rc == 0);
  assert(std::strcmp(cp_bbx, "cp=ok") == 0);
  char mv_bbx[64];
  rc = bbx(std::string("run") + '\x1f' + "mv kit/copy kit/moved", mv_bbx, sizeof(mv_bbx));
  assert(rc == 0);
  assert(std::strcmp(mv_bbx, "mv=ok") == 0);
  rc = bbx(std::string("run") + '\x1f' + "cat kit/copy", cat_bbx, sizeof(cat_bbx));
  assert(rc == 0);
  assert(std::strcmp(cat_bbx, "cat=ERROR:open") == 0);
  rc = bbx(std::string("run") + '\x1f' + "cat kit/moved", cat_bbx, sizeof(cat_bbx));
  assert(rc == 0);
  assert(std::strcmp(cat_bbx, "hello from the heap") == 0);
  char rm_bbx[64];
  rc = bbx(std::string("run") + '\x1f' + "rm kit/moved", rm_bbx, sizeof(rm_bbx));
  assert(rc == 0);
  assert(std::strcmp(rm_bbx, "rm=ok") == 0);
  char env_bbx[32];
  rc = bbx(std::string("run") + '\x1f' + "env", env_bbx, sizeof(env_bbx));
  assert(rc == 0);
  assert(std::strcmp(env_bbx, "env=1") == 0);
  char proc_bbx[8192];
  rc = wasmgocos_call("ProcFs", "", proc_bbx, sizeof(proc_bbx));
  assert(rc == 0);
  assert(std::strstr(proc_bbx, "1912602625 echo") != nullptr);
  assert(std::strstr(proc_bbx, " raw ") != nullptr);
  char proc_false[32];
  rc = vfs(std::string("read") + '\x1f' + "/proc/false", proc_false, sizeof(proc_false));
  assert(rc == 0);
  assert(std::strcmp(proc_false, "false=1") == 0);

  auto fd = [](const std::string& args, char* out, size_t cap) {
    return wasmgocos_call("Fd", args.c_str(), out, cap);
  };
  char flist[256];
  rc = fd("", flist, sizeof(flist));
  if (rc != 0) std::fprintf(stderr, "Fd rc=%d reply=%s\n", rc, flist);
  assert(rc == 0);
  assert(std::strstr(flist, "0 #c/cons rw\n") != nullptr);
  assert(std::strstr(flist, "cwd=/") != nullptr);
  char fcons[512];
  rc = fd(std::string("read") + '\x1f' + "0", fcons, sizeof(fcons));
  assert(rc == 0);
  assert(std::strstr(fcons, "wpr-bbx") != nullptr);
  assert(std::strstr(fcons, "bbx-sh") != nullptr);
  char fdir[64];
  rc = vfs(std::string("mkdir") + '\x1f' + "/os", fdir, sizeof(fdir));
  assert(rc == 0);
  char fcwd[64];
  rc = fd(std::string("chdir") + '\x1f' + "/os", fcwd, sizeof(fcwd));
  assert(rc == 0);
  assert(std::strcmp(fcwd, "/os") == 0);
  char fopen_[64];
  rc = fd(std::string("open") + '\x1f' + "note" + '\x1f' + "w", fopen_, sizeof(fopen_));
  assert(rc == 0);
  assert(std::strcmp(fopen_, "fd=3 path=/os/note") == 0);
  char fwrite[64];
  rc = fd(std::string("write") + '\x1f' + "3" + '\x1f' + "hello from the heap", fwrite, sizeof(fwrite));
  assert(rc == 0);
  assert(std::strcmp(fwrite, "write 3 19") == 0);
  rc = fd(std::string("open") + '\x1f' + "note" + '\x1f' + "r", fopen_, sizeof(fopen_));
  assert(rc == 0);
  assert(std::strcmp(fopen_, "fd=4 path=/os/note") == 0);
  char fnote[64];
  rc = fd(std::string("read") + '\x1f' + "4", fnote, sizeof(fnote));
  assert(rc == 0);
  assert(std::strcmp(fnote, "hello from the heap") == 0);
  char fclose_[32];
  rc = fd(std::string("close") + '\x1f' + "3", fclose_, sizeof(fclose_));
  assert(rc == 0);
  assert(std::strcmp(fclose_, "close 3") == 0);
  rc = fd(std::string("read") + '\x1f' + "3", fnote, sizeof(fnote));
  assert(rc == 0);
  assert(std::strcmp(fnote, "fd miss") == 0);
  rc = fd(std::string("chdir") + '\x1f' + "..", fcwd, sizeof(fcwd));
  assert(rc == 0);
  assert(std::strcmp(fcwd, "/") == 0);
  char pipe_reply[64];
  rc = fd("pipe", pipe_reply, sizeof(pipe_reply));
  assert(rc == 0);
  int pipe_r = 0;
  int pipe_w = 0;
  assert(std::sscanf(pipe_reply, "pipe %d %d", &pipe_r, &pipe_w) == 2);
  char pipe_wr[64];
  rc = fd(std::string("write") + '\x1f' + std::to_string(pipe_w) + '\x1f' + "hello from the heap",
          pipe_wr, sizeof(pipe_wr));
  assert(rc == 0);
  assert(std::strcmp(pipe_wr, (std::string("write ") + std::to_string(pipe_w) + " 19").c_str()) == 0);
  char pipe_rd[64];
  rc = fd(std::string("read") + '\x1f' + std::to_string(pipe_r), pipe_rd, sizeof(pipe_rd));
  assert(rc == 0);
  assert(std::strcmp(pipe_rd, "hello from the heap") == 0);
  rc = fd(std::string("read") + '\x1f' + std::to_string(pipe_r), pipe_rd, sizeof(pipe_rd));
  assert(rc == 0);
  assert(pipe_rd[0] == '\0');
  rc = fd(std::string("close") + '\x1f' + std::to_string(pipe_w), pipe_wr, sizeof(pipe_wr));
  assert(rc == 0);
  rc = fd(std::string("write") + '\x1f' + std::to_string(pipe_r) + '\x1f' + "x", pipe_wr, sizeof(pipe_wr));
  assert(rc == 0);
  assert(std::strcmp(pipe_wr, "walk miss") == 0);
  auto env = [](const std::string& args, char* out, size_t cap) {
    return wasmgocos_call("Env", args.c_str(), out, cap);
  };
  char env_list[128];
  rc = env("", env_list, sizeof(env_list));
  assert(rc == 0);
  assert(std::strstr(env_list, "PATH=/proc\n") != nullptr);
  char env_set[32];
  rc = env(std::string("set") + '\x1f' + "HOME" + '\x1f' + "/", env_set, sizeof(env_set));
  assert(rc == 0);
  assert(std::strcmp(env_set, "set HOME") == 0);
  char env_get[32];
  rc = env(std::string("get") + '\x1f' + "HOME", env_get, sizeof(env_get));
  assert(rc == 0);
  assert(std::strcmp(env_get, "/") == 0);
  char env_n[32];
  rc = bbx(std::string("run") + '\x1f' + "env", env_n, sizeof(env_n));
  assert(rc == 0);
  assert(std::strcmp(env_n, "env=2") == 0);
  rc = env(std::string("unset") + '\x1f' + "HOME", env_set, sizeof(env_set));
  assert(rc == 0);
  assert(std::strcmp(env_set, "unset HOME") == 0);
  rc = env(std::string("get") + '\x1f' + "HOME", env_get, sizeof(env_get));
  assert(rc == 0);
  assert(std::strcmp(env_get, "env miss") == 0);

  auto sh = [](const char* line, char* out, size_t cap) {
    return wasmgocos_call("Sh", line, out, cap);
  };
  char sh_out[128];
  rc = sh("cd /os", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(sh_out[0] == '\0');
  rc = fd("cwd", fcwd, sizeof(fcwd));
  assert(rc == 0);
  assert(std::strcmp(fcwd, "/os") == 0);
  rc = sh("echo hello from the heap > sh-out", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(sh_out[0] == '\0');
  char sh_file[64];
  rc = vfs(std::string("read") + '\x1f' + "/os/sh-out", sh_file, sizeof(sh_file));
  assert(rc == 0);
  assert(std::strcmp(sh_file, "hello from the heap") == 0);
  rc = sh("wc < sh-out", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "wc=[lines=0 words=4 bytes=19]") == 0);
  rc = sh("echo hello from the heap | wc", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "wc=[lines=0 words=4 bytes=19]") == 0);
  rc = sh("pwd", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "/os") == 0);
  rc = sh("cat sh-out", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "hello from the heap") == 0);
  rc = sh("export HOME=/", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(sh_out[0] == '\0');
  rc = env(std::string("get") + '\x1f' + "HOME", env_get, sizeof(env_get));
  assert(rc == 0);
  assert(std::strcmp(env_get, "/") == 0);
  rc = sh("unset HOME", sh_out, sizeof(sh_out));
  assert(rc == 0);
  rc = sh("cd /", sh_out, sizeof(sh_out));
  assert(rc == 0);
  rc = fd("cwd", fcwd, sizeof(fcwd));
  assert(rc == 0);
  assert(std::strcmp(fcwd, "/") == 0);
  rc = sh("echo job-out &", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strncmp(sh_out, "job ", 4) == 0);
  assert(std::strstr(sh_out, " pid=") != nullptr);
  char jobs_list[256];
  rc = sh("jobs", jobs_list, sizeof(jobs_list));
  assert(rc == 0);
  assert(std::strstr(jobs_list, " cmd.exe running\n") != nullptr);
  rc = sh("wait", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strstr(sh_out, "exit=0") != nullptr);
  assert(std::strstr(sh_out, "out=[job-out]") != nullptr);
  rc = sh("jobs", jobs_list, sizeof(jobs_list));
  assert(rc == 0);
  assert(std::strstr(jobs_list, " exited 0\n") != nullptr);
  char job_cons[512];
  rc = fd(std::string("read") + '\x1f' + "0", job_cons, sizeof(job_cons));
  assert(rc == 0);
  assert(std::strstr(job_cons, "job-out") != nullptr);
  rc = sh("true", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "true=0") == 0);
  rc = sh("status", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "0") == 0);
  rc = sh("false", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "false=1") == 0);
  rc = sh("status", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "1") == 0);
  rc = sh("ping -n 30 127.0.0.1 &", sh_out, sizeof(sh_out));
  assert(rc == 0);
  int job_id = 0;
  assert(std::sscanf(sh_out, "job %d pid=", &job_id) == 1);
  std::string kill = "kill " + std::to_string(job_id);
  rc = sh(kill.c_str(), sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, kill.c_str()) == 0);
  std::string reap = "wait " + std::to_string(job_id);
  char reap_out[256];
  rc = sh(reap.c_str(), reap_out, sizeof(reap_out));
  assert(rc == 0);
  assert(std::strstr(reap_out, "exit=1") != nullptr);
  rc = sh("status", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "1") == 0);
  rc = sh("cat < #e/PATH", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "/proc") == 0);
  rc = sh("echo /home > #e/HOME", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(sh_out[0] == '\0');
  rc = env(std::string("get") + '\x1f' + "HOME", env_get, sizeof(env_get));
  assert(rc == 0);
  assert(std::strcmp(env_get, "/home") == 0);
  char eopen[64];
  rc = fd(std::string("open") + '\x1f' + "#e" + '\x1f' + "r", eopen, sizeof(eopen));
  assert(rc == 0);
  assert(std::strstr(eopen, "path=#e") != nullptr);
  int efd = 0;
  assert(std::sscanf(eopen, "fd=%d", &efd) == 1);
  char enames[128];
  rc = fd(std::string("read") + '\x1f' + std::to_string(efd), enames, sizeof(enames));
  assert(rc == 0);
  assert(std::strstr(enames, "HOME\n") != nullptr);
  assert(std::strstr(enames, "PATH\n") != nullptr);
  rc = fd(std::string("close") + '\x1f' + std::to_string(efd), eopen, sizeof(eopen));
  assert(rc == 0);
  rc = fd(std::string("open") + '\x1f' + "#e/PATH/nope", eopen, sizeof(eopen));
  assert(rc == 0);
  assert(std::strcmp(eopen, "walk miss") == 0);
  rc = sh("unset HOME", sh_out, sizeof(sh_out));
  assert(rc == 0);
  rc = env(std::string("get") + '\x1f' + "HOME", env_get, sizeof(env_get));
  assert(rc == 0);
  assert(std::strcmp(env_get, "env miss") == 0);
  std::string self = std::to_string(wasmgocos::KernelPid());
  rc = sh("cat < #p/self", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, self.c_str()) == 0);
  rc = sh(("cat < #p/" + self).c_str(), sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "gockrnl") == 0);
  rc = sh("cat < #p", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strstr(sh_out, self.c_str()) != nullptr);
  assert(std::strstr(sh_out, "gockrnl") != nullptr);
  char popen_[64];
  rc = fd(std::string("open") + '\x1f' + "#p" + '\x1f' + "w", popen_, sizeof(popen_));
  assert(rc == 0);
  assert(std::strcmp(popen_, "walk miss") == 0);
  rc = sh("bind #e /env", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "bind #e /env") == 0);
  rc = sh("bind -b #c /n", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "bind -b #c /n") == 0);
  rc = sh("bind -a #e /n", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "bind -a #e /n") == 0);
  char union_open[64];
  rc = fd(std::string("open") + '\x1f' + "/n/cons" + '\x1f' + "r", union_open, sizeof(union_open));
  assert(rc == 0);
  assert(std::strstr(union_open, "path=#c/cons") != nullptr);
  int union_fd = 0;
  assert(std::sscanf(union_open, "fd=%d", &union_fd) == 1);
  rc = fd(std::string("close") + '\x1f' + std::to_string(union_fd), union_open, sizeof(union_open));
  assert(rc == 0);
  rc = fd(std::string("open") + '\x1f' + "/n/PATH" + '\x1f' + "r", union_open, sizeof(union_open));
  assert(rc == 0);
  assert(std::strstr(union_open, "path=#e/PATH") != nullptr);
  assert(std::sscanf(union_open, "fd=%d", &union_fd) == 1);
  rc = fd(std::string("close") + '\x1f' + std::to_string(union_fd), union_open, sizeof(union_open));
  assert(rc == 0);
  rc = fd(std::string("open") + '\x1f' + "/n" + '\x1f' + "r", union_open, sizeof(union_open));
  assert(rc == 0);
  assert(std::strstr(union_open, "path=/n") != nullptr);
  assert(std::sscanf(union_open, "fd=%d", &union_fd) == 1);
  char union_ls[256];
  rc = fd(std::string("read") + '\x1f' + std::to_string(union_fd), union_ls, sizeof(union_ls));
  assert(rc == 0);
  assert(std::strstr(union_ls, "cons\n") != nullptr);
  assert(std::strstr(union_ls, "PATH\n") != nullptr);
  rc = fd(std::string("close") + '\x1f' + std::to_string(union_fd), union_open, sizeof(union_open));
  assert(rc == 0);
  rc = sh("cat < /env/PATH", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "/proc") == 0);
  rc = sh("echo dos-con > CON", sh_out, sizeof(sh_out));
  assert(rc == 0);
  rc = sh("echo gone > NUL", sh_out, sizeof(sh_out));
  assert(rc == 0);
  char con_read[512];
  rc = fd(std::string("read") + '\x1f' + "0", con_read, sizeof(con_read));
  assert(rc == 0);
  assert(std::strstr(con_read, "dos-con") != nullptr);
  assert(std::strstr(con_read, "gone") == nullptr);
  char uname_sh[512];
  rc = sh("uname", uname_sh, sizeof(uname_sh));
  assert(rc == 0);
  assert(std::strstr(uname_sh, "Windows_NT ") == uname_sh);
  assert(std::strchr(uname_sh, '.') != nullptr);
  auto stamp = [](const char* s) {
    return std::strlen(s) >= 23 && s[4] == '-' && s[7] == '-' && s[10] == 'T' && s[13] == ':' &&
           s[16] == ':' && s[19] == '.' && std::strncmp(s, "2026-", 5) == 0;
  };
  rc = sh("date", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(stamp(sh_out));
  rc = sh("cat < #t", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(stamp(sh_out));
  rc = sh("echo one ; echo two", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "one\ntwo") == 0);
  rc = sh("true ; false", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "true=0\nfalse=1") == 0);
  rc = sh("status", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "1") == 0);
  rc = sh("false && echo no", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "false=1") == 0);
  rc = sh("status", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "1") == 0);
  rc = sh("false || echo yes", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "false=1\nyes") == 0);
  rc = sh("status", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "0") == 0);
  rc = sh("true && echo yes", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "true=0\nyes") == 0);
  rc = sh("true || echo no", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "true=0") == 0);
  rc = sh("false && echo no ; echo after", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "false=1\nafter") == 0);
  rc = sh("status", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "0") == 0);
  rc = sh("echo $PATH", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "/proc") == 0);
  rc = sh("echo ${PATH}", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "/proc") == 0);
  rc = sh("echo %PATH%", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "/proc") == 0);
  rc = sh("export HOME=/home ; echo $HOME", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "/home") == 0);
  rc = sh("false ; echo $?", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "false=1\n1") == 0);
  rc = sh("false ; echo %ERRORLEVEL%", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strcmp(sh_out, "false=1\n1") == 0);
  rc = sh("unset HOME", sh_out, sizeof(sh_out));
  assert(rc == 0);
#if defined(WASMGOCOS_HAS_VCPU)
  uint32_t ctr_exits = wasmturbo_vcpu_io_exits();
#endif
  char ctr_load[512];
  rc = wasmgocos_call("Ctr", (std::string("load") + '\x1f' + "demo").c_str(), ctr_load,
                      sizeof(ctr_load));
  assert(rc == 0);
#if defined(WASMGOCOS_HAS_VCPU)
  assert(wasmturbo_vcpu_io_exits() == ctr_exits);
#endif
  assert(std::strstr(ctr_load, "name=demo") != nullptr);
  assert(std::strstr(ctr_load, " service=hello from the container") != nullptr);
  unsigned ctr_pid = 0;
  unsigned ctr_gia = 0;
  unsigned ctr_chpt = 0;
  unsigned ctr_tpt = 0;
  unsigned ctr_ept = 0;
  unsigned long long ctr_cage = 0;
  assert(std::sscanf(ctr_load, "ctr pid=%u name=demo gia=%u chpt=%u tpt=%u ept=%u cage=%llu",
                     &ctr_pid, &ctr_gia, &ctr_chpt, &ctr_tpt, &ctr_ept, &ctr_cage) == 6);
  assert(ctr_pid != 0 && ctr_gia != 0 && ctr_chpt != 0 && ctr_tpt != 0 && ctr_ept != 0);
  assert(ctr_cage == std::strtoull(cage + 5, nullptr, 10));
  const char* ctr_ipt = std::strstr(ctr_load, " ipt=");
  const char* ctr_gia_ipt = std::strstr(ctr_load, " gia_ipt=");
  assert(ctr_ipt != nullptr && ctr_gia_ipt != nullptr);
  unsigned ipt = 0;
  unsigned gia_ipt = 0;
  assert(std::sscanf(ctr_ipt, " ipt=%u", &ipt) == 1);
  assert(std::sscanf(ctr_gia_ipt, " gia_ipt=%u", &gia_ipt) == 1);
  assert(ipt != 0 && gia_ipt != 0 && ipt != gia_ipt);
  char ctr_file[64];
  rc = vfs(std::string("read") + '\x1f' + "/ctr/demo", ctr_file, sizeof(ctr_file));
  assert(rc == 0);
  assert(std::strncmp(ctr_file, "hello from the container", 24) == 0);
  char ctr_unil[16];
  rc = vfs(std::string("read") + '\x1f' + "/ctr/demo.unil", ctr_unil, sizeof(ctr_unil));
  assert(rc == 0);
  assert(std::strncmp(ctr_unil, "UNIL", 4) == 0);
  char ctr_region[64];
  char ctr_ls[128];
  rc = vfs(std::string("ls") + '\x1f' + "/ctr/demo", ctr_ls, sizeof(ctr_ls));
  assert(rc == 0);
  assert(std::strstr(ctr_ls, "service\n") != nullptr);
  assert(std::strstr(ctr_ls, "name\n") != nullptr);
  assert(std::strstr(ctr_ls, "gocvm.Call\n") != nullptr);
  assert(std::strstr(ctr_ls, "app.js\n") != nullptr);
  rc = vfs(std::string("read") + '\x1f' + "/ctr/demo/result", ctr_region, sizeof(ctr_region));
  assert(rc == 0);
  assert(std::strcmp(ctr_region, "3") == 0);
  rc = vfs(std::string("read") + '\x1f' + "/ctr/demo/service", ctr_region, sizeof(ctr_region));
  assert(rc == 0);
  assert(std::strcmp(ctr_region, "hello from the container") == 0);
  rc = vfs(std::string("read") + '\x1f' + "/ctr/demo/name", ctr_region, sizeof(ctr_region));
  assert(rc == 0);
  assert(std::strcmp(ctr_region, "demo") == 0);
  const char* ctr_rewritten = "hello from the object!!!";
  rc = vfs(std::string("write") + '\x1f' + "/ctr/demo/service" + '\x1f' + ctr_rewritten, ctr_region,
           sizeof(ctr_region));
  assert(rc == 0);
  assert(std::strcmp(ctr_region, "write /ctr/demo/service") == 0);
  rc = vfs(std::string("read") + '\x1f' + "/ctr/demo/service", ctr_region, sizeof(ctr_region));
  assert(rc == 0);
  assert(std::strcmp(ctr_region, ctr_rewritten) == 0);
  char ctr_gia_file[32];
  rc = vfs(std::string("gia") + '\x1f' + "/ctr/demo", ctr_gia_file, sizeof(ctr_gia_file));
  assert(rc == 0);
  assert(std::strcmp(ctr_gia_file, "ipt miss") == 0);
  char ctr_body[64];
  rc = wasmgocos_call("Ctr",
                      (std::string("read") + '\x1f' + std::to_string(ctr_pid) + '\x1f' + "service")
                          .c_str(),
                      ctr_body, sizeof(ctr_body));
  assert(rc == 0);
  assert(std::strcmp(ctr_body, "hello from the object!!!") == 0);
  const char* ctr_grown = "hello from the grown object";
  rc = vfs(std::string("write") + '\x1f' + "/ctr/demo/service" + '\x1f' + ctr_grown, ctr_region,
           sizeof(ctr_region));
  assert(rc == 0);
  assert(std::strcmp(ctr_region, "write /ctr/demo/service") == 0);
  rc = wasmgocos_call("Ctr",
                      (std::string("read") + '\x1f' + std::to_string(ctr_pid) + '\x1f' + "service")
                          .c_str(),
                      ctr_body, sizeof(ctr_body));
  assert(rc == 0);
  assert(std::strcmp(ctr_body, ctr_grown) == 0);
  rc = vfs(std::string("read") + '\x1f' + "/ctr/demo/name", ctr_region, sizeof(ctr_region));
  assert(rc == 0);
  assert(std::strcmp(ctr_region, "demo") == 0);
  rc = vfs(std::string("read") + '\x1f' + "/ctr/demo", ctr_file, sizeof(ctr_file));
  assert(rc == 0);
  assert(std::strncmp(ctr_file, ctr_grown, std::strlen(ctr_grown)) == 0);
  char box_open[64];
  int box_fd = 0;
  rc = sh("bind /ctr/demo /box", sh_out, sizeof(sh_out));
  assert(rc == 0);
  assert(std::strstr(sh_out, "bind /ctr/demo /box") != nullptr);
  rc = fd(std::string("open") + '\x1f' + "/box/service" + '\x1f' + "r", box_open, sizeof(box_open));
  assert(rc == 0);
  assert(std::sscanf(box_open, "fd=%d", &box_fd) == 1);
  rc = fd(std::string("read") + '\x1f' + std::to_string(box_fd), ctr_file, sizeof(ctr_file));
  assert(rc == 0);
  assert(std::strcmp(ctr_file, ctr_grown) == 0);
  rc = fd(std::string("close") + '\x1f' + std::to_string(box_fd), box_open, sizeof(box_open));
  assert(rc == 0);
  assert(std::strstr(ctr_load, " call=gocvm.Call") != nullptr);
  char ctr_call[64];
  rc = wasmgocos_call("Ctr",
                      (std::string("call") + '\x1f' + std::to_string(ctr_pid) + '\x1f' + "gocos" +
                       '\x1f' + "Rc")
                          .c_str(),
                      ctr_call, sizeof(ctr_call));
  assert(rc == 0);
#if defined(WASMGOCOS_HAS_VCPU)
  assert(wasmturbo_vcpu_io_exits() == ctr_exits);
#endif
  assert(std::strcmp(ctr_call, "0") == 0);
  std::string ctr_proc(ctr_load);
  size_t ctr_svc = ctr_proc.find(" service=");
  assert(ctr_svc != std::string::npos);
  ctr_proc.resize(ctr_svc);
  char proc_open[64];
  rc = fd(std::string("open") + '\x1f' + "#p/" + std::to_string(ctr_pid) + '\x1f' + "r", proc_open,
          sizeof(proc_open));
  assert(rc == 0);
  int proc_fd = 0;
  assert(std::sscanf(proc_open, "fd=%d", &proc_fd) == 1);
  char proc_full[512];
  rc = fd(std::string("read") + '\x1f' + std::to_string(proc_fd), proc_full, sizeof(proc_full));
  assert(rc == 0);
  assert(std::strcmp(proc_full, ctr_proc.c_str()) == 0);
  assert(std::strstr(proc_full, "name=demo") != nullptr);
  assert(std::strstr(proc_full, " call=gocvm.Call") != nullptr);
  assert(std::strstr(proc_full, " chpt=") != nullptr);
  assert(std::strstr(proc_full, " tpt=") != nullptr);
  assert(std::strstr(proc_full, " ept=") != nullptr);
  rc = fd(std::string("close") + '\x1f' + std::to_string(proc_fd), proc_open, sizeof(proc_open));
  assert(rc == 0);
  rc = fd(std::string("open") + '\x1f' + "#p" + '\x1f' + "r", proc_open, sizeof(proc_open));
  assert(rc == 0);
  assert(std::sscanf(proc_open, "fd=%d", &proc_fd) == 1);
  char proc_table[1024];
  rc = fd(std::string("read") + '\x1f' + std::to_string(proc_fd), proc_table, sizeof(proc_table));
  assert(rc == 0);
  assert(std::strstr(proc_table, ctr_proc.c_str()) != nullptr);
  rc = fd(std::string("close") + '\x1f' + std::to_string(proc_fd), proc_open, sizeof(proc_open));
  assert(rc == 0);
  char ctr_sh[1024];
  rc = sh("ctr load shellbox", ctr_sh, sizeof(ctr_sh));
  assert(rc == 0);
  assert(std::strstr(ctr_sh, "name=shellbox") != nullptr);
  assert(std::strstr(ctr_sh, " service=hello from the container") != nullptr);
  assert(std::strstr(ctr_sh, " engine=quickjs") != nullptr);
  assert(std::strstr(ctr_sh, " js=quickjs-ng") != nullptr);
  assert(std::strstr(ctr_sh, " rt=wazgoc") != nullptr);
  unsigned shell_pid = 0;
  assert(std::sscanf(ctr_sh, "ctr pid=%u name=shellbox", &shell_pid) == 1);
  assert(shell_pid != 0 && shell_pid != ctr_pid);
  rc = sh(("ctr read " + std::to_string(shell_pid) + " service").c_str(), ctr_sh, sizeof(ctr_sh));
  assert(rc == 0);
  assert(std::strcmp(ctr_sh, "hello from the container") == 0);
  rc = sh(("ctr eval " + std::to_string(shell_pid) + " 1 + 2").c_str(), ctr_sh, sizeof(ctr_sh));
  assert(rc == 0);
  assert(std::strcmp(ctr_sh, "3") == 0);
  rc = sh(("ctr call " + std::to_string(shell_pid) + " gocos Rc").c_str(), ctr_sh, sizeof(ctr_sh));
  assert(rc == 0);
  assert(std::strcmp(ctr_sh, "0") == 0);
  char ctl_open[64];
  int ctl_fd = 0;
  rc = fd(std::string("open") + '\x1f' + "#u/shellbox/ctl" + '\x1f' + "w", ctl_open, sizeof(ctl_open));
  assert(rc == 0);
  assert(std::sscanf(ctl_open, "fd=%d", &ctl_fd) == 1);
  rc = fd(std::string("write") + '\x1f' + std::to_string(ctl_fd) + '\x1f' + "eval 4 + 5", ctl_open,
          sizeof(ctl_open));
  assert(rc == 0);
  assert(std::strncmp(ctl_open, "write ", 6) == 0);
  rc = fd(std::string("close") + '\x1f' + std::to_string(ctl_fd), ctl_open, sizeof(ctl_open));
  assert(rc == 0);
  char ctl_frame[384];
  rc = fd(std::string("open") + '\x1f' + "#u/shellbox/gocvm.Call" + '\x1f' + "r", ctl_open,
          sizeof(ctl_open));
  assert(rc == 0);
  assert(std::sscanf(ctl_open, "fd=%d", &ctl_fd) == 1);
  rc = fd(std::string("read") + '\x1f' + std::to_string(ctl_fd), ctl_frame, sizeof(ctl_frame));
  assert(rc == 0);
  assert(ctl_frame[192] == '9' && ctl_frame[193] == 0);
  char unil_open[64];
  rc = fd(std::string("open") + '\x1f' + "#u" + '\x1f' + "r", unil_open, sizeof(unil_open));
  assert(rc == 0);
  int unil_fd = 0;
  assert(std::sscanf(unil_open, "fd=%d", &unil_fd) == 1);
  char unil_names[128];
  rc = fd(std::string("read") + '\x1f' + std::to_string(unil_fd), unil_names, sizeof(unil_names));
  assert(rc == 0);
  assert(std::strstr(unil_names, "demo\n") != nullptr);
  assert(std::strstr(unil_names, "shellbox\n") != nullptr);
  rc = fd(std::string("close") + '\x1f' + std::to_string(unil_fd), unil_open, sizeof(unil_open));
  assert(rc == 0);
  rc = fd(std::string("open") + '\x1f' + "#u/shellbox/service" + '\x1f' + "r", unil_open,
          sizeof(unil_open));
  assert(rc == 0);
  assert(std::sscanf(unil_open, "fd=%d", &unil_fd) == 1);
  rc = fd(std::string("read") + '\x1f' + std::to_string(unil_fd), unil_names, sizeof(unil_names));
  assert(rc == 0);
  assert(std::strcmp(unil_names, "hello from the container") == 0);
  rc = fd(std::string("close") + '\x1f' + std::to_string(unil_fd), unil_open, sizeof(unil_open));
  assert(rc == 0);
  rc = sh("ctr", ctr_sh, sizeof(ctr_sh));
  assert(rc == 0);
  assert(std::strstr(ctr_sh, "name=demo") != nullptr);
  assert(std::strstr(ctr_sh, "name=shellbox") != nullptr);

  char svc_out[512];
  rc = sh("svc rc boot 2 echo boot-b", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strstr(svc_out, "kind=init") != nullptr);
  assert(std::strstr(svc_out, "order=2") != nullptr);
  rc = sh("svc rc net 1 echo boot-a", svc_out, sizeof(svc_out));
  assert(rc == 0);
  rc = vfs(std::string("write") + '\x1f' + "/etc/rc.d/early" + '\x1f' + "echo from-rc", svc_out,
           sizeof(svc_out));
  assert(rc == 0);
  rc = sh("svc init", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strstr(svc_out, "start net out=[boot-a]") != nullptr);
  assert(std::strstr(svc_out, "start boot out=[boot-b]") != nullptr);
  assert(std::strstr(svc_out, "start early out=[from-rc]") != nullptr);
  rc = sh("svc daemon beep echo up", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strstr(svc_out, "kind=daemon") != nullptr);
  rc = sh("svc start beep", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strcmp(svc_out, "start beep out=[up]") == 0);
  rc = sh("svc stop beep", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strcmp(svc_out, "stop beep") == 0);
  rc = sh("svc sched pulse 1 echo tock", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strstr(svc_out, "kind=sched") != nullptr);
  rc = sh("svc start pulse", svc_out, sizeof(svc_out));
  assert(rc == 0);
  rc = sh("svc tick", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strcmp(svc_out, "tick pulse out=[tock]") == 0);
  rc = vfs(std::string("write") + '\x1f' + "/etc/daemon/lamp" + '\x1f' + "echo on", svc_out,
           sizeof(svc_out));
  assert(rc == 0);
  rc = sh("svc start lamp", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strcmp(svc_out, "start lamp out=[on]") == 0);
  rc = vfs(std::string("write") + '\x1f' + "/etc/sched/beat" + '\x1f' + "2 echo beat", svc_out,
           sizeof(svc_out));
  assert(rc == 0);
  rc = sh("svc start beat", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strstr(svc_out, "start beat out=[beat]") != nullptr);
  char svc_tab[1024];
  int svc_fd = 0;
  rc = fd(std::string("open") + '\x1f' + "#s" + '\x1f' + "r", svc_tab, sizeof(svc_tab));
  assert(rc == 0);
  assert(std::sscanf(svc_tab, "fd=%d", &svc_fd) == 1);
  rc = fd(std::string("read") + '\x1f' + std::to_string(svc_fd), svc_tab, sizeof(svc_tab));
  assert(rc == 0);
  assert(std::strstr(svc_tab, "name=lamp kind=daemon state=running") != nullptr);
  assert(std::strstr(svc_tab, "name=pulse kind=sched") != nullptr);
  rc = fd(std::string("close") + '\x1f' + std::to_string(svc_fd), svc_out, sizeof(svc_out));
  assert(rc == 0);
  rc = fd(std::string("open") + '\x1f' + "#s/beat" + '\x1f' + "r", svc_tab, sizeof(svc_tab));
  assert(rc == 0);
  assert(std::sscanf(svc_tab, "fd=%d", &svc_fd) == 1);
  rc = fd(std::string("read") + '\x1f' + std::to_string(svc_fd), svc_tab, sizeof(svc_tab));
  assert(rc == 0);
  assert(std::strstr(svc_tab, "name=beat kind=sched state=waiting every=2") != nullptr);
  rc = sh("svc tick", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strstr(svc_out, "tick pulse out=[tock]") != nullptr);
  assert(std::strstr(svc_out, "tick beat") == nullptr);
  rc = sh("svc tick", svc_out, sizeof(svc_out));
  assert(rc == 0);
  assert(std::strstr(svc_out, "tick beat out=[beat]") != nullptr);

  std::vector<uint8_t> peek = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  Section(&peek, 1, {0x01, 0x60, 0x00, 0x01, 0x7f});
  Section(&peek, 3, {0x01, 0x00});
  Section(&peek, 5, {0x01, 0x00, 0x01});
  Section(&peek, 7, {0x01, 0x04, 'p', 'e', 'e', 'k', 0x00, 0x00});
  Section(&peek, 10, {0x01, 0x07, 0x00, 0x41, 0x00, 0x28, 0x02, 0x00, 0x0b});
  Section(&peek, 11, {0x01, 0x00, 0x41, 0x00, 0x0b, 0x04, 42, 0x00, 0x00, 0x00});
  wasmgocos::JitModule* cached = wasmgocos::jit_load(peek.data(), peek.size());
  assert(cached != nullptr);
  int32_t peeked = 0;
#if defined(WASMGOCOS_HAS_VCPU)
  assert(wasmgocos::jit_i32(cached, "peek", &peeked, nullptr, 0));
#else
  assert(wasmgocos::jit_cage_i32(cached, "peek", &peeked, nullptr, 0));
#endif
  assert(peeked == 42);
  wasmgocos::JitNames peek_names;
  assert(wasmgocos::jit_names(cached, &peek_names));
  assert(peek_names.memory_gia != 0);
#if defined(WASMGOCOS_HAS_VCPU)
  assert(wasmturbo_vcpu_cache_gia() == peek_names.memory_gia);
#endif
  assert(peek_names.memory_base != peek_names.memory_gia);
  wasmgocos::jit_drop(cached);
  return 0;
#endif
}
