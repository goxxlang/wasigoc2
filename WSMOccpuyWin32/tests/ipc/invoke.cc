#include "test.h"

#include "wow/ipc.h"

TEST(IpcOccupyWin32) {
  auto i = wow::Ipc::OccupyWin32();
  EXPECT(i.name == "occupyWin32");
  EXPECT(i.ordinal != 0);
  EXPECT(i.is_win32());
  auto json = i.ToJson();
  wow::Ipc parsed;
  EXPECT(wow::Ipc::FromJson(json, &parsed));
  EXPECT(parsed.name == "occupyWin32");
}
