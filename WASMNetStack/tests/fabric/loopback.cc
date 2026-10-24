#include "test.h"

#include "wns/c/system.h"
#include "wns/stack.h"

#include <string>

TEST(WssHandshakeAndText) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  wns::WssConnect c;
  c.resource = "/v1/telemetry";
  c.host = "api.example.com";
  c.protocols.push_back("json-rpc");
  c.requested_streams = 16;
  wns::Ipv4 ip;
  EXPECT(wns::ParseIpv4("203.0.113.10", &ip));
  c.ipv4.push_back(ip);
  EXPECT(a.WssHandshake(c));
  auto* as = a.FindWss(2);
  auto* bs = b.FindWss(1);
  EXPECT(as != nullptr && bs != nullptr);
  EXPECT_EQ(as->ready_state(), wns::WssReady::kOpen);
  EXPECT_EQ(bs->ready_state(), wns::WssReady::kOpen);
  EXPECT_EQ(as->stream_min(), wns::sctp::kStreamWssMin);
  uint8_t msg[] = {'h', 'i'};
  EXPECT(a.WssSend(as->stream_min(), wns::sctp::kWssOpText, msg));
  wns::WssFrame rx;
  EXPECT(bs->TakeRx(&rx));
  EXPECT_EQ(rx.opcode, wns::sctp::kWssOpText);
  EXPECT_EQ(rx.payload.size(), 2u);
}

TEST(RpcRequestResponse) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  b.SetRpcHandler([](uint32_t svc, uint32_t method, std::span<const uint8_t> p) {
    EXPECT_EQ(svc, 3u);
    EXPECT_EQ(method, 9u);
    std::vector<uint8_t> out(p.begin(), p.end());
    out.push_back(0xff);
    return out;
  });
  uint8_t req[] = {1, 2};
  uint64_t xid = a.RpcCall(3, 9, req, false);
  auto* ep = a.FindRpc(2);
  EXPECT(ep != nullptr);
  EXPECT(!ep->Pending(xid));
  auto* rx = ep->last_rx();
  EXPECT(rx != nullptr);
  EXPECT_EQ(rx->xid, xid);
  EXPECT_EQ(rx->payload.size(), 3u);
  EXPECT_EQ(rx->payload[2], 0xff);
}

TEST(RpcOverGls) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  b.BindLabel(1, wns::LabelOp::kPop, 0, wns::sctp::kStreamRpcMin);
  uint8_t req[] = {4};
  uint64_t xid = a.RpcCall(1, 1, req, true);
  auto* ep = a.FindRpc(2);
  EXPECT(ep != nullptr);
  EXPECT(!ep->Pending(xid));
}

TEST(GossipIpChangeOverAssoc) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  b.classifier().AddTorExit("185.220.101.0/24");
  wns::Ipv4 exit;
  EXPECT(wns::ParseIpv4("185.220.101.9", &exit));
  a.Announce(wns::sctp::kFamily4, wns::sctp::kOpReplace, {exit}, {});
  EXPECT_EQ(b.PathOf(1).path, wns::PathClass::kTor);
}

TEST(MultiHomeFailover) {
  wns::Stack a(1);
  wns::Ipv4 p1, p2;
  EXPECT(wns::ParseIpv4("192.0.2.1", &p1));
  EXPECT(wns::ParseIpv4("198.51.100.1", &p2));
  a.AddLocal4(p1, false);
  a.AddLocal4(p2, false);
  EXPECT(a.assoc().Primary() != nullptr);
  EXPECT(a.assoc().Primary()->v4 == p1);
  EXPECT(a.FailPrimary());
  EXPECT(a.assoc().Primary()->v4 == p2);
}

