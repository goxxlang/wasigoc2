#include "test.h"

#include "wns/c/system.h"
#include "wns/nat.h"
#include "wns/stack.h"

TEST(NatStackInitVsPacketSrc) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  wns::Ipv4 real, seen;
  EXPECT(wns::ParseIpv4("192.168.1.10", &real));
  EXPECT(wns::ParseIpv4("203.0.113.10", &seen));
  EXPECT(a.AddLocal4(real, false));
  EXPECT(a.SendInit(wns::PacketSrc::V4(seen)));
  auto scope = b.ScopeOf(1);
  EXPECT_EQ(scope.scope, wns::PathScope::kNat);
  EXPECT(scope.natted);
  EXPECT(scope.has_real);
  EXPECT(scope.has_internet);
  auto* n = b.locators().Find(1);
  EXPECT(n != nullptr);
  EXPECT(n->real4[0] == real);
  EXPECT(n->internet4[0] == seen);
}

TEST(DetectsDirectWhenInitMatchesInternet) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  wns::Ipv4 pub;
  EXPECT(wns::ParseIpv4("203.0.113.10", &pub));
  EXPECT(a.AddLocal4(pub, false));
  EXPECT(a.SendInit(wns::PacketSrc::V4(pub)));
  EXPECT_EQ(b.ScopeOf(1).scope, wns::PathScope::kDirect);
  EXPECT(!b.ScopeOf(1).natted);
}

TEST(DetectsLanWhenBothPrivate) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  wns::Ipv4 lan;
  EXPECT(wns::ParseIpv4("10.0.0.8", &lan));
  EXPECT(a.AddLocal4(lan, false));
  EXPECT(a.SendInit(wns::PacketSrc::V4(lan)));
  EXPECT_EQ(b.ScopeOf(1).scope, wns::PathScope::kLan);
}

TEST(IpifyInternetVsInitRealIsNat) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  wns::Ipv4 real;
  EXPECT(wns::ParseIpv4("192.168.0.5", &real));
  EXPECT(a.AddLocal4(real, false));
  EXPECT(a.SendInit());
  EXPECT_EQ(b.ScopeOf(1).scope, wns::PathScope::kLan);
  EXPECT(a.ApplyIpifyJson("{\"ip\":\"198.51.100.9\"}", true));
  auto scope = b.ScopeOf(1);
  EXPECT_EQ(scope.scope, wns::PathScope::kNat);
  EXPECT(scope.natted);
}

TEST(InternetOnlyFromIpify) {
  wns::Ipv4 seen;
  EXPECT(wns::ParseIpv4("203.0.113.50", &seen));
  auto r = wns::NatStack::Scope({}, {}, {seen}, {});
  EXPECT_EQ(r.scope, wns::PathScope::kInternet);
  EXPECT(r.has_internet);
  EXPECT(!r.has_real);
}

TEST(CAbiNatScope) {
  EXPECT_EQ(WnsInit(), WNS_RESULT_OK);
  WnsHandle a = WnsStackCreate(1);
  WnsHandle b = WnsStackCreate(2);
  EXPECT_EQ(WnsConnect(a, b), WNS_RESULT_OK);
  EXPECT_EQ(WnsAddLocal4(a, "192.168.1.10", 0), WNS_RESULT_OK);
  EXPECT_EQ(WnsSendInit(a, "203.0.113.10"), WNS_RESULT_OK);
  uint32_t scope = 0;
  EXPECT_EQ(WnsPathScope(b, 1, &scope), WNS_RESULT_OK);
  EXPECT_EQ(scope, WNS_SCOPE_NAT);
  WnsClose(a);
  WnsClose(b);
  WnsShutdown();
}
