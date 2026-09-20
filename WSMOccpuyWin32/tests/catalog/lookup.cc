#include "test.h"

#include "wow/catalog.h"

TEST(DriveOwnsWin32Topics) {
  auto c = wow::Catalog::Drive();
  uint32_t ord = 0;
  EXPECT(c.Lookup("occupyWin32", &ord, nullptr));
  EXPECT(c.MayFire(ord));
  EXPECT(c.Lookup("win32.occupyCatalog", &ord, nullptr));
  EXPECT(c.Lookup("occupyKernel32", &ord, nullptr));
  EXPECT(c.Lookup("occupyNtdll", &ord, nullptr));
  EXPECT(c.Lookup("occupyVmem", &ord, nullptr));
  EXPECT(c.Lookup("occupyPipe", &ord, nullptr));
  EXPECT(c.Lookup("occupyHv", &ord, nullptr));
  EXPECT(c.Lookup("occupySys", &ord, nullptr));
  EXPECT(c.Lookup("occupyDriver", &ord, nullptr));
  EXPECT(c.Lookup("occupyLoadDriver", &ord, nullptr));
  EXPECT(c.Lookup("occupyOle32", &ord, nullptr));
  EXPECT(c.Lookup("occupyCom", &ord, nullptr));
  EXPECT(c.Lookup("occupyToken", &ord, nullptr));
  EXPECT(c.Lookup("occupyRegistry", &ord, nullptr));
  EXPECT(c.Lookup("occupyScm", &ord, nullptr));
  EXPECT(c.Lookup("occupyDevice", &ord, nullptr));
  EXPECT(c.Lookup("occupyCmd", &ord, nullptr));
  EXPECT(c.Lookup("occupyWasmtty", &ord, nullptr));
  EXPECT(c.Lookup("occupyGocvm", &ord, nullptr));
  EXPECT(c.Lookup("occupyCalc", &ord, nullptr));
  EXPECT(c.Lookup("thinMap", &ord, nullptr));
  EXPECT(ord == wow::Fnv1a31("thinMap"));
}

TEST(DoesNotOwnStolenTopics) {
  auto c = wow::Catalog::Drive();
  uint32_t ord = 0;
  EXPECT(!c.Lookup("skia.occupySkia", &ord, nullptr));
  EXPECT(!c.Lookup("mojo.occupyMojo", &ord, nullptr));
  EXPECT(!c.Lookup("webgpu.occupyGpu", &ord, nullptr));
  EXPECT(!c.Lookup("os.exec", &ord, nullptr));
  EXPECT(!c.Lookup("v8.sandbox", &ord, nullptr));
  EXPECT(!c.Lookup("wst.createMessagePipe", &ord, nullptr));
  EXPECT(!c.Lookup("tty.hello", &ord, nullptr));
  EXPECT(!c.Lookup("net.call", &ord, nullptr));
  EXPECT(!c.Lookup("vtpm.attest", &ord, nullptr));
}
