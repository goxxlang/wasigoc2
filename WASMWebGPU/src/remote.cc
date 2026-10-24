// webgpu.h for a wasigocvm module: the browser's WebGPU, reached over one
// suspending import.
//
// Every call here is recorded as a JSON command in a batch; a call that
// needs an answer (an adapter, a device, a mapped range, a popped error
// scope, ...) sends the batch through the import, which the JS host running
// this module executes against its own navigator.gpu (js/webgpu-exec.js).
// The import is a JSPI suspending import: while a WebGPU promise is pending
// the module is suspended and the host's event loop settles it; no other
// thread and no shared memory is involved. Objects are ids the module picks,
// so creating one needs no round trip. An async call's result is known when
// it returns: a CallbackMode_AllowSpontaneous callback fires before it
// returns, the others on wgpuInstanceProcessEvents / wgpuInstanceWaitAny, as
// webgpu.h allows.
//
// The import's name is WASIGO_WEBGPU_IMPORT_MODULE . WASIGO_WEBGPU_IMPORT_NAME
// (default webgpu.batch); HeapVM builds this file as heap.gpu.
//
// Enum values are those of the emdawn webgpu.h this is compiled against
// (include/webgpu, emdawn v20250517); the tables below turn them into the
// strings navigator.gpu takes.

#include <webgpu/webgpu.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef WASIGO_WEBGPU_IMPORT_MODULE
#define WASIGO_WEBGPU_IMPORT_MODULE "webgpu"
#endif
#ifndef WASIGO_WEBGPU_IMPORT_NAME
#define WASIGO_WEBGPU_IMPORT_NAME "batch"
#endif

// batch(req, len, out, cap): the host runs the batch at [req, req+len) and
// writes its reply to [out, out+cap). The reply's size, or -1 when there is
// no WebGPU to run it (no JSPI or no navigator.gpu in this host).
extern "C" __attribute__((import_module(WASIGO_WEBGPU_IMPORT_MODULE),
                          import_name(WASIGO_WEBGPU_IMPORT_NAME))) int32_t
webgpu_batch(int32_t req_ptr, int32_t req_len, int32_t out_ptr, int32_t out_cap);

namespace {

// ---------------------------------------------------------------------
// Objects

struct Obj {
  uint32_t id = 0;
  uint32_t refs = 1;
  virtual ~Obj() = default;
};

}  // namespace

struct WGPUInstanceImpl : Obj {};
struct WGPUAdapterImpl : Obj {
  std::string vendor, architecture, device, description;
  uint32_t subgroup_min = 0, subgroup_max = 0;
  bool fallback = false;
  std::vector<WGPUFeatureName> features;
  WGPULimits limits{};
};
struct WGPUQueueImpl : Obj {
  struct WGPUDeviceImpl* device = nullptr;
};
struct WGPUDeviceImpl : Obj {
  WGPUAdapterImpl* adapter = nullptr;
  WGPUQueueImpl* queue = nullptr;
  std::vector<WGPUFeatureName> features;
  WGPULimits limits{};
  WGPUDeviceLostCallbackInfo lost{};
  WGPUUncapturedErrorCallbackInfo uncaptured{};
  uint64_t lost_future = 0;
  bool lost_fired = false;
};
struct WGPUBufferImpl : Obj {
  WGPUDeviceImpl* device = nullptr;
  uint64_t size = 0;
  WGPUBufferUsage usage = 0;
  WGPUBufferMapState state = WGPUBufferMapState_Unmapped;
  bool write = false;
  size_t map_offset = 0;
  size_t map_size = 0;
  std::vector<uint8_t> shadow;
};
struct WGPUTextureImpl : Obj {
  uint32_t width = 0, height = 0, depth = 1, mips = 1, samples = 1;
  WGPUTextureDimension dimension = WGPUTextureDimension_2D;
  WGPUTextureFormat format = WGPUTextureFormat_Undefined;
  WGPUTextureUsage usage = 0;
};
struct WGPUQuerySetImpl : Obj {
  WGPUQueryType type = WGPUQueryType_Occlusion;
  uint32_t count = 0;
};
struct WGPUSurfaceImpl : Obj {
  bool configured = false;
  WGPUTextureFormat format = WGPUTextureFormat_Undefined;
  WGPUTextureUsage usage = 0;
  uint32_t width = 0, height = 0;
};
struct WGPUBindGroupImpl : Obj {};
struct WGPUBindGroupLayoutImpl : Obj {};
struct WGPUCommandBufferImpl : Obj {};
struct WGPUCommandEncoderImpl : Obj {};
struct WGPUComputePassEncoderImpl : Obj {};
struct WGPUComputePipelineImpl : Obj {};
struct WGPUPipelineLayoutImpl : Obj {};
struct WGPURenderBundleImpl : Obj {};
struct WGPURenderBundleEncoderImpl : Obj {};
struct WGPURenderPassEncoderImpl : Obj {};
struct WGPURenderPipelineImpl : Obj {};
struct WGPUSamplerImpl : Obj {};
struct WGPUShaderModuleImpl : Obj {};
struct WGPUTextureViewImpl : Obj {};

