#include "wns/proxyd.h"

#include <chrono>
#include <utility>

namespace wns {
namespace {

constexpr size_t kIoBuf = 32768;
constexpr size_t kBurst = 32;
// A TCP peer that stops reading must not let the association queue grow
// without bound; past this the flow is reset.
constexpr size_t kMaxPending = 4u << 20;
// A UDP flow waiting on its OPEN_ACK holds this much, then starts dropping.
constexpr size_t kMaxQueued = 64u << 10;
// While the link is this far behind, ingress sockets are left unread. That is
// the only backpressure the association has, so it has to be real.
constexpr size_t kLinkHighWater = 64;

uint64_t NowMs() {
  using clock = std::chrono::steady_clock;
  return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                      clock::now().time_since_epoch())
                      .count());
}

bool SameHost(const ProxyTarget& rule, const ProxyTarget& t) {
  if (rule.kind != t.kind) return false;
  if (rule.kind == sctp::kProxyAddr4) return rule.v4 == t.v4;
  if (rule.kind == sctp::kProxyAddr6) return rule.ipv6 == t.ipv6;
  return rule.name == t.name;
}

}  // namespace

bool ProxyPolicy::Permits(uint8_t proto, const ProxyTarget& t) const {
  if (!egress) return false;
  if (proto == sctp::kProxyTcp && !allow_tcp) return false;
  if (proto == sctp::kProxyUdp && !allow_udp) return false;
  if (allow.empty()) return true;
  for (const auto& rule : allow) {
    if (!SameHost(rule, t)) continue;
    if (rule.port == 0 || rule.port == t.port) return true;
  }
  return false;
}

ProxyNode::ProxyNode(Stack* stack) : stack_(stack) {
  buf_.resize(kIoBuf);
  WireHandlers();
}

ProxyNode::~ProxyNode() {
  if (!stack_) return;
  auto& mux = stack_->proxy();
  mux.SetOpenHandler(nullptr);
  mux.SetAckHandler(nullptr);
  mux.SetDataHandler(nullptr);
  mux.SetDatagramHandler(nullptr);
  mux.SetCloseHandler(nullptr);
}

void ProxyNode::WireHandlers() {
  if (!stack_) return;
  auto& mux = stack_->proxy();
  mux.SetOpenHandler([this](ProxyChannel* ch) { return OnOpen(ch); });
  mux.SetAckHandler([this](ProxyChannel* ch) { OnAck(ch); });
  mux.SetDataHandler(
      [this](ProxyChannel* ch, std::span<const uint8_t> d) { OnData(ch, d); });
  mux.SetDatagramHandler(
      [this](ProxyChannel* ch, const ProxyDatagram& dg) { OnDatagram(ch, dg); });
  mux.SetCloseHandler([this](ProxyChannel* ch, uint8_t r) { OnClose(ch, r); });
}

ProxyNode::Flow* ProxyNode::Find(uint16_t channel) {
  for (auto& f : flows_) {
    if (f->channel == channel && !f->dead) return f.get();
  }
  return nullptr;
}

ProxyNode::Flow* ProxyNode::FindUdpClient(UdpSocket* reply, const Endpoint& client) {
  for (auto& f : flows_) {
    if (f->dead || !f->udp || f->reply != reply) continue;
    if (f->client == client) return f.get();
  }
  return nullptr;
}

ProxyNode::Flow* ProxyNode::Add(uint16_t channel, bool udp, bool ingress) {
  auto f = std::make_unique<Flow>();
  f->channel = channel;
  f->udp = udp;
  f->ingress = ingress;
  f->last_ms = NowMs();
  flows_.push_back(std::move(f));
  ++stats_.opened;
  return flows_.back().get();
}

bool ProxyNode::AddTcpIngress(const Endpoint& at, const ProxyTarget& target) {
  auto l = std::make_unique<TcpListener>();
  if (!l->Listen(at)) return false;
  ProxyIngress rec;
  rec.udp = false;
  rec.listen = l->local();
  rec.target = target;
  ingress_.push_back(std::move(rec));
  tcp_rules_.push_back(TcpRule{std::move(l), target});
  return true;
}

bool ProxyNode::AddUdpIngress(const Endpoint& at, const ProxyTarget& target) {
  auto s = std::make_unique<UdpSocket>();
  if (!s->Bind(at)) return false;
  ProxyIngress rec;
  rec.udp = true;
  rec.listen = s->local();
  rec.target = target;
  ingress_.push_back(std::move(rec));
  udp_rules_.push_back(UdpRule{std::move(s), target});
  return true;
}

