#include "test.h"

#include "wns/addr.h"

TEST(ParseIpv4Dotted) {
  wns::Ipv4 a;
  EXPECT(wns::ParseIpv4("203.0.113.10", &a));
  EXPECT_EQ(a.o[0], 203);
  EXPECT_EQ(a.o[3], 10);
  EXPECT(wns::FormatIpv4(a) == "203.0.113.10");
}

TEST(ParseIpv6Compressed) {
  wns::Ipv6 a;
  EXPECT(wns::ParseIpv6("2001:db8::10", &a));
  EXPECT_EQ(a.o[0], 0x20);
  EXPECT_EQ(a.o[1], 0x01);
  EXPECT_EQ(a.o[2], 0x0d);
  EXPECT_EQ(a.o[3], 0xb8);
  EXPECT_EQ(a.o[14], 0);
  EXPECT_EQ(a.o[15], 0x10);
  wns::Ipv6 b;
  EXPECT(wns::ParseIpv6(wns::FormatIpv6(a), &b));
  EXPECT(a == b);
}

TEST(Ipv4PrefixMatch) {
  wns::Ipv4 a, net;
  EXPECT(wns::ParseIpv4("198.51.100.20", &a));
  EXPECT(wns::ParseIpv4("198.51.100.0", &net));
  EXPECT(wns::Ipv4PrefixMatch(a, net, 24));
  EXPECT(!wns::Ipv4PrefixMatch(a, net, 32));
}
