#include "gocos/jitrt.hpp"

#include "gocos/dispatch.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cppgc/default-platform.h"
#include "src/hostcalls/host-calls.h"
#include "src/instance.h"
#include "src/runtime/intern.h"
#include "src/runtime/mss.h"
#include "src/runtime/unil.h"
#include "src/runtime/vfs.h"

namespace wasmgocos {

// The process cage: every container, guest memory, and IPT object. A
// wasigocvm guest's memory grows by copying, so it needs room for two
// copies of itself. Untouched pages cost nothing.
constexpr size_t kProcessCageBytes = size_t{512} << 20;

wasmturbo::Runtime& ProcessRuntime() {
  // Never destroyed: containers, jobs, and modules released at exit give
  // their objects back to it (Instance::~Instance), in any order.
  static wasmturbo::Runtime& runtime = *new wasmturbo::Runtime(kProcessCageBytes);
  // GIA 0 is null: MSS classifies cage offset 0 as raw. The first object
  // the cage hands out sits at offset 0, so the runtime keeps it � a guard
  // no module, container, or file ever gets.
  static const bool guarded = [] {
    static wasmturbo::CageObject guard;
    return runtime.GrowCageObject(&guard, 64, wasmturbo::kTypeIdGlobal) && guard.gia == 0;
  }();
  (void)guarded;
  return runtime;
}

}  // namespace wasmgocos

// TurboSpace's vCPU takes its memory from here, the cage the JIT runs
// modules on; the kernel's RAM is then a range of that vCPU's window.
extern "C" v8::internal::Sandbox* wasmturbo_process_cage() {
  return &wasmgocos::ProcessRuntime().cage();
}