void ProxyNode::Kill(Flow* f, uint8_t reason, bool tell_peer) {
  if (!f || f->dead) return;
  if (tell_peer && stack_) stack_->ProxyClose(f->channel, reason);
  if (f->tcp) f->tcp->Close();
  if (f->sock) f->sock->Close();
  f->dead = true;
  ++stats_.closed;
}

void ProxyNode::Sweep() {
  uint64_t now = NowMs();
  for (auto& f : flows_) {
    if (f->dead) continue;
    auto* ch = stack_ ? stack_->proxy().Find(f->channel) : nullptr;
    if (!ch || ch->done()) {
      Kill(f.get(), sctp::kProxyReset, false);
      continue;
    }
    if (f->udp && udp_idle_ms_ && now - f->last_ms > udp_idle_ms_) {
      Kill(f.get(), sctp::kProxyTimeout, true);
    }
  }
  std::vector<std::unique_ptr<Flow>> keep;
  keep.reserve(flows_.size());
  for (auto& f : flows_) {
    if (!f->dead) keep.push_back(std::move(f));
  }
  flows_ = std::move(keep);
  if (stack_) stack_->proxy().Reap();
}

// --- peer-driven events -----------------------------------------------------

uint8_t ProxyNode::OnOpen(ProxyChannel* ch) {
  if (!policy_.Permits(ch->proto, ch->target)) {
    ++stats_.refused;
    return sctp::kProxyErrPolicy;
  }
  Endpoint dst{};
  if (!ResolveTarget(ch->target, ch->proto == sctp::kProxyUdp, &dst)) {
    ++stats_.refused;
    return sctp::kProxyErrUnreachable;
  }
  if (ch->proto == sctp::kProxyTcp) {
    auto conn = std::make_unique<TcpConn>();
    if (!conn->Connect(dst)) {
      ++stats_.refused;
      return sctp::kProxyErrRefused;
    }
    auto* f = Add(ch->id, false, false);
    f->remote = dst;
    f->tcp = std::move(conn);
    ch->bound = TargetOf(dst);
    return sctp::kProxyOk;
  }
  auto sock = std::make_unique<UdpSocket>();
  if (!sock->Open(dst.v6)) {
    ++stats_.refused;
    return sctp::kProxyErrResources;
  }
  auto* f = Add(ch->id, true, false);
  f->remote = dst;
  f->sock = std::move(sock);
  ch->bound = TargetOf(dst);
  return sctp::kProxyOk;
}

void ProxyNode::FlushQueued(Flow* f) {
  if (f->queued.empty()) return;
  for (auto& d : f->queued) {
    if (stack_->ProxySendDatagram(f->channel, d)) stats_.bytes_to_peer += d.size();
  }
  f->queued.clear();
  f->queued_bytes = 0;
}

void ProxyNode::OnAck(ProxyChannel* ch) {
  auto* f = Find(ch->id);
  if (!f) return;
  if (ch->status != sctp::kProxyOk) {
    Kill(f, sctp::kProxyReset, false);
    return;
  }
  f->last_ms = NowMs();
  FlushQueued(f);
}

void ProxyNode::OnData(ProxyChannel* ch, std::span<const uint8_t> data) {
  auto* f = Find(ch->id);
  if (!f || !f->tcp) return;
  f->last_ms = NowMs();
  stats_.bytes_from_peer += data.size();
  size_t before = f->tcp->pending();
  f->tcp->Queue(data);
  auto st = f->tcp->Flush();
  if (st == IoStatus::kError) {
    Kill(f, sctp::kProxyReset, true);
    return;
  }
  // The window reopens for what the downstream socket took, not for what we
  // buffered, so backpressure reaches all the way back to the origin.
  size_t after = f->tcp->pending();
  stack_->proxy().Consumed(ch->id, data.size() + before - after);
  if (f->tcp->pending() > kMaxPending) Kill(f, sctp::kProxyReset, true);
}

