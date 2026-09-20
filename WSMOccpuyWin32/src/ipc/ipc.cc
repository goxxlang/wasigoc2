#include "wow/ipc.h"

#include "wow/surface.h"

#include <cstdlib>

namespace wow {
namespace {

std::string Field(std::string_view json, const char* key) {
  std::string pat = std::string("\"") + key + "\"";
  auto p = json.find(pat);
  if (p == std::string_view::npos) {
    return {};
  }
  p = json.find(':', p);
  if (p == std::string_view::npos) {
    return {};
  }
  ++p;
  while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) {
    ++p;
  }
  if (p < json.size() && json[p] == '"') {
    auto e = json.find('"', p + 1);
    if (e == std::string_view::npos) {
      return {};
    }
    return std::string(json.substr(p + 1, e - p - 1));
  }
  size_t n = p;
  while (n < json.size() && json[n] != ',' && json[n] != '}' &&
         json[n] != ' ') {
    ++n;
  }
  return std::string(json.substr(p, n - p));
}

}  // namespace

Ipc Ipc::Invoke(std::string_view name, uint32_t ordinal) {
  Ipc i;
  i.name = std::string(name);
  i.ordinal = ordinal;
  return i;
}

Ipc Ipc::OccupyWin32(const Catalog& cat) {
  uint32_t ord = 0;
  cat.Lookup("occupyWin32", &ord, nullptr);
  return Invoke("occupyWin32", ord);
}

Ipc Ipc::ThinMap(const Catalog& cat) {
  uint32_t ord = 0;
  cat.Lookup("thinMap", &ord, nullptr);
  return Invoke("thinMap", ord);
}

Ipc Ipc::OccupancyStatus(const Catalog& cat) {
  uint32_t ord = 0;
  cat.Lookup("occupancyStatus", &ord, nullptr);
  return Invoke("occupancyStatus", ord);
}

std::string Ipc::ToJson() const {
  std::string o = "{\"action\":";
  o += JsonEscape(action);
  o += ",\"name\":";
  o += JsonEscape(name);
  o += ",\"ordinal\":";
  o += std::to_string(ordinal);
  o += ",\"id\":";
  o += std::to_string(id);
  if (!data.empty()) {
    o += ",\"data\":";
    o += JsonEscape(data);
  }
  o += "}";
  return o;
}

bool Ipc::FromJson(std::string_view json, Ipc* out) {
  if (!out) {
    return false;
  }
  Ipc i;
  i.action = Field(json, "action");
  if (i.action.empty()) {
    i.action = "invoke";
  }
  i.name = Field(json, "name");
  std::string ord = Field(json, "ordinal");
  if (!ord.empty()) {
    i.ordinal = static_cast<uint32_t>(std::strtoul(ord.c_str(), nullptr, 10));
  }
  i.data = Field(json, "data");
  i.args_json = Field(json, "args");
  std::string ids = Field(json, "id");
  if (!ids.empty()) {
    i.id = static_cast<int>(std::strtol(ids.c_str(), nullptr, 10));
  }
  if (i.name.empty() && i.ordinal == 0) {
    i.name = "occupancyStatus";
  }
  if (i.ordinal == 0 && !i.name.empty()) {
    DefaultCatalog().Lookup(i.name, &i.ordinal, nullptr);
  }
  *out = std::move(i);
  return true;
}

bool Ipc::is_win32() const {
  uint32_t ord = ordinal;
  if (!ord && !name.empty()) {
    DefaultCatalog().Lookup(name, &ord, nullptr);
  }
  return DefaultCatalog().Owns(ord);
}

}  // namespace wow
