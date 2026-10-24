// The `webgpu` gocvm catalog (include/webgpu/catalog.h): a compute device on
// webgpu.h, which remote.cc implements on the host's navigator.gpu.

#include "webgpu/catalog.h"

#include <webgpu/webgpu.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace wasmwebgpu {
namespace {

WGPUStringView Sv(std::string_view s) { return WGPUStringView{s.data(), s.size()}; }

std::string Str(WGPUStringView v) {
  if (v.data == nullptr) return {};
  if (v.length == WGPU_STRLEN) return std::string(v.data);
  return std::string(v.data, v.length);
}

std::string Err(const std::string& s) { return "error: webgpu: " + s; }

std::vector<std::string> Words(std::string_view s) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    size_t b = i;
    while (i < s.size() && s[i] != ' ' && s[i] != '\t' && s[i] != '\n' && s[i] != '\r') ++i;
    if (i > b) out.emplace_back(s.substr(b, i - b));
  }
  return out;
}

bool ParseU64(const std::string& s, uint64_t* v) {
  if (s.empty()) return false;
  char* end = nullptr;
  *v = std::strtoull(s.c_str(), &end, 0);
  return end && *end == '\0';
}

struct Buf {
  WGPUBuffer b = nullptr;
  uint64_t size = 0;
};

struct Device {
  bool open = false;
  WGPUInstance instance = nullptr;
  WGPUAdapter adapter = nullptr;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;
  WGPULimits limits = WGPU_LIMITS_INIT;
  std::string info;  // the Open reply
  uint32_t next = 1;
  std::map<uint32_t, Buf> bufs;
  std::map<uint32_t, WGPUShaderModule> shaders;
  std::map<uint32_t, WGPUComputePipeline> pipes;
  std::map<uint32_t, WGPUBindGroup> groups;
  std::string uncaptured;
};

Device& D() {
  static Device d;
  return d;
}

void OnUncaptured(WGPUDevice const*, WGPUErrorType, WGPUStringView message, void*, void*) {
  D().uncaptured += Str(message) + "\n";
}

// A validation + out-of-memory scope around `work`; the scope's error, or "".
template <typename Fn>
std::string Scoped(Fn work) {
  Device& d = D();
  wgpuDevicePushErrorScope(d.device, WGPUErrorFilter_Validation);
  wgpuDevicePushErrorScope(d.device, WGPUErrorFilter_OutOfMemory);
  work();
  std::string err;
  auto pop = [&](const char* what) {
    WGPUPopErrorScopeCallbackInfo cb = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    cb.mode = WGPUCallbackMode_AllowSpontaneous;
    cb.userdata1 = &err;
    cb.userdata2 = const_cast<char*>(what);
    cb.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView msg, void* u1, void* u2) {
      std::string& e = *static_cast<std::string*>(u1);
      if (status != WGPUPopErrorScopeStatus_Success) e += "error scope: " + Str(msg) + "\n";
      else if (type != WGPUErrorType_NoError) e += std::string(static_cast<const char*>(u2)) + ": " + Str(msg) + "\n";
    };
    wgpuDevicePopErrorScope(d.device, cb);
  };
  pop("out of memory");
  pop("validation");
  if (!d.uncaptured.empty()) {
    err += d.uncaptured;
    d.uncaptured.clear();
  }
  return err;
}

void Line(std::string* s, const char* k, uint64_t v) { *s += std::string(k) + "=" + std::to_string(v) + "\n"; }