void ProxyNode::OnDatagram(ProxyChannel* ch, const ProxyDatagram& dg) {
  auto* f = Find(ch->id);
  if (!f) return;
  f->last_ms = NowMs();
  stats_.bytes_from_peer += dg.payload.size();
  if (f->ingress) {
    // Reply to the local client that owns this channel.
    if (f->reply) f->reply->SendTo(dg.payload, f->client);
    return;
  }
  if (!f->sock) return;
  Endpoint dst = f->remote;
  if (dg.peer != TargetOf(f->remote) && !ResolveTarget(dg.peer, true, &dst)) dst = f->remote;
  f->sock->SendTo(dg.payload, dst);
}

void ProxyNode::OnClose(ProxyChannel* ch, uint8_t reason) {
  auto* f = Find(ch->id);
  if (!f) return;
  if (reason == sctp::kProxyEof && f->tcp) {
    // Half close: drain what is queued, then send FIN downstream. Flush()
    // can leave bytes behind on a non-blocking socket that wasn't ready;
    // ShutdownWrite() has no idea those are still sitting in TcpConn's own
    // buffer, so calling it before the drain finishes silently discards
    // them. Finish the shutdown from ServiceFlows() once pending() is
    // actually zero instead.
    if (f->tcp->Flush() == IoStatus::kError) {
      Kill(f, sctp::kProxyReset, true);
      return;
    }
    if (f->tcp->pending() == 0) {
      f->tcp->ShutdownWrite();
    } else {
      f->close_pending = true;
    }
    if (f->sock_eof && !f->close_pending) Kill(f, sctp::kProxyEof, false);
    return;
  }
  Kill(f, reason, false);
}

// --- socket-driven work -----------------------------------------------------

int ProxyNode::ServiceListeners(Poller* p) {
  int work = 0;
  for (auto& rule : tcp_rules_) {
    if (!rule.sock->open() || !p->Readable(rule.sock->handle())) continue;
    for (size_t i = 0; i < kBurst; ++i) {
      Endpoint peer{};
      SockHandle h = rule.sock->Accept(&peer);
      if (h == kBadSock) break;
      ++work;
      uint16_t ch = stack_ ? stack_->ProxyOpen(sctp::kProxyTcp, rule.target) : 0;
      if (ch == 0) {
        SockClose(h);
        continue;
      }
      auto* f = Add(ch, false, true);
      f->client = peer;
      f->tcp = std::make_unique<TcpConn>();
      f->tcp->Adopt(h, peer);
    }
  }
  for (auto& rule : udp_rules_) {
    if (!rule.sock->open() || !p->Readable(rule.sock->handle())) continue;
    for (size_t i = 0; i < kBurst; ++i) {
      size_t got = 0;
      Endpoint from{};
      if (rule.sock->RecvFrom(buf_.data(), buf_.size(), &got, &from) != IoStatus::kOk) break;
      ++work;
      auto* f = FindUdpClient(rule.sock.get(), from);
      if (!f) {
        uint16_t ch = stack_ ? stack_->ProxyOpen(sctp::kProxyUdp, rule.target) : 0;
        if (ch == 0) continue;
        f = Add(ch, true, true);
        f->client = from;
        f->reply = rule.sock.get();
      }
      f->last_ms = NowMs();
      auto* chan = stack_->proxy().Find(f->channel);
      if (!chan) continue;
      if (chan->state == ProxyState::kOpening) {
        // Hold it until the OPEN_ACK; a one-shot client gets its answer.
        if (f->queued_bytes + got <= kMaxQueued) {
          f->queued.emplace_back(buf_.data(), buf_.data() + got);
          f->queued_bytes += got;
        }
        continue;
      }
      if (chan->state != ProxyState::kOpen) continue;
      if (stack_->ProxySendDatagram(f->channel, std::span<const uint8_t>(buf_.data(), got))) {
        stats_.bytes_to_peer += got;
      }
    }
  }
  return work;
}

bool ProxyNode::LinkCongested() const {
  return link_ && link_->pending() > kLinkHighWater;
}

