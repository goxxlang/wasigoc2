#include "test.h"

#include "wns/stack.h"

TEST(WssStaysOpenOnIpChange) {
  wns::Stack a(1);
  wns::Stack b(2);
  auto& sock = b.OpenWss(1);
  wns::WssConnect c;
  c.resource = "/v1/telemetry";
  c.host = "api.example.com";
  c.requested_streams = 16;
  wns::Ipv4 ip;
  EXPECT(wns::ParseIpv4("203.0.113.10", &ip));
  c.ipv4.push_back(ip);
  EXPECT(sock.Accept(c, wns::sctp::kStreamWssMin, wns::sctp::kStreamWssMin + 15));
  EXPECT_EQ(sock.ready_state(), wns::WssReady::kOpen);
  uint16_t smin = sock.stream_min();
  uint16_t smax = sock.stream_max();

  wns::Ipv4 next;
  EXPECT(wns::ParseIpv4("203.0.113.99", &next));
  auto wire = a.Announce(wns::sctp::kFamily4, wns::sctp::kOpReplace, {next}, {});
  EXPECT_EQ(b.RecvGossip(wire), wns::GossipResult::kApplied);
  EXPECT_EQ(sock.ready_state(), wns::WssReady::kOpen);
  EXPECT_EQ(sock.stream_min(), smin);
  EXPECT_EQ(sock.stream_max(), smax);
  auto* n = b.locators().Find(1);
  EXPECT(n != nullptr);
  EXPECT(n->ipv4[0] == next);
}

TEST(WssConnectLocatorsJson) {
  wns::WssConnect c;
  c.resource = "/v1/telemetry";
  c.host = "api.example.com";
  c.protocols.push_back("json-rpc");
  c.requested_streams = 16;
  wns::Ipv4 v4;
  wns::Ipv6 v6;
  EXPECT(wns::ParseIpv4("203.0.113.10", &v4));
  EXPECT(wns::ParseIpv6("2001:db8::10", &v6));
  c.ipv4.push_back(v4);
  c.ipv6.push_back(v6);
  auto js = wns::EncodeWssConnect(c);
  wns::WssConnect back;
  EXPECT(wns::DecodeWssConnect(js, &back));
  EXPECT(back.host == "api.example.com");
  EXPECT_EQ(back.ipv4.size(), 1u);
  EXPECT(back.ipv4[0] == v4);
  EXPECT_EQ(back.ipv6.size(), 1u);
  EXPECT(back.ipv6[0] == v6);
}

TEST(RpcXidSurvivesIpChange) {
  wns::Stack a(1);
  wns::Stack b(2);
  auto& rpc = b.OpenRpc(1);
  uint8_t payload[] = {1, 2, 3};
  uint64_t xid = rpc.SendRequest(9, 4, payload);
  EXPECT(rpc.Pending(xid));
  wns::Ipv4 next;
  EXPECT(wns::ParseIpv4("198.51.100.7", &next));
  auto wire = a.Announce(wns::sctp::kFamily4, wns::sctp::kOpReplace, {next}, {});
  EXPECT_EQ(b.RecvGossip(wire), wns::GossipResult::kApplied);
  EXPECT(rpc.Pending(xid));
  EXPECT_EQ(rpc.pending_count(), 1u);
}

TEST(RpcFrameRoundtrip) {
  wns::RpcFrame f;
  f.flags = wns::sctp::kRpcFlagReq;
  f.xid = 0x1122334455667788ull;
  f.service = 3;
  f.method = 9;
  f.payload = {0xaa, 0xbb};
  auto wire = wns::EncodeRpcFrame(f);
  EXPECT_EQ(wire.size(), 26u);
  wns::RpcFrame back;
  EXPECT(wns::DecodeRpcFrame(wire, &back));
  EXPECT_EQ(back.xid, f.xid);
  EXPECT_EQ(back.service, 3u);
  EXPECT_EQ(back.payload.size(), 2u);
}