std::string Open() {
  Device& d = D();
  if (d.open) return d.info;
  d.instance = wgpuCreateInstance(nullptr);
  WGPURequestAdapterOptions opts = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
  opts.powerPreference = WGPUPowerPreference_HighPerformance;
  WGPURequestAdapterCallbackInfo acb = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
  acb.mode = WGPUCallbackMode_AllowSpontaneous;
  acb.userdata1 = &d.adapter;
  acb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter a, WGPUStringView, void* u1, void*) {
    if (status == WGPURequestAdapterStatus_Success) *static_cast<WGPUAdapter*>(u1) = a;
  };
  wgpuInstanceRequestAdapter(d.instance, &opts, acb);
  if (!d.adapter) return Err("no WebGPU adapter (does this host have WebGPU and JSPI?)");

  // Ask for everything the adapter has: model weights want the largest
  // buffers and bindings the hardware allows.
  WGPULimits want = WGPU_LIMITS_INIT;
  wgpuAdapterGetLimits(d.adapter, &want);
  std::vector<WGPUFeatureName> features;
  for (WGPUFeatureName f : {WGPUFeatureName_ShaderF16, WGPUFeatureName_Subgroups,
                            WGPUFeatureName_TimestampQuery})
    if (wgpuAdapterHasFeature(d.adapter, f)) features.push_back(f);
  WGPUDeviceDescriptor dd = WGPU_DEVICE_DESCRIPTOR_INIT;
  dd.label = Sv("go++ webgpu");
  dd.requiredLimits = &want;
  dd.requiredFeatureCount = features.size();
  dd.requiredFeatures = features.data();
  dd.uncapturedErrorCallbackInfo.callback = OnUncaptured;
  WGPURequestDeviceCallbackInfo dcb = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
  dcb.mode = WGPUCallbackMode_AllowSpontaneous;
  dcb.userdata1 = &d.device;
  dcb.callback = [](WGPURequestDeviceStatus status, WGPUDevice dev, WGPUStringView, void* u1, void*) {
    if (status == WGPURequestDeviceStatus_Success) *static_cast<WGPUDevice*>(u1) = dev;
  };
  wgpuAdapterRequestDevice(d.adapter, &dd, dcb);
  if (!d.device) return Err("the adapter gave no device");
  d.queue = wgpuDeviceGetQueue(d.device);
  wgpuDeviceGetLimits(d.device, &d.limits);

  std::string s;
  WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
  wgpuAdapterGetInfo(d.adapter, &info);
  s += "vendor=" + Str(info.vendor) + "\n";
  s += "architecture=" + Str(info.architecture) + "\n";
  s += "device=" + Str(info.device) + "\n";
  s += "description=" + Str(info.description) + "\n";
  s += std::string("type=") + (info.adapterType == WGPUAdapterType_CPU ? "cpu" : "gpu") + "\n";
  wgpuAdapterInfoFreeMembers(info);
  for (WGPUFeatureName f : features) {
    if (f == WGPUFeatureName_ShaderF16) s += "feature=shader-f16\n";
    if (f == WGPUFeatureName_Subgroups) s += "feature=subgroups\n";
    if (f == WGPUFeatureName_TimestampQuery) s += "feature=timestamp-query\n";
  }
  WGPUSupportedWGSLLanguageFeatures wgsl = {};
  wgpuInstanceGetWGSLLanguageFeatures(d.instance, &wgsl);
  for (size_t i = 0; i < wgsl.featureCount; ++i) {
    if (wgsl.features[i] == WGPUWGSLLanguageFeatureName_Packed4x8IntegerDotProduct)
      s += "wgsl=packed_4x8_integer_dot_product\n";
    if (wgsl.features[i] == WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures)
      s += "wgsl=readonly_and_readwrite_storage_textures\n";
  }
  wgpuSupportedWGSLLanguageFeaturesFreeMembers(wgsl);
  Line(&s, "maxBufferSize", d.limits.maxBufferSize);
  Line(&s, "maxStorageBufferBindingSize", d.limits.maxStorageBufferBindingSize);
  Line(&s, "maxUniformBufferBindingSize", d.limits.maxUniformBufferBindingSize);
  Line(&s, "maxStorageBuffersPerShaderStage", d.limits.maxStorageBuffersPerShaderStage);
  Line(&s, "minStorageBufferOffsetAlignment", d.limits.minStorageBufferOffsetAlignment);
  Line(&s, "minUniformBufferOffsetAlignment", d.limits.minUniformBufferOffsetAlignment);
  Line(&s, "maxComputeWorkgroupStorageSize", d.limits.maxComputeWorkgroupStorageSize);
  Line(&s, "maxComputeInvocationsPerWorkgroup", d.limits.maxComputeInvocationsPerWorkgroup);
  Line(&s, "maxComputeWorkgroupsPerDimension", d.limits.maxComputeWorkgroupsPerDimension);
  d.info = s;
  d.open = true;
  return s;
}

