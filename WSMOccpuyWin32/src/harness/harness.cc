#include "wow/harness.h"

namespace wow {
namespace {

std::vector<uint8_t> BytesOf(std::string_view s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

}  // namespace

Harness::Harness(Catalog catalog) : catalog_(std::move(catalog)) {}

Shot Harness::Prepare(std::string_view name,
                      std::span<const uint8_t> payload) const {
  Shot s;
  s.name = std::string(name);
  s.payload.assign(payload.begin(), payload.end());
  uint32_t ord = 0;
  std::string iface;
  if (catalog_.Lookup(s.name, &ord, &iface)) {
    s.ordinal = ord;
    if (!iface.empty()) {
      s.iface = iface;
    }
  }
  return s;
}

WowResult Harness::Send(const Shot& s) {
  last_payload_ = s.payload;
  if (!s.ordinal) {
    return WOW_RESULT_NOT_FOUND;
  }
  if (!catalog_.MayFire(s.ordinal)) {
    return WOW_RESULT_PERMISSION_DENIED;
  }
  if (!fire_) {
    return WOW_RESULT_OK;
  }
  return fire_(s);
}

WowResult Harness::FireOccupyWin32() {
  WowResult r = occupancy_.OccupyWin32("win32");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupyWin32", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireOccupyCatalog() {
  WowResult r = occupancy_.OccupyCatalog("catalog");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupyCatalog", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireThinMap() {
  return Send(Prepare("thinMap", BytesOf(occupancy_.ThinMapJson())));
}

WowResult Harness::FireOccupancyStatus() {
  return Send(Prepare("occupancyStatus", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireOccupyKernel32() {
  WowResult r = occupancy_.OccupyKernel32("kernel32");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupyKernel32", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireOccupyNtdll() {
  WowResult r = occupancy_.OccupyNtdll("ntdll");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupyNtdll", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireOccupyVmem() {
  WowResult r = occupancy_.OccupyVmem("vmem");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupyVmem", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireOccupyPipe() {
  WowResult r = occupancy_.OccupyPipe("pipe");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupyPipe", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireOccupySys() {
  WowResult r = occupancy_.OccupySys("sys");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupySys", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireOccupyDriver() {
  WowResult r = occupancy_.OccupyDriver("wowwin32");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupyDriver", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireOccupyGocvm() {
  WowResult r = occupancy_.OccupyGocvm("wasigocvm");
  if (r != WOW_RESULT_OK) {
    last_payload_.clear();
    return r;
  }
  return Send(Prepare("occupyGocvm", BytesOf(occupancy_.StatusJson())));
}

WowResult Harness::FireIpc(const Ipc& ipc) {
  return FireName(ipc.name);
}

WowResult Harness::FireIpcJson(std::string_view json) {
  Ipc ipc;
  if (!Ipc::FromJson(json, &ipc)) {
    return WOW_RESULT_INVALID_ARGUMENT;
  }
  return FireIpc(ipc);
}

WowResult Harness::FireName(std::string_view name,
                            std::span<const uint8_t> payload) {
  std::string reply;
  std::string err;
  std::string body(payload.begin(), payload.end());
  if (!occupancy_.DispatchTopic(name, body, &reply, &err)) {
    last_payload_.clear();
    return WOW_RESULT_NOT_FOUND;
  }
  return Send(Prepare(name, BytesOf(reply)));
}

}  // namespace wow