namespace wasmgocos {

uint32_t KernelPid() {
#if defined(_WIN32)
  return GetCurrentProcessId();
#else
  return static_cast<uint32_t>(getpid());
#endif
}

namespace {

struct InstallGocosCall {
  InstallGocosCall() { wasmturbo_set_gocos_call(wasmgocos_call); }
};
InstallGocosCall install_gocos_call;

void Append(std::vector<uint8_t>* m, std::initializer_list<uint8_t> bytes) {
  m->insert(m->end(), bytes.begin(), bytes.end());
}

void Section(std::vector<uint8_t>* m, uint8_t id, const std::vector<uint8_t>& body) {
  m->push_back(id);
  m->push_back(static_cast<uint8_t>(body.size()));
  m->insert(m->end(), body.begin(), body.end());
}

// (module
//   (import "win32" "GetCurrentProcessId" (func (result i32)))
//   (func (export "pid") (result i32) call 0))
// The call is the only way this module can produce a pid. Liftoff emits
// it as an OUT; the vCPU exit is the catalog Call.
std::vector<uint8_t> PidModule() {
  std::vector<uint8_t> m = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  Section(&m, 1, {0x01, 0x60, 0x00, 0x01, 0x7f});

  const char* mod = "win32";
  const char* name = "GetCurrentProcessId";
  std::vector<uint8_t> imp = {0x01};
  imp.push_back(static_cast<uint8_t>(std::char_traits<char>::length(mod)));
  imp.insert(imp.end(), mod, mod + std::char_traits<char>::length(mod));
  imp.push_back(static_cast<uint8_t>(std::char_traits<char>::length(name)));
  imp.insert(imp.end(), name, name + std::char_traits<char>::length(name));
  Append(&imp, {0x00, 0x00});
  Section(&m, 2, imp);

  Section(&m, 3, {0x01, 0x00});
  Section(&m, 7, {0x01, 0x03, 'p', 'i', 'd', 0x00, 0x01});
  Section(&m, 10, {0x01, 0x04, 0x00, 0x10, 0x00, 0x0b});
  return m;
}

// (module
//   (memory 1)
//   (data (i32.const 0) "\xx\00\00\00")
//   (func (export "peek") (result i32)
//     i32.const 0
//     i32.load))
// The load reads the module's cage object, resolved through CHPT.
std::vector<uint8_t> PeekModule(uint8_t byte0) {
  std::vector<uint8_t> m = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  Section(&m, 1, {0x01, 0x60, 0x00, 0x01, 0x7f});
  Section(&m, 3, {0x01, 0x00});
  Section(&m, 5, {0x01, 0x00, 0x01});
  Section(&m, 7, {0x01, 0x04, 'p', 'e', 'e', 'k', 0x00, 0x00});
  Section(&m, 10, {0x01, 0x07, 0x00, 0x41, 0x00, 0x28, 0x02, 0x00, 0x0b});
  Section(&m, 11, {0x01, 0x00, 0x41, 0x00, 0x0b, 0x04, byte0, 0x00, 0x00, 0x00});
  return m;
}

// No memory section. The UNIL store is the data segment: topic at 0
// (19 bytes), an empty payload, reply at 19 (32 bytes).
// gocos.Call(0, 19, 19, 0, 19, 32)
std::vector<uint8_t> CallUnilModule() {
  std::vector<uint8_t> m = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  Section(&m, 1, {0x02, 0x60, 0x06, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x00,
                  0x01, 0x7f});
  Section(&m, 2, {0x01, 0x05, 'g', 'o', 'c', 'o', 's', 0x04, 'C', 'a', 'l', 'l', 0x00, 0x00});
  Section(&m, 3, {0x01, 0x01});
  Section(&m, 7, {0x01, 0x04, 'c', 'a', 'l', 'l', 0x00, 0x01});
  Section(&m, 10, {0x01, 0x10, 0x00, 0x41, 0x00, 0x41, 0x13, 0x41, 0x13, 0x41, 0x00, 0x41, 0x13,
                   0x41, 0x20, 0x10, 0x00, 0x0b});
  return m;
}

}  // namespace

struct JitModule {
  std::unique_ptr<wasmturbo::Instance> inst;
};

JitModule* jit_load(const uint8_t* wasm, size_t n) {
  return jit_load_unil(wasm, n, nullptr, 0);
}

JitModule* jit_load_unil(const uint8_t* wasm, size_t n, const uint8_t* unil, size_t unil_n) {
  if (wasm == nullptr || n == 0) return nullptr;
  wasmturbo::UnilStore store;
  if (unil != nullptr && unil_n > 0) store = wasmturbo::UnilStore::Parse(unil, unil_n);
  auto module = std::make_unique<JitModule>();
  module->inst = std::make_unique<wasmturbo::Instance>(
      ProcessRuntime(), std::vector<uint8_t>(wasm, wasm + n), std::move(store));
  return module.release();
}

bool jit_unil_read(JitModule* module, const char* name, void* out, size_t cap, size_t* n) {
  if (module == nullptr || module->inst == nullptr || name == nullptr || n == nullptr) return false;
  const wasmturbo::UnilRegion* region = module->inst->unil().Find(name);
  if (region == nullptr || cap < region->size) return false;
  std::vector<uint8_t> bytes = module->inst->UnilBytes(name);
  if (out != nullptr && !bytes.empty()) std::memcpy(out, bytes.data(), bytes.size());
  *n = bytes.size();
  return true;
}

bool jit_unil_write(JitModule* module, const char* name, const void* bytes, size_t n) {
  if (module == nullptr || module->inst == nullptr || name == nullptr) return false;
  const wasmturbo::UnilRegion* region = module->inst->unil().Find(name);
  if (region == nullptr || region->size != n) return false;
  const auto* p = static_cast<const uint8_t*>(bytes);
  module->inst->WriteUnil(name, std::vector<uint8_t>(p, p + n));
  return true;
}

void jit_drop(JitModule* module) { delete module; }

bool jit_i32(JitModule* module, const char* export_name, int32_t* result, const int32_t* args,
             int nargs) {
  if (module == nullptr || module->inst == nullptr || export_name == nullptr || result == nullptr ||
      nargs < 0 || nargs > 2)
    return false;
  if (nargs > 0 && args == nullptr) return false;
  std::optional<int64_t> ran;
  if (nargs <= 0)
    ran = module->inst->RunInHypervisor(export_name);
  else if (nargs == 1)
    ran = module->inst->RunInHypervisor(export_name, {args[0]});
  else
    ran = module->inst->RunInHypervisor(export_name, {args[0], args[1]});
  if (!ran) return false;
  *result = static_cast<int32_t>(*ran);
  return true;
}

JitModule* jit_load_file(const char* path) {
  if (path == nullptr || path[0] == 0) return nullptr;
  std::ifstream in(path, std::ios::binary);
  if (!in) return nullptr;
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return jit_load(bytes.data(), bytes.size());
}

bool jit_cage_i32(JitModule* module, const char* export_name, int32_t* result, const int32_t* args,
                  int nargs) {
  if (module == nullptr || module->inst == nullptr || export_name == nullptr || result == nullptr ||
      nargs < 0 || nargs > 2)
    return false;
  if (nargs > 0 && args == nullptr) return false;
  int32_t value = 0;
  if (nargs <= 0)
    value = module->inst->CallI32(export_name);
  else if (nargs == 1)
    value = module->inst->CallI32(export_name, {args[0]});
  else
    value = module->inst->CallI32(export_name, {args[0], args[1]});
  *result = value;
  return true;
}

bool jit_entry(JitModule* module) {
  if (module == nullptr || module->inst == nullptr) return false;
  return module->inst->RunEntryPointIfPresent();
}

bool jit_names(JitModule* module, JitNames* out) {
  if (module == nullptr || module->inst == nullptr || out == nullptr) return false;
  wasmturbo::Instance& inst = *module->inst;
  wasmturbo::Runtime& runtime = inst.runtime();
  out->cage_base = runtime.cage().base();
  out->engine = inst.names().engine;
  out->hypervisor = runtime.AttachHypervisor();
  out->instance_ept = inst.names().instance_ept;
  out->type_key = inst.names().type_keys.empty() ? 0 : inst.names().type_keys[0];
  const wasmturbo::CageObject& mem = inst.memory_object();
  out->memory_chpt = mem.chpt;
  out->memory_gia = 0;
  if (mem.chpt != 0) {
    if (!runtime.cage().Contains(reinterpret_cast<void*>(mem.base))) return false;
    if (mem.gia != runtime.Encode(mem.base)) return false;
    out->memory_gia = mem.gia;
    out->memory_base = mem.base;
  }
  return out->cage_base != 0 && out->engine != 0;
}

std::string jit_share() {
  std::vector<uint8_t> first = PeekModule(42);
  std::vector<uint8_t> second = PeekModule(7);
  JitModule* a = jit_load(first.data(), first.size());
  JitModule* b = jit_load(second.data(), second.size());
  if (a == nullptr || b == nullptr) {
    jit_drop(a);
    jit_drop(b);
    return "error: jit: no module";
  }
  JitNames na;
  JitNames nb;
  bool named = jit_names(a, &na) && jit_names(b, &nb);
  int32_t peek = 0;
  bool ran = named && jit_cage_i32(a, "peek", &peek, nullptr, 0);
  uint32_t word = 0;
  if (ran) {
    std::memcpy(&word, reinterpret_cast<const void*>(a->inst->memory_object().base), sizeof(word));
  }
  jit_drop(a);
  jit_drop(b);
  if (!ran || peek != 42 || word != 42) return "error: jit: peek";
  if (na.cage_base != nb.cage_base || na.engine != nb.engine || na.type_key == 0 ||
      na.type_key != nb.type_key || na.hypervisor != nb.hypervisor || na.memory_gia == 0 ||
      nb.memory_gia == 0 || na.memory_gia == nb.memory_gia)
    return "error: jit: share";
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "cage=%llu engine=%u hv=%u gia=%u peer=%u type=%u peek=42",
                static_cast<unsigned long long>(na.cage_base), na.engine, na.hypervisor,
                na.memory_gia, nb.memory_gia, na.type_key);
  return buf;
}