namespace {

uint32_t g_next_id = 1;
std::unordered_map<uint32_t, WGPUDeviceImpl*> g_devices;

template <typename T>
T* Make() {
  T* o = new T;
  o->id = g_next_id++;
  return o;
}

std::string_view Sv(WGPUStringView v) {
  if (v.data == nullptr) return {};
  if (v.length == WGPU_STRLEN) return std::string_view(v.data);
  return std::string_view(v.data, v.length);
}

WGPUStringView View(const std::string& s) { return WGPUStringView{s.data(), s.size()}; }

// ---------------------------------------------------------------------
// Enum names, as navigator.gpu spells them. nullptr: leave the member out.

template <size_t N>
const char* Pick(const char* const (&names)[N], uint32_t v) {
  return v < N ? names[v] : nullptr;
}

const char* TextureFormatName(WGPUTextureFormat f) {
  static const char* const kNames[] = {
      nullptr, "r8unorm", "r8snorm", "r8uint", "r8sint", "r16uint", "r16sint", "r16float",
      "rg8unorm", "rg8snorm", "rg8uint", "rg8sint", "r32float", "r32uint", "r32sint", "rg16uint",
      "rg16sint", "rg16float", "rgba8unorm", "rgba8unorm-srgb", "rgba8snorm", "rgba8uint",
      "rgba8sint", "bgra8unorm", "bgra8unorm-srgb", "rgb10a2uint", "rgb10a2unorm", "rg11b10ufloat",
      "rgb9e5ufloat", "rg32float", "rg32uint", "rg32sint", "rgba16uint", "rgba16sint", "rgba16float",
      "rgba32float", "rgba32uint", "rgba32sint", "stencil8", "depth16unorm", "depth24plus",
      "depth24plus-stencil8", "depth32float", "depth32float-stencil8", "bc1-rgba-unorm",
      "bc1-rgba-unorm-srgb", "bc2-rgba-unorm", "bc2-rgba-unorm-srgb", "bc3-rgba-unorm",
      "bc3-rgba-unorm-srgb", "bc4-r-unorm", "bc4-r-snorm", "bc5-rg-unorm", "bc5-rg-snorm",
      "bc6h-rgb-ufloat", "bc6h-rgb-float", "bc7-rgba-unorm", "bc7-rgba-unorm-srgb", "etc2-rgb8unorm",
      "etc2-rgb8unorm-srgb", "etc2-rgb8a1unorm", "etc2-rgb8a1unorm-srgb", "etc2-rgba8unorm",
      "etc2-rgba8unorm-srgb", "eac-r11unorm", "eac-r11snorm", "eac-rg11unorm", "eac-rg11snorm",
      "astc-4x4-unorm", "astc-4x4-unorm-srgb", "astc-5x4-unorm", "astc-5x4-unorm-srgb",
      "astc-5x5-unorm", "astc-5x5-unorm-srgb", "astc-6x5-unorm", "astc-6x5-unorm-srgb",
      "astc-6x6-unorm", "astc-6x6-unorm-srgb", "astc-8x5-unorm", "astc-8x5-unorm-srgb",
      "astc-8x6-unorm", "astc-8x6-unorm-srgb", "astc-8x8-unorm", "astc-8x8-unorm-srgb",
      "astc-10x5-unorm", "astc-10x5-unorm-srgb", "astc-10x6-unorm", "astc-10x6-unorm-srgb",
      "astc-10x8-unorm", "astc-10x8-unorm-srgb", "astc-10x10-unorm", "astc-10x10-unorm-srgb",
      "astc-12x10-unorm", "astc-12x10-unorm-srgb", "astc-12x12-unorm", "astc-12x12-unorm-srgb"};
  static_assert(sizeof(kNames) / sizeof(kNames[0]) == WGPUTextureFormat_ASTC12x12UnormSrgb + 1);
  return Pick(kNames, f);
}


const char* VertexFormatName(WGPUVertexFormat f) {
  static const char* const kNames[] = {
      nullptr, "uint8", "uint8x2", "uint8x4", "sint8", "sint8x2", "sint8x4", "unorm8", "unorm8x2",
      "unorm8x4", "snorm8", "snorm8x2", "snorm8x4", "uint16", "uint16x2", "uint16x4", "sint16",
      "sint16x2", "sint16x4", "unorm16", "unorm16x2", "unorm16x4", "snorm16", "snorm16x2",
      "snorm16x4", "float16", "float16x2", "float16x4", "float32", "float32x2", "float32x3",
      "float32x4", "uint32", "uint32x2", "uint32x3", "uint32x4", "sint32", "sint32x2", "sint32x3",
      "sint32x4", "unorm10-10-10-2", "unorm8x4-bgra"};
  static_assert(sizeof(kNames) / sizeof(kNames[0]) == WGPUVertexFormat_Unorm8x4BGRA + 1);
  return Pick(kNames, f);
}

const char* AddressModeName(WGPUAddressMode v) {
  static const char* const k[] = {nullptr, "clamp-to-edge", "repeat", "mirror-repeat"};
  return Pick(k, v);
}
const char* BlendFactorName(WGPUBlendFactor v) {
  static const char* const k[] = {nullptr, "zero", "one", "src", "one-minus-src", "src-alpha",
                                  "one-minus-src-alpha", "dst", "one-minus-dst", "dst-alpha",
                                  "one-minus-dst-alpha", "src-alpha-saturated", "constant",
                                  "one-minus-constant", "src1", "one-minus-src1", "src1-alpha",
                                  "one-minus-src1-alpha"};
  return Pick(k, v);
}
const char* BlendOperationName(WGPUBlendOperation v) {
  static const char* const k[] = {nullptr, "add", "subtract", "reverse-subtract", "min", "max"};
  return Pick(k, v);
}
const char* BufferBindingTypeName(WGPUBufferBindingType v) {
  static const char* const k[] = {nullptr, nullptr, "uniform", "storage", "read-only-storage"};
  return Pick(k, v);
}
const char* CompareFunctionName(WGPUCompareFunction v) {
  static const char* const k[] = {nullptr, "never", "less", "equal", "less-equal",
                                  "greater", "not-equal", "greater-equal", "always"};
  return Pick(k, v);
}
const char* CullModeName(WGPUCullMode v) {
  static const char* const k[] = {nullptr, "none", "front", "back"};
  return Pick(k, v);
}
const char* ErrorFilterName(WGPUErrorFilter v) {
  static const char* const k[] = {nullptr, "validation", "out-of-memory", "internal"};
  return Pick(k, v);
}
const char* FeatureLevelName(WGPUFeatureLevel v) {
  static const char* const k[] = {nullptr, "compatibility", "core"};
  return Pick(k, v);
}
const char* FilterModeName(uint32_t v) {  // FilterMode and MipmapFilterMode
  static const char* const k[] = {nullptr, "nearest", "linear"};
  return Pick(k, v);
}
const char* FrontFaceName(WGPUFrontFace v) {
  static const char* const k[] = {nullptr, "ccw", "cw"};
  return Pick(k, v);
}
const char* IndexFormatName(WGPUIndexFormat v) {
  static const char* const k[] = {nullptr, "uint16", "uint32"};
  return Pick(k, v);
}
const char* LoadOpName(WGPULoadOp v) {
  static const char* const k[] = {nullptr, "load", "clear"};
  return Pick(k, v);
}
const char* StoreOpName(WGPUStoreOp v) {
  static const char* const k[] = {nullptr, "store", "discard"};
  return Pick(k, v);
}
const char* PowerPreferenceName(WGPUPowerPreference v) {
  static const char* const k[] = {nullptr, "low-power", "high-performance"};
  return Pick(k, v);
}
const char* TopologyName(WGPUPrimitiveTopology v) {
  static const char* const k[] = {nullptr, "point-list", "line-list", "line-strip",
                                  "triangle-list", "triangle-strip"};
  return Pick(k, v);
}
const char* QueryTypeName(WGPUQueryType v) {
  static const char* const k[] = {nullptr, "occlusion", "timestamp"};
  return Pick(k, v);
}
const char* SamplerBindingTypeName(WGPUSamplerBindingType v) {
  static const char* const k[] = {nullptr, nullptr, "filtering", "non-filtering", "comparison"};
  return Pick(k, v);
}
const char* StencilOperationName(WGPUStencilOperation v) {
  static const char* const k[] = {nullptr, "keep", "zero", "replace", "invert",
                                  "increment-clamp", "decrement-clamp", "increment-wrap",
                                  "decrement-wrap"};
  return Pick(k, v);
}
const char* StorageAccessName(WGPUStorageTextureAccess v) {
  static const char* const k[] = {nullptr, nullptr, "write-only", "read-only", "read-write"};
  return Pick(k, v);
}
const char* AspectName(WGPUTextureAspect v) {
  static const char* const k[] = {nullptr, "all", "stencil-only", "depth-only"};
  return Pick(k, v);
}
const char* DimensionName(WGPUTextureDimension v) {
  static const char* const k[] = {nullptr, "1d", "2d", "3d"};
  return Pick(k, v);
}
const char* SampleTypeName(WGPUTextureSampleType v) {
  static const char* const k[] = {nullptr, nullptr, "float", "unfilterable-float",
                                  "depth", "sint", "uint"};
  return Pick(k, v);
}
const char* ViewDimensionName(WGPUTextureViewDimension v) {
  static const char* const k[] = {nullptr, "1d", "2d", "2d-array", "cube", "cube-array", "3d"};
  return Pick(k, v);
}
const char* StepModeName(WGPUVertexStepMode v) {
  static const char* const k[] = {nullptr, "vertex", "instance"};
  return Pick(k, v);
}
const char* AlphaModeName(WGPUCompositeAlphaMode v) {
  static const char* const k[] = {nullptr, "opaque", "premultiplied", nullptr, nullptr};
  return Pick(k, v);
}

struct FeatureRow {
  WGPUFeatureName value;
  const char* name;
};
const FeatureRow kFeatures[] = {
    {WGPUFeatureName_DepthClipControl, "depth-clip-control"},
    {WGPUFeatureName_Depth32FloatStencil8, "depth32float-stencil8"},
    {WGPUFeatureName_TimestampQuery, "timestamp-query"},
    {WGPUFeatureName_TextureCompressionBC, "texture-compression-bc"},
    {WGPUFeatureName_TextureCompressionBCSliced3D, "texture-compression-bc-sliced-3d"},
    {WGPUFeatureName_TextureCompressionETC2, "texture-compression-etc2"},
    {WGPUFeatureName_TextureCompressionASTC, "texture-compression-astc"},
    {WGPUFeatureName_TextureCompressionASTCSliced3D, "texture-compression-astc-sliced-3d"},
    {WGPUFeatureName_IndirectFirstInstance, "indirect-first-instance"},
    {WGPUFeatureName_ShaderF16, "shader-f16"},
    {WGPUFeatureName_RG11B10UfloatRenderable, "rg11b10ufloat-renderable"},
    {WGPUFeatureName_BGRA8UnormStorage, "bgra8unorm-storage"},
    {WGPUFeatureName_Float32Filterable, "float32-filterable"},
    {WGPUFeatureName_Float32Blendable, "float32-blendable"},
    {WGPUFeatureName_ClipDistances, "clip-distances"},
    {WGPUFeatureName_DualSourceBlending, "dual-source-blending"},
    {WGPUFeatureName_Subgroups, "subgroups"},
    {WGPUFeatureName_CoreFeaturesAndLimits, "core-features-and-limits"},
    {WGPUFeatureName_Unorm16TextureFormats, "unorm16-texture-formats"},
    {WGPUFeatureName_Snorm16TextureFormats, "snorm16-texture-formats"},
    {WGPUFeatureName_MultiDrawIndirect, "multi-draw-indirect"},
};

const char* FeatureNameOf(WGPUFeatureName f) {
  for (const FeatureRow& r : kFeatures)
    if (r.value == f) return r.name;
  return nullptr;
}

bool FeatureOf(std::string_view s, WGPUFeatureName* out) {
  for (const FeatureRow& r : kFeatures)
    if (s == r.name) {
      *out = r.value;
      return true;
    }
  return false;
}

struct WgslRow {
  WGPUWGSLLanguageFeatureName value;
  const char* name;
};
const WgslRow kWgslFeatures[] = {
    {WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures,
     "readonly_and_readwrite_storage_textures"},
    {WGPUWGSLLanguageFeatureName_Packed4x8IntegerDotProduct, "packed_4x8_integer_dot_product"},
    {WGPUWGSLLanguageFeatureName_UnrestrictedPointerParameters, "unrestricted_pointer_parameters"},
    {WGPUWGSLLanguageFeatureName_PointerCompositeAccess, "pointer_composite_access"},
    {WGPUWGSLLanguageFeatureName_SizedBindingArray, "sized_binding_array"},
};

// The limits, by the name the worker reports them under.
struct LimitRow {
  const char* name;
  size_t offset;
  bool wide;
};
#define VGPU_LIMIT32(n) {#n, offsetof(WGPULimits, n), false}
#define VGPU_LIMIT64(n) {#n, offsetof(WGPULimits, n), true}
const LimitRow kLimits[] = {
    VGPU_LIMIT32(maxTextureDimension1D),
    VGPU_LIMIT32(maxTextureDimension2D),
    VGPU_LIMIT32(maxTextureDimension3D),
    VGPU_LIMIT32(maxTextureArrayLayers),
    VGPU_LIMIT32(maxBindGroups),
    VGPU_LIMIT32(maxBindGroupsPlusVertexBuffers),
    VGPU_LIMIT32(maxBindingsPerBindGroup),
    VGPU_LIMIT32(maxDynamicUniformBuffersPerPipelineLayout),
    VGPU_LIMIT32(maxDynamicStorageBuffersPerPipelineLayout),
    VGPU_LIMIT32(maxSampledTexturesPerShaderStage),
    VGPU_LIMIT32(maxSamplersPerShaderStage),
    VGPU_LIMIT32(maxStorageBuffersPerShaderStage),
    VGPU_LIMIT32(maxStorageTexturesPerShaderStage),
    VGPU_LIMIT32(maxUniformBuffersPerShaderStage),
    VGPU_LIMIT64(maxUniformBufferBindingSize),
    VGPU_LIMIT64(maxStorageBufferBindingSize),
    VGPU_LIMIT32(minUniformBufferOffsetAlignment),
    VGPU_LIMIT32(minStorageBufferOffsetAlignment),
    VGPU_LIMIT32(maxVertexBuffers),
    VGPU_LIMIT64(maxBufferSize),
    VGPU_LIMIT32(maxVertexAttributes),
    VGPU_LIMIT32(maxVertexBufferArrayStride),
    VGPU_LIMIT32(maxInterStageShaderVariables),
    VGPU_LIMIT32(maxColorAttachments),
    VGPU_LIMIT32(maxColorAttachmentBytesPerSample),
    VGPU_LIMIT32(maxComputeWorkgroupStorageSize),
    VGPU_LIMIT32(maxComputeInvocationsPerWorkgroup),
    VGPU_LIMIT32(maxComputeWorkgroupSizeX),
    VGPU_LIMIT32(maxComputeWorkgroupSizeY),
    VGPU_LIMIT32(maxComputeWorkgroupSizeZ),
    VGPU_LIMIT32(maxComputeWorkgroupsPerDimension),
    VGPU_LIMIT32(maxImmediateSize),
    VGPU_LIMIT32(maxStorageBuffersInVertexStage),
    VGPU_LIMIT32(maxStorageTexturesInVertexStage),
    VGPU_LIMIT32(maxStorageBuffersInFragmentStage),
    VGPU_LIMIT32(maxStorageTexturesInFragmentStage),
};
#undef VGPU_LIMIT32
#undef VGPU_LIMIT64

void ClearLimits(WGPULimits* l) {
  WGPUChainedStruct* next = l->nextInChain;
  for (const LimitRow& r : kLimits) {
    char* p = reinterpret_cast<char*>(l) + r.offset;
    if (r.wide) {
      uint64_t v = UINT64_MAX;
      std::memcpy(p, &v, 8);
    } else {
      uint32_t v = UINT32_MAX;
      std::memcpy(p, &v, 4);
    }
  }
  l->nextInChain = next;
}

bool SetLimit(WGPULimits* l, std::string_view name, uint64_t v) {
  for (const LimitRow& r : kLimits) {
    if (name != r.name) continue;
    char* p = reinterpret_cast<char*>(l) + r.offset;
    if (r.wide) {
      std::memcpy(p, &v, 8);
    } else {
      uint32_t n = v > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(v);
      std::memcpy(p, &n, 4);
    }
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------
// The batch: a JSON array of commands plus a byte blob they refer to.

class Writer {
 public:
  std::string s;

  Writer& Obj() {
    Sep();
    s += '{';
    stack_.push_back(false);
    return *this;
  }
  Writer& End() {
    s += '}';
    stack_.pop_back();
    return *this;
  }
  Writer& Arr() {
    Sep();
    s += '[';
    stack_.push_back(false);
    return *this;
  }
  Writer& EndArr() {
    s += ']';
    stack_.pop_back();
    return *this;
  }
  Writer& K(const char* k) {
    Sep();
    s += '"';
    s += k;
    s += "\":";
    after_key_ = true;
    return *this;
  }
  Writer& Str(std::string_view v) {
    Sep();
    Quote(v);
    return *this;
  }
  Writer& U(uint64_t v) {
    Sep();
    s += std::to_string(v);
    return *this;
  }
  Writer& I(int64_t v) {
    Sep();
    s += std::to_string(v);
    return *this;
  }
  Writer& D(double v) {
    Sep();
    if (!std::isfinite(v)) {
      s += "null";
      return *this;
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    s += buf;
    return *this;
  }
  Writer& B(bool v) {
    Sep();
    s += v ? "true" : "false";
    return *this;
  }
  Writer& Null() {
    Sep();
    s += "null";
    return *this;
  }
  // A JS `undefined` argument: an optional one left out.
  Writer& Undef() {
    Sep();
    s += "{\"u\":1}";
    return *this;
  }
  // An object argument: the worker's JS object for this id.
  Writer& Ref(const ::Obj* o) {
    Sep();
    if (o == nullptr) {
      s += "null";
    } else {
      s += "{\"$\":";
      s += std::to_string(o->id);
      s += '}';
    }
    return *this;
  }
  // Bytes in the blob.
  Writer& Bytes(size_t off, size_t len) {
    Sep();
    s += "{\"#\":[" + std::to_string(off) + "," + std::to_string(len) + "]}";
    return *this;
  }
  // Optional members: written only when there is a value.
  Writer& KS(const char* k, const char* v) {
    if (v) K(k).Str(v);
    return *this;
  }
  Writer& KLabel(WGPUStringView label) {
    std::string_view v = Sv(label);
    if (!v.empty()) K("label").Str(v);
    return *this;
  }
  void Reset() {
    s.clear();
    stack_.clear();
    after_key_ = false;
  }

 private:
  void Sep() {
    if (after_key_) {
      after_key_ = false;
      return;
    }
    if (!stack_.empty()) {
      if (stack_.back()) s += ',';
      stack_.back() = true;
    }
  }
  void Quote(std::string_view v) {
    s += '"';
    for (unsigned char c : v) {
      if (c == '"' || c == '\\') {
        s += '\\';
        s += static_cast<char>(c);
      } else if (c == '\n') {
        s += "\\n";
      } else if (c == '\r') {
        s += "\\r";
      } else if (c == '\t') {
        s += "\\t";
      } else if (c < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
        s += buf;
      } else {
        s += static_cast<char>(c);
      }
    }
    s += '"';
  }
  std::vector<bool> stack_;
  bool after_key_ = false;
};

Writer g_w;
std::string g_blob;
bool g_open = false;
size_t g_count = 0;

// Batches this big go out without waiting for a sync point.
constexpr size_t kBatchMax = 8u << 20;

Writer& Cmd(const char* op) {
  if (!g_open) {
    g_w.Reset();
    g_w.Arr();
    g_open = true;
  }
  ++g_count;
  g_w.Obj().K("o").Str(op);
  return g_w;
}

size_t Blob(const void* p, size_t n) {
  size_t off = g_blob.size();
  if (n) g_blob.append(static_cast<const char*>(p), n);
  return off;
}

void Flush();
void EndCmd() {
  g_w.End();
  if (g_w.s.size() + g_blob.size() > kBatchMax) Flush();
}

// The worker's answer: `key=value` lines, then bytes.
struct Reply {
  bool answered = false;
  std::vector<std::pair<std::string, std::string>> kv;
  std::string bin;
  std::vector<std::string> errors;

  const std::string* Get(std::string_view k) const {
    for (const auto& p : kv)
      if (p.first == k) return &p.second;
    return nullptr;
  }
  std::string Str(std::string_view k) const {
    const std::string* v = Get(k);
    return v ? *v : std::string();
  }
  uint64_t U(std::string_view k) const {
    const std::string* v = Get(k);
    return v ? std::strtoull(v->c_str(), nullptr, 10) : 0;
  }
  bool Ok() const { return answered && errors.empty(); }
  std::string Error() const {
    if (!answered) return "vgpu: WebGPU did not answer (JSPI or navigator.gpu missing)";
    std::string out;
    for (const std::string& e : errors) {
      if (!out.empty()) out += "; ";
      out += e;
    }
    return out;
  }
};

std::string Unescape(std::string_view v) {
  std::string out;
  out.reserve(v.size());
  for (size_t i = 0; i < v.size(); ++i) {
    if (v[i] == '\\' && i + 1 < v.size()) {
      char c = v[++i];
      out += c == 'n' ? '\n' : c == 't' ? '\t' : c;
    } else {
      out += v[i];
    }
  }
  return out;
}

void FireLost(WGPUDeviceImpl* d, WGPUDeviceLostReason reason, const std::string& msg);

void Uncaptured(uint32_t dev_id, WGPUErrorType type, const std::string& msg) {
  auto it = g_devices.find(dev_id);
  if (it != g_devices.end() && it->second->uncaptured.callback) {
    WGPUDevice d = it->second;
    it->second->uncaptured.callback(&d, type, View(msg), it->second->uncaptured.userdata1,
                                    it->second->uncaptured.userdata2);
    return;
  }
  std::fprintf(stderr, "vgpu: %s\n", msg.c_str());
}

WGPUErrorType ErrorTypeOf(std::string_view s) {
  if (s == "validation") return WGPUErrorType_Validation;
  if (s == "out-of-memory") return WGPUErrorType_OutOfMemory;
  if (s == "internal") return WGPUErrorType_Internal;
  if (s == "none") return WGPUErrorType_NoError;
  return WGPUErrorType_Unknown;
}

// Sends the batch and returns the worker's answer. Errors WebGPU met on the
// way are in `errors`; uncaptured device errors and lost devices are
// delivered here.
Reply Send() {
  Reply r;
  if (!g_open) {
    g_w.Reset();
    g_w.Arr();
  }
  g_w.EndArr();
  std::string req;
  uint32_t jlen = static_cast<uint32_t>(g_w.s.size());
  req.resize(4);
  std::memcpy(req.data(), &jlen, 4);
  req += g_w.s;
  req += g_blob;
  g_w.Reset();
  g_blob.clear();
  g_open = false;
  g_count = 0;

  static std::vector<uint8_t> out(256 * 1024 - 64);
  int32_t n = webgpu_batch(static_cast<int32_t>(reinterpret_cast<uintptr_t>(req.data())),
                       static_cast<int32_t>(req.size()),
                       static_cast<int32_t>(reinterpret_cast<uintptr_t>(out.data())),
                       static_cast<int32_t>(out.size()));
  if (n < 4) return r;
  r.answered = true;
  uint32_t tlen = 0;
  std::memcpy(&tlen, out.data(), 4);
  if (tlen > static_cast<uint32_t>(n) - 4) tlen = static_cast<uint32_t>(n) - 4;
  std::string_view text(reinterpret_cast<const char*>(out.data()) + 4, tlen);
  r.bin.assign(reinterpret_cast<const char*>(out.data()) + 4 + tlen, static_cast<size_t>(n) - 4 - tlen);
  size_t at = 0;
  while (at < text.size()) {
    size_t nl = text.find('\n', at);
    if (nl == std::string_view::npos) nl = text.size();
    std::string_view line = text.substr(at, nl - at);
    at = nl + 1;
    size_t eq = line.find('=');
    if (eq == std::string_view::npos) continue;
    std::string key(line.substr(0, eq));
    std::string val = Unescape(line.substr(eq + 1));
    if (key == "error") {
      r.errors.push_back(val);
    } else if (key == "uncaptured" || key == "lost") {
      // "<device id>\t<kind>\t<message>"
      size_t t1 = val.find('\t');
      size_t t2 = t1 == std::string::npos ? std::string::npos : val.find('\t', t1 + 1);
      if (t2 == std::string::npos) continue;
      uint32_t dev = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
      std::string kind = val.substr(t1 + 1, t2 - t1 - 1);
      std::string msg = val.substr(t2 + 1);
      if (key == "uncaptured") {
        Uncaptured(dev, ErrorTypeOf(kind), msg);
      } else {
        auto it = g_devices.find(dev);
        if (it != g_devices.end())
          FireLost(it->second,
                   kind == "destroyed" ? WGPUDeviceLostReason_Destroyed : WGPUDeviceLostReason_Unknown,
                   msg);
      }
    } else {
      r.kv.emplace_back(std::move(key), std::move(val));
    }
  }
  return r;
}

// A flush nobody waits on: what went wrong is reported, not returned.
void Flush() {
  if (!g_open || g_count == 0) return;
  Reply r = Send();
  if (!r.answered) {
    std::fprintf(stderr, "vgpu: WebGPU did not answer (JSPI or navigator.gpu missing)\n");
    return;
  }
  for (const std::string& e : r.errors) std::fprintf(stderr, "vgpu: %s\n", e.c_str());
}

// A command that makes an object: JS object `id` is `parent.method(args)`.
Writer& New(const Obj* o, const Obj* parent, const char* method) {
  Writer& w = Cmd("new");
  w.K("id").U(o->id).K("t").U(parent ? parent->id : 0).K("m").Str(method).K("a").Arr();
  return w;
}
// A call on a JS object: `target.method(args)`.
Writer& Call(const Obj* target, const char* method) {
  Writer& w = Cmd("call");
  w.K("t").U(target ? target->id : 0).K("m").Str(method).K("a").Arr();
  return w;
}
void EndArgs() {
  g_w.EndArr();
  EndCmd();
}

void SetLabel(const Obj* o, WGPUStringView label) {
  if (!o) return;
  Writer& w = Cmd("set");
  w.K("t").U(o->id).K("k").Str("label").K("v").Str(Sv(label));
  EndCmd();
}

void Drop(Obj* o) {
  if (!o) return;
  Writer& w = Cmd("drop");
  w.K("t").U(o->id);
  EndCmd();
  delete o;
}

// ---------------------------------------------------------------------
// Futures

struct Queued {
  uint64_t id;
  WGPUCallbackMode mode;
  std::function<void()> fire;
};

uint64_t g_next_future = 1;
std::deque<Queued> g_queued;
std::set<uint64_t> g_open_futures;  // futures that have not completed

WGPUFuture Complete(WGPUCallbackMode mode, bool has_callback, std::function<void()> fire) {
  uint64_t id = g_next_future++;
  if (!has_callback) return WGPUFuture{id};
  if (mode == WGPUCallbackMode_AllowSpontaneous) fire();
  else g_queued.push_back(Queued{id, mode, std::move(fire)});
  return WGPUFuture{id};
}

void FireLost(WGPUDeviceImpl* d, WGPUDeviceLostReason reason, const std::string& msg) {
  if (d->lost_fired) return;
  d->lost_fired = true;
  g_open_futures.erase(d->lost_future);
  if (!d->lost.callback) return;
  WGPUDeviceLostCallbackInfo cb = d->lost;
  WGPUDevice dev = d;
  auto fire = [cb, dev, reason, msg]() {
    WGPUDevice copy = dev;
    cb.callback(&copy, reason, View(msg), cb.userdata1, cb.userdata2);
  };
  if (cb.mode == WGPUCallbackMode_AllowSpontaneous) fire();
  else g_queued.push_back(Queued{d->lost_future, cb.mode, fire});
}

// ---------------------------------------------------------------------
// Descriptors

void WriteExtent(Writer& w, const WGPUExtent3D& e) {
  w.Obj().K("width").U(e.width).K("height").U(e.height).K("depthOrArrayLayers").U(e.depthOrArrayLayers).End();
}

void WriteCopyTexture(Writer& w, const WGPUTexelCopyTextureInfo* t) {
  w.Obj().K("texture").Ref(t->texture).K("mipLevel").U(t->mipLevel);
  w.K("origin").Obj().K("x").U(t->origin.x).K("y").U(t->origin.y).K("z").U(t->origin.z).End();
  w.KS("aspect", AspectName(t->aspect)).End();
}

void WriteLayout(Writer& w, const WGPUTexelCopyBufferLayout& l, const WGPUBuffer buffer) {
  w.Obj();
  if (buffer) w.K("buffer").Ref(buffer);
  w.K("offset").U(l.offset);
  if (l.bytesPerRow != WGPU_COPY_STRIDE_UNDEFINED) w.K("bytesPerRow").U(l.bytesPerRow);
  if (l.rowsPerImage != WGPU_COPY_STRIDE_UNDEFINED) w.K("rowsPerImage").U(l.rowsPerImage);
  w.End();
}

void WriteConstants(Writer& w, size_t n, const WGPUConstantEntry* c) {
  if (n == 0) return;
  w.K("constants").Obj();
  for (size_t i = 0; i < n; ++i) w.K(std::string(Sv(c[i].key)).c_str()).D(c[i].value);
  w.End();
}

void WriteStage(Writer& w, WGPUShaderModule module, WGPUStringView entry, size_t nconst,
                const WGPUConstantEntry* constants) {
  w.K("module").Ref(module);
  std::string_view e = Sv(entry);
  if (!e.empty()) w.K("entryPoint").Str(e);
  WriteConstants(w, nconst, constants);
}

void WriteTimestamps(Writer& w, const WGPUPassTimestampWrites* t) {
  if (!t) return;
  w.K("timestampWrites").Obj().K("querySet").Ref(t->querySet);
  if (t->beginningOfPassWriteIndex != WGPU_QUERY_SET_INDEX_UNDEFINED)
    w.K("beginningOfPassWriteIndex").U(t->beginningOfPassWriteIndex);
  if (t->endOfPassWriteIndex != WGPU_QUERY_SET_INDEX_UNDEFINED)
    w.K("endOfPassWriteIndex").U(t->endOfPassWriteIndex);
  w.End();
}

void WriteStencilFace(Writer& w, const char* k, const WGPUStencilFaceState& s) {
  w.K(k).Obj().KS("compare", CompareFunctionName(s.compare)).KS("failOp", StencilOperationName(s.failOp));
  w.KS("depthFailOp", StencilOperationName(s.depthFailOp)).KS("passOp", StencilOperationName(s.passOp)).End();
}

void WriteBlendComponent(Writer& w, const char* k, const WGPUBlendComponent& c) {
  w.K(k).Obj().KS("operation", BlendOperationName(c.operation));
  w.KS("srcFactor", BlendFactorName(c.srcFactor)).KS("dstFactor", BlendFactorName(c.dstFactor)).End();
}

void WriteRenderPipeline(Writer& w, const WGPURenderPipelineDescriptor* d) {
  w.Obj().KLabel(d->label);
  if (d->layout) w.K("layout").Ref(d->layout);
  else w.K("layout").Str("auto");
  w.K("vertex").Obj();
  WriteStage(w, d->vertex.module, d->vertex.entryPoint, d->vertex.constantCount, d->vertex.constants);
  if (d->vertex.bufferCount) {
    w.K("buffers").Arr();
    for (size_t i = 0; i < d->vertex.bufferCount; ++i) {
      const WGPUVertexBufferLayout& b = d->vertex.buffers[i];
      if (b.attributeCount == 0 && b.stepMode == WGPUVertexStepMode_Undefined) {
        w.Null();
        continue;
      }
      w.Obj().K("arrayStride").U(b.arrayStride).KS("stepMode", StepModeName(b.stepMode));
      w.K("attributes").Arr();
      for (size_t j = 0; j < b.attributeCount; ++j) {
        const WGPUVertexAttribute& a = b.attributes[j];
        w.Obj().KS("format", VertexFormatName(a.format)).K("offset").U(a.offset);
        w.K("shaderLocation").U(a.shaderLocation).End();
      }
      w.EndArr().End();
    }
    w.EndArr();
  }
  w.End();
  const WGPUPrimitiveState& p = d->primitive;
  w.K("primitive").Obj().KS("topology", TopologyName(p.topology));
  w.KS("stripIndexFormat", IndexFormatName(p.stripIndexFormat)).KS("frontFace", FrontFaceName(p.frontFace));
  w.KS("cullMode", CullModeName(p.cullMode));
  if (p.unclippedDepth) w.K("unclippedDepth").B(true);
  w.End();
  if (const WGPUDepthStencilState* ds = d->depthStencil) {
    w.K("depthStencil").Obj().KS("format", TextureFormatName(ds->format));
    if (ds->depthWriteEnabled != WGPUOptionalBool_Undefined)
      w.K("depthWriteEnabled").B(ds->depthWriteEnabled == WGPUOptionalBool_True);
    w.KS("depthCompare", CompareFunctionName(ds->depthCompare));
    WriteStencilFace(w, "stencilFront", ds->stencilFront);
    WriteStencilFace(w, "stencilBack", ds->stencilBack);
    w.K("stencilReadMask").U(ds->stencilReadMask).K("stencilWriteMask").U(ds->stencilWriteMask);
    w.K("depthBias").I(ds->depthBias).K("depthBiasSlopeScale").D(ds->depthBiasSlopeScale);
    w.K("depthBiasClamp").D(ds->depthBiasClamp).End();
  }
  const WGPUMultisampleState& m = d->multisample;
  w.K("multisample").Obj().K("count").U(m.count ? m.count : 1).K("mask").U(m.mask);
  w.K("alphaToCoverageEnabled").B(m.alphaToCoverageEnabled != 0).End();
  if (const WGPUFragmentState* f = d->fragment) {
    w.K("fragment").Obj();
    WriteStage(w, f->module, f->entryPoint, f->constantCount, f->constants);
    w.K("targets").Arr();
    for (size_t i = 0; i < f->targetCount; ++i) {
      const WGPUColorTargetState& t = f->targets[i];
      if (t.format == WGPUTextureFormat_Undefined) {
        w.Null();
        continue;
      }
      w.Obj().KS("format", TextureFormatName(t.format)).K("writeMask").U(t.writeMask);
      if (t.blend) {
        w.K("blend").Obj();
        WriteBlendComponent(w, "color", t.blend->color);
        WriteBlendComponent(w, "alpha", t.blend->alpha);
        w.End();
      }
      w.End();
    }
    w.EndArr().End();
  }
  w.End();
}

void WriteComputePipeline(Writer& w, const WGPUComputePipelineDescriptor* d) {
  w.Obj().KLabel(d->label);
  if (d->layout) w.K("layout").Ref(d->layout);
  else w.K("layout").Str("auto");
  w.K("compute").Obj();
  WriteStage(w, d->compute.module, d->compute.entryPoint, d->compute.constantCount, d->compute.constants);
  w.End().End();
}

void WriteRenderPass(Writer& w, const WGPURenderPassDescriptor* d) {
  w.Obj().KLabel(d->label).K("colorAttachments").Arr();
  for (size_t i = 0; i < d->colorAttachmentCount; ++i) {
    const WGPURenderPassColorAttachment& c = d->colorAttachments[i];
    if (!c.view) {
      w.Null();
      continue;
    }
    w.Obj().K("view").Ref(c.view);
    if (c.depthSlice != WGPU_DEPTH_SLICE_UNDEFINED) w.K("depthSlice").U(c.depthSlice);
    if (c.resolveTarget) w.K("resolveTarget").Ref(c.resolveTarget);
    w.KS("loadOp", LoadOpName(c.loadOp)).KS("storeOp", StoreOpName(c.storeOp));
    w.K("clearValue").Obj().K("r").D(c.clearValue.r).K("g").D(c.clearValue.g);
    w.K("b").D(c.clearValue.b).K("a").D(c.clearValue.a).End().End();
  }
  w.EndArr();
  if (const WGPURenderPassDepthStencilAttachment* ds = d->depthStencilAttachment) {
    w.K("depthStencilAttachment").Obj().K("view").Ref(ds->view);
    w.KS("depthLoadOp", LoadOpName(ds->depthLoadOp)).KS("depthStoreOp", StoreOpName(ds->depthStoreOp));
    if (!std::isnan(ds->depthClearValue)) w.K("depthClearValue").D(ds->depthClearValue);
    w.K("depthReadOnly").B(ds->depthReadOnly != 0);
    w.KS("stencilLoadOp", LoadOpName(ds->stencilLoadOp)).KS("stencilStoreOp", StoreOpName(ds->stencilStoreOp));
    w.K("stencilClearValue").U(ds->stencilClearValue).K("stencilReadOnly").B(ds->stencilReadOnly != 0);
    w.End();
  }
  if (d->occlusionQuerySet) w.K("occlusionQuerySet").Ref(d->occlusionQuerySet);
  WriteTimestamps(w, d->timestampWrites);
  for (const WGPUChainedStruct* c = d->nextInChain; c; c = c->next)
    if (c->sType == WGPUSType_RenderPassMaxDrawCount)
      w.K("maxDrawCount").U(reinterpret_cast<const WGPURenderPassMaxDrawCount*>(c)->maxDrawCount);
  w.End();
}

void WriteBindGroupLayout(Writer& w, const WGPUBindGroupLayoutDescriptor* d) {
  w.Obj().KLabel(d->label).K("entries").Arr();
  for (size_t i = 0; i < d->entryCount; ++i) {
    const WGPUBindGroupLayoutEntry& e = d->entries[i];
    w.Obj().K("binding").U(e.binding).K("visibility").U(e.visibility);
    if (e.buffer.type != WGPUBufferBindingType_BindingNotUsed) {
      w.K("buffer").Obj().KS("type", BufferBindingTypeName(e.buffer.type));
      w.K("hasDynamicOffset").B(e.buffer.hasDynamicOffset != 0).K("minBindingSize").U(e.buffer.minBindingSize);
      w.End();
    }
    if (e.sampler.type != WGPUSamplerBindingType_BindingNotUsed)
      w.K("sampler").Obj().KS("type", SamplerBindingTypeName(e.sampler.type)).End();
    if (e.texture.sampleType != WGPUTextureSampleType_BindingNotUsed) {
      w.K("texture").Obj().KS("sampleType", SampleTypeName(e.texture.sampleType));
      w.KS("viewDimension", ViewDimensionName(e.texture.viewDimension));
      w.K("multisampled").B(e.texture.multisampled != 0).End();
    }
    if (e.storageTexture.access != WGPUStorageTextureAccess_BindingNotUsed) {
      w.K("storageTexture").Obj().KS("access", StorageAccessName(e.storageTexture.access));
      w.KS("format", TextureFormatName(e.storageTexture.format));
      w.KS("viewDimension", ViewDimensionName(e.storageTexture.viewDimension)).End();
    }
    w.End();
  }
  w.EndArr().End();
}

void WriteBindGroup(Writer& w, const WGPUBindGroupDescriptor* d) {
  w.Obj().KLabel(d->label).K("layout").Ref(d->layout).K("entries").Arr();
  for (size_t i = 0; i < d->entryCount; ++i) {
    const WGPUBindGroupEntry& e = d->entries[i];
    w.Obj().K("binding").U(e.binding).K("resource");
    if (e.buffer) {
      w.Obj().K("buffer").Ref(e.buffer).K("offset").U(e.offset);
      if (e.size != WGPU_WHOLE_SIZE) w.K("size").U(e.size);
      w.End();
    } else if (e.sampler) {
      w.Ref(e.sampler);
    } else {
      w.Ref(e.textureView);
    }
    w.End();
  }
  w.EndArr().End();
}

void WriteTexture(Writer& w, const WGPUTextureDescriptor* d) {
  w.Obj().KLabel(d->label).K("size");
  WriteExtent(w, d->size);
  w.K("mipLevelCount").U(d->mipLevelCount ? d->mipLevelCount : 1).K("sampleCount").U(d->sampleCount ? d->sampleCount : 1);
  w.KS("dimension", DimensionName(d->dimension)).KS("format", TextureFormatName(d->format)).K("usage").U(d->usage);
  if (d->viewFormatCount) {
    w.K("viewFormats").Arr();
    for (size_t i = 0; i < d->viewFormatCount; ++i) w.Str(TextureFormatName(d->viewFormats[i]) ? TextureFormatName(d->viewFormats[i]) : "");
    w.EndArr();
  }
  w.End();
}

void WriteTextureView(Writer& w, const WGPUTextureViewDescriptor* d) {
  w.Obj();
  if (d) {
    w.KLabel(d->label).KS("format", TextureFormatName(d->format)).KS("dimension", ViewDimensionName(d->dimension));
    w.KS("aspect", AspectName(d->aspect)).K("baseMipLevel").U(d->baseMipLevel);
    if (d->mipLevelCount != WGPU_MIP_LEVEL_COUNT_UNDEFINED) w.K("mipLevelCount").U(d->mipLevelCount);
    w.K("baseArrayLayer").U(d->baseArrayLayer);
    if (d->arrayLayerCount != WGPU_ARRAY_LAYER_COUNT_UNDEFINED) w.K("arrayLayerCount").U(d->arrayLayerCount);
    if (d->usage) w.K("usage").U(d->usage);
  }
  w.End();
}

void WriteSampler(Writer& w, const WGPUSamplerDescriptor* d) {
  w.Obj();
  if (d) {
    w.KLabel(d->label).KS("addressModeU", AddressModeName(d->addressModeU));
    w.KS("addressModeV", AddressModeName(d->addressModeV)).KS("addressModeW", AddressModeName(d->addressModeW));
    w.KS("magFilter", FilterModeName(d->magFilter)).KS("minFilter", FilterModeName(d->minFilter));
    w.KS("mipmapFilter", FilterModeName(d->mipmapFilter)).K("lodMinClamp").D(d->lodMinClamp);
    w.K("lodMaxClamp").D(d->lodMaxClamp).KS("compare", CompareFunctionName(d->compare));
    w.K("maxAnisotropy").U(d->maxAnisotropy ? d->maxAnisotropy : 1);
  }
  w.End();
}

void WriteSetBindGroup(const Obj* pass, uint32_t index, WGPUBindGroup group, size_t n, const uint32_t* offs) {
  Writer& w = Call(pass, "setBindGroup");
  w.U(index).Ref(group).Arr();
  for (size_t i = 0; i < n; ++i) w.U(offs[i]);
  w.EndArr();
  EndArgs();
}

void Marker(const Obj* o, const char* m, WGPUStringView label) {
  Writer& w = Call(o, m);
  w.Str(Sv(label));
  EndArgs();
}

void NoArgs(const Obj* o, const char* m) {
  Call(o, m);
  EndArgs();
}

void SizeOrUndef(Writer& w, uint64_t size) {
  if (size == WGPU_WHOLE_SIZE) w.Undef();
  else w.U(size);
}

// Adapter or device features and limits, from a reply.
void ReadCaps(const Reply& r, std::vector<WGPUFeatureName>* features, WGPULimits* limits) {
  features->clear();
  ClearLimits(limits);
  for (const auto& kv : r.kv) {
    if (kv.first == "feature") {
      WGPUFeatureName f;
      if (FeatureOf(kv.second, &f)) features->push_back(f);
    } else if (kv.first.rfind("limit.", 0) == 0) {
      SetLimit(limits, std::string_view(kv.first).substr(6), std::strtoull(kv.second.c_str(), nullptr, 10));
    }
  }
}

void CopyLimits(const WGPULimits& from, WGPULimits* to) {
  WGPUChainedStruct* next = to->nextInChain;
  *to = from;
  to->nextInChain = next;
}

void CopyFeatures(const std::vector<WGPUFeatureName>& from, WGPUSupportedFeatures* to) {
  to->featureCount = from.size();
  WGPUFeatureName* p = nullptr;
  if (!from.empty()) {
    p = static_cast<WGPUFeatureName*>(std::malloc(from.size() * sizeof(WGPUFeatureName)));
    std::memcpy(p, from.data(), from.size() * sizeof(WGPUFeatureName));
  }
  to->features = p;
}

WGPUStringView Owned(const std::string& s) {
  char* p = static_cast<char*>(std::malloc(s.size() + 1));
  std::memcpy(p, s.data(), s.size());
  p[s.size()] = 0;
  return WGPUStringView{p, s.size()};
}

void FillInfo(const WGPUAdapterImpl* a, WGPUAdapterInfo* info) {
  WGPUChainedStruct* next = info->nextInChain;
  *info = WGPUAdapterInfo{};
  info->nextInChain = next;
  info->vendor = Owned(a->vendor);
  info->architecture = Owned(a->architecture);
  info->device = Owned(a->device);
  info->description = Owned(a->description);
  info->backendType = WGPUBackendType_WebGPU;
  info->adapterType = a->fallback ? WGPUAdapterType_CPU : WGPUAdapterType_Unknown;
  info->subgroupMinSize = a->subgroup_min;
  info->subgroupMaxSize = a->subgroup_max;
}

bool HasFeature(const std::vector<WGPUFeatureName>& fs, WGPUFeatureName f) {
  for (WGPUFeatureName x : fs)
    if (x == f) return true;
  return false;
}

std::vector<WGPUWGSLLanguageFeatureName> g_wgsl;
bool g_wgsl_known = false;

void LoadWgslFeatures() {
  if (g_wgsl_known) return;
  Cmd("wgsl");
  EndCmd();
  Reply r = Send();
  if (!r.answered) return;
  g_wgsl_known = true;
  for (const auto& kv : r.kv) {
    if (kv.first != "wgsl") continue;
    for (const WgslRow& row : kWgslFeatures)
      if (kv.second == row.name) g_wgsl.push_back(row.value);
  }
}

// Reads [offset, offset+size) of a buffer the worker has mapped.
bool PullMapped(WGPUBufferImpl* b, const Reply& first, size_t size, std::string* err) {
  b->shadow.assign(size, 0);
  size_t got = first.bin.size() < size ? first.bin.size() : size;
  if (got) std::memcpy(b->shadow.data(), first.bin.data(), got);
  while (got < size) {
    size_t want = size - got;
    if (want > 192 * 1024) want = 192 * 1024;
    Writer& w = Cmd("pull");
    w.K("t").U(b->id).K("from").U(got).K("len").U(want);
    EndCmd();
    Reply r = Send();
    if (!r.Ok() || r.bin.empty()) {
      *err = r.Ok() ? "vgpu: short read of a mapped range" : r.Error();
      return false;
    }
    size_t n = r.bin.size() < want ? r.bin.size() : want;
    std::memcpy(b->shadow.data() + got, r.bin.data(), n);
    got += n;
  }
  return true;
}

}  // namespace

// =====================================================================
// webgpu.h

#define VGPU_REFS(Type)                                            \
  void wgpu##Type##AddRef(WGPU##Type o) {                          \
    if (o) ++o->refs;                                              \
  }                                                                \
  void wgpu##Type##Release(WGPU##Type o) {                         \
    if (o && --o->refs == 0) Drop(o);                              \
  }
#define VGPU_LABEL(Type)                                           \
  void wgpu##Type##SetLabel(WGPU##Type o, WGPUStringView label) {  \
    SetLabel(o, label);                                            \
  }

