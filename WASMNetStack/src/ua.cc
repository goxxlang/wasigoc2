#include "wns/ua.h"

#include <cctype>
#include <string>

namespace wns {
namespace {

bool Has(const std::string& s, const char* tok) {
  return s.find(tok) != std::string::npos;
}

std::string Lower(std::string_view in) {
  std::string s(in);
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool TorBrowser(const std::string& s) {
  if (Has(s, "torbrowser") || Has(s, "tbb/") || Has(s, "tor/")) return true;
  if (!Has(s, "firefox/")) return false;
  if (Has(s, "android") || Has(s, "iphone") || Has(s, "ipad")) return false;
  if (!Has(s, "windows nt")) return false;
  // Tor Browser 15 (Firefox 140 ESR) ships Win64. Older TBB omitted it.
  if (Has(s, "firefox/115") || Has(s, "firefox/128") || Has(s, "firefox/140")) {
    return true;
  }
  if (Has(s, "win64") || Has(s, "wow64") || Has(s, "x64") || Has(s, "x86_64") ||
      Has(s, "amd64")) {
    return false;
  }
  return true;
}

}  // namespace

void FillOsGpu(UaRoute& r, const std::string& s) {
  if (Has(s, "android")) {
    r.os = HostOS::kAndroid;
  } else if (Has(s, "iphone") || Has(s, "ipad") || Has(s, "ios")) {
    r.os = HostOS::kIOS;
  } else if (Has(s, "cros")) {
    r.os = HostOS::kChromeOS;
  } else if (Has(s, "mac os") || Has(s, "macintosh")) {
    r.os = HostOS::kMacOS;
  } else if (Has(s, "windows")) {
    r.os = HostOS::kWindows;
  } else if (Has(s, "linux")) {
    r.os = HostOS::kLinux;
  }
  switch (r.os) {
    case HostOS::kWindows:
      r.gpu = GpuRoute::kWindows;
      r.gpu_topic = "occupyDxgi";
      r.gpu_ordinal = kOrdOccupyDxgi;
      r.gpu_via = "woggpu.sys";
      break;
    case HostOS::kLinux:
    case HostOS::kChromeOS:
      r.gpu = GpuRoute::kLinux;
      r.gpu_topic = "occupyLinux";
      r.gpu_ordinal = kOrdOccupyLinux;
      r.gpu_via = "woggpu.ko";
      break;
    case HostOS::kAndroid:
      r.gpu = GpuRoute::kAndroid;
      r.gpu_topic = "occupyAndroid";
      r.gpu_ordinal = kOrdOccupyAndroid;
      r.gpu_via = "woggpu.ko";
      break;
    default:
      break;
  }
}

UaRoute RouteUserAgent(std::string_view ua) {
  UaRoute r;
  auto s = Lower(ua);
  auto done = [&]() {
    FillOsGpu(r, s);
    return r;
  };
  if (s.empty()) return done();

  // Tor Browser is Gecko (Firefox ESR). Occupancy is MothmanBridge; Mojo still
  // HolePunch. This is the WASMDNS/iBGP speaker hop (phisheries.dev/p).
  if (TorBrowser(s)) {
    r.engine = Engine::kGecko;
    r.flavor = Flavor::kTor;
    r.occupancy = OccupancyBus::kMothmanBridge;
    r.holepunch_mojo = true;
    r.mothman_chrome = true;
    r.wasm = "/mothman.wasm";
    r.dest = LeaveDest(r.flavor);
    return done();
  }

  // Firefox / Gecko first: Chrome's UA also contains "like Gecko" and AppleWebKit.
  if (Has(s, "firefox/") || Has(s, "fxios/")) {
    r.engine = Engine::kGecko;
    r.flavor = Flavor::kFirefox;
    r.occupancy = OccupancyBus::kMothmanBridge;
    r.holepunch_mojo = true;
    r.mothman_chrome = true;
    r.wasm = "/mothman.wasm";
    r.dest = LeaveDest(r.flavor);
    return done();
  }

  // Chromium-core flavors. Opera (OPR/) is Blink, not Presto.
  if (Has(s, "opr/") || (Has(s, "opera") && Has(s, "chrome/"))) {
    r.engine = Engine::kChromium;
    r.flavor = Flavor::kOpera;
    r.occupancy = OccupancyBus::kHolePunch;
    r.holepunch_mojo = true;
    r.wasm = "/whp.wasm";
    r.dest = LeaveDest(r.flavor);
    return done();
  }
  if (Has(s, "edg/") || Has(s, "edga/") || Has(s, "edgios/")) {
    r.engine = Engine::kChromium;
    r.flavor = Flavor::kEdge;
    r.occupancy = OccupancyBus::kHolePunch;
    r.holepunch_mojo = true;
    r.wasm = "/whp.wasm";
    r.dest = LeaveDest(r.flavor);
    return done();
  }
  if (Has(s, "brave/")) {
    r.engine = Engine::kChromium;
    r.flavor = Flavor::kBrave;
    r.occupancy = OccupancyBus::kHolePunch;
    r.holepunch_mojo = true;
    r.wasm = "/whp.wasm";
    r.dest = LeaveDest(r.flavor);
    return done();
  }
  if (Has(s, "vivaldi/")) {
    r.engine = Engine::kChromium;
    r.flavor = Flavor::kVivaldi;
    r.occupancy = OccupancyBus::kHolePunch;
    r.holepunch_mojo = true;
    r.wasm = "/whp.wasm";
    r.dest = LeaveDest(r.flavor);
    return done();
  }
  if (Has(s, "chrome/") || Has(s, "crios/") || Has(s, "chromium/")) {
    r.engine = Engine::kChromium;
    r.flavor = Flavor::kChrome;
    r.occupancy = OccupancyBus::kHolePunch;
    r.holepunch_mojo = true;
    r.wasm = "/whp.wasm";
    r.dest = LeaveDest(r.flavor);
    return done();
  }

  // Safari / WebKit: AppleWebKit without Chrome/. YetiTunnel replaces HolePunch.
  if (Has(s, "applewebkit/") || Has(s, "webkit/")) {
    r.engine = Engine::kWebKit;
    r.flavor = Flavor::kSafari;
    r.occupancy = OccupancyBus::kYetiTunnel;
    r.holepunch_mojo = false;
    r.wasm = "/wyt.wasm";
    r.dest = LeaveDest(r.flavor);
    return done();
  }
  r.dest = LeaveDest(r.flavor);
  return done();
}

}  // namespace wns