std::string jit_run(const char* path) {
  JitModule* module = jit_load_file(path);
  if (module == nullptr) return "error: jit: no file";
  bool ran = jit_entry(module);
  JitNames names;
  bool named = jit_names(module, &names);
  jit_drop(module);
  if (!named) return "error: jit: names";
  char buf[256];
  std::snprintf(buf, sizeof(buf), "entry=%d cage=%llu engine=%u hv=%u", ran ? 1 : 0,
                static_cast<unsigned long long>(names.cage_base), names.engine, names.hypervisor);
  return buf;
}

std::string jit_unil() {
  const char topic[] = "GetCurrentProcessId";
  wasmturbo::UnilStore store;
  store.Add("topic", std::vector<uint8_t>(topic, topic + sizeof(topic) - 1));
  store.Add("payload", 0);
  store.Add("reply", 32);
  const wasmturbo::UnilRegion* reply = store.Find("reply");
  const wasmturbo::UnilRegion* topic_region = store.Find("topic");
  if (topic_region == nullptr || reply == nullptr || topic_region->offset != 0 ||
      topic_region->size != sizeof(topic) - 1 || reply->offset != sizeof(topic) - 1 ||
      reply->size != 32)
    return "error: jit: unil layout";
  std::vector<uint8_t> container = store.Encode();
  std::vector<uint8_t> wasm = CallUnilModule();
  JitModule* module = jit_load_unil(wasm.data(), wasm.size(), container.data(), container.size());
  if (module == nullptr) return "error: jit: no module";
  int32_t rc = -1;
  bool ran = jit_cage_i32(module, "call", &rc, nullptr, 0);
  char text[33];
  size_t n = 0;
  bool read = ran && rc == 0 && jit_unil_read(module, "reply", text, sizeof(text) - 1, &n);
  JitNames names;
  bool named = jit_names(module, &names);
  jit_drop(module);
  if (!read || !named || n < 32) return "error: jit: unil";
  text[n < sizeof(text) ? n : sizeof(text) - 1] = 0;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "cage=%llu engine=%u reply=%s",
                static_cast<unsigned long long>(names.cage_base), names.engine, text);
  return buf;
}

