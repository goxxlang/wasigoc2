#include "gocos/jitrt.hpp"
#include "gocos/host.hpp"
#include "gocos/files.hpp"
#include "gocos/klock.hpp"
#include "gocos/unil.hpp"
#include "gocos/wasi.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "src/hostcalls/host-calls.h"
#include "src/instance.h"
#include "src/runtime/intern.h"
#include "v8.h"
#include "src/runtime/exn.h"
#include "src/runtime/mss.h"
#include "src/runtime/trap.h"
#include "src/runtime/unil.h"
#include "src/runtime/vfs.h"

namespace wasmgocos {
namespace {

struct Ctr {
  uint32_t pid = 0;
  std::string name;
  wasmturbo::CageObject mem;
  wasmturbo::UnilStore store;
  std::unique_ptr<wasmturbo::Instance> mod;
  uint32_t gia_ipt = 0;
  uint32_t raw_ipt = 0;
  uint32_t payload_len = 0;
  std::map<std::string, wasmturbo::MssEntry> parts;
  v8::Isolate* iso = nullptr;
  JSContext* ctx = nullptr;
  // A wasigocvm guest (`run`): `mod` is its own core module, `mem` its
  // linear memory, and `outputs` what it wrote to stdout and stderr. It
  // has no UNIL store and no JS engine.
  bool guest = false;
  std::unique_ptr<WasiHost> host;
  std::map<std::string, std::string> outputs;

  ~Ctr() {
    if (iso != nullptr) iso->Dispose();
  }
};

std::vector<std::unique_ptr<Ctr>>& Table() {
  static std::vector<std::unique_ptr<Ctr>> table;
  return table;
}

uint32_t& NextPid() {
  static uint32_t next = 0xC7000001u;
  return next;
}

// The frame go++ leaves in every module for gocvm.Call: topic, payload,
// reply. Offsets are into the container's cage object.
constexpr uint32_t kTopic = 64;
constexpr uint32_t kPayload = 128;
constexpr uint32_t kReply = 128;
constexpr uint32_t kFrame = kTopic + kPayload + kReply;

std::vector<std::string> Fields(const std::string& args) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= args.size()) {
    size_t sep = args.find('\x1f', start);
    if (sep == std::string::npos) {
      out.push_back(args.substr(start));
      break;
    }
    out.push_back(args.substr(start, sep - start));
    start = sep + 1;
  }
  return out;
}

Ctr* Find(uint32_t pid) {
  for (const std::unique_ptr<Ctr>& ctr : Table()) {
    if (ctr->pid == pid) return ctr.get();
  }
  return nullptr;
}

std::string Line(const Ctr& ctr) {
  return "ctr pid=" + std::to_string(ctr.pid) + " name=" + ctr.name +
         " gia=" + std::to_string(ctr.mem.gia) + " chpt=" + std::to_string(ctr.mem.chpt) +
         " tpt=" + std::to_string(ctr.mem.tpt) + " ept=" + std::to_string(ctr.mem.ept) +
         " cage=" + std::to_string(ProcessRuntime().cage().base()) +
         " ipt=" + std::to_string(ctr.raw_ipt) + " gia_ipt=" + std::to_string(ctr.gia_ipt) +
         (ctr.guest ? " rt=wazgoc guest=wasigocvm"
                    : " call=gocvm.Call engine=quickjs js=quickjs-ng rt=wazgoc");
}

std::vector<uint8_t> WasiGocvm() {
  return {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0b, 0x01, 0x60, 0x06, 0x7f,
          0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x02, 0x0e, 0x01, 0x05, 'g',  'o',  'c',
          'o',  's',  0x04, 'C',  'a',  'l',  'l',  0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x05,
          0x03, 0x01, 0x00, 0x01, 0x07, 0x08, 0x01, 0x04, 'c',  'a',  'l',  'l',  0x00, 0x01,
          0x0a, 0x12, 0x01, 0x10, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x20, 0x03, 0x20,
          0x04, 0x20, 0x05, 0x10, 0x00, 0x0b};
}

void RunPacked(Ctr& ctr) {
  const wasmturbo::UnilRegion* result = ctr.store.Find("result");
  std::string last;
  v8::Isolate::Scope isolate_scope(ctr.iso);
  v8::HandleScope handle_scope(ctr.iso);
  v8::Local<v8::Context> context(ctr.ctx);
  v8::Context::Scope context_scope(context);
  for (const wasmturbo::UnilRegion& region : ctr.store.regions()) {
    if (region.name.size() < 3 || region.name.compare(region.name.size() - 3, 3, ".js") != 0)
      continue;
    std::string source(reinterpret_cast<const char*>(ctr.mem.base + region.offset), region.size);
    while (!source.empty() && source.back() == '\0') source.pop_back();
    if (source.empty()) continue;
    v8::MaybeLocal<v8::String> text = v8::String::NewFromUtf8(ctr.iso, source.c_str());
    if (text.IsEmpty()) continue;
    v8::MaybeLocal<v8::Script> script = v8::Script::Compile(context, text.ToLocalChecked());
    if (script.IsEmpty()) continue;
    v8::MaybeLocal<v8::Value> value = script.ToLocalChecked()->Run(context);
    if (value.IsEmpty()) continue;
    v8::String::Utf8Value utf8(ctr.iso, value.ToLocalChecked());
    last.assign(*utf8, static_cast<size_t>(utf8.length()));
  }
  if (result == nullptr || last.empty() || result->size == 0) return;
  auto* slot = reinterpret_cast<char*>(ctr.mem.base + result->offset);
  std::memset(slot, 0, result->size);
  size_t n = last.size();
  if (n >= result->size) n = result->size - 1;
  std::memcpy(slot, last.data(), n);
}

