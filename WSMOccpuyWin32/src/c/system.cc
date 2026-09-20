#include "wow/c/system.h"

#include "wow/catalog.h"
#include "wow/gocvm.h"
#include "wow/occupancy.h"

#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

bool g_inited = false;
std::unique_ptr<wow::Occupancy> g_occ;
std::unique_ptr<wow::OccupancyBridge> g_bridge;

wow::Occupancy& Table() {
  if (!g_occ) {
    g_occ = std::make_unique<wow::Occupancy>();
  }
  return *g_occ;
}

wow::OccupancyBridge& Bridge() {
  if (!g_bridge) {
    g_bridge = std::make_unique<wow::OccupancyBridge>(&Table());
  }
  return *g_bridge;
}

WowResult CopyStr(const std::string& src, char* buf, uint32_t len) {
  if (!buf || len == 0) {
    return WOW_RESULT_INVALID_ARGUMENT;
  }
  if (src.size() + 1 > len) {
    return WOW_RESULT_RESOURCE_EXHAUSTED;
  }
  std::memcpy(buf, src.c_str(), src.size() + 1);
  return WOW_RESULT_OK;
}

}  // namespace

extern "C" {

WowResult WowInit(void) {
  g_inited = true;
  (void)Table();
  (void)Bridge();
  return WOW_RESULT_OK;
}

void WowShutdown(void) {
  g_bridge.reset();
  g_occ.reset();
  g_inited = false;
}

uint32_t WowOrdinal(const char* name) {
  if (!name) {
    return 0;
  }
  uint32_t ord = 0;
  if (!wow::DefaultCatalog().Lookup(name, &ord, nullptr)) {
    return 0;
  }
  return ord;
}

WowResult WowSetPack(int pack) {
  wow::SetDefaultPack(pack == WOW_PACK_DAGGER ? wow::Pack::kDagger
                                              : wow::Pack::kDrive);
  return WOW_RESULT_OK;
}

int WowIsWin32Ordinal(uint32_t ordinal) {
  return wow::DefaultCatalog().Owns(ordinal) ? 1 : 0;
}

WowResult WowOccupyWin32(const char* label) {
  return Table().OccupyWin32(label ? label : "win32");
}

WowResult WowOccupyCatalog(const char* label) {
  return Table().OccupyCatalog(label ? label : "catalog");
}

WowResult WowOpen(int kind, const char* label, int parent, int* out_id) {
  return Table().Open(static_cast<wow::SurfaceKind>(kind),
                      label ? label : "", parent, out_id);
}

WowResult WowWrite(int id, const void* bytes, uint32_t len) {
  if (!bytes && len) {
    return WOW_RESULT_INVALID_ARGUMENT;
  }
  auto* p = static_cast<const uint8_t*>(bytes);
  return Table().Write(id, std::span<const uint8_t>(p, len));
}

WowResult WowRead(int id, void* buf, uint32_t len, uint32_t* out_len) {
  std::vector<uint8_t> bytes;
  WowResult r = Table().Read(id, &bytes);
  if (r != WOW_RESULT_OK) {
    return r;
  }
  if (out_len) {
    *out_len = static_cast<uint32_t>(bytes.size());
  }
  if (!buf) {
    return WOW_RESULT_OK;
  }
  if (bytes.size() > len) {
    return WOW_RESULT_RESOURCE_EXHAUSTED;
  }
  if (!bytes.empty()) {
    std::memcpy(buf, bytes.data(), bytes.size());
  }
  return WOW_RESULT_OK;
}

WowResult WowMap(int id) { return Table().Map(id); }
WowResult WowUnmap(int id) { return Table().Unmap(id); }

WowResult WowStatus(char* buf, uint32_t len) {
  return CopyStr(Table().StatusJson(), buf, len);
}
WowResult WowThinMap(char* buf, uint32_t len) {
  return CopyStr(Table().ThinMapJson(), buf, len);
}
WowResult WowLastEvent(char* buf, uint32_t len) {
  return CopyStr(Table().events().LastJson(), buf, len);
}
WowResult WowLastError(char* buf, uint32_t len) {
  return CopyStr(Table().last_error(), buf, len);
}

WowResult WowCall(const char* topic, const char* payload, char* buf,
                  uint32_t len) {
  if (!topic) {
    return WOW_RESULT_INVALID_ARGUMENT;
  }
  std::string reply;
  std::string err;
  if (!Bridge().Call(topic, payload ? payload : "", &reply, &err)) {
    if (!err.empty()) {
      CopyStr(err, buf, len);
    }
    return WOW_RESULT_PERMISSION_DENIED;
  }
  return CopyStr(reply, buf, len);
}

}  // extern "C"