std::string jit_pid() {
  std::vector<uint8_t> wasm = PidModule();
  JitModule* module = jit_load(wasm.data(), wasm.size());
  if (module == nullptr) return "error: jit: no module";
  int32_t pid = 0;
  bool ok = jit_i32(module, "pid", &pid, nullptr, 0);
  jit_drop(module);
  if (!ok) return "error: jit: no vcpu";
  return std::to_string(pid);
}

class MssTickJob final : public cppgc::JobTask {
 public:
  explicit MssTickJob(int* n) : n_(n) {}
  void Run(cppgc::JobDelegate*) override { ++*n_; }
  size_t GetMaxConcurrency(size_t worker_count) const override {
    return worker_count == 0 ? 1 : 0;
  }

 private:
  int* n_;
};

class MssTickTask final : public cppgc::Task {
 public:
  explicit MssTickTask(int* n) : n_(n) {}
  void Run() override { ++*n_; }

 private:
  int* n_;
};

bool jit_file_plant(const char* path, const void* body, size_t body_n, uint32_t* gia_ipt,
                    uint32_t* gia, uint32_t* len) {
  if (path == nullptr || path[0] == 0 || gia_ipt == nullptr || gia == nullptr || len == nullptr) {
    return false;
  }
  if (body != nullptr && body_n > 0) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(static_cast<const char*>(body), static_cast<std::streamsize>(body_n));
    if (!out) return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
  if (bytes.empty()) return false;
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  queue.Install();
  wasmturbo::MssEntry entry;
  if (!queue.Schedule(ProcessRuntime(), bytes.data(), bytes.size(), &entry)) return false;
  *gia_ipt = entry.gia_ipt;
  *gia = entry.gia;
  *len = entry.len;
  return true;
}

bool jit_file_take(uint32_t gia_ipt, void* out, size_t cap, size_t* n) {
  return wasmturbo::MssQueue::Process().Take(gia_ipt, out, cap, n);
}

