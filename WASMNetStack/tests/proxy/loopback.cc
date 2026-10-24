#include "test.h"

#include "wns/link.h"
#include "wns/proxyd.h"
#include "wns/sock.h"
#include "wns/stack.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace std::chrono;

std::span<const uint8_t> Span(const std::string& s) {
  return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

wns::Endpoint LoopbackAny() {
  wns::Endpoint e;
  EXPECT(wns::ParseEndpoint("127.0.0.1:0", &e));
  return e;
}

uint64_t NowMs() {
  return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// A non-blocking TCP echo origin, pumped from the same loop as everything else.
struct TcpEcho {
  wns::TcpListener listener;
  std::vector<std::unique_ptr<wns::TcpConn>> conns;
  std::string seen;

  bool Start() { return listener.Listen(LoopbackAny()); }
  wns::Endpoint local() const { return listener.local(); }

  void Pump() {
    for (;;) {
      wns::Endpoint peer{};
      wns::SockHandle h = listener.Accept(&peer);
      if (h == wns::kBadSock) break;
      auto c = std::make_unique<wns::TcpConn>();
      c->Adopt(h, peer);
      conns.push_back(std::move(c));
    }
    uint8_t buf[4096];
    for (auto& c : conns) {
      if (!c->open()) continue;
      c->Flush();
      for (;;) {
        size_t got = 0;
        auto st = c->Recv(buf, sizeof(buf), &got);
        if (st == wns::IoStatus::kOk) {
          seen.append(reinterpret_cast<char*>(buf), got);
          c->Queue(std::span<const uint8_t>(buf, got));
          c->Flush();
          continue;
        }
        if (st == wns::IoStatus::kEof) {
          c->Flush();
          c->ShutdownWrite();
        }
        break;
      }
    }
  }
};

struct UdpEcho {
  wns::UdpSocket sock;
  int datagrams = 0;

  bool Start() { return sock.Bind(LoopbackAny()); }
  wns::Endpoint local() const { return sock.local(); }

  void Pump() {
    uint8_t buf[2048];
    for (;;) {
      size_t got = 0;
      wns::Endpoint from{};
      if (sock.RecvFrom(buf, sizeof(buf), &got, &from) != wns::IoStatus::kOk) break;
      ++datagrams;
      std::string out = "echo:";
      out.append(reinterpret_cast<char*>(buf), got);
      sock.SendTo(Span(out), from);
    }
  }
};

// Spins the whole datapath until `done` or the deadline passes.
bool RunUntil(const std::function<bool()>& done, const std::function<void()>& step,
              int budget_ms = 5000) {
  uint64_t end = NowMs() + uint64_t(budget_ms);
  for (;;) {
    step();
    if (done()) return true;
    if (NowMs() > end) return false;
  }
}

}  // namespace

TEST(ProxyTcpThroughAssociation) {
  EXPECT(wns::NetInit());
  TcpEcho echo;
  EXPECT(echo.Start());

  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  wns::ProxyNode ingress(&a);
  wns::ProxyNode egress(&b);
  wns::ProxyPolicy pol;
  pol.egress = true;
  pol.allow.push_back(wns::TargetOf(echo.local()));
  egress.SetPolicy(pol);

  EXPECT(ingress.AddTcpIngress(LoopbackAny(), wns::TargetOf(echo.local())));
  wns::Endpoint front = ingress.ingress()[0].listen;
  EXPECT(front.port != 0);

  wns::TcpConn client;
  EXPECT(client.Connect(front));
  client.Queue(Span(std::string("hello proxy")));

  std::string got;
  auto step = [&] {
    client.Flush();
    ingress.Poll(1);
    egress.Poll(1);
    echo.Pump();
    uint8_t buf[1024];
    size_t n = 0;
    if (client.Recv(buf, sizeof(buf), &n) == wns::IoStatus::kOk) {
      got.append(reinterpret_cast<char*>(buf), n);
    }
  };
  EXPECT(RunUntil([&] { return got == "hello proxy"; }, step));
  EXPECT(echo.seen == "hello proxy");
  EXPECT_EQ(ingress.stats().opened, 1u);
  EXPECT_EQ(egress.stats().opened, 1u);
  EXPECT_EQ(egress.stats().refused, 0u);
  EXPECT_EQ(ingress.stats().bytes_to_peer, 11u);
  EXPECT_EQ(ingress.stats().bytes_from_peer, 11u);

  // A client FIN walks the chain: ingress socket EOF -> PXY EOF -> egress FIN.
  client.ShutdownWrite();
  EXPECT(RunUntil([&] { return ingress.flow_count() == 0; }, step));
  EXPECT_EQ(a.proxy().live_count(), 0u);
  EXPECT(RunUntil([&] { return egress.flow_count() == 0; }, step));
}

TEST(ProxyTcpBulkStaysByteExact) {
  EXPECT(wns::NetInit());
  TcpEcho echo;
  EXPECT(echo.Start());

  wns::Stack a(1);
  wns::Stack b(2);
  wns::UdpLink la;
  wns::UdpLink lb;
  EXPECT(la.Bind(LoopbackAny()));
  EXPECT(lb.Bind(LoopbackAny()));
  la.SetPeer(lb.local());
  lb.SetPeer(la.local());
  la.Attach(&a);
  lb.Attach(&b);
  a.ConnectRemote(2);
  b.ConnectRemote(1);

  wns::ProxyNode ingress(&a);
  wns::ProxyNode egress(&b);
  ingress.AttachLink(&la);
  egress.AttachLink(&lb);
  wns::ProxyPolicy pol;
  pol.egress = true;
  egress.SetPolicy(pol);
  EXPECT(ingress.AddTcpIngress(LoopbackAny(), wns::TargetOf(echo.local())));

  // A megabyte through a link with no retransmission: the channel window is
  // what keeps the far side from being overrun.
  std::string payload;
  payload.reserve(1u << 20);
  for (size_t i = 0; i < (1u << 20); ++i) payload.push_back(char('a' + (i * 7 + i / 991) % 26));

  wns::TcpConn client;
  EXPECT(client.Connect(ingress.ingress()[0].listen));
  client.Queue(Span(payload));

  std::string got;
  bool fin = false;
  auto step = [&] {
    if (client.Flush() == wns::IoStatus::kOk && !fin) {
      client.ShutdownWrite();
      fin = true;
    }
    ingress.Poll(1);
    egress.Poll(1);
    echo.Pump();
    uint8_t buf[16384];
    size_t n = 0;
    while (client.Recv(buf, sizeof(buf), &n) == wns::IoStatus::kOk && n) {
      got.append(reinterpret_cast<char*>(buf), n);
      n = 0;
    }
  };
  EXPECT(RunUntil([&] { return got.size() >= payload.size(); }, step, 30000));
  EXPECT_EQ(got.size(), payload.size());
  EXPECT(got == payload);
  EXPECT_EQ(la.packets_dropped(), 0u);
  EXPECT_EQ(lb.packets_dropped(), 0u);
}

TEST(ProxyEgressPolicyRefuses) {
  EXPECT(wns::NetInit());
  TcpEcho echo;
  EXPECT(echo.Start());

  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  wns::ProxyNode ingress(&a);
  wns::ProxyNode egress(&b);
  wns::ProxyPolicy pol;
  pol.egress = true;
  pol.allow.push_back(wns::TargetOf(echo.local()));
  egress.SetPolicy(pol);

  // Same host, a port the policy does not list.
  auto blocked = wns::TargetOf(echo.local());
  blocked.port = uint16_t(blocked.port + 1);
  EXPECT(ingress.AddTcpIngress(LoopbackAny(), blocked));
  wns::Endpoint front = ingress.ingress()[0].listen;

  wns::TcpConn client;
  EXPECT(client.Connect(front));
  auto step = [&] {
    ingress.Poll(1);
    egress.Poll(1);
  };
  EXPECT(RunUntil([&] { return egress.stats().refused == 1u; }, step, 2000));
  EXPECT(RunUntil([&] { return ingress.flow_count() == 0; }, step, 2000));
  EXPECT_EQ(egress.stats().opened, 0u);
  EXPECT_EQ(echo.seen.size(), 0u);
}

TEST(ProxyUdpOverSctpOverUdpLink) {
  EXPECT(wns::NetInit());
  UdpEcho echo;
  EXPECT(echo.Start());

  wns::Stack a(1);
  wns::Stack b(2);
  wns::UdpLink la;
  wns::UdpLink lb;
  EXPECT(la.Bind(LoopbackAny()));
  EXPECT(lb.Bind(LoopbackAny()));
  la.SetPeer(lb.local());
  lb.SetPeer(la.local());
  la.Attach(&a);
  lb.Attach(&b);
  a.ConnectRemote(2);
  b.ConnectRemote(1);

  // INIT / INIT-ACK over the real link settles the verification tags.
  EXPECT(a.SendInit());
  auto pump = [&] {
    la.Pump();
    lb.Pump();
  };
  EXPECT(RunUntil([&] { return a.assoc().peer_vtag != 0 && b.assoc().peer_vtag != 0; },
                  pump, 2000));

  wns::ProxyNode ingress(&a);
  wns::ProxyNode egress(&b);
  ingress.AttachLink(&la);
  egress.AttachLink(&lb);
  wns::ProxyPolicy pol;
  pol.egress = true;
  pol.allow_tcp = false;
  egress.SetPolicy(pol);

  EXPECT(ingress.AddUdpIngress(LoopbackAny(), wns::TargetOf(echo.local())));
  wns::Endpoint front = ingress.ingress()[0].listen;

  wns::UdpSocket client;
  EXPECT(client.Open(false));

  std::string reply;
  bool sent = false;
  auto step = [&] {
    // One datagram, no retry: it lands before the channel is accepted and has
    // to be held until the OPEN_ACK, the way a one-shot resolver client needs.
    if (!sent) {
      EXPECT(client.SendTo(Span(std::string("ping")), front) == wns::IoStatus::kOk);
      sent = true;
    }
    ingress.Poll(1);
    egress.Poll(1);
    echo.Pump();
    uint8_t buf[2048];
    size_t n = 0;
    wns::Endpoint from{};
    if (client.RecvFrom(buf, sizeof(buf), &n, &from) == wns::IoStatus::kOk) {
      reply.assign(reinterpret_cast<char*>(buf), n);
    }
  };
  EXPECT(RunUntil([&] { return reply == "echo:ping"; }, step));
  EXPECT(echo.datagrams >= 1);
  EXPECT(la.packets_tx() > 0);
  EXPECT(lb.packets_rx() > 0);
  EXPECT_EQ(ingress.flow_count(), 1u);
  EXPECT_EQ(egress.flow_count(), 1u);

  // An idle UDP flow is reclaimed on both sides.
  ingress.set_udp_idle_ms(1);
  EXPECT(RunUntil([&] { return ingress.flow_count() == 0 && egress.flow_count() == 0; },
                  [&] {
                    ingress.Poll(1);
                    egress.Poll(1);
                  },
                  2000));
}