bool NameOk(const std::string& name) {
  return !name.empty() && name.find(' ') == std::string::npos &&
         name.find('/') == std::string::npos && name.find('') == std::string::npos;
}

// Instantiate the container's store on the shared runtime, plant the
// bundle and its regions on the origin filesystem, and seat it on the
// process table as a GIA. `packed` is the store's own encoding.
std::string Seat(std::unique_ptr<Ctr> ctr, const std::vector<uint8_t>& packed) {
  const std::string name = ctr->name;
  if (ctr->store.Find("gocvm.Call") == nullptr) return "error: ctr: unil";
  const std::vector<uint8_t>& payload = ctr->store.payload();
  if (payload.empty()) return "ctr miss";
  wasmturbo::UnilStore image = ctr->store;
  ctr->mod = std::make_unique<wasmturbo::Instance>(ProcessRuntime(), WasiGocvm(), std::move(image));
  ctr->mem = ctr->mod->memory_object();
  if (ctr->mem.base == 0 || ctr->mem.chpt == 0) return "error: ctr: cage";
  wasmturbo::MssEntry kept;
  if (!wasmturbo::MssQueue::Process().Retain(ProcessRuntime(), ctr->mem.base,
                                             static_cast<uint32_t>(payload.size()), &kept))
    return "error: ctr: ipt";
  ctr->gia_ipt = kept.gia_ipt;
  ctr->raw_ipt = kept.raw_ipt;
  ctr->payload_len = static_cast<uint32_t>(payload.size());
  ctr->pid = NextPid()++;
  wasmturbo::MssEntry bundle;
  if (!wasmturbo::MssQueue::Process().Schedule(ProcessRuntime(), packed.data(), packed.size(), &bundle))
    return "error: ctr: unil";
  wasmturbo::Vfs::Origin().Plant(ProcessRuntime(), KernelPid(), "/ctr/" + name + ".unil",
                                 bundle.raw_ipt, bundle.gia_ipt, bundle.len, bundle.raw);
  wasmturbo::Vfs::Origin().Plant(ProcessRuntime(), KernelPid(), "/ctr/" + name,
                                 ctr->raw_ipt, ctr->gia_ipt, static_cast<uint32_t>(payload.size()),
                                 ctr->mem.base);
  for (const wasmturbo::UnilRegion& region : ctr->store.regions()) {
    if (region.size == 0 || region.name.empty() || region.name.find('/') != std::string::npos)
      continue;
    wasmturbo::MssEntry part;
    uint64_t addr = ctr->mem.base + region.offset;
    if (!wasmturbo::MssQueue::Process().Retain(ProcessRuntime(), addr, region.size, &part))
      return "error: ctr: ipt " + region.name;
    ctr->parts[region.name] = part;
    wasmturbo::Vfs::Origin().Plant(ProcessRuntime(), KernelPid(),
                                   "/ctr/" + name + "/" + region.name, part.raw_ipt, part.gia_ipt,
                                   region.size, addr);
  }
  wasmturbo::MssEntry seated;
  if (!wasmturbo::MssQueue::Process().AddProcess(ProcessRuntime(), ctr->pid, ctr->mem.base, name,
                                                 &seated))
    return "error: ctr: seat";
  if (seated.addr != wasmturbo::MssAddr::kGia || seated.gia != ctr->mem.gia || ctr->mem.chpt == 0 ||
      ctr->mem.tpt == 0 || ctr->mem.ept == 0)
    return "error: ctr: gia";
  ctr->iso = v8::Isolate::New();
  if (ctr->iso == nullptr) return "error: ctr: js";
  {
    v8::Isolate::Scope isolate_scope(ctr->iso);
    v8::HandleScope handle_scope(ctr->iso);
    v8::Local<v8::Context> context = v8::Context::New(ctr->iso);
    ctr->ctx = context.context_for_wasmv8_internal();
  }
  if (ctr->ctx == nullptr) return "error: ctr: js";
  RunPacked(*ctr);
  std::string line = Line(*ctr);
  Table().push_back(std::move(ctr));
  return line;
}