std::string jit_file(const char* args) {
  if (args == nullptr || args[0] == 0) return "error: jit: no file";
  std::string spec(args);
  std::string path = spec;
  std::string body;
  size_t sep = spec.find('\x1f');
  if (sep != std::string::npos) {
    path = spec.substr(0, sep);
    body = spec.substr(sep + 1);
  }
  if (path.empty()) return "error: jit: no file";
  uint32_t gia_ipt = 0;
  uint32_t gia = 0;
  uint32_t len = 0;
  const void* body_ptr = body.empty() ? nullptr : body.data();
  if (!jit_file_plant(path.c_str(), body_ptr, body.size(), &gia_ipt, &gia, &len)) {
    return "error: jit: file";
  }
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  uint32_t matched_gia = 0;
  uint32_t raw_ipt = 0;
  uint64_t raw = 0;
  if (!queue.MatchGia(gia_ipt, &matched_gia) || matched_gia != gia) return "error: jit: file";
  if (!queue.RawForGia(gia_ipt, &raw_ipt, &raw)) return "error: jit: file";
  if (queue.MatchGia(raw_ipt, &matched_gia)) return "error: jit: file";
  std::vector<uint8_t> taken(len + 1);
  size_t n = 0;
  if (!jit_file_take(gia_ipt, taken.data(), len, &n) || n != len) return "error: jit: file";
  taken[len] = 0;
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "gia_ipt=%u raw_ipt=%u gia=%u raw=%llu len=%u cage=%llu text=%s",
                gia_ipt, raw_ipt, gia, static_cast<unsigned long long>(raw), len,
                static_cast<unsigned long long>(ProcessRuntime().cage().base()),
                reinterpret_cast<const char*>(taken.data()));
  return buf;
}

std::string jit_mss() {
  static const char kText[] = "hello from the heap";
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  queue.Install();
  wasmturbo::MssEntry entry;
  if (!queue.Schedule(ProcessRuntime(), kText, sizeof(kText) - 1, &entry)) {
    return "error: mss: schedule";
  }
  uint32_t gia = 0;
  uint64_t raw = 0;
  if (!queue.MatchGia(entry.gia_ipt, &gia) || gia != entry.gia) return "error: mss: gia";
  if (!queue.MatchRaw(entry.raw_ipt, &raw) || raw != entry.raw) return "error: mss: raw";
  uint32_t crossed_gia = 0;
  uint64_t crossed_raw = 0;
  if (queue.MatchGia(entry.raw_ipt, &crossed_gia)) return "error: mss: gia matched raw";
  if (queue.MatchRaw(entry.gia_ipt, &crossed_raw)) return "error: mss: raw matched gia";
  if (std::memcmp(reinterpret_cast<const void*>(raw), kText, sizeof(kText) - 1) != 0) {
    return "error: mss: bytes";
  }
  size_t jobs_before = queue.count(wasmturbo::MssKind::kJob);
  size_t tasks_before = queue.count(wasmturbo::MssKind::kTask);
  int job_ran = 0;
  int task_ran = 0;
  cppgc::DefaultPlatform platform;
  auto handle = platform.PostJob(cppgc::TaskPriority::kUserBlocking,
                                 std::make_unique<MssTickJob>(&job_ran));
  platform.GetForegroundTaskRunner(cppgc::TaskPriority::kUserBlocking)
      ->PostTask(std::make_unique<MssTickTask>(&task_ran));
  if (handle == nullptr || job_ran != 1 || task_ran != 1) return "error: mss: job";
  size_t jobs = queue.count(wasmturbo::MssKind::kJob);
  size_t tasks = queue.count(wasmturbo::MssKind::kTask);
  if (jobs != jobs_before + 1 || tasks != tasks_before + 1) return "error: mss: queue";
  char text[64];
  std::memcpy(text, reinterpret_cast<const void*>(raw), sizeof(kText) - 1);
  text[sizeof(kText) - 1] = 0;
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "cage=%llu gia=%u raw=%llu jobs=%u tasks=%u depth=%u text=%s",
                static_cast<unsigned long long>(ProcessRuntime().cage().base()), gia,
                static_cast<unsigned long long>(raw), static_cast<unsigned>(jobs),
                static_cast<unsigned>(tasks), static_cast<unsigned>(queue.depth()), text);
  return buf;
}