std::string NewBuffer(const std::string& rest) {
  Device& d = D();
  uint64_t size = 0;
  std::vector<std::string> a = Words(rest);
  if (a.size() != 1 || !ParseU64(a[0], &size) || size == 0) return Err("Buffer <size>");
  size = (size + 15) / 16 * 16;
  if (size > d.limits.maxBufferSize)
    return Err("buffer of " + std::to_string(size) + " bytes is over maxBufferSize " +
               std::to_string(d.limits.maxBufferSize));
  WGPUBuffer b = nullptr;
  std::string e = Scoped([&] {
    WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
    bd.size = size;
    bd.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_Uniform | WGPUBufferUsage_CopySrc |
               WGPUBufferUsage_CopyDst;
    b = wgpuDeviceCreateBuffer(d.device, &bd);
  });
  if (!e.empty() || !b) {
    if (b) wgpuBufferRelease(b);
    return Err("Buffer " + std::to_string(size) + ": " + e);
  }
  uint32_t id = d.next++;
  d.bufs[id] = Buf{b, size};
  return std::to_string(id);
}

std::string Free(const std::string& rest) {
  Device& d = D();
  uint64_t id = 0;
  if (!ParseU64(rest, &id)) return Err("Free <id>");
  uint32_t k = static_cast<uint32_t>(id);
  if (auto it = d.bufs.find(k); it != d.bufs.end()) {
    wgpuBufferDestroy(it->second.b);
    wgpuBufferRelease(it->second.b);
    d.bufs.erase(it);
  } else if (auto is = d.shaders.find(k); is != d.shaders.end()) {
    wgpuShaderModuleRelease(is->second);
    d.shaders.erase(is);
  } else if (auto ip = d.pipes.find(k); ip != d.pipes.end()) {
    wgpuComputePipelineRelease(ip->second);
    d.pipes.erase(ip);
  } else if (auto ig = d.groups.find(k); ig != d.groups.end()) {
    wgpuBindGroupRelease(ig->second);
    d.groups.erase(ig);
  } else {
    return Err("no object " + rest);
  }
  return "";
}

std::string Write(const std::string& rest) {
  Device& d = D();
  size_t nl = rest.find('\n');
  if (nl == std::string::npos) return Err("Write <id> <off>\\n<bytes>");
  std::vector<std::string> a = Words(std::string_view(rest).substr(0, nl));
  uint64_t id = 0, off = 0;
  if (a.size() != 2 || !ParseU64(a[0], &id) || !ParseU64(a[1], &off)) return Err("Write <id> <off>\\n<bytes>");
  auto it = d.bufs.find(static_cast<uint32_t>(id));
  if (it == d.bufs.end()) return Err("no buffer " + a[0]);
  std::string bytes = rest.substr(nl + 1);
  bytes.resize((bytes.size() + 3) / 4 * 4, '\0');
  if (off % 4 != 0 || off + bytes.size() > it->second.size)
    return Err("Write " + a[0] + ": [" + std::to_string(off) + ", +" + std::to_string(bytes.size()) +
               ") is outside the buffer or unaligned");
  if (!bytes.empty()) wgpuQueueWriteBuffer(d.queue, it->second.b, off, bytes.data(), bytes.size());
  return "";
}

// [off, off+len) of a buffer through a staging copy, as the encoder `enc`'s
// last command; the staging buffer to map once `enc` is submitted.
WGPUBuffer StageRead(WGPUCommandEncoder enc, const Buf& src, uint64_t off, uint64_t len) {
  WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
  bd.size = len;
  bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
  WGPUBuffer staging = wgpuDeviceCreateBuffer(D().device, &bd);
  wgpuCommandEncoderCopyBufferToBuffer(enc, src.b, off, staging, 0, len);
  return staging;
}

bool MapStaged(WGPUBuffer staging, uint64_t len, std::string* out, std::string* err) {
  struct Ctx {
    bool ok = false;
    std::string msg;
  } ctx;
  WGPUBufferMapCallbackInfo cb = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
  cb.mode = WGPUCallbackMode_AllowSpontaneous;
  cb.userdata1 = &ctx;
  cb.callback = [](WGPUMapAsyncStatus status, WGPUStringView msg, void* u1, void*) {
    Ctx& c = *static_cast<Ctx*>(u1);
    c.ok = status == WGPUMapAsyncStatus_Success;
    c.msg = Str(msg);
  };
  wgpuBufferMapAsync(staging, WGPUMapMode_Read, 0, static_cast<size_t>(len), cb);
  if (ctx.ok) {
    const void* p = wgpuBufferGetConstMappedRange(staging, 0, static_cast<size_t>(len));
    if (p) out->append(static_cast<const char*>(p), static_cast<size_t>(len));
    wgpuBufferUnmap(staging);
  } else {
    *err += "read: " + ctx.msg + "\n";
  }
  wgpuBufferDestroy(staging);
  wgpuBufferRelease(staging);
  return ctx.ok;
}

