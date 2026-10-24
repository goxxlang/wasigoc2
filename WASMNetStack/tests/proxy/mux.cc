#include "test.h"

#include "wns/proxy.h"
#include "wns/stack.h"

#include <cctype>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> Bytes(const char* s) {
  std::vector<uint8_t> o;
  for (const char* p = s; *p; ++p) o.push_back(uint8_t(*p));
  return o;
}

std::string Text(std::span<const uint8_t> b) {
  return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

// Cross-wires two muxes so a Send on one lands in OnWire on the other.
struct Pair {
  wns::ProxyMux client{false};
  wns::ProxyMux server{true};
  std::vector<uint16_t> client_sids;
  std::vector<uint16_t> server_sids;

  Pair() {
    client.SetSender([this](uint16_t sid, uint32_t ppid, std::span<const uint8_t> user,
                            bool) {
      client_sids.push_back(sid);
      EXPECT(ppid == wns::sctp::kPpidProxyControl || ppid == wns::sctp::kPpidProxyData);
      server.OnWire(user, sid);
    });
    server.SetSender([this](uint16_t sid, uint32_t ppid, std::span<const uint8_t> user,
                            bool) {
      server_sids.push_back(sid);
      EXPECT(ppid == wns::sctp::kPpidProxyControl || ppid == wns::sctp::kPpidProxyData);
      client.OnWire(user, sid);
    });
  }
};

}  // namespace

TEST(ProxyChannelOpenStreamAndHalfClose) {
  Pair p;
  std::string served;
  std::string got_back;
  uint8_t closed_reason = 0xff;
  int closes = 0;

  p.server.SetOpenHandler([&](wns::ProxyChannel* ch) {
    EXPECT_EQ(ch->proto, wns::sctp::kProxyTcp);
    EXPECT(ch->target.Text() == "origin.example:8080");
    return wns::sctp::kProxyOk;
  });
  p.server.SetDataHandler([&](wns::ProxyChannel* ch, std::span<const uint8_t> d) {
    served += Text(d);
    // Echo back over the same channel.
    auto up = Text(d);
    for (auto& c : up) c = char(std::toupper(c));
    p.server.Send(ch->id, std::span<const uint8_t>(
                              reinterpret_cast<const uint8_t*>(up.data()), up.size()));
  });
  p.server.SetCloseHandler([&](wns::ProxyChannel*, uint8_t r) {
    closed_reason = r;
    ++closes;
  });
  p.client.SetDataHandler(
      [&](wns::ProxyChannel*, std::span<const uint8_t> d) { got_back += Text(d); });

  uint16_t ch = p.client.Open(wns::sctp::kProxyTcp,
                              wns::ProxyTarget::Name("origin.example", 8080));
  EXPECT(ch != 0);
  auto* cc = p.client.Find(ch);
  auto* sc = p.server.Find(ch);
  EXPECT(cc != nullptr && sc != nullptr);
  EXPECT_EQ(cc->state, wns::ProxyState::kOpen);
  EXPECT_EQ(sc->state, wns::ProxyState::kOpen);
  EXPECT(wns::sctp::IsProxyStream(cc->stream));
  EXPECT_EQ(cc->stream, sc->stream);
  // OPEN and OPEN_ACK ride the control stream.
  EXPECT_EQ(p.client_sids.front(), wns::sctp::kStreamControl);
  EXPECT_EQ(p.server_sids.front(), wns::sctp::kStreamControl);

  EXPECT(p.client.Send(ch, Bytes("ping")));
  EXPECT(served == "ping");
  EXPECT(got_back == "PING");
  EXPECT_EQ(cc->bytes_tx, 4u);
  EXPECT_EQ(cc->bytes_rx, 4u);
  // DATA rides the channel's own stream.
  EXPECT_EQ(p.client_sids.back(), cc->stream);

  // Half close: the reverse direction stays up.
  EXPECT(p.client.Close(ch, wns::sctp::kProxyEof));
  EXPECT(cc->local_eof);
  EXPECT(sc->remote_eof);
  EXPECT_EQ(closed_reason, wns::sctp::kProxyEof);
  EXPECT_EQ(sc->state, wns::ProxyState::kOpen);
  EXPECT(!p.client.Send(ch, Bytes("more")));
  EXPECT(p.server.Send(ch, Bytes("tail")));
  EXPECT(got_back == "PINGtail");

  // Second half closes the channel on both ends.
  EXPECT(p.server.Close(ch, wns::sctp::kProxyEof));
  EXPECT_EQ(sc->state, wns::ProxyState::kClosed);
  EXPECT_EQ(cc->state, wns::ProxyState::kClosed);
  EXPECT_EQ(closes, 1);
  EXPECT_EQ(p.client.live_count(), 0u);
  EXPECT_EQ(p.client.Reap(), 1u);
}