std::string HexRaw(uint64_t raw) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(raw));
  return buf;
}

std::vector<std::string> Fields(const char* args) {
  std::vector<std::string> out;
  if (args == nullptr) return out;
  std::string spec(args);
  size_t start = 0;
  while (start <= spec.size()) {
    size_t sep = spec.find('\x1f', start);
    if (sep == std::string::npos) {
      out.push_back(spec.substr(start));
      break;
    }
    out.push_back(spec.substr(start, sep - start));
    start = sep + 1;
  }
  return out;
}

bool Decimal(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

std::string jit_heap(const char* args) {
  std::vector<std::string> fields = Fields(args);
  if (fields.empty() || fields[0].empty()) return "unmatched 0";
  uint32_t ord = 0;
  if (Decimal(fields[0])) {
    ord = static_cast<uint32_t>(std::strtoul(fields[0].c_str(), nullptr, 10));
  } else {
    ord = wasmturbo::HeapOrdinal(fields[0]);
  }
  const uint32_t write_ord = wasmturbo::HeapOrdinal("file.Heap.Write");
  const uint32_t read_ord = wasmturbo::HeapOrdinal("file.Heap.Read");
  const uint32_t exec_ord = wasmturbo::HeapOrdinal("file.Heap.Exec");
  if (ord == write_ord) {
    if (fields.size() < 3 || fields[1].empty()) return "unmatched " + std::to_string(ord);
    std::ofstream out(fields[1], std::ios::binary | std::ios::trunc);
    if (!out) return "unmatched " + std::to_string(ord);
    out.write(fields[2].data(), static_cast<std::streamsize>(fields[2].size()));
    if (!out) return "unmatched " + std::to_string(ord);
    return "write " + std::to_string(ord) + " writeFS " + fields[1];
  }
  if (ord == read_ord) {
    if (fields.size() < 2 || fields[1].empty()) return "read miss";
    std::ifstream in(fields[1], std::ios::binary);
    if (!in) return "read miss";
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    if (bytes.empty()) return "read miss";
    wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
    queue.Install();
    wasmturbo::MssEntry entry;
    if (!queue.Schedule(ProcessRuntime(), bytes.data(), bytes.size(), &entry)) return "read miss";
    if (!queue.Bind("file.Heap.Read", entry.raw_ipt)) return "read miss";
    if (!queue.Bind("file.Heap.Exec", entry.raw_ipt)) return "read miss";
    return "read " + std::to_string(ord) + " readHeap " + HexRaw(entry.raw);
  }
  if (ord == exec_ord) {
    wasmturbo::MssEntry entry;
    wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
    if (!queue.ByOrdinal(exec_ord, &entry)) return "heapExec miss";
    std::vector<uint8_t> bytes(entry.len);
    size_t n = 0;
    if (!queue.Take(entry.gia_ipt, bytes.data(), bytes.size(), &n) || n != entry.len) {
      return "heapExec miss";
    }
    std::string text(reinterpret_cast<const char*>(bytes.data()), n);
    return "heapExec " + std::to_string(ord) + " heapExec " + HexRaw(entry.raw) + " " + text;
  }
  return "unmatched " + std::to_string(ord);
}

std::string jit_procfs(const char* args) {
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  if (args == nullptr || args[0] == 0) return queue.ProcFs();
  if (!Decimal(args)) return "read miss";
  uint32_t ord = static_cast<uint32_t>(std::strtoul(args, nullptr, 10));
  wasmturbo::MssEntry entry;
  if (!queue.ByOrdinal(ord, &entry)) return "read miss";
  std::vector<uint8_t> bytes(entry.len);
  size_t n = 0;
  if (!queue.Take(entry.gia_ipt, bytes.data(), bytes.size(), &n) || n != entry.len) {
    return "read miss";
  }
  return std::string(reinterpret_cast<const char*>(bytes.data()), n);
}

std::string jit_proctable(const char* args) {
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  queue.Install();
  uint32_t pid = KernelPid();
  uint32_t self = wasmturbo::HeapOrdinal(std::string("proc.") + std::to_string(pid));
  wasmturbo::MssEntry seated;
  if (!queue.ByOrdinal(self, &seated) || seated.kind != wasmturbo::MssKind::kProcess) {
    // The process address is a host pointer. It is not a cage offset, and
    // it does not fit in a 32-bit GIA, so MSS names it raw.
    auto* rec = new uint32_t(pid);
    uint64_t address = reinterpret_cast<uint64_t>(rec);
    if (wasmturbo::MssQueue::Classify(ProcessRuntime(), address) != wasmturbo::MssAddr::kRaw) {
      return "error: proc: gia";
    }
    if (!queue.AddProcess(ProcessRuntime(), pid, address, "gockrnl", &seated)) {
      return "error: proc: seat";
    }
    if (seated.addr != wasmturbo::MssAddr::kRaw) return "error: proc: gia";
  }
  if (args != nullptr && args[0] != 0) {
    if (!Decimal(args)) return "read miss";
    uint32_t ord = static_cast<uint32_t>(std::strtoul(args, nullptr, 10));
    wasmturbo::MssEntry entry;
    if (!queue.ByOrdinal(ord, &entry) || entry.kind != wasmturbo::MssKind::kProcess) {
      return "read miss";
    }
    std::vector<uint8_t> bytes(entry.len);
    size_t n = 0;
    if (!queue.Take(entry.gia_ipt, bytes.data(), bytes.size(), &n) || n != entry.len) {
      return "read miss";
    }
    return std::string(reinterpret_cast<const char*>(bytes.data()), n);
  }
  return queue.ProcTable();
}

std::string jit_service(const char* args) {
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  queue.Install();
  queue.EnsureCoreServices(ProcessRuntime(), KernelPid());
  if (args == nullptr || args[0] == 0) {
    std::string all = queue.ProcFs();
    std::string out;
    size_t start = 0;
    while (start < all.size()) {
      size_t end = all.find('\n', start);
      if (end == std::string::npos) end = all.size();
      std::string line = all.substr(start, end - start);
      if (line.find("core.") != std::string::npos) {
        out += line;
        out.push_back('\n');
      }
      if (end == all.size()) break;
      start = end + 1;
    }
    if (out.empty()) return "empty";
    return out;
  }
  std::string spec(args);
  bool as_gia = false;
  if (spec.rfind("gia ", 0) == 0) {
    as_gia = true;
    spec = spec.substr(4);
  }
  if (spec.empty()) return "ipt miss";
  uint32_t ord = Decimal(spec) ? static_cast<uint32_t>(std::strtoul(spec.c_str(), nullptr, 10))
                               : wasmturbo::HeapOrdinal(spec);
  wasmturbo::MssEntry entry;
  if (!queue.ByOrdinal(ord, &entry) || entry.kind != wasmturbo::MssKind::kService) {
    return "ipt miss";
  }
  char buf[256];
  buf[0] = 0;
  int rc = as_gia ? queue.RunIpt(entry.gia_ipt, buf, sizeof(buf))
                  : queue.RunService(ord, buf, sizeof(buf));
  if (rc != 0) return "ipt miss";
  return buf;
}

std::string jit_vfs(const char* args) {
  std::string spec = args != nullptr ? args : "";
  if (spec.rfind("write", 0) == 0) {
    size_t a = spec.find('\x1f');
    size_t b = a == std::string::npos ? std::string::npos : spec.find('\x1f', a + 1);
    if (a != std::string::npos) {
      std::string path = spec.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
      std::string body = b == std::string::npos ? std::string() : spec.substr(b + 1);
      std::string grew = jit_ctr_grow(path.c_str(), body.c_str());
      if (!grew.empty()) return grew;
    }
  }
  return wasmturbo::Vfs::Origin().Call(ProcessRuntime(), KernelPid(), spec);
}

}  // namespace wasmgocos
