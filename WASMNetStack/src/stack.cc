#include "wns/stack.h"

#include "wns/chunk.h"
#include "wns/demux.h"
#include "wns/ipify.h"
#include "wns/nat.h"

namespace wns {
namespace {

std::vector<uint8_t> Bytes(std::string_view s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string AsText(std::span<const uint8_t> b) {
  return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

}  // namespace

Stack::Stack(uint32_t node_id) : node_id_(node_id), flow_(&assoc_) {
  assoc_.local_vtag = MakeVtag(node_id);
  assoc_.local_port = sctp::kPort;
  assoc_.peer_port = sctp::kPort;
  proxy_.SetSender([this](uint16_t sid, uint32_t ppid, std::span<const uint8_t> user,
                          bool unord) { SendData(sid, ppid, user, unord); });
  tty_.SetSender([this](uint16_t sid, uint32_t ppid, std::span<const uint8_t> user,
                        bool unord) { SendData(sid, ppid, user, unord); });
  // flow_ emits raw DATA/SACK chunks; Transmit wraps them in an SCTP packet
  // the same way any other chunk (INIT, GOSSIP, LABEL_DATA) already is.
  flow_.SetSender([this](std::span<const uint8_t> chunk) { Transmit(chunk); });
}

void Stack::Tick(uint64_t now_ms) {
  now_ms_ = now_ms;
  flow_.Tick(now_ms);
}

void Stack::Connect(Stack& peer) {
  peer_ = &peer;
  peer.peer_ = this;
  assoc_.peer_node = peer.node_id_;
  assoc_.peer_vtag = peer.assoc_.local_vtag;
  assoc_.peer_port = peer.assoc_.local_port;
  peer.assoc_.peer_node = node_id_;
  peer.assoc_.peer_vtag = assoc_.local_vtag;
  peer.assoc_.peer_port = assoc_.local_port;
  // Mirror halves of the proxy channel space so both ends may open channels.
  proxy_.set_server(node_id_ > peer.node_id_);
  peer.proxy_.set_server(peer.node_id_ > node_id_);
  tty_.set_server(node_id_ > peer.node_id_);
  peer.tty_.set_server(peer.node_id_ > node_id_);
}

void Stack::ConnectRemote(uint32_t peer_node, uint16_t peer_port) {
  assoc_.peer_node = peer_node;
  assoc_.peer_port = peer_port;
  // The peer opens channels from the mirrored half of the channel space.
  proxy_.set_server(node_id_ > peer_node);
  tty_.set_server(node_id_ > peer_node);
}

void Stack::SetTransport(Transport t) { transport_ = std::move(t); }

WssSocket& Stack::OpenWss(uint32_t peer_node) {
  if (auto* s = FindWss(peer_node)) return *s;
  wss_.push_back(std::make_unique<WssSocket>(&locators_, &classifier_, node_id_, peer_node));
  return *wss_.back();
}

RpcEndpoint& Stack::OpenRpc(uint32_t peer_node) {
  if (auto* r = FindRpc(peer_node)) return *r;
  rpc_.push_back(std::make_unique<RpcEndpoint>(&locators_, &classifier_, node_id_, peer_node));
  return *rpc_.back();
}

WssSocket* Stack::FindWss(uint32_t peer_node) {
  for (auto& s : wss_) {
    if (s->peer_node() == peer_node) return s.get();
  }
  return nullptr;
}

RpcEndpoint* Stack::FindRpc(uint32_t peer_node) {
  for (auto& r : rpc_) {
    if (r->peer_node() == peer_node) return r.get();
  }
  return nullptr;
}

void Stack::RefreshClass(uint32_t node_id, const ClassifyResult* prev, uint32_t prev_seq,
                         uint32_t seq) {
  auto* n = locators_.Find(node_id);
  if (!n) return;
  auto path = classifier_.Classify(n->ipv4, n->ipv6, prev, prev_seq, seq);
  locators_.SetPath(node_id, path);
}

void Stack::NotifyPeers(uint32_t node_id) {
  for (auto& s : wss_) {
    if (s->peer_node() == node_id) s->OnPeerLocatorsChanged();
  }
  for (auto& r : rpc_) {
    if (r->peer_node() == node_id) r->OnPeerLocatorsChanged();
  }
}

std::vector<uint8_t> Stack::Announce(uint8_t family, uint8_t op,
                                     const std::vector<Ipv4>& v4,
                                     const std::vector<Ipv6>& v6, uint8_t extra_flags) {
  ClassifyResult prev{};
  uint32_t prev_seq = 0;
  if (auto* n = locators_.Find(node_id_)) {
    prev = n->path;
    prev_seq = n->seq;
  }
  GossipControl g;
  g.originator = node_id_;
  g.seq = ++seq_;
  IpChangeTlv t;
  t.family = family;
  t.op = op;
  t.flags = uint8_t(sctp::kIpChangeAuth | extra_flags);
  t.ipv4 = v4;
  t.ipv6 = v6;
  g.changes.push_back(t);
  ApplyGossip(&locators_, g);
  RefreshClass(node_id_, &prev, prev_seq, g.seq);
  RefreshScope(node_id_);
  auto chunk = EncodeGossipControl(g);
  if (peer_ || transport_) Transmit(chunk);
  return chunk;
}

GossipResult Stack::RecvGossip(std::span<const uint8_t> wire) {
  GossipControl g;
  if (!DecodeGossipControl(wire, &g)) return GossipResult::kBad;
  ClassifyResult prev{};
  uint32_t prev_seq = 0;
  if (auto* n = locators_.Find(g.originator)) {
    prev = n->path;
    prev_seq = n->seq;
  }
  auto r = ApplyGossip(&locators_, g);
  if (r != GossipResult::kApplied) return r;
  RefreshClass(g.originator, &prev, prev_seq, g.seq);
  RefreshScope(g.originator);
  NotifyPeers(g.originator);
  return r;
}

void Stack::Transmit(std::span<const uint8_t> chunk) {
  Transmit(chunk, assoc_.peer_vtag, {});
}

void Stack::Transmit(std::span<const uint8_t> chunk, uint32_t vtag, PacketSrc src) {
  if (!peer_ && !transport_) return;
  SctpHeader h;
  h.src_port = assoc_.local_port;
  h.dst_port = assoc_.peer_port;
  h.vtag = vtag;
  auto packet = EncodePacket(h, chunk);
  if (peer_) {
    peer_->Ingress(packet, src);
    return;
  }
  transport_(packet);
}

void Stack::SendData(uint16_t sid, uint32_t ppid, std::span<const uint8_t> user, bool unord) {
  // Reliability and pacing live in flow_: it allocates the TSN/SSN, and
  // transmits now or queues behind the send window, retrying on loss.
  flow_.Send(sid, ppid, user, unord, now_ms_);
}

bool Stack::Ingress(std::span<const uint8_t> wire, PacketSrc src) {
  last_src_ = src;
  if (wire.size() >= 12) {
    SctpHeader h;
    std::span<const uint8_t> chunk;
    uint16_t sport = uint16_t((uint16_t(wire[0]) << 8) | wire[1]);
    if (sport == assoc_.peer_port || sport == sctp::kPort) {
      if (DecodePacket(wire, &h, &chunk)) {
        uint8_t type = ChunkType(chunk);
        if (type != sctp::kChunkInit && h.vtag != 0 && h.vtag != assoc_.local_vtag) {
          return false;
        }
        return RecvChunk(chunk);
      }
    }
  }
  return RecvChunk(wire);
}

bool Stack::RecvChunk(std::span<const uint8_t> chunk) {
  if (chunk.empty()) return false;
  uint8_t type = chunk[0];
  if (type == sctp::kChunkGossipControl) {
    return RecvGossip(chunk) == GossipResult::kApplied;
  }
  if (type == sctp::kChunkLabelData) {
    LabelData ld;
    if (!DecodeLabelData(chunk, &ld)) return false;
    return RecvLabel(ld);
  }
  if (type == sctp::kChunkInit || type == sctp::kChunkInitAck) {
    return RecvInit(chunk);
  }
  if (type == sctp::kChunkSack) {
    return flow_.OnSack(chunk, now_ms_);
  }
  if (type == sctp::kChunkData) {
    DataChunk d;
    auto admit = flow_.OnData(chunk, &d, now_ms_);
    if (admit == DataAdmit::kMalformed) return false;
    if (admit == DataAdmit::kDuplicate) return true;
    return RecvData(d);
  }
  return false;
}

bool Stack::RecvData(const DataChunk& d) {
  if (d.sid == sctp::kStreamControl) {
    if (d.ppid == sctp::kPpidProxyControl) return RecvProxy(d);
    auto kind = ClassifyControl(sctp::kChunkData, d.ppid);
    if (kind == ControlKind::kWss) return RecvWssControl(d.user);
    if (kind == ControlKind::kBgp) {
      if (BgpType(d.user) == kBgpOpen) {
        bool need_open = bgp_.state() == BgpState::kIdle;
        if (!bgp_.configured()) bgp_.Configure(uint16_t(node_id_), node_id_);
        if (!bgp_.RecvOpen(d.user)) return false;
        if (need_open) {
          SendData(sctp::kStreamControl, sctp::kPpidBgpControl, bgp_.SendOpen(), false);
        }
        SendData(sctp::kStreamControl, sctp::kPpidBgpControl, EncodeBgpKeepalive(), false);
        return true;
      }
      if (BgpType(d.user) == kBgpKeepalive) {
        bgp_.Established();
        return true;
      }
      return false;
    }
    if (kind == ControlKind::kDosctp) {
      DnsMessage m;
      if (!DecodeDns(d.user, &m)) return false;
      last_dns_ = m;
      have_dns_ = true;
      if ((m.flags & 0x8000) == 0 && !m.questions.empty()) {
        for (const auto& z : zone_) {
          if (z.first == m.questions[0].name) {
            auto ans = DnsAnswerA(m, z.second);
            SendData(d.sid ? d.sid : sctp::kStreamDnsMin, sctp::kPpidDosctp, EncodeDns(ans),
                     true);
            break;
          }
        }
      }
      return true;
    }
    return false;
  }
  if (sctp::IsBgpStream(d.sid)) {
    BgpUpdate u;
    if (!DecodeBgpUpdate(d.user, &u)) return false;
    return bgp_.Install(u);
  }
  if (sctp::IsDnsStream(d.sid) || d.ppid == sctp::kPpidDosctp) {
    DnsMessage m;
    if (!DecodeDns(d.user, &m)) return false;
    last_dns_ = m;
    have_dns_ = true;
    if ((m.flags & 0x8000) == 0 && !m.questions.empty()) {
      for (const auto& z : zone_) {
        if (z.first == m.questions[0].name) {
          auto ans = DnsAnswerA(m, z.second);
          SendData(d.sid, sctp::kPpidDosctp, EncodeDns(ans), true);
          break;
        }
      }
    }
    return true;
  }
  if (d.ppid == sctp::kPpidProxyControl || d.ppid == sctp::kPpidProxyData ||
      sctp::IsProxyStream(d.sid)) {
    return RecvProxy(d);
  }
  if (d.ppid == sctp::kPpidTty || sctp::IsTtyStream(d.sid)) {
    return RecvTty(d);
  }
  if (sctp::IsWssStream(d.sid) || d.ppid == sctp::kPpidWssText ||
      d.ppid == sctp::kPpidWssBinary) {
    WssFrame f;
    if (!DecodeWssFrame(d.user, &f)) return false;
    f.stream_id = d.sid;
    auto& sock = OpenWss(assoc_.peer_node);
    sock.PushRx(std::move(f));
    return true;
  }
  if (sctp::IsRpcStream(d.sid) || d.ppid == sctp::kPpidRpc) {
    RpcFrame f;
    if (!DecodeRpcFrame(d.user, &f)) return false;
    return RecvRpc(f, d.sid);
  }
  return false;
}

bool Stack::RecvLabel(const LabelData& ld) {
  LabelData cur = ld;
  std::vector<uint8_t> delivered;
  IlmEntry used;
  auto r = gls_.Process(&cur, &delivered, &used);
  if (r == GlsResult::kDrop) return false;
  if (r == GlsResult::kForward) {
    Transmit(EncodeLabelData(cur));
    return true;
  }
  RpcFrame rf;
  if (DecodeRpcFrame(delivered, &rf)) return RecvRpc(rf, used.out_stream);
  DnsMessage dm;
  if (DecodeDns(delivered, &dm)) {
    last_dns_ = dm;
    have_dns_ = true;
    return true;
  }
  return true;
}

bool Stack::RecvWssControl(std::span<const uint8_t> json) {
  auto s = AsText(json);
  if (s.find("\"CONNECTED\"") != std::string::npos) {
    WssConnected c;
    if (!DecodeWssConnected(s, &c)) return false;
    auto& sock = OpenWss(assoc_.peer_node);
    return sock.OnConnected(c);
  }
  WssConnect req;
  if (!DecodeWssConnect(s, &req)) return false;
  auto& sock = OpenWss(assoc_.peer_node);
  uint16_t min = sctp::kStreamWssMin;
  uint16_t max = uint16_t(min + (req.requested_streams ? req.requested_streams : 16) - 1);
  if (!sock.Accept(req, min, max)) return false;
  WssConnected c;
  c.status = 200;
  if (!req.protocols.empty()) c.selected_protocol = req.protocols.front();
  c.stream_min = min;
  c.stream_max = max;
  auto js = EncodeWssConnected(c);
  SendData(sctp::kStreamControl, sctp::kPpidWssControl, Bytes(js), false);
  return true;
}

bool Stack::RecvRpc(const RpcFrame& f, uint16_t sid) {
  auto& ep = OpenRpc(assoc_.peer_node);
  ep.RecvFrame(f);
  if ((f.flags & sctp::kRpcFlagReq) && !(f.flags & sctp::kRpcFlagOne) &&
      !(f.flags & sctp::kRpcFlagRes)) {
    RpcFrame res;
    res.flags = sctp::kRpcFlagRes;
    res.xid = f.xid;
    res.service = f.service;
    res.method = f.method;
    if (rpc_handler_) {
      res.payload = rpc_handler_(f.service, f.method, f.payload);
    } else {
      res.payload = f.payload;
    }
    SendData(sid ? sid : sctp::kStreamRpcMin, sctp::kPpidRpc, EncodeRpcFrame(res), false);
  }
  return true;
}

bool Stack::RecvProxy(const DataChunk& d) { return proxy_.OnWire(d.user, d.sid); }

bool Stack::RecvTty(const DataChunk& d) { return tty_.OnWire(d.user, d.sid); }

uint16_t Stack::TtyOpen(uint16_t cols, uint16_t rows) { return tty_.Open(cols, rows); }

bool Stack::TtyStdin(uint16_t stream, std::span<const uint8_t> data) {
  return tty_.Stdin(stream, data);
}

bool Stack::TtyStdout(uint16_t stream, std::span<const uint8_t> data) {
  return tty_.Stdout(stream, data);
}

bool Stack::TtyResize(uint16_t stream, uint16_t cols, uint16_t rows) {
  return tty_.Resize(stream, cols, rows);
}

uint16_t Stack::ProxyOpen(uint8_t proto, const ProxyTarget& target) {
  return proxy_.Open(proto, target);
}

bool Stack::ProxySend(uint16_t channel, std::span<const uint8_t> data) {
  return proxy_.Send(channel, data);
}

bool Stack::ProxySendDatagram(uint16_t channel, std::span<const uint8_t> data,
                              const ProxyTarget* peer) {
  return proxy_.SendDatagram(channel, data, peer);
}

bool Stack::ProxyClose(uint16_t channel, uint8_t reason) {
  return proxy_.Close(channel, reason);
}

bool Stack::AddLocal4(Ipv4 a, bool gossip) {
  assoc_.AddPath4(a, assoc_.paths.empty());
  locators_.AddReal4(node_id_, a);
  locators_.Add4(node_id_, a);
  RefreshScope(node_id_);
  if (!gossip) return true;
  Announce(sctp::kFamily4, sctp::kOpAdd, {a}, {}, sctp::kIpChangeReal);
  return true;
}

bool Stack::ApplyIpifyJson(std::string_view json, bool gossip) {
  IpifyReply r;
  if (!DecodeIpifyJson(json, &r)) return false;
  if (r.v6) {
    assoc_.AddPath6(r.ipv6, assoc_.paths.empty());
    locators_.AddInternet6(node_id_, r.ipv6);
    appear_as_ = PacketSrc::V6(r.ipv6);
    if (gossip) {
      Announce(sctp::kFamily6, sctp::kOpReplace, {}, {r.ipv6}, sctp::kIpChangeInternet);
    } else {
      locators_.Replace6(node_id_, {r.ipv6});
      locators_.ReplaceInternet6(node_id_, {r.ipv6});
    }
    RefreshScope(node_id_);
    return true;
  }
  assoc_.AddPath4(r.v4, assoc_.paths.empty());
  locators_.AddInternet4(node_id_, r.v4);
  appear_as_ = PacketSrc::V4(r.v4);
  if (gossip) {
    Announce(sctp::kFamily4, sctp::kOpReplace, {r.v4}, {}, sctp::kIpChangeInternet);
  } else {
    locators_.Replace4(node_id_, {r.v4});
    locators_.ReplaceInternet4(node_id_, {r.v4});
  }
  RefreshScope(node_id_);
  return true;
}

std::string Stack::VisibleJson() const {
  if (auto* p = assoc_.Primary()) {
    if (p->v6) return EncodeIpifyJson(FormatIpv6(p->ipv6));
    return EncodeIpifyJson(FormatIpv4(p->v4));
  }
  if (auto* n = locators_.Find(node_id_)) {
    if (n->has_primary4) return EncodeIpifyJson(FormatIpv4(n->primary4));
    if (n->has_primary6) return EncodeIpifyJson(FormatIpv6(n->primary6));
  }
  return {};
}

bool Stack::FailPrimary() { return assoc_.FailPrimary(); }

bool Stack::WssHandshake(const WssConnect& req) {
  auto& sock = OpenWss(assoc_.peer_node);
  if (!sock.Connect(req)) return false;
  auto js = EncodeWssConnect(req);
  SendData(sctp::kStreamControl, sctp::kPpidWssControl, Bytes(js), false);
  return sock.ready_state() == WssReady::kOpen;
}

bool Stack::WssSend(uint16_t stream, uint8_t opcode, std::span<const uint8_t> payload) {
  WssFrame f;
  f.stream_id = stream;
  f.opcode = opcode;
  f.payload.assign(payload.begin(), payload.end());
  uint32_t ppid = opcode == sctp::kWssOpText ? sctp::kPpidWssText : sctp::kPpidWssBinary;
  SendData(stream, ppid, EncodeWssFrame(f), (f.flags & sctp::kWssUnordered) != 0);
  return true;
}

uint64_t Stack::RpcCall(uint32_t service, uint32_t method, std::span<const uint8_t> payload,
                        bool via_gls) {
  auto& ep = OpenRpc(assoc_.peer_node);
  uint64_t xid = ep.SendRequest(service, method, payload);
  if (via_gls) {
    auto* ilm = gls_.Lookup(1);
    uint32_t label = ilm ? ilm->in_label : 1;
    auto ld = gls_.Push(label, ep.last_frame());
    ld.ssn = assoc_.NextSsn(sctp::kStreamGlsMin);
    Transmit(EncodeLabelData(ld));
  } else {
    SendData(rpc_stream_, sctp::kPpidRpc, ep.last_frame(), false);
  }
  return xid;
}

bool Stack::DnsQuerySend(std::string name, uint16_t type) {
  auto q = DnsQuery(dns_id_++, std::move(name), type);
  SendData(sctp::kStreamDnsMin, sctp::kPpidDosctp, EncodeDns(q), true);
  return true;
}

void Stack::DnsPutA(std::string name, Ipv4 addr) { zone_.push_back({std::move(name), addr}); }

bool Stack::BgpOpen() {
  bgp_.Configure(uint16_t(node_id_), node_id_);
  SendData(sctp::kStreamControl, sctp::kPpidBgpControl, bgp_.SendOpen(), false);
  return true;
}

bool Stack::BgpAdvertise(const BgpUpdate& u) {
  uint16_t sid = u.nlri6.empty() ? sctp::kStreamBgp4 : sctp::kStreamBgp6;
  uint32_t ppid = u.nlri6.empty() ? sctp::kPpidBgpCommonUpdate : sctp::kPpidBgpMultiprotocol;
  SendData(sid, ppid, EncodeBgpUpdate(u), false);
  return true;
}

void Stack::BindLabel(uint32_t in_label, LabelOp op, uint32_t out_label, uint16_t stream) {
  IlmEntry e;
  e.in_label = in_label;
  e.op = op;
  e.out_label = out_label;
  e.out_stream = stream;
  gls_.SetIlm(e);
}

void Stack::RefreshScope(uint32_t node_id) {
  auto* n = locators_.Find(node_id);
  if (!n) return;
  locators_.SetScope(node_id, NatStack::Scope(n->real4, n->real6, n->internet4, n->internet6));
}

bool Stack::SendInit(PacketSrc appear_as) {
  if (appear_as.has4 || appear_as.has6) appear_as_ = appear_as;
  InitChunk ic;
  ic.initiate_tag = assoc_.local_vtag;
  ic.a_rwnd = flow_.local_rwnd();
  ic.outbound = 16;
  ic.inbound = 16;
  ic.initial_tsn = 1;
  for (const auto& p : assoc_.paths) {
    if (!p.v6) ic.ipv4.push_back(p.v4);
    else ic.ipv6.push_back(p.ipv6);
  }
  if (auto* n = locators_.Find(node_id_)) {
    if (ic.ipv4.empty()) ic.ipv4 = n->real4.empty() ? n->ipv4 : n->real4;
    if (ic.ipv6.empty()) ic.ipv6 = n->real6.empty() ? n->ipv6 : n->real6;
  }
  Transmit(EncodeInit(ic), 0, appear_as_);
  return true;
}

bool Stack::RecvInit(std::span<const uint8_t> chunk) {
  InitChunk ic;
  if (!DecodeInit(chunk, &ic)) return false;
  uint32_t peer = assoc_.peer_node;
  if (ic.initiate_tag != 0) assoc_.peer_vtag = ic.initiate_tag;
  if (peer == 0) return false;
  flow_.SetPeerRwnd(ic.a_rwnd);
  for (auto a : ic.ipv4) {
    locators_.AddReal4(peer, a);
    locators_.Add4(peer, a);
  }
  for (auto a : ic.ipv6) {
    locators_.AddReal6(peer, a);
    locators_.Add6(peer, a);
  }
  if (last_src_.has4) {
    locators_.Add4(peer, last_src_.v4);
    if (IsInternet4(last_src_.v4)) locators_.AddInternet4(peer, last_src_.v4);
  }
  if (last_src_.has6) {
    locators_.Add6(peer, last_src_.v6);
    if (IsInternet6(last_src_.v6)) locators_.AddInternet6(peer, last_src_.v6);
  }
  RefreshScope(peer);
  NotifyPeers(peer);
  if (ic.ack) return true;
  InitChunk ack;
  ack.ack = true;
  ack.initiate_tag = assoc_.local_vtag;
  ack.a_rwnd = flow_.local_rwnd();
  ack.outbound = 16;
  ack.inbound = 16;
  ack.initial_tsn = 1;
  ack.cookie = {'w', 'n', 's', 0};
  for (const auto& p : assoc_.paths) {
    if (!p.v6) ack.ipv4.push_back(p.v4);
    else ack.ipv6.push_back(p.ipv6);
  }
  Transmit(EncodeInit(ack), assoc_.peer_vtag, appear_as_);
  return true;
}

ClassifyResult Stack::PathOf(uint32_t node_id) const {
  if (auto* n = locators_.Find(node_id)) return n->path;
  return {};
}

RealInternet Stack::ScopeOf(uint32_t node_id) const {
  if (auto* n = locators_.Find(node_id)) return n->scope;
  return {};
}

}  // namespace wns