std::string Read(const std::string& rest) {
  Device& d = D();
  std::vector<std::string> a = Words(rest);
  uint64_t id = 0, off = 0, len = 0;
  if (a.size() != 3 || !ParseU64(a[0], &id) || !ParseU64(a[1], &off) || !ParseU64(a[2], &len))
    return Err("Read <id> <off> <len>");
  auto it = d.bufs.find(static_cast<uint32_t>(id));
  if (it == d.bufs.end()) return Err("no buffer " + a[0]);
  if (off % 4 != 0 || len % 4 != 0 || off + len > it->second.size) return Err("Read " + a[0] + ": bad range");
  if (len == 0) return "";
  std::string out, err;
  WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(d.device, nullptr);
  WGPUBuffer staging = StageRead(enc, it->second, off, len);
  WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
  wgpuQueueSubmit(d.queue, 1, &cmd);
  wgpuCommandBufferRelease(cmd);
  wgpuCommandEncoderRelease(enc);
  if (!MapStaged(staging, len, &out, &err)) return Err(err);
  return out;
}

std::string NewShader(const std::string& wgsl) {
  Device& d = D();
  WGPUShaderModule m = nullptr;
  std::string e = Scoped([&] {
    WGPUShaderSourceWGSL src = WGPU_SHADER_SOURCE_WGSL_INIT;
    src.code = Sv(wgsl);
    WGPUShaderModuleDescriptor md = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    md.nextInChain = &src.chain;
    m = wgpuDeviceCreateShaderModule(d.device, &md);
  });
  // Compile messages name the line; the scope only says it failed.
  std::string msgs;
  if (m) {
    WGPUCompilationInfoCallbackInfo cb = WGPU_COMPILATION_INFO_CALLBACK_INFO_INIT;
    cb.mode = WGPUCallbackMode_AllowSpontaneous;
    cb.userdata1 = &msgs;
    cb.callback = [](WGPUCompilationInfoRequestStatus, WGPUCompilationInfo const* info, void* u1, void*) {
      if (!info) return;
      std::string& s = *static_cast<std::string*>(u1);
      for (size_t i = 0; i < info->messageCount; ++i) {
        const WGPUCompilationMessage& m = info->messages[i];
        if (m.type != WGPUCompilationMessageType_Error) continue;
        s += std::to_string(m.lineNum) + ":" + std::to_string(m.linePos) + ": " + Str(m.message) + "\n";
      }
    };
    wgpuShaderModuleGetCompilationInfo(m, cb);
  }
  if (!e.empty() || !msgs.empty() || !m) {
    if (m) wgpuShaderModuleRelease(m);
    return Err("Shader: " + msgs + e);
  }
  uint32_t id = d.next++;
  d.shaders[id] = m;
  return std::to_string(id);
}

std::string NewPipeline(const std::string& rest) {
  Device& d = D();
  std::vector<std::string> a = Words(rest);
  uint64_t sid = 0;
  if (a.size() < 2 || !ParseU64(a[0], &sid)) return Err("Pipeline <shader> <entry> [name=value ...]");
  auto it = d.shaders.find(static_cast<uint32_t>(sid));
  if (it == d.shaders.end()) return Err("no shader " + a[0]);
  std::vector<std::string> keys;
  std::vector<WGPUConstantEntry> consts;
  keys.reserve(a.size());
  for (size_t i = 2; i < a.size(); ++i) {
    size_t eq = a[i].find('=');
    if (eq == std::string::npos) return Err("Pipeline: constant `" + a[i] + "` is not name=value");
    keys.push_back(a[i].substr(0, eq));
    WGPUConstantEntry c = WGPU_CONSTANT_ENTRY_INIT;
    c.value = std::strtod(a[i].c_str() + eq + 1, nullptr);
    consts.push_back(c);
  }
  for (size_t i = 0; i < consts.size(); ++i) consts[i].key = Sv(keys[i]);
  WGPUComputePipeline p = nullptr;
  std::string e = Scoped([&] {
    WGPUComputePipelineDescriptor pd = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
    pd.compute.module = it->second;
    pd.compute.entryPoint = Sv(a[1]);
    pd.compute.constantCount = consts.size();
    pd.compute.constants = consts.data();
    p = wgpuDeviceCreateComputePipeline(d.device, &pd);
  });
  if (!e.empty() || !p) {
    if (p) wgpuComputePipelineRelease(p);
    return Err("Pipeline " + a[1] + ": " + e);
  }
  uint32_t id = d.next++;
  d.pipes[id] = p;
  return std::to_string(id);
}

