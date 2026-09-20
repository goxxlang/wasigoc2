#include "test.h"

#include "wow/gocvm.h"
#include "wow/occupancy.h"

#include <string>

TEST(GocvmBridgeOwnsWin32) {
  EXPECT(wow::OccupancyBridge::OwnsTopic("win32.occupyWin32"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyCatalog"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyVmem"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyHv"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyDriver"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyLoadDriver"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyToken"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyRegistry"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyScm"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyCom"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyDevice"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyCmd"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyGocvm"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("occupyCalc"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("thinMap"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("win32"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("wsl"));
  EXPECT(wow::OccupancyBridge::OwnsTopic("nix"));
  EXPECT(!wow::OccupancyBridge::OwnsTopic("skia.occupySkia"));
  EXPECT(!wow::OccupancyBridge::OwnsTopic("mojo.occupyMojo"));
  EXPECT(!wow::OccupancyBridge::OwnsTopic("webgpu.occupyGpu"));
  EXPECT(!wow::OccupancyBridge::OwnsTopic("os.exec"));
  EXPECT(!wow::OccupancyBridge::OwnsTopic("v8.sandbox"));
  EXPECT(!wow::OccupancyBridge::OwnsTopic("wst.createMessagePipe"));
  EXPECT(!wow::OccupancyBridge::OwnsTopic("tty.hello"));
  EXPECT(!wow::OccupancyBridge::OwnsTopic("vtpm.attest"));
}

TEST(GocvmBridgeCall) {
  wow::Occupancy o;
  wow::OccupancyBridge b(&o);
  std::string reply;
  std::string err;
  EXPECT(b.Call("win32.occupyKernel32", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"host\"") != std::string::npos);
  EXPECT(b.Call("win32.occupyVmem", "", &reply, &err));
  EXPECT(reply.find("\"isolation\":\"host\"") != std::string::npos);
#if defined(WOW_HAS_WIN32)
  EXPECT(b.Call("win32", "GetCurrentProcessId", &reply, &err));
  EXPECT(!reply.empty());
  EXPECT(reply.find("error:") == std::string::npos);
  EXPECT(o.gocvm());
  EXPECT(o.StatusJson().find("\"toolkit\":\"gocvm\"") != std::string::npos);
#endif
}

#if defined(WOW_HAS_GOCVM)
#include "runtime.hpp"

TEST(GocvmRuntimeRegister) {
  wow::Occupancy o;
  wow::RegisterOccupancyHostBridge(&o);
  auto r = wasigo::gocvm::Call("win32.occupyWin32", "");
  EXPECT(r.r1 == nullptr);
  EXPECT(r.r0.find("\"thin\":true") != std::string::npos);
  auto t = wasigo::gocvm::Call("win32.thinMap", "");
  EXPECT(t.r1 == nullptr);
  EXPECT(t.r0.find("host") != std::string::npos);
  wow::UnregisterOccupancyHostBridge();
}
#endif