// webgpu.h declares all of these extern "C".

// ---- free functions

void wgpuAdapterInfoFreeMembers(WGPUAdapterInfo value) {
  std::free(const_cast<char*>(value.vendor.data));
  std::free(const_cast<char*>(value.architecture.data));
  std::free(const_cast<char*>(value.device.data));
  std::free(const_cast<char*>(value.description.data));
}

WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor*) { return Make<WGPUInstanceImpl>(); }

WGPUStatus wgpuGetInstanceCapabilities(WGPUInstanceCapabilities* caps) {
  if (!caps) return WGPUStatus_Error;
  // Everything completes before the call that starts it returns, so a
  // timed wait never has to wait.
  caps->timedWaitAnyEnable = 1;
  caps->timedWaitAnyMaxCount = 64;
  return WGPUStatus_Success;
}

WGPUProc wgpuGetProcAddress(WGPUStringView) { return nullptr; }

void wgpuSupportedFeaturesFreeMembers(WGPUSupportedFeatures value) {
  std::free(const_cast<WGPUFeatureName*>(value.features));
}

void wgpuSupportedWGSLLanguageFeaturesFreeMembers(WGPUSupportedWGSLLanguageFeatures value) {
  std::free(const_cast<WGPUWGSLLanguageFeatureName*>(value.features));
}