int ProxyNode::ServiceFlows(Poller* p) {
  int work = 0;
  for (size_t i = 0; i < flows_.size(); ++i) {
    Flow* f = flows_[i].get();
    if (f->dead) continue;
    auto* chan = stack_->proxy().Find(f->channel);
    if (!chan || chan->state != ProxyState::kOpen) continue;

    if (f->tcp && f->tcp->open()) {
      if (f->tcp->pending() && p->Writable(f->tcp->handle())) {
        ++work;
        size_t before = f->tcp->pending();
        if (f->tcp->Flush() == IoStatus::kError) {
          Kill(f, sctp::kProxyReset, true);
          continue;
        }
        stack_->proxy().Consumed(f->channel, before - f->tcp->pending());
      }
      if (f->close_pending && f->tcp->pending() == 0) {
        f->tcp->ShutdownWrite();
        f->close_pending = false;
        if (f->sock_eof) Kill(f, sctp::kProxyEof, false);
      }
      if (!f->sock_eof && p->Readable(f->tcp->handle())) {
        for (size_t n = 0; n < kBurst; ++n) {
          // Each segment of a read becomes its own packet, so the link can
          // fall behind inside a single burst.
          if (link_) link_->FlushOut();
          if (LinkCongested()) break;
          size_t room = stack_->proxy().Writable(f->channel);
          if (room == 0) break;
          if (room > buf_.size()) room = buf_.size();
          size_t got = 0;
          auto st = f->tcp->Recv(buf_.data(), room, &got);
          if (st == IoStatus::kOk) {
            ++work;
            if (stack_->ProxySend(f->channel, std::span<const uint8_t>(buf_.data(), got))) {
              stats_.bytes_to_peer += got;
            }
            continue;
          }
          if (st == IoStatus::kAgain) break;
          ++work;
          if (st == IoStatus::kEof) {
            f->sock_eof = true;
            stack_->ProxyClose(f->channel, sctp::kProxyEof);
            if (chan->remote_eof || chan->done()) Kill(f, sctp::kProxyEof, false);
          } else {
            Kill(f, sctp::kProxyReset, true);
          }
          break;
        }
      }
      continue;
    }

    if (f->sock && f->sock->open() && p->Readable(f->sock->handle())) {
      for (size_t n = 0; n < kBurst; ++n) {
        if (link_) link_->FlushOut();
        if (LinkCongested()) break;
        size_t got = 0;
        Endpoint from{};
        if (f->sock->RecvFrom(buf_.data(), buf_.size(), &got, &from) != IoStatus::kOk) break;
        ++work;
        f->last_ms = NowMs();
        auto peer = TargetOf(from);
        if (stack_->ProxySendDatagram(f->channel,
                                      std::span<const uint8_t>(buf_.data(), got), &peer)) {
          stats_.bytes_to_peer += got;
        }
      }
    }
  }
  return work;
}

int ProxyNode::Poll(int timeout_ms) {
  if (!stack_) return 0;
  poller_.Reset();
  bool congested = false;
  if (link_ && link_->open()) {
    congested = link_->pending() > kLinkHighWater;
    poller_.Watch(link_->handle(), true, link_->pending() > 0);
  }
  for (auto& rule : tcp_rules_) {
    if (rule.sock->open()) poller_.Watch(rule.sock->handle(), true, false);
  }
  for (auto& rule : udp_rules_) {
    if (rule.sock->open()) poller_.Watch(rule.sock->handle(), true, false);
  }
  for (auto& fp : flows_) {
    Flow* f = fp.get();
    if (f->dead) continue;
    auto* chan = stack_->proxy().Find(f->channel);
    bool open = chan && chan->state == ProxyState::kOpen;
    if (f->tcp && f->tcp->open()) {
      // An unacked channel keeps its socket unread: that is the backpressure
      // that stops us buffering a stream the peer has not accepted.
      bool room = open && stack_->proxy().Writable(f->channel) > 0;
      poller_.Watch(f->tcp->handle(), room && !f->sock_eof && !congested,
                    f->tcp->pending() > 0);
    } else if (f->sock && f->sock->open()) {
      poller_.Watch(f->sock->handle(), open && !congested, false);
    }
  }

  int ready = poller_.Wait(timeout_ms);
  int work = 0;
  // Every pass, not just when something is ready: a lost packet means
  // nothing arrives, and the retransmission timeout still has to fire.
  stack_->Tick(NowMs());
  // The association first: OPEN_ACKs and inbound payload land before we decide
  // what to read from sockets.
  if (ready > 0 && link_ && link_->open()) {
    if (poller_.Writable(link_->handle())) work += int(link_->FlushOut());
    if (poller_.Readable(link_->handle())) work += int(link_->Pump());
  }
  if (ready > 0) {
    work += ServiceListeners(&poller_);
    work += ServiceFlows(&poller_);
  }
  if (link_) link_->FlushOut();
  Sweep();
  return work;
}

}  // namespace wns
