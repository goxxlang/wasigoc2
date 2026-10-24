#include "test.h"

#include "wns/stack.h"

TEST(DetectsTorExitOnIpChange) {
  wns::Stack a(1);
  wns::Stack b(2);
  EXPECT(b.classifier().AddTorExit("185.220.101.0/24"));
  auto& sock = b.OpenWss(1);
  wns::WssConnect c;
  wns::Ipv4 home;
  EXPECT(wns::ParseIpv4("203.0.113.10", &home));
  c.ipv4.push_back(home);
  EXPECT(sock.Accept(c, 1024, 1039));
  EXPECT_EQ(sock.path().path, wns::PathClass::kClearnet);

  wns::Ipv4 exit;
  EXPECT(wns::ParseIpv4("185.220.101.42", &exit));
  auto wire = a.Announce(wns::sctp::kFamily4, wns::sctp::kOpReplace, {exit}, {});
  EXPECT_EQ(b.RecvGossip(wire), wns::GossipResult::kApplied);
  EXPECT_EQ(sock.ready_state(), wns::WssReady::kOpen);
  EXPECT_EQ(sock.path().path, wns::PathClass::kTor);
  EXPECT_EQ(b.PathOf(1).path, wns::PathClass::kTor);
}

TEST(DetectsVpnCidrOnIpChange) {
  wns::Stack a(1);
  wns::Stack b(2);
  EXPECT(b.classifier().AddVpnCidr("198.51.100.0/24"));
  auto& rpc = b.OpenRpc(1);
  wns::Ipv4 vpn;
  EXPECT(wns::ParseIpv4("198.51.100.8", &vpn));
  auto wire = a.Announce(wns::sctp::kFamily4, wns::sctp::kOpReplace, {vpn}, {});
  EXPECT_EQ(b.RecvGossip(wire), wns::GossipResult::kApplied);
  EXPECT_EQ(rpc.path().path, wns::PathClass::kVpn);
}

TEST(DetectsVpnFamilySplit) {
  wns::Classifier cls;
  EXPECT(cls.AddVpnCidr("198.51.100.0/24"));
  wns::Ipv4 v4;
  wns::Ipv6 v6;
  EXPECT(wns::ParseIpv4("198.51.100.8", &v4));
  EXPECT(wns::ParseIpv6("2001:db8::1", &v6));
  auto r = cls.Classify({v4}, {v6}, nullptr, 0, 1);
  EXPECT(r.family_split);
  EXPECT_EQ(r.path, wns::PathClass::kVpn);
}

TEST(DetectsVpnIpv6Drop) {
  wns::Classifier cls;
  wns::Ipv4 v4;
  wns::Ipv6 v6;
  EXPECT(wns::ParseIpv4("203.0.113.10", &v4));
  EXPECT(wns::ParseIpv6("2001:db8::10", &v6));
  auto before = cls.Classify({v4}, {v6}, nullptr, 0, 1);
  EXPECT_EQ(before.path, wns::PathClass::kClearnet);
  wns::Ipv4 vpn4;
  EXPECT(wns::ParseIpv4("203.0.113.99", &vpn4));
  auto after = cls.Classify({vpn4}, {}, &before, 1, 2);
  EXPECT(after.ipv6_dropped);
  EXPECT_EQ(after.path, wns::PathClass::kVpn);
}

TEST(TorPrefixBeatsClearnet) {
  wns::Classifier cls;
  EXPECT(cls.AddTorExit("2001:db8:1::/48"));
  wns::Ipv6 a;
  EXPECT(wns::ParseIpv6("2001:db8:1::53", &a));
  EXPECT_EQ(cls.Lookup6(a), wns::PathClass::kTor);
}
