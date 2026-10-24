#include "test.h"

#include "wns/proxy.h"

#include <string>

TEST(ProxyTargetParse) {
  wns::ProxyTarget t;
  EXPECT(wns::ParseProxyTarget("192.0.2.9:443", &t));
  EXPECT_EQ(t.kind, wns::sctp::kProxyAddr4);
  EXPECT_EQ(t.port, 443);
  EXPECT(t.Text() == "192.0.2.9:443");

  EXPECT(wns::ParseProxyTarget("[2001:db8::1]:53", &t));
  EXPECT_EQ(t.kind, wns::sctp::kProxyAddr6);
  EXPECT_EQ(t.port, 53);

  EXPECT(wns::ParseProxyTarget("api.example.com:80", &t));
  EXPECT_EQ(t.kind, wns::sctp::kProxyAddrName);
  EXPECT(t.name == "api.example.com");
  EXPECT(t.Text() == "api.example.com:80");

  EXPECT(!wns::ParseProxyTarget("api.example.com", &t));
  EXPECT(!wns::ParseProxyTarget("api.example.com:70000", &t));
  EXPECT(!wns::ParseProxyTarget("api.example.com:http", &t));
}

TEST(ProxyTargetRoundtrip) {
  std::vector<uint8_t> w;
  auto name = wns::ProxyTarget::Name("db.internal", 5432);
  wns::PutProxyTarget(&w, name);
  wns::ProxyTarget back;
  size_t used = 0;
  EXPECT(wns::GetProxyTarget(w, &back, &used));
  EXPECT_EQ(used, w.size());
  EXPECT(back == name);

  wns::Ipv6 a6;
  EXPECT(wns::ParseIpv6("2001:db8::7", &a6));
  auto v6 = wns::ProxyTarget::V6(a6, 853);
  w.clear();
  wns::PutProxyTarget(&w, v6);
  EXPECT(wns::GetProxyTarget(w, &back, &used));
  EXPECT_EQ(used, 20u);
  EXPECT(back == v6);
}

TEST(ProxyFrameRoundtrip) {
  wns::ProxyFrame f;
  f.type = wns::sctp::kProxyData;
  f.channel = 0x8042;
  f.flags = 0;
  f.proto = wns::sctp::kProxyTcp;
  f.stream = wns::sctp::kStreamProxyMin + 4;
  f.payload = {'G', 'E', 'T', ' ', '/'};
  auto w = wns::EncodeProxyFrame(f);
  EXPECT_EQ(w.size(), wns::kProxyHeaderSize + 5);

  wns::ProxyFrame g;
  EXPECT(wns::DecodeProxyFrame(w, &g));
  EXPECT_EQ(g.version, wns::sctp::kProxyVersion);
  EXPECT_EQ(g.type, wns::sctp::kProxyData);
  EXPECT_EQ(g.channel, 0x8042);
  EXPECT_EQ(g.proto, wns::sctp::kProxyTcp);
  EXPECT_EQ(g.stream, wns::sctp::kStreamProxyMin + 4);
  EXPECT_EQ(g.payload.size(), 5u);
  EXPECT_EQ(g.payload[0], 'G');

  // A header that claims more payload than it carries is rejected.
  w.resize(w.size() - 1);
  EXPECT(!wns::DecodeProxyFrame(w, &g));
  EXPECT(!wns::DecodeProxyFrame(std::span<const uint8_t>(w.data(), 4), &g));
}

TEST(ProxyStreamBlockIsDisjoint) {
  EXPECT(wns::sctp::IsProxyStream(wns::sctp::kStreamProxyMin));
  EXPECT(wns::sctp::IsProxyStream(wns::sctp::kStreamProxyMax));
  EXPECT(!wns::sctp::IsProxyStream(wns::sctp::kStreamRpcMax));
  EXPECT(!wns::sctp::IsProxyStream(wns::sctp::kStreamGlsMin));
  EXPECT(!wns::sctp::IsRpcStream(wns::sctp::kStreamProxyMin));
  EXPECT(!wns::sctp::IsWssStream(wns::sctp::kStreamProxyMin));

  wns::ProxyMux client(false);
  wns::ProxyMux server(true);
  // Client and server lanes never share a stream.
  for (uint16_t i = 1; i < 64; ++i) {
    uint16_t cs = client.StreamFor(i);
    uint16_t ss = server.StreamFor(uint16_t(i | wns::sctp::kProxyServerBit));
    EXPECT(wns::sctp::IsProxyStream(cs));
    EXPECT(wns::sctp::IsProxyStream(ss));
    EXPECT(cs != ss);
  }
}
