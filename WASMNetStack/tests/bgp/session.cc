#include "test.h"

#include "wns/stack.h"

TEST(BgpOpenEstablished) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  EXPECT(a.BgpOpen());
  EXPECT_EQ(a.bgp().state(), wns::BgpState::kEstablished);
  EXPECT_EQ(b.bgp().state(), wns::BgpState::kEstablished);
  EXPECT_EQ(b.bgp().peer_as(), 1);
}

TEST(BgpIpv4Update) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  a.BgpOpen();
  wns::BgpUpdate u;
  wns::BgpPrefix4 p;
  EXPECT(wns::ParseIpv4("192.0.2.0", &p.addr));
  p.plen = 24;
  u.nlri4.push_back(p);
  EXPECT(a.BgpAdvertise(u));
  EXPECT_EQ(b.bgp().rib4().size(), 1u);
  EXPECT_EQ(b.bgp().rib4()[0].plen, 24);
}
