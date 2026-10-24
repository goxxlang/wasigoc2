#include "test.h"

#include "wns/gossip.h"
#include "wns/locator.h"

TEST(GossipIpChangeRoundtrip) {
  wns::GossipControl g;
  g.originator = 7;
  g.seq = 3;
  wns::IpChangeTlv t4;
  t4.family = wns::sctp::kFamily4;
  t4.op = wns::sctp::kOpReplace;
  t4.flags = wns::sctp::kIpChangeAuth;
  wns::Ipv4 a;
  EXPECT(wns::ParseIpv4("203.0.113.10", &a));
  t4.ipv4.push_back(a);
  wns::IpChangeTlv t6;
  t6.family = wns::sctp::kFamily6;
  t6.op = wns::sctp::kOpReplace;
  t6.flags = wns::sctp::kIpChangeAuth;
  wns::Ipv6 b;
  EXPECT(wns::ParseIpv6("2001:db8::10", &b));
  t6.ipv6.push_back(b);
  g.changes.push_back(t4);
  g.changes.push_back(t6);

  auto wire = wns::EncodeGossipControl(g);
  EXPECT(wire.size() >= 16);
  EXPECT_EQ(wire[0], wns::sctp::kChunkGossipControl);

  wns::GossipControl back;
  EXPECT(wns::DecodeGossipControl(wire, &back));
  EXPECT_EQ(back.originator, 7u);
  EXPECT_EQ(back.seq, 3u);
  EXPECT_EQ(back.changes.size(), 2u);
  EXPECT_EQ(back.changes[0].ipv4.size(), 1u);
  EXPECT(back.changes[0].ipv4[0] == a);
  EXPECT_EQ(back.changes[1].ipv6.size(), 1u);
  EXPECT(back.changes[1].ipv6[0] == b);
}

TEST(GossipApplyUpdatesLocators) {
  wns::LocatorTable table;
  wns::GossipControl g;
  g.originator = 9;
  g.seq = 1;
  wns::IpChangeTlv t;
  t.family = wns::sctp::kFamily4;
  t.op = wns::sctp::kOpAdd;
  t.flags = wns::sctp::kIpChangeAuth;
  wns::Ipv4 a;
  EXPECT(wns::ParseIpv4("192.0.2.1", &a));
  t.ipv4.push_back(a);
  g.changes.push_back(t);
  EXPECT_EQ(wns::ApplyGossip(&table, g), wns::GossipResult::kApplied);
  auto* n = table.Find(9);
  EXPECT(n != nullptr);
  EXPECT_EQ(n->ipv4.size(), 1u);
  EXPECT(n->ipv4[0] == a);
  EXPECT_EQ(wns::ApplyGossip(&table, g), wns::GossipResult::kDuplicate);
}

TEST(GossipRejectsUnauth) {
  wns::LocatorTable table;
  wns::GossipControl g;
  g.originator = 1;
  g.seq = 1;
  wns::IpChangeTlv t;
  t.family = wns::sctp::kFamily4;
  t.op = wns::sctp::kOpReplace;
  t.flags = 0;
  wns::Ipv4 a;
  EXPECT(wns::ParseIpv4("192.0.2.8", &a));
  t.ipv4.push_back(a);
  g.changes.push_back(t);
  EXPECT_EQ(wns::ApplyGossip(&table, g), wns::GossipResult::kUnauth);
  EXPECT(table.Find(1) == nullptr);
}