// Surface capabilities point at static tables.
void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities) {}

// ---- Adapter

void wgpuAdapterGetFeatures(WGPUAdapter a, WGPUSupportedFeatures* features) {
  if (a && features) CopyFeatures(a->features, features);
}

WGPUStatus wgpuAdapterGetInfo(WGPUAdapter a, WGPUAdapterInfo* info) {
  if (!a || !info) return WGPUStatus_Error;
  FillInfo(a, info);
  return WGPUStatus_Success;
}

WGPUStatus wgpuAdapterGetLimits(WGPUAdapter a, WGPULimits* limits) {
  if (!a || !limits) return WGPUStatus_Error;
  CopyLimits(a->limits, limits);
  return WGPUStatus_Success;
}

WGPUBool wgpuAdapterHasFeature(WGPUAdapter a, WGPUFeatureName f) { return a && HasFeature(a->features, f); }

WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter a, const WGPUDeviceDescriptor* desc,
                                    WGPURequestDeviceCallbackInfo cb) {
  WGPUDeviceImpl* d = Make<WGPUDeviceImpl>();
  WGPUQueueImpl* q = Make<WGPUQueueImpl>();
  d->adapter = a;
  d->queue = q;
  q->device = d;
  if (desc) {
    d->lost = desc->deviceLostCallbackInfo;
    d->uncaptured = desc->uncapturedErrorCallbackInfo;
  }
  Writer& w = Cmd("device");
  w.K("id").U(d->id).K("t").U(a ? a->id : 0).K("q").U(q->id).K("a").Arr().Obj();
  if (desc) {
    w.KLabel(desc->label);
    if (desc->requiredFeatureCount) {
      w.K("requiredFeatures").Arr();
      for (size_t i = 0; i < desc->requiredFeatureCount; ++i) {
        const char* n = FeatureNameOf(desc->requiredFeatures[i]);
        if (n) w.Str(n);
      }
      w.EndArr();
    }
    if (const WGPULimits* l = desc->requiredLimits) {
      w.K("requiredLimits").Obj();
      for (const LimitRow& r : kLimits) {
        const char* p = reinterpret_cast<const char*>(l) + r.offset;
        uint64_t v = 0;
        if (r.wide) {
          std::memcpy(&v, p, 8);
          if (v == UINT64_MAX) continue;
        } else {
          uint32_t n = 0;
          std::memcpy(&n, p, 4);
          if (n == UINT32_MAX) continue;
          v = n;
        }
        w.K(r.name).U(v);
      }
      w.End();
    }
    std::string_view ql = Sv(desc->defaultQueue.label);
    if (!ql.empty()) w.K("defaultQueue").Obj().K("label").Str(ql).End();
  }
  w.End().EndArr();
  EndCmd();
  Reply r = Send();

  WGPURequestDeviceStatus status = WGPURequestDeviceStatus_Success;
  std::string msg;
  if (!r.Ok()) {
    status = WGPURequestDeviceStatus_Error;
    msg = r.Error();
    delete q;
    delete d;
    d = nullptr;
  } else {
    ReadCaps(r, &d->features, &d->limits);
    d->lost_future = g_next_future++;
    g_open_futures.insert(d->lost_future);
    g_devices[d->id] = d;
  }
  WGPUDevice out = d;
  return Complete(cb.mode, cb.callback != nullptr, [cb, status, out, msg]() {
    cb.callback(status, out, View(msg), cb.userdata1, cb.userdata2);
  });
}