TEST(CAbiProxyChannel) {
  EXPECT_EQ(WnsInit(), WNS_RESULT_OK);
  WnsHandle a = WnsStackCreate(1);
  WnsHandle b = WnsStackCreate(2);
  EXPECT_EQ(WnsConnect(a, b), WNS_RESULT_OK);
  EXPECT_EQ(WnsProxyServe(b, 1), WNS_RESULT_OK);

  uint32_t ch = 0;
  EXPECT_EQ(WnsProxyOpen(a, WNS_PROXY_TCP, "origin.example:80", &ch), WNS_RESULT_OK);
  EXPECT(ch != 0);
  uint32_t state = 0;
  EXPECT_EQ(WnsProxyState(a, ch, &state), WNS_RESULT_OK);
  EXPECT_EQ(state, WNS_PROXY_OPEN);
  char target[64];
  EXPECT_EQ(WnsProxyTarget(b, ch, target, sizeof(target)), WNS_RESULT_OK);
  EXPECT(std::string(target) == "origin.example:80");

  const uint8_t req[] = {'p', 'i', 'n', 'g'};
  EXPECT_EQ(WnsProxySend(a, ch, req, sizeof(req)), WNS_RESULT_OK);
  uint8_t buf[16];
  size_t got = 0;
  EXPECT_EQ(WnsProxyRecv(b, ch, buf, sizeof(buf), &got), WNS_RESULT_OK);
  EXPECT_EQ(got, 4u);
  EXPECT_EQ(WnsProxySend(b, ch, buf, got), WNS_RESULT_OK);
  got = 0;
  EXPECT_EQ(WnsProxyRecv(a, ch, buf, sizeof(buf), &got), WNS_RESULT_OK);
  EXPECT_EQ(got, 4u);
  EXPECT_EQ(buf[0], 'p');

  EXPECT_EQ(WnsProxyClose(a, ch, WNS_PROXY_RESET), WNS_RESULT_OK);
  EXPECT_EQ(WnsProxyState(b, ch, &state), WNS_RESULT_OK);
  EXPECT_EQ(state, WNS_PROXY_CLOSED);

  // A node that has not opted in refuses the channel.
  WnsHandle c = WnsStackCreate(3);
  WnsHandle d = WnsStackCreate(4);
  EXPECT_EQ(WnsConnect(c, d), WNS_RESULT_OK);
  uint32_t blocked = 0;
  EXPECT_EQ(WnsProxyOpen(c, WNS_PROXY_TCP, "origin.example:80", &blocked), WNS_RESULT_OK);
  EXPECT_EQ(WnsProxyState(c, blocked, &state), WNS_RESULT_OK);
  EXPECT_EQ(state, WNS_PROXY_CLOSED);

  WnsClose(a);
  WnsClose(b);
  WnsClose(c);
  WnsClose(d);
  WnsShutdown();
}

TEST(CAbiFabric) {
  EXPECT_EQ(WnsInit(), WNS_RESULT_OK);
  WnsHandle a = WnsStackCreate(1);
  WnsHandle b = WnsStackCreate(2);
  EXPECT(a != WNS_HANDLE_INVALID);
  EXPECT_EQ(WnsConnect(a, b), WNS_RESULT_OK);
  EXPECT_EQ(WnsWssHandshake(a, "/v1/telemetry", "api.example.com"), WNS_RESULT_OK);
  uint32_t ready = 0;
  EXPECT_EQ(WnsWssReady(a, &ready), WNS_RESULT_OK);
  EXPECT_EQ(ready, WNS_WSS_OPEN);
  EXPECT_EQ(WnsAddTorExit(b, "185.220.101.0/24"), WNS_RESULT_OK);
  EXPECT_EQ(WnsAnnounce4(a, 4, "185.220.101.1"), WNS_RESULT_OK);
  uint32_t cls = 0;
  EXPECT_EQ(WnsPathClass(b, 1, &cls), WNS_RESULT_OK);
  EXPECT_EQ(cls, WNS_PATH_TOR);
  EXPECT_EQ(WnsDnsPutA(b, "x.example", "192.0.2.9"), WNS_RESULT_OK);
  EXPECT_EQ(WnsDnsQuery(a, "x.example"), WNS_RESULT_OK);
  char ip[32];
  EXPECT_EQ(WnsDnsAnswerIp(a, ip, sizeof(ip)), WNS_RESULT_OK);
  EXPECT(std::string(ip) == "192.0.2.9");
  EXPECT_EQ(WnsBgpOpen(a), WNS_RESULT_OK);
  uint32_t st = 0;
  EXPECT_EQ(WnsBgpState(a, &st), WNS_RESULT_OK);
  EXPECT_EQ(st, 2u);
  WnsClose(a);
  WnsClose(b);
  WnsShutdown();
}