TEST(ProxyWindowStopsSenderUntilAcked) {
  Pair p;
  p.client.set_window(4096);
  p.server.set_window(4096);
  size_t delivered = 0;
  p.server.SetOpenHandler([](wns::ProxyChannel*) { return wns::sctp::kProxyOk; });
  p.server.SetDataHandler(
      [&](wns::ProxyChannel*, std::span<const uint8_t> d) { delivered += d.size(); });

  uint16_t ch = p.client.Open(wns::sctp::kProxyTcp,
                              wns::ProxyTarget::Name("bulk.example", 9000));
  EXPECT(ch != 0);
  EXPECT_EQ(p.client.Writable(ch), 4096u);

  std::vector<uint8_t> block(4096, 0xab);
  EXPECT(p.client.Send(ch, block));
  EXPECT_EQ(delivered, 4096u);
  // Everything is in flight and nothing acknowledged: the window is shut.
  EXPECT_EQ(p.client.Writable(ch), 0u);

  // A partial consume below the quarter-window threshold sends no ACK.
  p.server.Consumed(ch, 512);
  EXPECT_EQ(p.client.Writable(ch), 0u);
  p.server.Consumed(ch, 1536);
  EXPECT_EQ(p.client.Writable(ch), 2048u);
  EXPECT_EQ(p.client.Find(ch)->acked_tx, 2048u);

  p.server.Consumed(ch, 2048);
  EXPECT_EQ(p.client.Writable(ch), 4096u);
  EXPECT(p.client.Send(ch, block));
  EXPECT_EQ(delivered, 8192u);

  // A UDP channel is never windowed.
  wns::Ipv4 any;
  EXPECT(wns::ParseIpv4("192.0.2.1", &any));
  uint16_t dg = p.client.Open(wns::sctp::kProxyUdp, wns::ProxyTarget::V4(any, 9));
  EXPECT(dg != 0);
  EXPECT(p.client.Writable(dg) > 0u);
  p.client.SendDatagram(dg, Bytes("x"));
  EXPECT(p.client.Writable(dg) > 0u);
}

TEST(ProxyOpenRefusedByPolicy) {
  Pair p;
  p.server.SetOpenHandler([](wns::ProxyChannel*) { return wns::sctp::kProxyErrPolicy; });
  bool acked = false;
  p.client.SetAckHandler([&](wns::ProxyChannel* ch) {
    acked = true;
    EXPECT_EQ(ch->status, wns::sctp::kProxyErrPolicy);
  });
  uint16_t ch = p.client.Open(wns::sctp::kProxyTcp,
                              wns::ProxyTarget::Name("blocked.example", 22));
  EXPECT(ch != 0);
  EXPECT(acked);
  EXPECT_EQ(p.client.Find(ch)->state, wns::ProxyState::kClosed);
  EXPECT(!p.client.Send(ch, Bytes("x")));
}