VGPU_REFS(Adapter)

// ---- BindGroup / BindGroupLayout

VGPU_LABEL(BindGroup)
VGPU_REFS(BindGroup)
VGPU_LABEL(BindGroupLayout)
VGPU_REFS(BindGroupLayout)

// ---- Buffer

void wgpuBufferDestroy(WGPUBuffer b) {
  if (!b) return;
  b->shadow.clear();
  b->state = WGPUBufferMapState_Unmapped;
  NoArgs(b, "destroy");
}

const void* wgpuBufferGetConstMappedRange(WGPUBuffer b, size_t offset, size_t size) {
  if (!b || b->state != WGPUBufferMapState_Mapped) return nullptr;
  if (size == WGPU_WHOLE_MAP_SIZE) size = b->map_offset + b->map_size > offset ? b->map_offset + b->map_size - offset : 0;
  if (offset < b->map_offset || offset + size > b->map_offset + b->map_size) return nullptr;
  return b->shadow.data() + (offset - b->map_offset);
}

void* wgpuBufferGetMappedRange(WGPUBuffer b, size_t offset, size_t size) {
  if (!b || !b->write) return nullptr;
  return const_cast<void*>(wgpuBufferGetConstMappedRange(b, offset, size));
}

WGPUBufferMapState wgpuBufferGetMapState(WGPUBuffer b) { return b ? b->state : WGPUBufferMapState_Unmapped; }
uint64_t wgpuBufferGetSize(WGPUBuffer b) { return b ? b->size : 0; }
WGPUBufferUsage wgpuBufferGetUsage(WGPUBuffer b) { return b ? b->usage : 0; }

WGPUFuture wgpuBufferMapAsync(WGPUBuffer b, WGPUMapMode mode, size_t offset, size_t size,
                              WGPUBufferMapCallbackInfo cb) {
  WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Success;
  std::string msg;
  if (!b) {
    status = WGPUMapAsyncStatus_Error;
    msg = "vgpu: no buffer";
  } else if (b->state != WGPUBufferMapState_Unmapped) {
    status = WGPUMapAsyncStatus_Error;
    msg = "vgpu: the buffer is already mapped";
  } else {
    if (size == WGPU_WHOLE_MAP_SIZE) size = b->size > offset ? static_cast<size_t>(b->size - offset) : 0;
    Writer& w = Cmd("map");
    w.K("t").U(b->id).K("mode").U(mode).K("off").U(offset).K("size").U(size);
    EndCmd();
    Reply r = Send();
    if (!r.Ok()) {
      status = WGPUMapAsyncStatus_Error;
      msg = r.Error();
    } else if (!PullMapped(b, r, size, &msg)) {
      status = WGPUMapAsyncStatus_Error;
      b->shadow.clear();
    } else {
      b->state = WGPUBufferMapState_Mapped;
      b->write = (mode & WGPUMapMode_Write) != 0;
      b->map_offset = offset;
      b->map_size = size;
    }
  }
  return Complete(cb.mode, cb.callback != nullptr, [cb, status, msg]() {
    cb.callback(status, View(msg), cb.userdata1, cb.userdata2);
  });
}

WGPUStatus wgpuBufferReadMappedRange(WGPUBuffer b, size_t offset, void* data, size_t size) {
  const void* p = wgpuBufferGetConstMappedRange(b, offset, size);
  if (!p) return WGPUStatus_Error;
  std::memcpy(data, p, size);
  return WGPUStatus_Success;
}

WGPUStatus wgpuBufferWriteMappedRange(WGPUBuffer b, size_t offset, const void* data, size_t size) {
  void* p = wgpuBufferGetMappedRange(b, offset, size);
  if (!p) return WGPUStatus_Error;
  std::memcpy(p, data, size);
  return WGPUStatus_Success;
}

VGPU_LABEL(Buffer)

void wgpuBufferUnmap(WGPUBuffer b) {
  if (!b || b->state != WGPUBufferMapState_Mapped) return;
  Writer& w = Cmd("unmap");
  w.K("t").U(b->id);
  if (b->write) {
    size_t off = Blob(b->shadow.data(), b->shadow.size());
    w.K("w").Bytes(off, b->shadow.size());
  }
  EndCmd();
  b->state = WGPUBufferMapState_Unmapped;
  b->write = false;
  b->shadow.clear();
  b->shadow.shrink_to_fit();
}

VGPU_REFS(Buffer)

// ---- CommandBuffer

VGPU_LABEL(CommandBuffer)
VGPU_REFS(CommandBuffer)

// ---- CommandEncoder

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder e,
                                                          const WGPUComputePassDescriptor* d) {
  WGPUComputePassEncoderImpl* p = Make<WGPUComputePassEncoderImpl>();
  Writer& w = New(p, e, "beginComputePass");
  w.Obj();
  if (d) {
    w.KLabel(d->label);
    WriteTimestamps(w, d->timestampWrites);
  }
  w.End();
  EndArgs();
  return p;
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder e,
                                                        const WGPURenderPassDescriptor* d) {
  WGPURenderPassEncoderImpl* p = Make<WGPURenderPassEncoderImpl>();
  Writer& w = New(p, e, "beginRenderPass");
  WriteRenderPass(w, d);
  EndArgs();
  return p;
}

void wgpuCommandEncoderClearBuffer(WGPUCommandEncoder e, WGPUBuffer b, uint64_t offset, uint64_t size) {
  Writer& w = Call(e, "clearBuffer");
  w.Ref(b).U(offset);
  SizeOrUndef(w, size);
  EndArgs();
}

void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder e, WGPUBuffer src, uint64_t src_off,
                                          WGPUBuffer dst, uint64_t dst_off, uint64_t size) {
  Writer& w = Call(e, "copyBufferToBuffer");
  w.Ref(src).U(src_off).Ref(dst).U(dst_off);
  SizeOrUndef(w, size);
  EndArgs();
}

