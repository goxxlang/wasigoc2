#include "test.h"

#include "wns/anycast.h"
#include "wns/c/system.h"
#include "wns/stack.h"

#include <string>

TEST(AnycastBgpVipAndWasmdnsPin) {
  wns::Stack a(1);
  wns::Stack b(2);
  wns::Anycast plane;
  plane.Bind(&a, &b);
  EXPECT(plane.Advertise("203.0.113.53/32", "203.0.113.53"));
  EXPECT_EQ(plane.rib().size(), 1u);
  EXPECT_EQ(b.bgp().rib4().size(), 1u);
  EXPECT_EQ(b.bgp().rib4()[0].plen, 32);
  EXPECT(plane.Pin("ns.shadow"));
  wns::Ipv4 vip;
  EXPECT(plane.Lookup("ns.shadow", &vip));
  EXPECT(wns::FormatIpv4(vip) == "203.0.113.53");
  auto* ans = a.last_dns();
  EXPECT(ans != nullptr);
  EXPECT_EQ(ans->answers.size(), 1u);
  EXPECT(ans->answers[0].a == vip);
}

TEST(AnycastSeedCCCMatchesWasmdns) {
  wns::Anycast plane;
  plane.SeedCCC();
  wns::Ipv4 ip;
  EXPECT(plane.Lookup("example.com", &ip));
  EXPECT(plane.Lookup("www.example.com", &ip));
  EXPECT(plane.Lookup("bank.example.net", &ip));
  EXPECT(plane.Lookup("phisheries.dev", &ip));
  EXPECT(!plane.Lookup("not-pinned.invalid", &ip));
}

TEST(CAbiAnycastPlane) {
  EXPECT_EQ(WnsInit(), WNS_RESULT_OK);
  WnsHandle a = WnsStackCreate(1);
  WnsHandle b = WnsStackCreate(2);
  EXPECT_EQ(WnsConnect(a, b), WNS_RESULT_OK);
  WnsHandle p = WnsAnycastCreate(a, b);
  EXPECT(p != WNS_HANDLE_INVALID);
  EXPECT_EQ(WnsAnycastAdvertise(p, "203.0.113.53/32", "203.0.113.53"), WNS_RESULT_OK);
  EXPECT_EQ(WnsAnycastSeedCCC(p), WNS_RESULT_OK);
  char ip[32];
  EXPECT_EQ(WnsAnycastLookup(p, "login.example.org", ip, sizeof(ip)), WNS_RESULT_OK);
  EXPECT(std::string(ip) == "203.0.113.53");
  WnsClose(a);
  WnsClose(b);
  WnsShutdown();
}
