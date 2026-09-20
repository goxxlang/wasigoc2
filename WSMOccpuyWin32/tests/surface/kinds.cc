#include "test.h"

#include "wow/surface.h"

#include <string>

TEST(KindIsolationThin) {
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kKernel32) == wow::Isolation::kHost);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kNtdll) == wow::Isolation::kSys);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kUser32) ==
         wow::Isolation::kCompositor);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kPipe) == wow::Isolation::kIpc);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kVmem) == wow::Isolation::kHost);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kWinHv) == wow::Isolation::kHv);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kWs2) == wow::Isolation::kNet);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kWsl) == wow::Isolation::kTty);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kHandle) == wow::Isolation::kSfi);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kOccupancyDriver) ==
         wow::Isolation::kSys);
  EXPECT(wow::IsolationOf(wow::SurfaceKind::kCmd) == wow::Isolation::kShell);
  EXPECT(wow::IsThin(wow::SurfaceKind::kKernel32));
  EXPECT(wow::IsThin(wow::SurfaceKind::kVmem));
  EXPECT(!wow::IsThin(wow::SurfaceKind::kHandle));
  EXPECT(std::string(wow::KindToString(wow::SurfaceKind::kKernel32)) ==
         "kernel32");
  EXPECT(wow::KindFromDll("ntdll") == wow::SurfaceKind::kNtdll);
  EXPECT(wow::IsolationOfDll("ws2_32") == wow::Isolation::kNet);
}

TEST(ThinMapHasHostSysIpc) {
  auto json = wow::ThinMapJson();
  EXPECT(json.find("\"thin\":true") != std::string::npos);
  EXPECT(json.find("host") != std::string::npos);
  EXPECT(json.find("sys") != std::string::npos);
  EXPECT(json.find("ipc") != std::string::npos);
  EXPECT(json.find("compositor") != std::string::npos);
  EXPECT(json.find("hv") != std::string::npos);
  EXPECT(json.find("kernel32") != std::string::npos);
  EXPECT(json.find("vmem") != std::string::npos);
  EXPECT(json.find("shell") != std::string::npos);
  EXPECT(json.find("tty") != std::string::npos);
  EXPECT(json.find("net") != std::string::npos);
}