void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder e, const WGPUTexelCopyBufferInfo* src,
                                           const WGPUTexelCopyTextureInfo* dst, const WGPUExtent3D* size) {
  Writer& w = Call(e, "copyBufferToTexture");
  WriteLayout(w, src->layout, src->buffer);
  WriteCopyTexture(w, dst);
  WriteExtent(w, *size);
  EndArgs();
}

void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder e, const WGPUTexelCopyTextureInfo* src,
                                           const WGPUTexelCopyBufferInfo* dst, const WGPUExtent3D* size) {
  Writer& w = Call(e, "copyTextureToBuffer");
  WriteCopyTexture(w, src);
  WriteLayout(w, dst->layout, dst->buffer);
  WriteExtent(w, *size);
  EndArgs();
}

void wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder e, const WGPUTexelCopyTextureInfo* src,
                                            const WGPUTexelCopyTextureInfo* dst, const WGPUExtent3D* size) {
  Writer& w = Call(e, "copyTextureToTexture");
  WriteCopyTexture(w, src);
  WriteCopyTexture(w, dst);
  WriteExtent(w, *size);
  EndArgs();
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder e, const WGPUCommandBufferDescriptor* d) {
  WGPUCommandBufferImpl* c = Make<WGPUCommandBufferImpl>();
  Writer& w = New(c, e, "finish");
  w.Obj();
  if (d) w.KLabel(d->label);
  w.End();
  EndArgs();
  return c;
}

void wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder e, WGPUStringView l) { Marker(e, "insertDebugMarker", l); }
void wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder e) { NoArgs(e, "popDebugGroup"); }
void wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder e, WGPUStringView l) { Marker(e, "pushDebugGroup", l); }

void wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder e, WGPUQuerySet qs, uint32_t first, uint32_t count,
                                       WGPUBuffer dst, uint64_t dst_off) {
  Writer& w = Call(e, "resolveQuerySet");
  w.Ref(qs).U(first).U(count).Ref(dst).U(dst_off);
  EndArgs();
}

VGPU_LABEL(CommandEncoder)

void wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder e, WGPUQuerySet qs, uint32_t index) {
  Writer& w = Call(e, "writeTimestamp");
  w.Ref(qs).U(index);
  EndArgs();
}

VGPU_REFS(CommandEncoder)

// ---- ComputePassEncoder

void wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder p, uint32_t x, uint32_t y, uint32_t z) {
  Writer& w = Call(p, "dispatchWorkgroups");
  w.U(x).U(y).U(z);
  EndArgs();
}

void wgpuComputePassEncoderDispatchWorkgroupsIndirect(WGPUComputePassEncoder p, WGPUBuffer b, uint64_t off) {
  Writer& w = Call(p, "dispatchWorkgroupsIndirect");
  w.Ref(b).U(off);
  EndArgs();
}

void wgpuComputePassEncoderEnd(WGPUComputePassEncoder p) { NoArgs(p, "end"); }
void wgpuComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder p, WGPUStringView l) { Marker(p, "insertDebugMarker", l); }
void wgpuComputePassEncoderPopDebugGroup(WGPUComputePassEncoder p) { NoArgs(p, "popDebugGroup"); }
void wgpuComputePassEncoderPushDebugGroup(WGPUComputePassEncoder p, WGPUStringView l) { Marker(p, "pushDebugGroup", l); }

void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder p, uint32_t i, WGPUBindGroup g, size_t n,
                                        const uint32_t* offs) {
  WriteSetBindGroup(p, i, g, n, offs);
}

VGPU_LABEL(ComputePassEncoder)

void wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder p, WGPUComputePipeline pipe) {
  Writer& w = Call(p, "setPipeline");
  w.Ref(pipe);
  EndArgs();
}

void wgpuComputePassEncoderWriteTimestamp(WGPUComputePassEncoder p, WGPUQuerySet qs, uint32_t index) {
  Writer& w = Call(p, "writeTimestamp");
  w.Ref(qs).U(index);
  EndArgs();
}

VGPU_REFS(ComputePassEncoder)

// ---- ComputePipeline

WGPUBindGroupLayout wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline p, uint32_t index) {
  WGPUBindGroupLayoutImpl* l = Make<WGPUBindGroupLayoutImpl>();
  Writer& w = New(l, p, "getBindGroupLayout");
  w.U(index);
  EndArgs();
  return l;
}

VGPU_LABEL(ComputePipeline)
VGPU_REFS(ComputePipeline)

// ---- Device

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice d, const WGPUBindGroupDescriptor* desc) {
  WGPUBindGroupImpl* o = Make<WGPUBindGroupImpl>();
  Writer& w = New(o, d, "createBindGroup");
  WriteBindGroup(w, desc);
  EndArgs();
  return o;
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice d, const WGPUBindGroupLayoutDescriptor* desc) {
  WGPUBindGroupLayoutImpl* o = Make<WGPUBindGroupLayoutImpl>();
  Writer& w = New(o, d, "createBindGroupLayout");
  WriteBindGroupLayout(w, desc);
  EndArgs();
  return o;
}

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice d, const WGPUBufferDescriptor* desc) {
  WGPUBufferImpl* b = Make<WGPUBufferImpl>();
  b->device = d;
  b->size = desc->size;
  b->usage = desc->usage;
  Writer& w = New(b, d, "createBuffer");
  w.Obj().KLabel(desc->label).K("size").U(desc->size).K("usage").U(desc->usage);
  w.K("mappedAtCreation").B(desc->mappedAtCreation != 0).End();
  EndArgs();
  if (desc->mappedAtCreation) {
    b->state = WGPUBufferMapState_Mapped;
    b->write = true;
    b->map_offset = 0;
    b->map_size = static_cast<size_t>(desc->size);
    b->shadow.assign(b->map_size, 0);
  }
  return b;
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice d, const WGPUCommandEncoderDescriptor* desc) {
  WGPUCommandEncoderImpl* o = Make<WGPUCommandEncoderImpl>();
  Writer& w = New(o, d, "createCommandEncoder");
  w.Obj();
  if (desc) w.KLabel(desc->label);
  w.End();
  EndArgs();
  return o;
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice d, const WGPUComputePipelineDescriptor* desc) {
  WGPUComputePipelineImpl* o = Make<WGPUComputePipelineImpl>();
  Writer& w = New(o, d, "createComputePipeline");
  WriteComputePipeline(w, desc);
  EndArgs();
  return o;
}

namespace {

WGPUCreatePipelineAsyncStatus PipelineStatus(const Reply& r) {
  if (!r.answered) return WGPUCreatePipelineAsyncStatus_InternalError;
  if (r.Ok()) return WGPUCreatePipelineAsyncStatus_Success;
  return r.Str("reason") == "internal" ? WGPUCreatePipelineAsyncStatus_InternalError
                                       : WGPUCreatePipelineAsyncStatus_ValidationError;
}

}  // namespace

WGPUFuture wgpuDeviceCreateComputePipelineAsync(WGPUDevice d, const WGPUComputePipelineDescriptor* desc,
                                                WGPUCreateComputePipelineAsyncCallbackInfo cb) {
  WGPUComputePipelineImpl* o = Make<WGPUComputePipelineImpl>();
  Writer& w = Cmd("newAsync");
  w.K("id").U(o->id).K("t").U(d->id).K("m").Str("createComputePipelineAsync").K("a").Arr();
  WriteComputePipeline(w, desc);
  EndArgs();
  Reply r = Send();
  WGPUCreatePipelineAsyncStatus status = PipelineStatus(r);
  std::string msg = r.Ok() ? std::string() : r.Error();
  WGPUComputePipeline out = o;
  if (status != WGPUCreatePipelineAsyncStatus_Success) {
    delete o;
    out = nullptr;
  }
  return Complete(cb.mode, cb.callback != nullptr, [cb, status, out, msg]() {
    cb.callback(status, out, View(msg), cb.userdata1, cb.userdata2);
  });
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice d, const WGPUPipelineLayoutDescriptor* desc) {
  WGPUPipelineLayoutImpl* o = Make<WGPUPipelineLayoutImpl>();
  Writer& w = New(o, d, "createPipelineLayout");
  w.Obj().KLabel(desc->label).K("bindGroupLayouts").Arr();
  for (size_t i = 0; i < desc->bindGroupLayoutCount; ++i) w.Ref(desc->bindGroupLayouts[i]);
  w.EndArr().End();
  EndArgs();
  return o;
}

WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice d, const WGPUQuerySetDescriptor* desc) {
  WGPUQuerySetImpl* o = Make<WGPUQuerySetImpl>();
  o->type = desc->type;
  o->count = desc->count;
  Writer& w = New(o, d, "createQuerySet");
  w.Obj().KLabel(desc->label).KS("type", QueryTypeName(desc->type)).K("count").U(desc->count).End();
  EndArgs();
  return o;
}

WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice d,
                                                            const WGPURenderBundleEncoderDescriptor* desc) {
  WGPURenderBundleEncoderImpl* o = Make<WGPURenderBundleEncoderImpl>();
  Writer& w = New(o, d, "createRenderBundleEncoder");
  w.Obj().KLabel(desc->label).K("colorFormats").Arr();
  for (size_t i = 0; i < desc->colorFormatCount; ++i) {
    const char* n = TextureFormatName(desc->colorFormats[i]);
    if (n) w.Str(n);
    else w.Null();
  }
  w.EndArr().KS("depthStencilFormat", TextureFormatName(desc->depthStencilFormat));
  w.K("sampleCount").U(desc->sampleCount ? desc->sampleCount : 1);
  w.K("depthReadOnly").B(desc->depthReadOnly != 0).K("stencilReadOnly").B(desc->stencilReadOnly != 0).End();
  EndArgs();
  return o;
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice d, const WGPURenderPipelineDescriptor* desc) {
  WGPURenderPipelineImpl* o = Make<WGPURenderPipelineImpl>();
  Writer& w = New(o, d, "createRenderPipeline");
  WriteRenderPipeline(w, desc);
  EndArgs();
  return o;
}

WGPUFuture wgpuDeviceCreateRenderPipelineAsync(WGPUDevice d, const WGPURenderPipelineDescriptor* desc,
                                               WGPUCreateRenderPipelineAsyncCallbackInfo cb) {
  WGPURenderPipelineImpl* o = Make<WGPURenderPipelineImpl>();
  Writer& w = Cmd("newAsync");
  w.K("id").U(o->id).K("t").U(d->id).K("m").Str("createRenderPipelineAsync").K("a").Arr();
  WriteRenderPipeline(w, desc);
  EndArgs();
  Reply r = Send();
  WGPUCreatePipelineAsyncStatus status = PipelineStatus(r);
  std::string msg = r.Ok() ? std::string() : r.Error();
  WGPURenderPipeline out = o;
  if (status != WGPUCreatePipelineAsyncStatus_Success) {
    delete o;
    out = nullptr;
  }
  return Complete(cb.mode, cb.callback != nullptr, [cb, status, out, msg]() {
    cb.callback(status, out, View(msg), cb.userdata1, cb.userdata2);
  });
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice d, const WGPUSamplerDescriptor* desc) {
  WGPUSamplerImpl* o = Make<WGPUSamplerImpl>();
  Writer& w = New(o, d, "createSampler");
  WriteSampler(w, desc);
  EndArgs();
  return o;
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice d, const WGPUShaderModuleDescriptor* desc) {
  WGPUShaderModuleImpl* o = Make<WGPUShaderModuleImpl>();
  std::string_view code;
  bool spirv = false;
  for (const WGPUChainedStruct* c = desc->nextInChain; c; c = c->next) {
    if (c->sType == WGPUSType_ShaderSourceWGSL) code = Sv(reinterpret_cast<const WGPUShaderSourceWGSL*>(c)->code);
    if (c->sType == WGPUSType_ShaderSourceSPIRV) spirv = true;
  }
  if (code.empty() && spirv)
    std::fprintf(stderr, "vgpu: SPIR-V is not a WebGPU shader language in the browser; use WGSL\n");
  Writer& w = New(o, d, "createShaderModule");
  w.Obj().KLabel(desc->label).K("code").Str(code).End();
  EndArgs();
  return o;
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice d, const WGPUTextureDescriptor* desc) {
  WGPUTextureImpl* t = Make<WGPUTextureImpl>();
  t->width = desc->size.width;
  t->height = desc->size.height;
  t->depth = desc->size.depthOrArrayLayers;
  t->mips = desc->mipLevelCount ? desc->mipLevelCount : 1;
  t->samples = desc->sampleCount ? desc->sampleCount : 1;
  t->dimension = desc->dimension;
  t->format = desc->format;
  t->usage = desc->usage;
  Writer& w = New(t, d, "createTexture");
  WriteTexture(w, desc);
  EndArgs();
  return t;
}

