#include "wow/catalog.h"

#include <cstdlib>
#include <cstring>

namespace wow {
namespace {

std::string Lower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return s;
}

void Fill(Catalog* c, const Method* rows, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    c->Set(rows[i].name, rows[i].ordinal, rows[i].iface, rows[i].fire);
  }
}

}  // namespace

void Catalog::Set(std::string name, uint32_t ordinal, std::string iface,
                  bool fire) {
  if (name.empty() || ordinal == 0) {
    return;
  }
  std::string key = Lower(name);
  named_[key] = ordinal;
  named_[name] = ordinal;
  by_ord_[ordinal] = name;
  if (!iface.empty()) {
    iface_[ordinal] = std::move(iface);
  }
  fire_[ordinal] = fire;
  methods_.push_back({std::move(name), ordinal,
                      iface_.count(ordinal) ? iface_[ordinal] : std::string(),
                      fire});
}

bool Catalog::Lookup(std::string_view name, uint32_t* ordinal,
                     std::string* iface) const {
  if (!ordinal) {
    return false;
  }
  std::string key(name);
  if (key.rfind("win32.", 0) == 0) {
    key = key.substr(6);
  }
  auto it = named_.find(key);
  if (it == named_.end()) {
    it = named_.find(Lower(key));
  }
  if (it == named_.end()) {
    return false;
  }
  *ordinal = it->second;
  if (iface) {
    auto fi = iface_.find(*ordinal);
    if (fi != iface_.end()) {
      *iface = fi->second;
    }
  }
  return true;
}

const char* Catalog::NameOf(uint32_t ordinal) const {
  auto it = by_ord_.find(ordinal);
  return it == by_ord_.end() ? nullptr : it->second.c_str();
}

bool Catalog::Owns(uint32_t ordinal) const {
  return by_ord_.find(ordinal) != by_ord_.end();
}

bool Catalog::MayFire(uint32_t ordinal) const {
  auto it = fire_.find(ordinal);
  return it != fire_.end() && it->second;
}

WowResult Catalog::LoadJson(std::string_view json) {
  size_t pos = 0;
  int n = 0;
  while (pos < json.size()) {
    auto np = json.find("\"name\"", pos);
    if (np == std::string_view::npos) {
      break;
    }
    auto colon = json.find(':', np + 6);
    if (colon == std::string_view::npos) {
      break;
    }
    auto q1 = json.find('"', colon);
    if (q1 == std::string_view::npos) {
      break;
    }
    auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) {
      break;
    }
    std::string name(json.substr(q1 + 1, q2 - q1 - 1));
    size_t obj0 = json.rfind('{', np);
    size_t obj1 = json.find('}', q2);
    pos = q2 + 1;
    if (obj0 == std::string_view::npos || obj1 == std::string_view::npos ||
        obj1 < obj0) {
      continue;
    }
    auto slice = json.substr(obj0, obj1 - obj0 + 1);
    auto op = slice.find("\"ordinal\"");
    if (op == std::string_view::npos) {
      continue;
    }
    auto oc = slice.find(':', op);
    if (oc == std::string_view::npos) {
      continue;
    }
    uint32_t ord = static_cast<uint32_t>(
        std::strtoul(std::string(slice.substr(oc + 1)).c_str(), nullptr, 10));
    if (ord == 0 || !IsWin32Ordinal(ord) || name.empty()) {
      continue;
    }
    std::string iface;
    auto ip = slice.find("\"iface\"");
    if (ip != std::string_view::npos) {
      auto ic = slice.find(':', ip);
      auto iq1 = slice.find('"', ic);
      auto iq2 = slice.find('"', iq1 + 1);
      if (iq1 != std::string_view::npos && iq2 != std::string_view::npos) {
        iface = std::string(slice.substr(iq1 + 1, iq2 - iq1 - 1));
      }
    }
    if (iface.empty()) {
      iface = "win32";
    }
    Set(std::move(name), ord, std::move(iface), true);
    ++n;
  }
  return n ? WOW_RESULT_OK : WOW_RESULT_NOT_FOUND;
}