std::string Load(const std::string& name, const std::string& body) {
  if (!NameOk(name)) return "ctr miss";
  std::string service = body.empty() ? "hello from the container" : body;
  auto ctr = std::make_unique<Ctr>();
  ctr->name = name;
  ctr->store.Add("service", std::vector<uint8_t>(service.begin(), service.end()));
  ctr->store.Add("name", std::vector<uint8_t>(name.begin(), name.end()));
  ctr->store.Add("gocvm.Call", std::vector<uint8_t>(kFrame, 0));
  const char* script = "1 + 2";
  ctr->store.Add("app.js", std::vector<uint8_t>(script, script + 5));
  ctr->store.Add("result", std::vector<uint8_t>(32, 0));
  std::vector<uint8_t> packed = ctr->store.Encode();
  if (packed.size() < 4 || packed[0] != 'U' || packed[1] != 'N') return "error: ctr: unil";
  ctr->store = wasmturbo::UnilStore::Parse(packed.data(), packed.size());
  if (ctr->store.Find("service") == nullptr) return "error: ctr: unil";
  std::string line = Seat(std::move(ctr), packed);
  if (line.rfind("ctr pid=", 0) == 0) line += " service=" + service;
  return line;
}

// A `.unil` file packed outside this process (~/WASMUniLoader's gzip form or
// the plain one). Its regions are the container's; the kernel adds the
// gocvm.Call frame, `result`, and `name` when the bundle does not carry them.
std::string Open(const std::string& path, std::string name) {
  if (path.empty()) return "ctr miss";
  if (name.empty()) {
    size_t cut = path.find_last_of("/\\");
    name = cut == std::string::npos ? path : path.substr(cut + 1);
    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".unil") == 0) name.resize(name.size() - 5);
  }
  if (!NameOk(name)) return "ctr miss";
  std::ifstream in(path, std::ios::binary);
  if (!in) return "error: ctr: open " + path;
  std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<uint8_t> plain;
  std::string why;
  if (!unil_plain(file.data(), file.size(), &plain, &why)) return "error: ctr: unil " + why;
  auto ctr = std::make_unique<Ctr>();
  ctr->name = name;
  ctr->store = wasmturbo::UnilStore::Parse(plain.data(), plain.size());
  size_t carried = ctr->store.regions().size();
  if (ctr->store.Find("gocvm.Call") == nullptr)
    ctr->store.Add("gocvm.Call", std::vector<uint8_t>(kFrame, 0));
  if (ctr->store.Find("result") == nullptr) ctr->store.Add("result", std::vector<uint8_t>(64, 0));
  if (ctr->store.Find("name") == nullptr)
    ctr->store.Add("name", std::vector<uint8_t>(name.begin(), name.end()));
  std::vector<uint8_t> packed = ctr->store.Encode();
  ctr->store = wasmturbo::UnilStore::Parse(packed.data(), packed.size());
  std::string line = Seat(std::move(ctr), packed);
  if (line.rfind("ctr pid=", 0) == 0) {
    line += " regions=" + std::to_string(carried) + " file=" + std::to_string(file.size()) +
            " crc=" + std::to_string(unil_crc32(file.data(), file.size()));
  }
  return line;
}

std::string Read(uint32_t pid, const std::string& region) {
  Ctr* ctr = Find(pid);
  if (ctr == nullptr || region.empty()) return "ctr miss";
  if (ctr->guest) {
    auto it = ctr->outputs.find(region);
    return it == ctr->outputs.end() ? "ctr miss" : it->second;
  }
  const wasmturbo::UnilRegion* found = ctr->store.Find(region);
  if (found == nullptr) return "ctr miss";
  if (static_cast<uint64_t>(found->offset) + found->size > ctr->mem.size_bytes) return "ctr miss";
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  uint64_t raw = 0;
  if (!queue.MatchRaw(ctr->raw_ipt, &raw) || raw == 0) return "ipt miss";
  std::vector<uint8_t> bytes(ctr->mem.size_bytes);
  size_t n = 0;
  if (!queue.Take(ctr->gia_ipt, bytes.data(), bytes.size(), &n) ||
      n < static_cast<size_t>(found->offset) + found->size)
    return "ipt miss";
  return std::string(reinterpret_cast<const char*>(bytes.data() + found->offset), found->size);
}

// gocvm.Call(topic, payload). A go++ gocos call is topic "gocos" and a
// payload of api, then 0x1F, then the argument. The bytes are written
// into the container's IPT object and dispatched through gocos.Call.
std::string Call(uint32_t pid, const std::string& topic, const std::string& payload) {
  Ctr* ctr = Find(pid);
  if (ctr == nullptr || ctr->mod == nullptr || topic.empty()) return "ctr miss";
  const wasmturbo::UnilRegion* frame = ctr->store.Find("gocvm.Call");
  if (frame == nullptr || frame->size < kFrame) return "ctr miss";
  if (static_cast<uint64_t>(frame->offset) + kFrame > ctr->mem.size_bytes) return "ctr miss";
  std::string api = topic;
  std::string arg = payload;
  if (topic == "gocos" || topic == "gockrnl" || topic == "gocsys") {
    size_t cut = payload.find('\x1f');
    if (cut == std::string::npos) {
      api = payload;
      arg.clear();
    } else {
      api = payload.substr(0, cut);
      arg = payload.substr(cut + 1);
    }
    if (api.empty() || api.size() >= kTopic || arg.size() >= kPayload) return "ctr miss";
  } else if (topic.size() >= kTopic || payload.size() >= kPayload) {
    return "ctr miss";
  }
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  uint64_t raw = 0;
  if (!queue.MatchRaw(ctr->raw_ipt, &raw) || raw != ctr->mem.base) return "ipt miss";
  auto* mem = reinterpret_cast<uint8_t*>(raw);
  uint32_t at = frame->offset;
  std::memset(mem + at, 0, kFrame);
  std::memcpy(mem + at, api.data(), api.size());
  std::memcpy(mem + at + kTopic, arg.data(), arg.size());
  int32_t rc = ctr->mod->CallI32(
      "call", {static_cast<int32_t>(at), static_cast<int32_t>(api.size()),
               static_cast<int32_t>(at + kTopic), static_cast<int32_t>(arg.size()),
               static_cast<int32_t>(at + kTopic + kPayload), static_cast<int32_t>(kReply)});
  std::vector<uint8_t> bytes(ctr->mem.size_bytes);
  size_t n = 0;
  if (!queue.Take(ctr->gia_ipt, bytes.data(), bytes.size(), &n) ||
      n < static_cast<size_t>(at) + kFrame)
    return "ipt miss";
  const char* reply = reinterpret_cast<const char*>(bytes.data() + at + kTopic + kPayload);
  size_t reply_n = 0;
  while (reply_n < kReply && reply[reply_n] != 0) ++reply_n;
  if (rc != 0 && reply_n == 0) return "ctr miss";
  return std::string(reply, reply_n);
}