std::string NewGroup(const std::string& rest) {
  Device& d = D();
  std::vector<std::string> a = Words(rest);
  uint64_t pid = 0, index = 0;
  if (a.size() < 2 || !ParseU64(a[0], &pid) || !ParseU64(a[1], &index))
    return Err("Group <pipeline> <index> <binding>=<buf>[:<off>[:<size>]] ...");
  auto ip = d.pipes.find(static_cast<uint32_t>(pid));
  if (ip == d.pipes.end()) return Err("no pipeline " + a[0]);
  std::vector<WGPUBindGroupEntry> entries;
  for (size_t i = 2; i < a.size(); ++i) {
    std::string w = a[i];
    size_t eq = w.find('=');
    if (eq == std::string::npos) return Err("Group: `" + w + "` is not binding=buffer");
    std::vector<std::string> parts;
    size_t at = eq + 1;
    for (;;) {
      size_t c = w.find(':', at);
      parts.push_back(w.substr(at, c == std::string::npos ? std::string::npos : c - at));
      if (c == std::string::npos) break;
      at = c + 1;
    }
    uint64_t binding = 0, bid = 0, off = 0, size = 0;
    if (!ParseU64(w.substr(0, eq), &binding) || !ParseU64(parts[0], &bid)) return Err("Group: bad `" + w + "`");
    auto ib = d.bufs.find(static_cast<uint32_t>(bid));
    if (ib == d.bufs.end()) return Err("Group: no buffer " + parts[0]);
    if (parts.size() > 1 && !ParseU64(parts[1], &off)) return Err("Group: bad offset in `" + w + "`");
    if (parts.size() > 2 && !ParseU64(parts[2], &size)) return Err("Group: bad size in `" + w + "`");
    WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
    e.binding = static_cast<uint32_t>(binding);
    e.buffer = ib->second.b;
    e.offset = off;
    e.size = size ? size : ib->second.size - off;
    entries.push_back(e);
  }
  WGPUBindGroup g = nullptr;
  std::string e = Scoped([&] {
    WGPUBindGroupLayout layout = wgpuComputePipelineGetBindGroupLayout(ip->second, static_cast<uint32_t>(index));
    WGPUBindGroupDescriptor gd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    gd.layout = layout;
    gd.entryCount = entries.size();
    gd.entries = entries.data();
    g = wgpuDeviceCreateBindGroup(d.device, &gd);
    wgpuBindGroupLayoutRelease(layout);
  });
  if (!e.empty() || !g) {
    if (g) wgpuBindGroupRelease(g);
    return Err("Group: " + e);
  }
  uint32_t id = d.next++;
  d.groups[id] = g;
  return std::to_string(id);
}

