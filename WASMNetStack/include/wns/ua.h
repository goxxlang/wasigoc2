#ifndef WNS_UA_H_
#define WNS_UA_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace wns {

// Occupancy bus for a tagged browser. YetiTunnel replaces HolePunch on
// WebKit only. Firefox still uses HolePunch for Mojo because Gecko vendors
// Chromium ipc/chromium/src/mojo/core; chrome:// / XPConnect is MothmanBridge
// (WASMMothmanCompile). Chromium-core flavors (Chrome, Edge, Opera, Brave,
// Vivaldi) stay on HolePunch.
enum class Engine : uint8_t { kUnknown = 0, kWebKit = 1, kGecko = 2, kChromium = 3 };

enum class Flavor : uint8_t {
  kNone = 0,
  kChrome = 1,
  kEdge = 2,
  kOpera = 3,
  kBrave = 4,
  kVivaldi = 5,
  kFirefox = 6,
  kSafari = 7,
  kTor = 8,
};

enum class OccupancyBus : uint8_t {
  kNone = 0,
  kYetiTunnel = 1,
  kMothmanBridge = 2,
  kHolePunch = 3,
};

enum class HostOS : uint8_t {
  kUnknown = 0,
  kWindows = 1,
  kLinux = 2,
  kAndroid = 3,
  kMacOS = 4,
  kIOS = 5,
  kChromeOS = 6,
};

enum class GpuRoute : uint8_t {
  kNone = 0,
  kWindows = 1,
  kLinux = 2,
  kAndroid = 3,
};

inline constexpr uint32_t kOrdOccupyDxgi = 2068842960u;
inline constexpr uint32_t kOrdOccupyLinux = 1633927358u;
inline constexpr uint32_t kOrdOccupyAndroid = 894379151u;

inline constexpr const char kDestDuck[] = "https://duck.ai/";
inline constexpr const char kDestClaude[] = "https://claude.ai/";
inline constexpr const char kDestCopilot[] = "https://copilot.microsoft.com/";
inline constexpr const char kDestGemini[] = "https://gemini.google.com/app";

inline const char* LeaveDest(Flavor f) {
  switch (f) {
    case Flavor::kTor:
      return kDestDuck;
    case Flavor::kFirefox:
      return kDestClaude;
    case Flavor::kEdge:
      return kDestCopilot;
    default:
      return kDestGemini;
  }
}

struct UaRoute {
  Engine engine = Engine::kUnknown;
  Flavor flavor = Flavor::kNone;
  OccupancyBus occupancy = OccupancyBus::kNone;
  bool holepunch_mojo = false;
  bool mothman_chrome = false;
  const char* wasm = "";
  const char* dest = kDestGemini;
  HostOS os = HostOS::kUnknown;
  GpuRoute gpu = GpuRoute::kNone;
  const char* gpu_topic = "";
  uint32_t gpu_ordinal = 0;
  const char* gpu_via = "";
};

inline const char* EngineName(Engine e) {
  switch (e) {
    case Engine::kWebKit:
      return "webkit";
    case Engine::kGecko:
      return "gecko";
    case Engine::kChromium:
      return "chromium";
    default:
      return "unknown";
  }
}

inline const char* FlavorName(Flavor f) {
  switch (f) {
    case Flavor::kChrome:
      return "chrome";
    case Flavor::kEdge:
      return "edge";
    case Flavor::kOpera:
      return "opera";
    case Flavor::kBrave:
      return "brave";
    case Flavor::kVivaldi:
      return "vivaldi";
    case Flavor::kFirefox:
      return "firefox";
    case Flavor::kSafari:
      return "safari";
    case Flavor::kTor:
      return "tor";
    default:
      return "none";
  }
}

inline const char* OccupancyName(OccupancyBus b) {
  switch (b) {
    case OccupancyBus::kYetiTunnel:
      return "yeti";
    case OccupancyBus::kMothmanBridge:
      return "mothman";
    case OccupancyBus::kHolePunch:
      return "holepunch";
    default:
      return "none";
  }
}

UaRoute RouteUserAgent(std::string_view ua);

}  // namespace wns

#endif  // WNS_UA_H_
