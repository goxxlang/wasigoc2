#include "test.h"

#include "wow/harness.h"

TEST(HarnessOccupyWin32Thin) {
  wow::Harness h;
  EXPECT_EQ(h.FireOccupyKernel32(), WOW_RESULT_OK);
  EXPECT(h.occupancy().kernel32());
  EXPECT_EQ(h.FireOccupyVmem(), WOW_RESULT_OK);
  EXPECT(h.occupancy().vmem());
  EXPECT_EQ(h.FireOccupyPipe(), WOW_RESULT_OK);
  EXPECT(h.occupancy().pipe());
  EXPECT_EQ(h.FireThinMap(), WOW_RESULT_OK);
  EXPECT_EQ(h.FireOccupyWin32(), WOW_RESULT_OK);
  EXPECT(h.occupancy().win32());
  EXPECT(h.occupancy().hv());
  EXPECT_EQ(h.FireOccupySys(), WOW_RESULT_OK);
  EXPECT(h.occupancy().sys());
  EXPECT_EQ(h.FireOccupyDriver(), WOW_RESULT_OK);
  EXPECT(h.occupancy().win32_driver().loaded());
  EXPECT(h.occupancy().Called("NtLoadDriver"));
  EXPECT(h.occupancy().Called("ZwLoadDriver"));
}