// A Run program is little-endian u32 words, one op after another:
//
//   1 DISPATCH  pipeline ngroups (index group)*ngroups x y z
//   2 WRITE     buffer offset nbytes  then the bytes, padded to 4
//   3 COPY      src srcOffset dst dstOffset nbytes
//   4 READ      buffer offset nbytes  the bytes are appended to the reply
//
// Ops run in order. Consecutive dispatches share one compute pass (WebGPU
// orders storage writes between dispatches). A WRITE is a queue write, so
// the work recorded before it is submitted first. The reply is the READ
// bytes, in order, once everything has run.
std::string RunProgram(const std::string& prog) {
  Device& d = D();
  if (prog.size() % 4 != 0) return Err("Run: program is not whole words");
  const size_t nw = prog.size() / 4;
  auto word = [&](size_t i) {
    uint32_t v;
    std::memcpy(&v, prog.data() + i * 4, 4);
    return v;
  };
  std::string err;
  std::vector<std::pair<WGPUBuffer, uint64_t>> reads;
  WGPUCommandEncoder enc = nullptr;
  WGPUComputePassEncoder pass = nullptr;
  auto end_pass = [&] {
    if (pass) {
      wgpuComputePassEncoderEnd(pass);
      wgpuComputePassEncoderRelease(pass);
      pass = nullptr;
    }
  };
  auto submit = [&] {
    end_pass();
    if (!enc) return;
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(d.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
    enc = nullptr;
  };
  auto encoder = [&] {
    if (!enc) enc = wgpuDeviceCreateCommandEncoder(d.device, nullptr);
    return enc;
  };
  auto buf = [&](uint32_t id) -> const Buf* {
    auto it = d.bufs.find(id);
    if (it == d.bufs.end()) {
      err += "no buffer " + std::to_string(id) + "\n";
      return nullptr;
    }
    return &it->second;
  };

  std::string scoped = Scoped([&] {
    size_t i = 0;
    while (i < nw && err.empty()) {
      uint32_t op = word(i++);
      if (op == 1) {
        if (i + 2 > nw) { err += "DISPATCH: truncated\n"; break; }
        uint32_t pid = word(i++), ng = word(i++);
        if (i + 2 * ng + 3 > nw) { err += "DISPATCH: truncated\n"; break; }
        auto ip = d.pipes.find(pid);
        if (ip == d.pipes.end()) { err += "no pipeline " + std::to_string(pid) + "\n"; break; }
        if (!pass) {
          WGPUComputePassDescriptor pd = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
          pass = wgpuCommandEncoderBeginComputePass(encoder(), &pd);
        }
        wgpuComputePassEncoderSetPipeline(pass, ip->second);
        for (uint32_t g = 0; g < ng; ++g) {
          uint32_t index = word(i++), gid = word(i++);
          auto ig = d.groups.find(gid);
          if (ig == d.groups.end()) { err += "no group " + std::to_string(gid) + "\n"; break; }
          wgpuComputePassEncoderSetBindGroup(pass, index, ig->second, 0, nullptr);
        }
        uint32_t x = word(i++), y = word(i++), z = word(i++);
        if (err.empty()) wgpuComputePassEncoderDispatchWorkgroups(pass, x, y, z);
      } else if (op == 2) {
        if (i + 3 > nw) { err += "WRITE: truncated\n"; break; }
        uint32_t bid = word(i++), off = word(i++), n = word(i++);
        size_t words = (static_cast<size_t>(n) + 3) / 4;
        if (i + words > nw) { err += "WRITE: truncated\n"; break; }
        const Buf* b = buf(bid);
        if (!b) break;
        if (off % 4 != 0 || off + words * 4 > b->size) { err += "WRITE: outside buffer\n"; break; }
        submit();
        if (words) wgpuQueueWriteBuffer(d.queue, b->b, off, prog.data() + i * 4, words * 4);
        i += words;
      } else if (op == 3) {
        if (i + 5 > nw) { err += "COPY: truncated\n"; break; }
        uint32_t sid = word(i++), soff = word(i++), did = word(i++), doff = word(i++), n = word(i++);
        const Buf* s = buf(sid);
        const Buf* t = buf(did);
        if (!s || !t) break;
        end_pass();
        wgpuCommandEncoderCopyBufferToBuffer(encoder(), s->b, soff, t->b, doff, n);
      } else if (op == 4) {
        if (i + 3 > nw) { err += "READ: truncated\n"; break; }
        uint32_t bid = word(i++), off = word(i++), n = word(i++);
        const Buf* b = buf(bid);
        if (!b) break;
        if (off % 4 != 0 || n % 4 != 0 || n == 0 || off + n > b->size) { err += "READ: bad range\n"; break; }
        end_pass();
        reads.emplace_back(StageRead(encoder(), *b, off, n), n);
      } else {
        err += "unknown op " + std::to_string(op) + "\n";
        break;
      }
    }
    submit();
  });
  std::string out;
  for (auto& r : reads) MapStaged(r.first, r.second, &out, &err);
  err += scoped;
  if (!err.empty()) return Err("Run: " + err);
  return out;
}

}  // namespace

std::string webgpu_call(const std::string& api, const std::string& rest) {
  if (api == "Open") return Open();
  if (!D().open) {
    std::string r = Open();
    if (r.rfind("error:", 0) == 0) return r;
  }
  if (api == "Info") return D().info;
  if (api == "Buffer") return NewBuffer(rest);
  if (api == "Free") return Free(rest);
  if (api == "Write") return Write(rest);
  if (api == "Read") return Read(rest);
  if (api == "Shader") return NewShader(rest);
  if (api == "Pipeline") return NewPipeline(rest);
  if (api == "Group") return NewGroup(rest);
  if (api == "Run") return RunProgram(rest);
  return Err("unknown api `" + api + "`");
}

}  // namespace wasmwebgpu
