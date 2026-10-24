#include "test.h"

#include "wns/gls.h"

TEST(GlsPushPop) {
  wns::Gls g;
  g.SetIlm({1, wns::LabelOp::kPop, 0, 0, 8192});
  uint8_t user[] = {9, 8, 7};
  auto d = g.Push(1, user);
  std::vector<uint8_t> delivered;
  EXPECT_EQ(g.Process(&d, &delivered, nullptr), wns::GlsResult::kDeliver);
  EXPECT_EQ(delivered.size(), 3u);
  EXPECT_EQ(delivered[0], 9);
}

TEST(GlsSwapTtl) {
  wns::Gls g;
  g.SetIlm({1, wns::LabelOp::kSwap, 2, 0, 32768});
  uint8_t user[] = {1};
  auto d = g.Push(1, user, 2);
  std::vector<uint8_t> delivered;
  EXPECT_EQ(g.Process(&d, &delivered, nullptr), wns::GlsResult::kForward);
  EXPECT_EQ(d.stack.front().value, 2u);
  EXPECT_EQ(d.stack.front().ttl, 1);
}

TEST(GlsDropsBadDepth) {
  wns::Gls g;
  g.SetIlm({1, wns::LabelOp::kPop, 0, 0, 0});
  wns::LabelData d;
  EXPECT_EQ(g.Process(&d, nullptr, nullptr), wns::GlsResult::kDrop);
}