namespace {

const Method kDrive[] = {
    {"occupyWin32", Fnv1a31("occupyWin32"), "win32", true},
    {"occupyCatalog", Fnv1a31("occupyCatalog"), "win32", true},
    {"occupyKernel32", Fnv1a31("occupyKernel32"), "win32", true},
    {"occupyNtdll", Fnv1a31("occupyNtdll"), "win32", true},
    {"occupyUser32", Fnv1a31("occupyUser32"), "win32", true},
    {"occupyGdi32", Fnv1a31("occupyGdi32"), "win32", true},
    {"occupyOle32", Fnv1a31("occupyOle32"), "win32", true},
    {"occupyCom", Fnv1a31("occupyCom"), "win32", true},
    {"occupyAdvapi32", Fnv1a31("occupyAdvapi32"), "win32", true},
    {"occupyToken", Fnv1a31("occupyToken"), "win32", true},
    {"occupyRegistry", Fnv1a31("occupyRegistry"), "win32", true},
    {"occupyScm", Fnv1a31("occupyScm"), "win32", true},
    {"occupyDevice", Fnv1a31("occupyDevice"), "win32", true},
    {"occupySockets", Fnv1a31("occupySockets"), "win32", true},
    {"occupyBcrypt", Fnv1a31("occupyBcrypt"), "win32", true},
    {"occupyVmem", Fnv1a31("occupyVmem"), "win32", true},
    {"occupyPipe", Fnv1a31("occupyPipe"), "win32", true},
    {"occupyPe", Fnv1a31("occupyPe"), "win32", true},
    {"occupyProcess", Fnv1a31("occupyProcess"), "win32", true},
    {"occupyConsole", Fnv1a31("occupyConsole"), "win32", true},
    {"occupyHwnd", Fnv1a31("occupyHwnd"), "win32", true},
    {"occupyHv", Fnv1a31("occupyHv"), "win32", true},
    {"occupyWsl", Fnv1a31("occupyWsl"), "win32", true},
    {"occupyNix", Fnv1a31("occupyNix"), "win32", true},
    {"occupySys", Fnv1a31("occupySys"), "win32", true},
    {"occupyDriver", Fnv1a31("occupyDriver"), "win32", true},
    {"occupyLoadDriver", Fnv1a31("occupyLoadDriver"), "win32", true},
    {"driverEntry", Fnv1a31("driverEntry"), "win32", true},
    {"getDriverInfo", Fnv1a31("getDriverInfo"), "win32", true},
    {"deviceControl", Fnv1a31("deviceControl"), "win32", true},
    {"occupyWin32k", Fnv1a31("occupyWin32k"), "win32", true},
    {"occupyHostNtos", Fnv1a31("occupyHostNtos"), "win32", true},
    {"occupyCmd", Fnv1a31("occupyCmd"), "win32", true},
    {"occupyCalc", Fnv1a31("occupyCalc"), "win32", true},
    {"occupyLinux", Fnv1a31("occupyLinux"), "win32", true},
    {"occupySh", Fnv1a31("occupySh"), "win32", true},
    {"occupyWasmtty", Fnv1a31("occupyWasmtty"), "win32", true},
    {"occupyGocvm", Fnv1a31("occupyGocvm"), "win32", true},
    {"ttyHello", Fnv1a31("ttyHello"), "win32", true},
    {"ttyPing", Fnv1a31("ttyPing"), "win32", true},
    {"occupyNet", Fnv1a31("occupyNet"), "win32", true},
    {"commonRoutes", Fnv1a31("commonRoutes"), "win32", true},
    {"occupancyStatus", Fnv1a31("occupancyStatus"), "win32", true},
    {"thinMap", Fnv1a31("thinMap"), "win32", true},
    {"open", Fnv1a31("open"), "win32", true},
    {"write", Fnv1a31("write"), "win32", true},
    {"read", Fnv1a31("read"), "win32", true},
    {"map", Fnv1a31("map"), "win32", true},
    {"unmap", Fnv1a31("unmap"), "win32", true},
};

}  // namespace

Catalog Catalog::Drive() {
  Catalog c;
  Fill(&c, kDrive, sizeof(kDrive) / sizeof(kDrive[0]));
  return c;
}

Catalog Catalog::Dagger() {
  Catalog c;
  for (const auto& m : kDrive) {
    c.Set(m.name, Fnv131(m.name), m.iface, m.fire);
  }
  return c;
}

namespace {

Catalog* G() {
  static Catalog cat = Catalog::Drive();
  return &cat;
}

}  // namespace

Catalog& DefaultCatalog() { return *G(); }

void SetDefaultPack(Pack pack) {
  *G() = pack == Pack::kDagger ? Catalog::Dagger() : Catalog::Drive();
}

}  // namespace wow
