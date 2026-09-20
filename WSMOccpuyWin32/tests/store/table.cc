#include "test.h"

#include "wow/store.h"

TEST(TableOpenFind) {
  wow::OccupancyTable t;
  auto c = t.Open(wow::SurfaceKind::kKernel32, "k32");
  EXPECT(c.id == 1);
  EXPECT(c.isolation == wow::Isolation::kHost);
  auto f = t.Find(1);
  EXPECT(f.has_value());
  EXPECT(f->label == "k32");
  EXPECT(t.OfKind(wow::SurfaceKind::kKernel32).size() == 1u);
}
