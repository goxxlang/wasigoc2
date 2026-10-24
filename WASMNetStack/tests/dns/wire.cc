#include "test.h"

#include "wns/dns.h"
#include "wns/stack.h"

TEST(DnsWireA) {
  auto q = wns::DnsQuery(7, "example.com", wns::kDnsTypeA);
  wns::Ipv4 a;
  EXPECT(wns::ParseIpv4("93.184.216.34", &a));
  auto ans = wns::DnsAnswerA(q, a);
  auto wire = wns::EncodeDns(ans);
  wns::DnsMessage back;
  EXPECT(wns::DecodeDns(wire, &back));
  EXPECT_EQ(back.id, 7);
  EXPECT_EQ(back.questions.size(), 1u);
  EXPECT(back.questions[0].name == "example.com");
  EXPECT_EQ(back.answers.size(), 1u);
  EXPECT(back.answers[0].a == a);
}

TEST(DosctpQueryAnswer) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  wns::Ipv4 ip;
  EXPECT(wns::ParseIpv4("192.0.2.53", &ip));
  b.DnsPutA("n.example", ip);
  EXPECT(a.DnsQuerySend("n.example", wns::kDnsTypeA));
  auto* ans = a.last_dns();
  EXPECT(ans != nullptr);
  EXPECT_EQ(ans->answers.size(), 1u);
  EXPECT(ans->answers[0].a == ip);
}