std::string Eval(uint32_t pid, const std::string& source) {
  Ctr* ctr = Find(pid);
  if (ctr == nullptr || ctr->iso == nullptr || ctr->ctx == nullptr || source.empty())
    return "ctr miss";
  v8::Isolate::Scope isolate_scope(ctr->iso);
  v8::HandleScope handle_scope(ctr->iso);
  v8::Local<v8::Context> context(ctr->ctx);
  v8::Context::Scope context_scope(context);
  v8::MaybeLocal<v8::String> text = v8::String::NewFromUtf8(ctr->iso, source.c_str());
  if (text.IsEmpty()) return "ctr miss";
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(context, text.ToLocalChecked());
  if (script.IsEmpty()) return "ctr miss";
  v8::MaybeLocal<v8::Value> result = script.ToLocalChecked()->Run(context);
  if (result.IsEmpty()) return "ctr miss";
  v8::String::Utf8Value utf8(ctr->iso, result.ToLocalChecked());
  std::string reply(*utf8, static_cast<size_t>(utf8.length()));
  const wasmturbo::UnilRegion* frame = ctr->store.Find("gocvm.Call");
  if (frame != nullptr && frame->size >= kFrame &&
      static_cast<uint64_t>(frame->offset) + kFrame <= ctr->mem.size_bytes) {
    wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
    uint64_t raw = 0;
    if (queue.MatchRaw(ctr->raw_ipt, &raw) && raw == ctr->mem.base) {
      auto* mem = reinterpret_cast<uint8_t*>(raw);
      uint32_t at = frame->offset + kTopic + kPayload;
      std::memset(mem + at, 0, kReply);
      size_t n = reply.size();
      if (n >= kReply) n = kReply - 1;
      if (n > 0) std::memcpy(mem + at, reply.data(), n);
    }
  }
  return reply;
}

Ctr* FindName(const std::string& name) {
  for (const std::unique_ptr<Ctr>& ctr : Table()) {
    if (ctr->name == name) return ctr.get();
  }
  return nullptr;
}

// The live object back out as ~/WASMUniLoader's gzip form, which `open`
// reads on a later boot. The bytes come through the IPT the way `read`
// sees them, so a region that grew is saved at its new size. The file is
// written beside `path` and renamed over it: a torn save leaves the last
// image.
std::string Save(const std::string& who, const std::string& path) {
  Ctr* ctr = FindName(who);
  if (ctr == nullptr) ctr = Find(static_cast<uint32_t>(std::strtoul(who.c_str(), nullptr, 10)));
  if (ctr == nullptr || ctr->guest || path.empty()) return "ctr miss";
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  uint64_t raw = 0;
  if (!queue.MatchRaw(ctr->raw_ipt, &raw) || raw == 0) return "ipt miss";
  std::vector<uint8_t> bytes(ctr->mem.size_bytes);
  size_t n = 0;
  if (!queue.Take(ctr->gia_ipt, bytes.data(), bytes.size(), &n) || n < ctr->payload_len)
    return "ipt miss";
  std::vector<UnilPart> parts;
  for (const wasmturbo::UnilRegion& region : ctr->store.regions()) {
    if (static_cast<uint64_t>(region.offset) + region.size > ctr->payload_len) return "ctr miss";
    parts.push_back({region.name, region.mime, region.kind, region.flags,
                     bytes.data() + region.offset, region.size});
  }
  std::vector<uint8_t> file = unil_pack(parts);
  if (file.empty()) return "error: ctr: unil";
  std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return "error: ctr: save " + path;
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    out.close();
    if (!out) {
      std::remove(tmp.c_str());
      return "error: ctr: save " + path;
    }
  }
#if defined(_WIN32)
  bool moved = MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  bool moved = std::rename(tmp.c_str(), path.c_str()) == 0;
