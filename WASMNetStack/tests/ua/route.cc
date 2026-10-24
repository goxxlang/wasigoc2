#include "test.h"

#include "wns/c/system.h"
#include "wns/ua.h"

#include <string>

TEST(UaTorBrowserMothmanOnSpeaker) {
  auto r = wns::RouteUserAgent(
      "Mozilla/5.0 (Windows NT 10.0; rv:128.0) Gecko/20100101 Firefox/128.0");
  EXPECT_EQ(r.engine, wns::Engine::kGecko);
  EXPECT_EQ(r.flavor, wns::Flavor::kTor);
  EXPECT_EQ(r.occupancy, wns::OccupancyBus::kMothmanBridge);
  EXPECT(r.holepunch_mojo);
  EXPECT(r.mothman_chrome);
  EXPECT(std::string(r.wasm) == "/mothman.wasm");
  EXPECT(std::string(r.dest) == wns::kDestDuck);
}

TEST(UaTorBrowser15Win64IsTor) {
  auto r = wns::RouteUserAgent(
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:140.0) Gecko/20100101 Firefox/140.0");
  EXPECT_EQ(r.engine, wns::Engine::kGecko);
  EXPECT_EQ(r.flavor, wns::Flavor::kTor);
  EXPECT_EQ(r.occupancy, wns::OccupancyBus::kMothmanBridge);
  EXPECT(std::string(r.dest) == wns::kDestDuck);
}

TEST(UaFirefoxMothmanKeepsHolePunchMojo) {
  auto r = wns::RouteUserAgent(
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:122.0) Gecko/20100101 Firefox/122.0");
  EXPECT_EQ(r.engine, wns::Engine::kGecko);
  EXPECT_EQ(r.flavor, wns::Flavor::kFirefox);
  EXPECT_EQ(r.occupancy, wns::OccupancyBus::kMothmanBridge);
  EXPECT(r.holepunch_mojo);
  EXPECT(r.mothman_chrome);
  EXPECT(std::string(r.wasm) == "/mothman.wasm");
  EXPECT(std::string(r.dest) == wns::kDestClaude);
}

TEST(UaOperaIsChromiumHolePunch) {
  auto r = wns::RouteUserAgent(
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/192.168.1.5 Safari/537.36 OPR/121.0.0.0");
  EXPECT_EQ(r.engine, wns::Engine::kChromium);
  EXPECT_EQ(r.flavor, wns::Flavor::kOpera);
  EXPECT_EQ(r.occupancy, wns::OccupancyBus::kHolePunch);
  EXPECT(r.holepunch_mojo);
  EXPECT(!r.mothman_chrome);
  EXPECT(std::string(r.wasm) == "/whp.wasm");
  EXPECT(std::string(r.dest) == wns::kDestGemini);
}

TEST(UaSafariYetiReplacesHolePunch) {
  auto r = wns::RouteUserAgent(
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0) AppleWebKit/605.1.15 (KHTML, like Gecko) "
      "Version/17.0 Safari/605.1.15");
  EXPECT_EQ(r.engine, wns::Engine::kWebKit);
  EXPECT_EQ(r.flavor, wns::Flavor::kSafari);
  EXPECT_EQ(r.occupancy, wns::OccupancyBus::kYetiTunnel);
  EXPECT(!r.holepunch_mojo);
  EXPECT(std::string(r.wasm) == "/wyt.wasm");
  EXPECT(std::string(r.dest) == wns::kDestGemini);
}

TEST(UaChromeAndEdgeStayHolePunch) {
  auto chrome = wns::RouteUserAgent(
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/122.0.0.0 Safari/537.36");
  EXPECT_EQ(chrome.flavor, wns::Flavor::kChrome);
  EXPECT_EQ(chrome.occupancy, wns::OccupancyBus::kHolePunch);
  auto edge = wns::RouteUserAgent(
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/122.0.0.0 Safari/537.36 Edg/122.0.0.0");
  EXPECT_EQ(edge.flavor, wns::Flavor::kEdge);
  EXPECT_EQ(edge.occupancy, wns::OccupancyBus::kHolePunch);
  EXPECT(std::string(chrome.dest) == wns::kDestGemini);
  EXPECT(std::string(edge.dest) == wns::kDestCopilot);
  EXPECT_EQ(chrome.os, wns::HostOS::kWindows);
  EXPECT_EQ(chrome.gpu, wns::GpuRoute::kWindows);
  EXPECT(std::string(chrome.gpu_topic) == "occupyDxgi");
  EXPECT_EQ(chrome.gpu_ordinal, wns::kOrdOccupyDxgi);
  EXPECT(std::string(chrome.gpu_via) == "woggpu.sys");
}

TEST(UaLinuxAndAndroidGpuRoutes) {
  auto lin = wns::RouteUserAgent(
      "Mozilla/5.0 (X11; Linux x86_64; rv:122.0) Gecko/20100101 Firefox/122.0");
  EXPECT_EQ(lin.os, wns::HostOS::kLinux);
  EXPECT_EQ(lin.gpu, wns::GpuRoute::kLinux);
  EXPECT(std::string(lin.gpu_topic) == "occupyLinux");
  EXPECT(std::string(lin.gpu_via) == "woggpu.ko");
  auto andr = wns::RouteUserAgent(
      "Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/122.0.0.0 Mobile Safari/537.36");
  EXPECT_EQ(andr.os, wns::HostOS::kAndroid);
  EXPECT_EQ(andr.gpu, wns::GpuRoute::kAndroid);
  EXPECT(std::string(andr.gpu_topic) == "occupyAndroid");
}

TEST(CAbiRouteUserAgent) {
  uint32_t engine = 0, flavor = 0, occupancy = 0, flags = 0;
  char wasm[32];
  EXPECT_EQ(WnsRouteUserAgent("Mozilla/5.0 Firefox/128.0", &engine, &flavor, &occupancy, &flags,
                              wasm, sizeof(wasm)),
            WNS_RESULT_OK);
  EXPECT_EQ(engine, WNS_ENGINE_GECKO);
  EXPECT_EQ(flavor, WNS_FLAVOR_FIREFOX);
  EXPECT_EQ(occupancy, WNS_OCCUPANCY_MOTHMAN);
  EXPECT((flags & WNS_UA_HOLEPUNCH_MOJO) != 0);
  EXPECT((flags & WNS_UA_MOTHMAN_CHROME) != 0);
  EXPECT(std::string(wasm) == "/mothman.wasm");
}