void wgpuDeviceDestroy(WGPUDevice d) {
  if (!d) return;
  NoArgs(d, "destroy");
  Flush();
  FireLost(d, WGPUDeviceLostReason_Destroyed, "vgpu: device destroyed");
}

WGPUStatus wgpuDeviceGetAdapterInfo(WGPUDevice d, WGPUAdapterInfo* info) {
  if (!d || !d->adapter || !info) return WGPUStatus_Error;
  FillInfo(d->adapter, info);
  return WGPUStatus_Success;
}

void wgpuDeviceGetFeatures(WGPUDevice d, WGPUSupportedFeatures* features) {
  if (d && features) CopyFeatures(d->features, features);
}

WGPUStatus wgpuDeviceGetLimits(WGPUDevice d, WGPULimits* limits) {
  if (!d || !limits) return WGPUStatus_Error;
  CopyLimits(d->limits, limits);
  return WGPUStatus_Success;
}

WGPUFuture wgpuDeviceGetLostFuture(WGPUDevice d) { return WGPUFuture{d ? d->lost_future : 0}; }

WGPUQueue wgpuDeviceGetQueue(WGPUDevice d) {
  if (!d || !d->queue) return nullptr;
  ++d->queue->refs;
  return d->queue;
}

WGPUBool wgpuDeviceHasFeature(WGPUDevice d, WGPUFeatureName f) { return d && HasFeature(d->features, f); }

WGPUFuture wgpuDevicePopErrorScope(WGPUDevice d, WGPUPopErrorScopeCallbackInfo cb) {
  Writer& w = Cmd("pop");
  w.K("t").U(d ? d->id : 0);
  EndCmd();
  Reply r = Send();
  WGPUPopErrorScopeStatus status = WGPUPopErrorScopeStatus_Success;
  WGPUErrorType type = WGPUErrorType_NoError;
  std::string msg;
  if (!r.Ok()) {
    status = WGPUPopErrorScopeStatus_Error;
    msg = r.Error();
  } else {
    type = ErrorTypeOf(r.Str("type"));
    msg = r.Str("msg");
  }
  return Complete(cb.mode, cb.callback != nullptr, [cb, status, type, msg]() {
    cb.callback(status, type, View(msg), cb.userdata1, cb.userdata2);
  });
}

void wgpuDevicePushErrorScope(WGPUDevice d, WGPUErrorFilter filter) {
  Writer& w = Call(d, "pushErrorScope");
  w.Str(ErrorFilterName(filter) ? ErrorFilterName(filter) : "validation");
  EndArgs();
}

VGPU_LABEL(Device)

void wgpuDeviceAddRef(WGPUDevice d) {
  if (d) ++d->refs;
}

void wgpuDeviceRelease(WGPUDevice d) {
  if (!d || --d->refs != 0) return;
  g_devices.erase(d->id);
  g_open_futures.erase(d->lost_future);
  if (d->queue) {
    d->queue->device = nullptr;
    wgpuQueueRelease(d->queue);
    d->queue = nullptr;
  }
  Drop(d);
}

// ---- Instance

WGPUSurface wgpuInstanceCreateSurface(WGPUInstance, const WGPUSurfaceDescriptor* desc) {
  std::string_view selector = "#vgpu";
  for (const WGPUChainedStruct* c = desc ? desc->nextInChain : nullptr; c; c = c->next)
    if (c->sType == WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector)
      selector = Sv(reinterpret_cast<const WGPUEmscriptenSurfaceSourceCanvasHTMLSelector*>(c)->selector);
  WGPUSurfaceImpl* s = Make<WGPUSurfaceImpl>();
  Writer& w = Cmd("surface");
  w.K("id").U(s->id).K("sel").Str(selector);
  EndCmd();
  return s;
}

WGPUStatus wgpuInstanceGetWGSLLanguageFeatures(WGPUInstance, WGPUSupportedWGSLLanguageFeatures* features) {
  if (!features) return WGPUStatus_Error;
  LoadWgslFeatures();
  features->featureCount = g_wgsl.size();
  WGPUWGSLLanguageFeatureName* p = nullptr;
  if (!g_wgsl.empty()) {
    p = static_cast<WGPUWGSLLanguageFeatureName*>(std::malloc(g_wgsl.size() * sizeof(*p)));
    std::memcpy(p, g_wgsl.data(), g_wgsl.size() * sizeof(*p));
  }
  features->features = p;
  return WGPUStatus_Success;
}

WGPUBool wgpuInstanceHasWGSLLanguageFeature(WGPUInstance, WGPUWGSLLanguageFeatureName f) {
  LoadWgslFeatures();
  for (WGPUWGSLLanguageFeatureName x : g_wgsl)
    if (x == f) return 1;
  return 0;
}

void wgpuInstanceProcessEvents(WGPUInstance) {
  Flush();
  std::deque<Queued> keep;
  std::deque<Queued> run;
  for (Queued& q : g_queued) {
    if (q.mode == WGPUCallbackMode_WaitAnyOnly) keep.push_back(std::move(q));
    else run.push_back(std::move(q));
  }
  g_queued = std::move(keep);
  for (Queued& q : run) q.fire();
}

WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance, const WGPURequestAdapterOptions* o,
                                      WGPURequestAdapterCallbackInfo cb) {
  WGPUAdapterImpl* a = Make<WGPUAdapterImpl>();
  Writer& w = Cmd("adapter");
  w.K("id").U(a->id).K("a").Arr().Obj();
  if (o) {
    w.KS("powerPreference", PowerPreferenceName(o->powerPreference));
    w.KS("featureLevel", FeatureLevelName(o->featureLevel));
    if (o->forceFallbackAdapter) w.K("forceFallbackAdapter").B(true);
  }
  w.End().EndArr();
  EndCmd();
  Reply r = Send();
  WGPURequestAdapterStatus status = WGPURequestAdapterStatus_Success;
  std::string msg;
  if (!r.Ok()) {
    status = r.Get("unavailable") ? WGPURequestAdapterStatus_Unavailable : WGPURequestAdapterStatus_Error;
    msg = r.Error();
    delete a;
    a = nullptr;
  } else {
    a->vendor = r.Str("vendor");
    a->architecture = r.Str("architecture");
    a->device = r.Str("device");
    a->description = r.Str("description");
    a->subgroup_min = static_cast<uint32_t>(r.U("subgroupMinSize"));
    a->subgroup_max = static_cast<uint32_t>(r.U("subgroupMaxSize"));
    a->fallback = r.Str("fallback") == "1";
    ReadCaps(r, &a->features, &a->limits);
  }
  WGPUAdapter out = a;
  return Complete(cb.mode, cb.callback != nullptr, [cb, status, out, msg]() {
    cb.callback(status, out, View(msg), cb.userdata1, cb.userdata2);
  });
}

WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance, size_t count, WGPUFutureWaitInfo* futures, uint64_t) {
  if (count == 0) return WGPUWaitStatus_Success;
  Flush();
  bool any = false;
  for (size_t i = 0; i < count; ++i) {
    uint64_t id = futures[i].future.id;
    bool done = id != 0 && id < g_next_future && g_open_futures.count(id) == 0;
    for (auto it = g_queued.begin(); it != g_queued.end(); ++it) {
      if (it->id != id) continue;
      std::function<void()> fire = std::move(it->fire);
      g_queued.erase(it);
      fire();
      break;
    }
    futures[i].completed = done ? 1 : 0;
    any = any || done;
  }
  return any ? WGPUWaitStatus_Success : WGPUWaitStatus_TimedOut;
}

void wgpuInstanceAddRef(WGPUInstance i) {
  if (i) ++i->refs;
}
void wgpuInstanceRelease(WGPUInstance i) {
  if (i && --i->refs == 0) delete i;
}

// ---- PipelineLayout

VGPU_LABEL(PipelineLayout)
VGPU_REFS(PipelineLayout)

// ---- QuerySet

void wgpuQuerySetDestroy(WGPUQuerySet q) { NoArgs(q, "destroy"); }
uint32_t wgpuQuerySetGetCount(WGPUQuerySet q) { return q ? q->count : 0; }
WGPUQueryType wgpuQuerySetGetType(WGPUQuerySet q) { return q ? q->type : WGPUQueryType_Occlusion; }
VGPU_LABEL(QuerySet)
VGPU_REFS(QuerySet)

// ---- Queue

WGPUFuture wgpuQueueOnSubmittedWorkDone(WGPUQueue q, WGPUQueueWorkDoneCallbackInfo cb) {
  Writer& w = Cmd("done");
  w.K("t").U(q ? q->id : 0);
  EndCmd();
  Reply r = Send();
  WGPUQueueWorkDoneStatus status = r.Ok() ? WGPUQueueWorkDoneStatus_Success : WGPUQueueWorkDoneStatus_Error;
  if (!r.Ok()) std::fprintf(stderr, "vgpu: %s\n", r.Error().c_str());
  return Complete(cb.mode, cb.callback != nullptr, [cb, status]() {
    cb.callback(status, cb.userdata1, cb.userdata2);
  });
}

VGPU_LABEL(Queue)

void wgpuQueueSubmit(WGPUQueue q, size_t n, const WGPUCommandBuffer* cmds) {
  Writer& w = Call(q, "submit");
  w.Arr();
  for (size_t i = 0; i < n; ++i) w.Ref(cmds[i]);
  w.EndArr();
  EndArgs();
  // The work reaches the GPU now, not at the next sync point.
  Flush();
}

void wgpuQueueWriteBuffer(WGPUQueue q, WGPUBuffer b, uint64_t off, const void* data, size_t size) {
  size_t at = Blob(data, size);
  Writer& w = Call(q, "writeBuffer");
  w.Ref(b).U(off).Bytes(at, size);
  EndArgs();
}

void wgpuQueueWriteTexture(WGPUQueue q, const WGPUTexelCopyTextureInfo* dst, const void* data, size_t size,
                           const WGPUTexelCopyBufferLayout* layout, const WGPUExtent3D* extent) {
  size_t at = Blob(data, size);
  Writer& w = Call(q, "writeTexture");
  WriteCopyTexture(w, dst);
  w.Bytes(at, size);
  WriteLayout(w, *layout, nullptr);
  WriteExtent(w, *extent);
  EndArgs();
}

void wgpuQueueAddRef(WGPUQueue q) {
  if (q) ++q->refs;
}
void wgpuQueueRelease(WGPUQueue q) {
  if (!q || --q->refs != 0) return;
  if (q->device) q->device->queue = nullptr;
  Drop(q);
}

// ---- RenderBundle / RenderBundleEncoder

VGPU_LABEL(RenderBundle)
VGPU_REFS(RenderBundle)

namespace {

void Draw(const Obj* p, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) {
  Writer& w = Call(p, "draw");
  w.U(vc).U(ic).U(fv).U(fi);
  EndArgs();
}
void DrawIndexed(const Obj* p, uint32_t ic, uint32_t inst, uint32_t first, int32_t base, uint32_t fi) {
  Writer& w = Call(p, "drawIndexed");
  w.U(ic).U(inst).U(first).I(base).U(fi);
  EndArgs();
}
void Indirect(const Obj* p, const char* m, WGPUBuffer b, uint64_t off) {
  Writer& w = Call(p, m);
  w.Ref(b).U(off);
  EndArgs();
}
void SetIndexBuffer(const Obj* p, WGPUBuffer b, WGPUIndexFormat f, uint64_t off, uint64_t size) {
  Writer& w = Call(p, "setIndexBuffer");
  w.Ref(b).Str(IndexFormatName(f) ? IndexFormatName(f) : "uint32").U(off);
  SizeOrUndef(w, size);
  EndArgs();
}
void SetVertexBuffer(const Obj* p, uint32_t slot, WGPUBuffer b, uint64_t off, uint64_t size) {
  Writer& w = Call(p, "setVertexBuffer");
  w.U(slot).Ref(b).U(off);
  SizeOrUndef(w, size);
  EndArgs();
}
void SetRenderPipeline(const Obj* p, WGPURenderPipeline pipe) {
  Writer& w = Call(p, "setPipeline");
  w.Ref(pipe);
  EndArgs();
}

}  // namespace

void wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder e, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) {
  Draw(e, vc, ic, fv, fi);
}
void wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder e, uint32_t ic, uint32_t inst, uint32_t first,
                                        int32_t base, uint32_t fi) {
  DrawIndexed(e, ic, inst, first, base, fi);
}
void wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder e, WGPUBuffer b, uint64_t off) {
  Indirect(e, "drawIndexedIndirect", b, off);
}
void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder e, WGPUBuffer b, uint64_t off) {
  Indirect(e, "drawIndirect", b, off);
}
WGPURenderBundle wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder e, const WGPURenderBundleDescriptor* d) {
  WGPURenderBundleImpl* o = Make<WGPURenderBundleImpl>();
  Writer& w = New(o, e, "finish");
  w.Obj();
  if (d) w.KLabel(d->label);
  w.End();
  EndArgs();
  return o;
}
void wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder e, WGPUStringView l) {
  Marker(e, "insertDebugMarker", l);
}
void wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder e) { NoArgs(e, "popDebugGroup"); }
void wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder e, WGPUStringView l) {
  Marker(e, "pushDebugGroup", l);
}
void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder e, uint32_t i, WGPUBindGroup g, size_t n,
                                         const uint32_t* offs) {
  WriteSetBindGroup(e, i, g, n, offs);
}
void wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder e, WGPUBuffer b, WGPUIndexFormat f,
                                           uint64_t off, uint64_t size) {
  SetIndexBuffer(e, b, f, off, size);
}
VGPU_LABEL(RenderBundleEncoder)
void wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder e, WGPURenderPipeline p) { SetRenderPipeline(e, p); }
void wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder e, uint32_t slot, WGPUBuffer b, uint64_t off,
                                            uint64_t size) {
  SetVertexBuffer(e, slot, b, off, size);
}
VGPU_REFS(RenderBundleEncoder)

// ---- RenderPassEncoder

void wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder p, uint32_t i) {
  Writer& w = Call(p, "beginOcclusionQuery");
  w.U(i);
  EndArgs();
}
void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder p, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi) {
  Draw(p, vc, ic, fv, fi);
}
void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder p, uint32_t ic, uint32_t inst, uint32_t first,
                                      int32_t base, uint32_t fi) {
  DrawIndexed(p, ic, inst, first, base, fi);
}
void wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder p, WGPUBuffer b, uint64_t off) {
  Indirect(p, "drawIndexedIndirect", b, off);
}
void wgpuRenderPassEncoderDrawIndirect(WGPURenderPassEncoder p, WGPUBuffer b, uint64_t off) {
  Indirect(p, "drawIndirect", b, off);
}
void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder p) { NoArgs(p, "end"); }
void wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder p) { NoArgs(p, "endOcclusionQuery"); }
void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder p, size_t n, const WGPURenderBundle* bundles) {
  Writer& w = Call(p, "executeBundles");
  w.Arr();
  for (size_t i = 0; i < n; ++i) w.Ref(bundles[i]);
  w.EndArr();
  EndArgs();
}
void wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder p, WGPUStringView l) {
  Marker(p, "insertDebugMarker", l);
}
void wgpuRenderPassEncoderMultiDrawIndexedIndirect(WGPURenderPassEncoder p, WGPUBuffer b, uint64_t off,
                                                   uint32_t max, WGPUBuffer count_b, uint64_t count_off) {
  Writer& w = Call(p, "multiDrawIndexedIndirect");
  w.Ref(b).U(off).U(max).Ref(count_b).U(count_off);
  EndArgs();
}
void wgpuRenderPassEncoderMultiDrawIndirect(WGPURenderPassEncoder p, WGPUBuffer b, uint64_t off, uint32_t max,
                                            WGPUBuffer count_b, uint64_t count_off) {
  Writer& w = Call(p, "multiDrawIndirect");
  w.Ref(b).U(off).U(max).Ref(count_b).U(count_off);
  EndArgs();
}
void wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder p) { NoArgs(p, "popDebugGroup"); }
void wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder p, WGPUStringView l) {
  Marker(p, "pushDebugGroup", l);
}
void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder p, uint32_t i, WGPUBindGroup g, size_t n,
                                       const uint32_t* offs) {
  WriteSetBindGroup(p, i, g, n, offs);
}
void wgpuRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder p, const WGPUColor* c) {
  Writer& w = Call(p, "setBlendConstant");
  w.Obj().K("r").D(c->r).K("g").D(c->g).K("b").D(c->b).K("a").D(c->a).End();
  EndArgs();
}
void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder p, WGPUBuffer b, WGPUIndexFormat f, uint64_t off,
                                         uint64_t size) {
  SetIndexBuffer(p, b, f, off, size);
}
VGPU_LABEL(RenderPassEncoder)
void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder p, WGPURenderPipeline pipe) {
  SetRenderPipeline(p, pipe);
}
void wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder p, uint32_t x, uint32_t y, uint32_t wd, uint32_t h) {
  Writer& w = Call(p, "setScissorRect");
  w.U(x).U(y).U(wd).U(h);
  EndArgs();
}
void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder p, uint32_t ref) {
  Writer& w = Call(p, "setStencilReference");
  w.U(ref);
  EndArgs();
}
void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder p, uint32_t slot, WGPUBuffer b, uint64_t off,
                                          uint64_t size) {
  SetVertexBuffer(p, slot, b, off, size);
}
void wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder p, float x, float y, float wd, float h, float mn,
                                      float mx) {
  Writer& w = Call(p, "setViewport");
  w.D(x).D(y).D(wd).D(h).D(mn).D(mx);
  EndArgs();
}
void wgpuRenderPassEncoderWriteTimestamp(WGPURenderPassEncoder p, WGPUQuerySet qs, uint32_t index) {
  Writer& w = Call(p, "writeTimestamp");
  w.Ref(qs).U(index);
  EndArgs();
}
VGPU_REFS(RenderPassEncoder)

// ---- RenderPipeline

WGPUBindGroupLayout wgpuRenderPipelineGetBindGroupLayout(WGPURenderPipeline p, uint32_t index) {
  WGPUBindGroupLayoutImpl* l = Make<WGPUBindGroupLayoutImpl>();
  Writer& w = New(l, p, "getBindGroupLayout");
  w.U(index);
  EndArgs();
  return l;
}
VGPU_LABEL(RenderPipeline)
VGPU_REFS(RenderPipeline)

// ---- Sampler

VGPU_LABEL(Sampler)
VGPU_REFS(Sampler)

// ---- ShaderModule

WGPUFuture wgpuShaderModuleGetCompilationInfo(WGPUShaderModule m, WGPUCompilationInfoCallbackInfo cb) {
  Writer& w = Cmd("info");
  w.K("t").U(m ? m->id : 0);
  EndCmd();
  Reply r = Send();
  // "msg=<type>\t<line>\t<pos>\t<offset>\t<length>\t<text>"
  auto texts = std::make_shared<std::vector<std::string>>();
  auto msgs = std::make_shared<std::vector<WGPUCompilationMessage>>();
  for (const auto& kv : r.kv) {
    if (kv.first != "msg") continue;
    std::vector<std::string> f;
    size_t at = 0;
    for (int i = 0; i < 5; ++i) {
      size_t t = kv.second.find('\t', at);
      if (t == std::string::npos) break;
      f.push_back(kv.second.substr(at, t - at));
      at = t + 1;
    }
    if (f.size() != 5) continue;
    texts->push_back(kv.second.substr(at));
    WGPUCompilationMessage cm{};
    cm.type = f[0] == "error" ? WGPUCompilationMessageType_Error
              : f[0] == "warning" ? WGPUCompilationMessageType_Warning
                                  : WGPUCompilationMessageType_Info;
    cm.lineNum = std::strtoull(f[1].c_str(), nullptr, 10);
    cm.linePos = std::strtoull(f[2].c_str(), nullptr, 10);
    cm.offset = std::strtoull(f[3].c_str(), nullptr, 10);
    cm.length = std::strtoull(f[4].c_str(), nullptr, 10);
    msgs->push_back(cm);
  }
  for (size_t i = 0; i < msgs->size(); ++i) (*msgs)[i].message = View((*texts)[i]);
  WGPUCompilationInfoRequestStatus status = r.answered ? WGPUCompilationInfoRequestStatus_Success
                                                       : WGPUCompilationInfoRequestStatus_CallbackCancelled;
  return Complete(cb.mode, cb.callback != nullptr, [cb, status, texts, msgs]() {
    WGPUCompilationInfo info{};
    info.messageCount = msgs->size();
    info.messages = msgs->data();
    cb.callback(status, &info, cb.userdata1, cb.userdata2);
  });
}

VGPU_LABEL(ShaderModule)
VGPU_REFS(ShaderModule)

// ---- Surface

void wgpuSurfaceConfigure(WGPUSurface s, const WGPUSurfaceConfiguration* c) {
  if (!s || !c) return;
  s->configured = true;
  s->format = c->format;
  s->usage = c->usage;
  s->width = c->width;
  s->height = c->height;
  Writer& w = Cmd("canvas");
  w.K("t").U(s->id).K("w").U(c->width).K("h").U(c->height);
  EndCmd();
  Writer& cw = Call(s, "configure");
  cw.Obj().K("device").Ref(c->device).KS("format", TextureFormatName(c->format)).K("usage").U(c->usage);
  if (c->viewFormatCount) {
    cw.K("viewFormats").Arr();
    for (size_t i = 0; i < c->viewFormatCount; ++i)
      if (TextureFormatName(c->viewFormats[i])) cw.Str(TextureFormatName(c->viewFormats[i]));
    cw.EndArr();
  }
  cw.KS("alphaMode", AlphaModeName(c->alphaMode)).End();
  EndArgs();
}

WGPUStatus wgpuSurfaceGetCapabilities(WGPUSurface, WGPUAdapter, WGPUSurfaceCapabilities* caps) {
  if (!caps) return WGPUStatus_Error;
  static const WGPUTextureFormat kFormats[] = {WGPUTextureFormat_BGRA8Unorm, WGPUTextureFormat_RGBA8Unorm,
                                               WGPUTextureFormat_RGBA16Float};
  static const WGPUPresentMode kModes[] = {WGPUPresentMode_Fifo};
  static const WGPUCompositeAlphaMode kAlpha[] = {WGPUCompositeAlphaMode_Opaque,
                                                  WGPUCompositeAlphaMode_Premultiplied};
  WGPUChainedStruct* next = caps->nextInChain;
  *caps = WGPUSurfaceCapabilities{};
  caps->nextInChain = next;
  caps->usages = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst |
                 WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding;
  caps->formatCount = 3;
  caps->formats = kFormats;
  caps->presentModeCount = 1;
  caps->presentModes = kModes;
  caps->alphaModeCount = 2;
  caps->alphaModes = kAlpha;
  return WGPUStatus_Success;
}

void wgpuSurfaceGetCurrentTexture(WGPUSurface s, WGPUSurfaceTexture* out) {
  if (!out) return;
  out->texture = nullptr;
  if (!s || !s->configured) {
    out->status = WGPUSurfaceGetCurrentTextureStatus_Error;
    return;
  }
  WGPUTextureImpl* t = Make<WGPUTextureImpl>();
  t->width = s->width;
  t->height = s->height;
  t->format = s->format;
  t->usage = s->usage;
  New(t, s, "getCurrentTexture");
  EndArgs();
  out->texture = t;
  out->status = WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal;
}

// The browser shows a canvas's frame once its task ends; sending the batch
// is all a present needs.
void wgpuSurfacePresent(WGPUSurface) { Flush(); }

VGPU_LABEL(Surface)

void wgpuSurfaceUnconfigure(WGPUSurface s) {
  if (!s) return;
  s->configured = false;
  NoArgs(s, "unconfigure");
}

VGPU_REFS(Surface)

// ---- Texture / TextureView

WGPUTextureView wgpuTextureCreateView(WGPUTexture t, const WGPUTextureViewDescriptor* d) {
  WGPUTextureViewImpl* v = Make<WGPUTextureViewImpl>();
  Writer& w = New(v, t, "createView");
  WriteTextureView(w, d);
  EndArgs();
  return v;
}

void wgpuTextureDestroy(WGPUTexture t) { NoArgs(t, "destroy"); }
uint32_t wgpuTextureGetDepthOrArrayLayers(WGPUTexture t) { return t ? t->depth : 0; }
WGPUTextureDimension wgpuTextureGetDimension(WGPUTexture t) { return t ? t->dimension : WGPUTextureDimension_Undefined; }
WGPUTextureFormat wgpuTextureGetFormat(WGPUTexture t) { return t ? t->format : WGPUTextureFormat_Undefined; }
uint32_t wgpuTextureGetHeight(WGPUTexture t) { return t ? t->height : 0; }
uint32_t wgpuTextureGetMipLevelCount(WGPUTexture t) { return t ? t->mips : 0; }
uint32_t wgpuTextureGetSampleCount(WGPUTexture t) { return t ? t->samples : 0; }
WGPUTextureUsage wgpuTextureGetUsage(WGPUTexture t) { return t ? t->usage : 0; }
uint32_t wgpuTextureGetWidth(WGPUTexture t) { return t ? t->width : 0; }
VGPU_LABEL(Texture)
VGPU_REFS(Texture)

VGPU_LABEL(TextureView)
VGPU_REFS(TextureView)