#endif
  if (!moved) {
    std::remove(tmp.c_str());
    return "error: ctr: save " + path;
  }
  return "save " + path + " name=" + ctr->name + " regions=" + std::to_string(parts.size()) +
         " payload=" + std::to_string(ctr->payload_len) + " file=" + std::to_string(file.size()) +
         " crc=" + std::to_string(unil_crc32(file.data(), file.size()));
}

// The session environment as NAME=value pairs.
std::vector<std::pair<std::string, std::string>> SessionEnv() {
  std::vector<std::pair<std::string, std::string>> env;
  std::string lines = jit_env("");
  size_t start = 0;
  while (start < lines.size()) {
    size_t nl = lines.find('\n', start);
    std::string line = lines.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    size_t eq = line.find('=');
    if (eq != std::string::npos && eq > 0) env.emplace_back(line.substr(0, eq), line.substr(eq + 1));
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  return env;
}

// A wasigocvm guest as the container's service. The core module is
// instantiated on the shared runtime with the kernel's WASI host bound to
// every import (include/gocos/wasi.hpp), and its wasi:cli/run runs to
// completion, off the vCPU. Its linear memory is the container's cage
// object, seated on the process table as a GIA; stdout and stderr are
// files under /ctr/<name>, and stdout is written on the session console.
// A guest's bytes. A path starting with `/` or `#` is the kernel's own
// (walked as a guest walks it, include/gocos/files.hpp): a guest stored in
// the OS, `/bin/<name>.wasm`. Anything else is a host path.
bool ReadImage(const std::string& path, std::vector<uint8_t>* out) {
  if (!path.empty() && (path[0] == '/' || path[0] == '#')) {
    std::string bytes;
    if (!files::Read(files::Walk(path), &bytes)) return false;
    out->assign(bytes.begin(), bytes.end());
    return true;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out->assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return true;
}

// `install <host-path> <os-path>`: a host file into the OS, so `run` can
// name it there.
std::string Install(const std::string& from, const std::string& to) {
  if (from.empty() || to.empty() || (to[0] != '/' && to[0] != '#')) return "ctr miss";
  std::ifstream in(from, std::ios::binary);
  if (!in) return "error: ctr: open " + from;
  std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::string walked = files::Walk(to);
  if (!files::Write(walked, bytes)) return "error: ctr: install " + to;
  return "install " + walked + " " + std::to_string(bytes.size());
}

// A guest ready to run: its container (instance and host built), and
// the export to run.
struct Launch {
  std::unique_ptr<Ctr> ctr;
  std::string run;
  bool component = false;
};

struct GuestJob;
GuestJob* FindJob(const std::string& name);

// Reads and checks the image, instantiates it with the kernel's WASI host.
// Empty on success, else the reply.
std::string Prepare(const std::string& name, const std::string& path, const std::vector<std::string>& argv,
                    Launch* out) {
  if (!NameOk(name) || path.empty()) return "ctr miss";
  if (FindName(name) != nullptr || FindJob(name) != nullptr) return "error: ctr: " + name + " exists";
  std::vector<uint8_t> file;
  if (!ReadImage(path, &file)) return "error: ctr: open " + path;
  WasiImage image;
  std::string why;
  if (!wasi_image(file.data(), file.size(), &image, &why)) return "error: ctr: guest " + why;
  auto ctr = std::make_unique<Ctr>();
  ctr->name = name;
  ctr->guest = true;
  std::vector<std::string> args = {name};
  args.insert(args.end(), argv.begin(), argv.end());
  ctr->host = std::make_unique<WasiHost>(std::move(args), SessionEnv());
  WasiHost* host = ctr->host.get();
  ctr->mod = std::make_unique<wasmturbo::Instance>(
      ProcessRuntime(), std::move(image.core), wasmturbo::UnilStore{},
      [host](const std::string& module, const std::string& fn, const wasmturbo::FunctionSig& sig) {
        return host->Bind(module, fn, sig);
      });
  out->ctr = std::move(ctr);
  out->run = image.run;
  out->component = image.component;
  return "";
}

// Runs the guest to its end on this thread, which holds the kernel lock,
// then seats the container and replies.
std::string Execute(Launch launch) {
  std::unique_ptr<Ctr> ctr = std::move(launch.ctr);
  const std::string name = ctr->name;
  WasiHost* host = ctr->host.get();
  wasmturbo::ExnClear();
  // A trap in the guest (src/runtime/trap.h) ends the guest, not the
  // kernel: it arrives here as a termination with its spec message.
  int32_t rc = ctr->mod->CallI32(launch.run);
  host->FlushAll();
  bool killed = kernel_killed();
  bool uncaught = wasmturbo::ExnPending();
  const char* trap_cause = wasmturbo::TrapCause();
  std::string trap = trap_cause != nullptr ? trap_cause : "";
  wasmturbo::ExnClear();
  int code = killed           ? 137
             : host->exited() ? host->exit_code()
             : (uncaught || !trap.empty() || !host->fault().empty() || rc != 0) ? 1
                                                                                : 0;
  ctr->outputs["stdout"] = host->out();
  ctr->outputs["stderr"] = host->err();

  ctr->mem = ctr->mod->memory_object();
  if (ctr->mem.base == 0 || ctr->mem.chpt == 0) return "error: ctr: cage";
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  wasmturbo::MssEntry kept;
  if (!queue.Retain(ProcessRuntime(), ctr->mem.base, ctr->mem.size_bytes, &kept)) return "error: ctr: ipt";
  ctr->gia_ipt = kept.gia_ipt;
  ctr->raw_ipt = kept.raw_ipt;
  ctr->payload_len = ctr->mem.size_bytes;
  ctr->pid = NextPid()++;
  wasmturbo::Vfs::Origin().Plant(ProcessRuntime(), KernelPid(), "/ctr/" + name, ctr->raw_ipt,
                                 ctr->gia_ipt, ctr->mem.size_bytes, ctr->mem.base);
  for (const auto& [region, bytes] : ctr->outputs) {
    if (bytes.empty()) continue;
    wasmturbo::MssEntry part;
    if (!queue.Schedule(ProcessRuntime(), bytes.data(), bytes.size(), &part))
      return "error: ctr: ipt " + region;
    ctr->parts[region] = part;
    wasmturbo::Vfs::Origin().Plant(ProcessRuntime(), KernelPid(), "/ctr/" + name + "/" + region,
                                   part.raw_ipt, part.gia_ipt, part.len, part.raw);
  }
  wasmturbo::MssEntry seated;
  if (!queue.AddProcess(ProcessRuntime(), ctr->pid, ctr->mem.base, name, &seated)) return "error: ctr: seat";
  if (seated.addr != wasmturbo::MssAddr::kGia || seated.gia != ctr->mem.gia) return "error: ctr: gia";
  jit_cons(host->out());
  std::string line = Line(*ctr) + " exit=" + std::to_string(code) +
                     " stdout=" + std::to_string(host->out().size()) +
                     " stderr=" + std::to_string(host->err().size()) +
                     " calls=" + std::to_string(host->calls());
  if (launch.component) line += " component=1";
  if (killed) line += " fault=killed";
  else if (!host->fault().empty()) line += " fault=" + host->fault();
  if (!trap.empty()) line += " fault=trap: " + trap;
  if (uncaught) line += " fault=uncaught exception";
  Table().push_back(std::move(ctr));
  return line;
}

// A guest as a command (include/gocos/jitrt.hpp, jit_ctr_exec): the same
// host and runtime as `run`, with stdin, and no container. The instance is
// this frame's, so its memory, globals, and table go back to the cage
// when it returns (wasmturbo::Instance::~Instance). Its stderr, and why
// it ended if it did not end well, go to the session console.
std::string Exec(const std::string& path, const std::vector<std::string>& argv, const GuestIo& io,
                 std::string* out, int* code) {
  std::vector<uint8_t> file;
  if (!ReadImage(path, &file)) return "error: ctr: open " + path;
  WasiImage image;
  std::string why;
  if (!wasi_image(file.data(), file.size(), &image, &why)) return "error: ctr: guest " + why;
  WasiHost host(argv, SessionEnv());
  host.set_stdin(io.in);
  if (io.in_stream) host.set_stdin_stream(io.in_stream);
  if (io.out_stream) host.set_stdout_stream(io.out_stream);
  WasiHost* bound = &host;
  wasmturbo::Instance inst(ProcessRuntime(), std::move(image.core), wasmturbo::UnilStore{},
                           [bound](const std::string& module, const std::string& fn,
                                   const wasmturbo::FunctionSig& sig) { return bound->Bind(module, fn, sig); });
  wasmturbo::ExnClear();
  int32_t rc = inst.CallI32(image.run);
  host.FlushAll();
  bool killed = kernel_killed();
  bool uncaught = wasmturbo::ExnPending();
  const char* trap_cause = wasmturbo::TrapCause();
  std::string trap = trap_cause != nullptr ? trap_cause : "";
  wasmturbo::ExnClear();
  *code = killed           ? 137
          : host.exited()  ? host.exit_code()
          : (uncaught || !trap.empty() || !host.fault().empty() || rc != 0) ? 1
                                                                             : 0;
  *out = host.out();
  std::string err = host.err();
  const std::string& name = argv.empty() ? path : argv[0];
  if (killed) err += name + ": killed\n";
  else if (!host.fault().empty()) err += name + ": " + host.fault() + "\n";
  if (!trap.empty()) err += name + ": trap: " + trap + "\n";
  if (uncaught) err += name + ": uncaught exception\n";
  if (!err.empty()) jit_cons(err);
  return "";
}

std::string Run(const std::string& name, const std::string& path, const std::vector<std::string>& argv) {
  Launch launch;
  std::string refused = Prepare(name, path, argv, &launch);
  if (!refused.empty()) return refused;
  return Execute(std::move(launch));
}

// A guest running on its own thread (`ctr run ... &`). The thread takes
// the kernel lock, so the guest and the session take turns
// (include/gocos/klock.hpp); `line` is the run's reply once `done`.
struct GuestJob {
  std::string name;
  std::thread thread;
  std::atomic<bool> kill{false};
  std::atomic<bool> done{false};
  std::string line;
  int job = 0;
  std::string output;  // a background command's
};

// Never destroyed: a job still running at exit is killed and joined first
// (StopAll), and a destructor must not meet a joinable thread.
std::vector<std::unique_ptr<GuestJob>>& Jobs() {
  static auto* jobs = new std::vector<std::unique_ptr<GuestJob>>();
  return *jobs;
}

GuestJob* FindJob(const std::string& name) {
  for (const auto& job : Jobs()) {
    if (job->name == name) return job.get();
  }
  return nullptr;
}

void StopAll() {
  for (const auto& job : Jobs()) {
    job->kill = true;
    kernel_preempt();
    if (job->thread.joinable()) job->thread.join();
  }
}

std::string Start(const std::string& name, const std::string& path, const std::vector<std::string>& argv) {
  Launch launch;
  std::string refused = Prepare(name, path, argv, &launch);
  if (!refused.empty()) return refused;
  static const bool at_exit = [] { return std::atexit(StopAll) == 0; }();
  (void)at_exit;
  auto job = std::make_unique<GuestJob>();
  GuestJob* running = job.get();
  running->name = name;
  running->job = jit_job_guest(name);
  running->thread = std::thread([running, launch = std::move(launch)]() mutable {
    // Before the lock: a kill that comes while this thread still waits for
    // its first turn is seen when it gets it.
    kernel_set_kill(&running->kill);
    KernelLock lock;
    running->line = Execute(std::move(launch));
    kernel_set_kill(nullptr);
    running->done = true;
  });
  Jobs().push_back(std::move(job));
  return "job " + std::to_string(running->job) + " ctr name=" + name + " running";
}

// A command line as a job (include/gocos/jitrt.hpp): the same thread,
// lock, and kill flag as a guest's. Its reply is `sh <name> exit=<n>`,
// and a kill makes the status 137 whatever the last stage said.
std::string StartCommand(const std::string& line, std::function<std::string(int*)> run) {
  static int next = 1;
  std::string name = "sh" + std::to_string(next++);
  static const bool at_exit = [] { return std::atexit(StopAll) == 0; }();
  (void)at_exit;
  auto job = std::make_unique<GuestJob>();
  GuestJob* running = job.get();
  running->name = name;
  running->job = jit_job_guest(name, line);
  running->thread = std::thread([running, run = std::move(run)] {
    kernel_set_kill(&running->kill);
    KernelLock lock;
    int code = 0;
    running->output = run(&code);
    if (running->kill) code = 137;
    running->line = "sh " + running->name + " exit=" + std::to_string(code);
    jit_cons(running->output);
    kernel_set_kill(nullptr);
    running->done = true;
  });
  Jobs().push_back(std::move(job));
  return "job " + std::to_string(running->job) + " " + name + " running";
}

// The run's reply, once the guest has ended; the lock is given up while
// this waits.
std::string Wait(const std::string& name) {
  GuestJob* job = FindJob(name);
  if (job == nullptr) return "ctr miss";
  kernel_unlocked([job] {
    if (job->thread.joinable()) job->thread.join();
  });
  return job->line;
}

std::string Kill(const std::string& name) {
  GuestJob* job = FindJob(name);
  if (job == nullptr || job->done) return "ctr miss";
  job->kill = true;
  kernel_preempt();
  return "kill " + name;
}

std::string State(const std::string& name) {
  GuestJob* job = FindJob(name);
  if (job == nullptr) return "ctr miss";
  if (!job->done) return "running";
  size_t at = job->line.find(" exit=");
  return "exited " + (at == std::string::npos ? std::string("1") : std::to_string(std::atoi(job->line.c_str() + at + 6)));
}

std::string Grow(const std::string& path, const std::string& body) {
  if (body.empty() || path.rfind("/ctr/", 0) != 0) return "";
  std::string rest = path.substr(5);
  size_t slash = rest.find('/');
  if (slash == std::string::npos || slash == 0) return "";
  std::string name = rest.substr(0, slash);
  std::string region = rest.substr(slash + 1);
  if (region.empty() || region.find('/') != std::string::npos) return "";
  Ctr* ctr = FindName(name);
  if (ctr == nullptr || ctr->mod == nullptr) return "";
  const wasmturbo::UnilRegion* found = ctr->store.Find(region);
  if (found == nullptr || ctr->parts.find(region) == ctr->parts.end()) return "";
  uint32_t old = found->size;
  uint32_t at = found->offset;
  if (body.size() == old) return "";
  uint32_t neu = static_cast<uint32_t>(body.size());
  int32_t delta = static_cast<int32_t>(neu) - static_cast<int32_t>(old);
  uint32_t tail = ctr->payload_len - (at + old);
  uint32_t need = ctr->payload_len + static_cast<uint32_t>(delta > 0 ? delta : 0);
  wasmturbo::CageObject& live = ctr->mod->memory_object();
  if (need > live.size_bytes) {
    if (!ProcessRuntime().GrowCageObject(&live, need, live.type_id)) return "error: ctr: grow";
  }
  ctr->mem = live;
  auto* mem = reinterpret_cast<uint8_t*>(ctr->mem.base);
  if (tail > 0) std::memmove(mem + at + neu, mem + at + old, tail);
  std::memcpy(mem + at, body.data(), neu);
  if (!ctr->store.Resize(region, neu)) return "error: ctr: grow";
  ctr->payload_len = static_cast<uint32_t>(static_cast<int32_t>(ctr->payload_len) + delta);
  wasmturbo::MssQueue& queue = wasmturbo::MssQueue::Process();
  if (!queue.Retarget(ProcessRuntime(), ctr->raw_ipt, ctr->mem.base, ctr->payload_len))
    return "error: ctr: grow";
  wasmturbo::Vfs::Origin().Plant(ProcessRuntime(), KernelPid(), "/ctr/" + name,
                                 ctr->raw_ipt, ctr->gia_ipt, ctr->payload_len, ctr->mem.base);
  for (const wasmturbo::UnilRegion& part : ctr->store.regions()) {
    auto it = ctr->parts.find(part.name);
    if (it == ctr->parts.end() || part.size == 0) continue;
    uint64_t addr = ctr->mem.base + part.offset;
    if (!queue.Retarget(ProcessRuntime(), it->second.raw_ipt, addr, part.size))
      return "error: ctr: grow";
    it->second.raw = addr;
    it->second.len = part.size;
    wasmturbo::Vfs::Origin().Plant(ProcessRuntime(), KernelPid(),
                                   "/ctr/" + name + "/" + part.name, it->second.raw_ipt,
                                   it->second.gia_ipt, part.size, addr);
  }
  return "write " + path;
}

}  // namespace

std::string jit_ctr_exec(const std::string& path, const std::vector<std::string>& argv, const GuestIo& io,
                         std::string* out, int* code) {
  return Exec(path, argv, io, out, code);
}

std::string jit_ctr_start_command(const std::string& line, std::function<std::string(int* code)> run) {
  return StartCommand(line, std::move(run));
}

std::string jit_ctr_job_output(const std::string& name) {
  GuestJob* job = FindJob(name);
  return job != nullptr && job->done ? job->output : std::string();
}

std::string jit_ctr_grow(const char* path, const char* body) {
  return Grow(path != nullptr ? path : "", body != nullptr ? body : "");
}

std::string jit_ctr_line(uint32_t pid) {
  Ctr* ctr = Find(pid);
  if (ctr == nullptr) return "";
  return Line(*ctr);
}

std::string jit_ctr(const char* args) {
  std::vector<std::string> fields = Fields(args != nullptr ? args : "");
  if (fields.empty() || fields[0].empty() || fields[0] == "list") {
    if (Table().empty()) return "ctr=0";
    std::string out;
    for (const std::unique_ptr<Ctr>& ctr : Table()) {
      out += Line(*ctr);
      out.push_back('\n');
    }
    return out;
  }
  if (fields[0] == "load") {
    std::string name = fields.size() > 1 ? fields[1] : "";
    std::string body = fields.size() > 2 ? fields[2] : "";
    return Load(name, body);
  }
  if (fields[0] == "open") {
    if (fields.size() < 2) return "ctr miss";
    return Open(fields[1], fields.size() > 2 ? fields[2] : "");
  }
  if (fields[0] == "run" || fields[0] == "start") {
    if (fields.size() < 3) return "ctr miss";
    std::vector<std::string> argv(fields.begin() + 3, fields.end());
    if (fields[0] == "start") return Start(fields[1], fields[2], argv);
    return Run(fields[1], fields[2], argv);
  }
  if (fields[0] == "install") {
    if (fields.size() < 3) return "ctr miss";
    return Install(fields[1], fields[2]);
  }
  if (fields[0] == "wait" || fields[0] == "kill" || fields[0] == "state") {
    if (fields.size() < 2) return "ctr miss";
    if (fields[0] == "wait") return Wait(fields[1]);
    if (fields[0] == "kill") return Kill(fields[1]);
    return State(fields[1]);
  }
  if (fields[0] == "save") {
    if (fields.size() < 3) return "ctr miss";
    return Save(fields[1], fields[2]);
  }
  if (fields[0] == "read") {
    if (fields.size() < 3) return "ctr miss";
    uint32_t pid = static_cast<uint32_t>(std::strtoul(fields[1].c_str(), nullptr, 10));
    return Read(pid, fields[2]);
  }
  if (fields[0] == "eval") {
    if (fields.size() < 3) return "ctr miss";
    uint32_t pid = static_cast<uint32_t>(std::strtoul(fields[1].c_str(), nullptr, 10));
    std::string source;
    for (size_t i = 2; i < fields.size(); ++i) {
      if (i != 2) source.push_back('\x1f');
      source += fields[i];
    }
    return Eval(pid, source);
  }
  if (fields[0] == "call") {
    if (fields.size() < 3) return "ctr miss";
    uint32_t pid = static_cast<uint32_t>(std::strtoul(fields[1].c_str(), nullptr, 10));
    std::string payload;
    for (size_t i = 3; i < fields.size(); ++i) {
      if (i != 3) payload.push_back('\x1f');
      payload += fields[i];
    }
    return Call(pid, fields[2], payload);
  }
  return "ctr miss";
}

}  // namespace wasmgocos
