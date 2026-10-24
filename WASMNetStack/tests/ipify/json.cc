#include "test.h"

#include "wns/c/system.h"
#include "wns/ipify.h"
#include "wns/stack.h"

#include <string>

TEST(IpifyJsonRoundtrip) {
  auto js = wns::EncodeIpifyJson("203.0.113.10");
  EXPECT(js == "{\"ip\":\"203.0.113.10\"}");
  wns::IpifyReply r;
  EXPECT(wns::DecodeIpifyJson(js, &r));
  EXPECT(!r.v6);
  EXPECT(r.ip == "203.0.113.10");
  wns::Ipv4 a;
  EXPECT(wns::ParseIpv4("203.0.113.10", &a));
  EXPECT(r.v4 == a);
}

TEST(IpifyJsonIpv6) {
  wns::IpifyReply r;
  EXPECT(wns::DecodeIpifyJson("{\"ip\":\"2001:db8::10\"}", &r));
  EXPECT(r.v6);
  wns::Ipv6 a;
  EXPECT(wns::ParseIpv6("2001:db8::10", &a));
  EXPECT(r.ipv6 == a);
}

TEST(IpifyClientIsOurs) {
  std::string js(wns::kIpifyClientJs);
  EXPECT(js.find("api.ipify.org") == std::string::npos);
  EXPECT(js.find("/?format=json") != std::string::npos);
  EXPECT(js.find("cache: \"no-store\"") != std::string::npos);
  EXPECT(js.find("Visible IP:") != std::string::npos);
  auto http = wns::IpifyHttpJson("192.0.2.1");
  EXPECT(http.find("Cache-Control: no-store") != std::string::npos);
  EXPECT(http.find("{\"ip\":\"192.0.2.1\"}") != std::string::npos);
}

TEST(IpifyApplyGossipsLocator) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  EXPECT(a.ApplyIpifyJson("{\"ip\":\"198.51.100.7\"}", true));
  auto vis = a.VisibleJson();
  EXPECT(vis == "{\"ip\":\"198.51.100.7\"}");
  auto* n = b.locators().Find(1);
  EXPECT(n != nullptr);
  wns::Ipv4 want;
  EXPECT(wns::ParseIpv4("198.51.100.7", &want));
  EXPECT(n->ipv4[0] == want);
}

TEST(CAbiIpify) {
  EXPECT_EQ(WnsInit(), WNS_RESULT_OK);
  WnsHandle h = WnsStackCreate(3);
  EXPECT(h != WNS_HANDLE_INVALID);
  EXPECT_EQ(WnsApplyIpify(h, "{\"ip\":\"192.0.2.8\"}", 1), WNS_RESULT_OK);
  char buf[64];
  EXPECT_EQ(WnsVisibleJson(h, buf, sizeof(buf)), WNS_RESULT_OK);
  EXPECT(std::string(buf) == "{\"ip\":\"192.0.2.8\"}");
  WnsClose(h);
  WnsShutdown();
}