TEST(ProxyUdpDatagramCarriesPeer) {
  Pair p;
  std::vector<wns::ProxyDatagram> up;
  std::vector<wns::ProxyDatagram> down;
  p.server.SetOpenHandler([](wns::ProxyChannel* ch) {
    EXPECT_EQ(ch->proto, wns::sctp::kProxyUdp);
    return wns::sctp::kProxyOk;
  });
  p.server.SetDatagramHandler(
      [&](wns::ProxyChannel*, const wns::ProxyDatagram& d) { up.push_back(d); });
  p.client.SetDatagramHandler(
      [&](wns::ProxyChannel*, const wns::ProxyDatagram& d) { down.push_back(d); });

  wns::Ipv4 resolver;
  EXPECT(wns::ParseIpv4("192.0.2.53", &resolver));
  uint16_t ch = p.client.Open(wns::sctp::kProxyUdp, wns::ProxyTarget::V4(resolver, 53));
  EXPECT(ch != 0);
  EXPECT(p.client.SendDatagram(ch, Bytes("query")));
  EXPECT_EQ(up.size(), 1u);
  EXPECT(up[0].peer == wns::ProxyTarget::V4(resolver, 53));
  EXPECT(Text(up[0].payload) == "query");

  // The egress side reports the source of the reply, which may differ.
  wns::Ipv4 other;
  EXPECT(wns::ParseIpv4("192.0.2.54", &other));
  auto from = wns::ProxyTarget::V4(other, 53);
  EXPECT(p.server.SendDatagram(ch, Bytes("answer"), &from));
  EXPECT_EQ(down.size(), 1u);
  EXPECT(down[0].peer == from);

  // UDP has no half close.
  EXPECT(p.client.Close(ch, wns::sctp::kProxyEof));
  EXPECT_EQ(p.client.Find(ch)->state, wns::ProxyState::kClosed);
  EXPECT_EQ(p.server.Find(ch)->state, wns::ProxyState::kClosed);
}

TEST(ProxyRidesSctpAssociation) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  std::string seen;
  b.proxy().SetOpenHandler([](wns::ProxyChannel*) { return wns::sctp::kProxyOk; });
  b.proxy().SetDataHandler([&](wns::ProxyChannel* ch, std::span<const uint8_t> d) {
    seen += Text(d);
    b.ProxySend(ch->id, d);
  });
  std::string echoed;
  a.proxy().SetDataHandler(
      [&](wns::ProxyChannel*, std::span<const uint8_t> d) { echoed += Text(d); });

  uint16_t ch = a.ProxyOpen(wns::sctp::kProxyTcp,
                            wns::ProxyTarget::Name("origin.example", 80));
  EXPECT(ch != 0);
  EXPECT(a.ProxySend(ch, Bytes("GET /")));
  EXPECT(seen == "GET /");
  EXPECT(echoed == "GET /");
  EXPECT(a.ProxyClose(ch, wns::sctp::kProxyReset));
  EXPECT_EQ(b.proxy().Find(ch)->state, wns::ProxyState::kClosed);
  // The association's other subsystems are untouched.
  EXPECT_EQ(a.FindWss(2), nullptr);
  EXPECT_EQ(a.FindRpc(2), nullptr);
}

TEST(ProxyBothEndsOpenChannels) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  auto accept = [](wns::ProxyChannel*) { return wns::sctp::kProxyOk; };
  a.proxy().SetOpenHandler(accept);
  b.proxy().SetOpenHandler(accept);
  uint16_t from_a = a.ProxyOpen(wns::sctp::kProxyTcp,
                                wns::ProxyTarget::Name("x.example", 80));
  uint16_t from_b = b.ProxyOpen(wns::sctp::kProxyTcp,
                                wns::ProxyTarget::Name("y.example", 80));
  EXPECT(from_a != 0 && from_b != 0);
  EXPECT(from_a != from_b);
  // Node 2 owns the server half of the channel space.
  EXPECT((from_b & wns::sctp::kProxyServerBit) != 0);
  EXPECT((from_a & wns::sctp::kProxyServerBit) == 0);
  EXPECT(a.proxy().Find(from_b) != nullptr);
  EXPECT(b.proxy().Find(from_a) != nullptr);
}
