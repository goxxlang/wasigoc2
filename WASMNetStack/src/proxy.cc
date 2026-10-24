#include "wns/proxy.h"

#include "wire.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace wns {
namespace {

// Half the proxy block per direction so a client channel and a server channel
// never land on the same stream.
constexpr uint16_t kProxyStreamSpan =
    (sctp::kStreamProxyMax - sctp::kStreamProxyMin + 1) / 2;

std::string PortText(uint16_t port) {
  char b[8];
  std::snprintf(b, sizeof(b), "%u", unsigned(port));
  return b;
}

}  // namespace

ProxyTarget ProxyTarget::Name(std::string host, uint16_t port) {
  ProxyTarget t;
  t.kind = sctp::kProxyAddrName;
  t.name = std::move(host);
  t.port = port;
  return t;
}

ProxyTarget ProxyTarget::V4(Ipv4 a, uint16_t port) {
  ProxyTarget t;
  t.kind = sctp::kProxyAddr4;
  t.v4 = a;
  t.port = port;
  return t;
}

ProxyTarget ProxyTarget::V6(Ipv6 a, uint16_t port) {
  ProxyTarget t;
  t.kind = sctp::kProxyAddr6;
  t.ipv6 = a;
  t.port = port;
  return t;
}

bool ProxyTarget::operator==(const ProxyTarget& x) const {
  if (kind != x.kind || port != x.port) return false;
  if (kind == sctp::kProxyAddr4) return v4 == x.v4;
  if (kind == sctp::kProxyAddr6) return ipv6 == x.ipv6;
  return name == x.name;
}

std::string ProxyTarget::Text() const {
  if (kind == sctp::kProxyAddr4) return FormatIpv4(v4) + ":" + PortText(port);
  if (kind == sctp::kProxyAddr6) return "[" + FormatIpv6(ipv6) + "]:" + PortText(port);
  return name + ":" + PortText(port);
}

bool ParseProxyTarget(std::string_view spec, ProxyTarget* out) {
  if (!out || spec.empty()) return false;
  std::string s(spec);
  std::string host;
  std::string port;
  if (s.front() == '[') {
    auto rb = s.find(']');
    if (rb == std::string::npos) return false;
    host = s.substr(1, rb - 1);
    if (rb + 1 >= s.size() || s[rb + 1] != ':') return false;
    port = s.substr(rb + 2);
  } else {
    auto c = s.rfind(':');
    if (c == std::string::npos) return false;
    host = s.substr(0, c);
    port = s.substr(c + 1);
  }
  if (host.empty() || port.empty()) return false;
  unsigned p = 0;
  for (char c : port) {
    if (c < '0' || c > '9') return false;
    p = p * 10 + unsigned(c - '0');
    if (p > 65535) return false;
  }
  ProxyTarget t;
  t.port = uint16_t(p);
  if (ParseIpv4(host, &t.v4)) {
    t.kind = sctp::kProxyAddr4;
  } else if (ParseIpv6(host, &t.ipv6)) {
    t.kind = sctp::kProxyAddr6;
  } else {
    if (host.size() > 255) return false;
    t.kind = sctp::kProxyAddrName;
    t.name = std::move(host);
  }
  *out = std::move(t);
  return true;
}

void PutProxyTarget(std::vector<uint8_t>* out, const ProxyTarget& t) {
  if (!out) return;
  wire::PutU8(out, t.kind);
  wire::PutBe16(out, t.port);
  if (t.kind == sctp::kProxyAddr4) {
    wire::PutU8(out, 4);
    out->insert(out->end(), t.v4.o, t.v4.o + 4);
    return;
  }
  if (t.kind == sctp::kProxyAddr6) {
    wire::PutU8(out, 16);
    out->insert(out->end(), t.ipv6.o, t.ipv6.o + 16);
    return;
  }
  uint8_t n = uint8_t(t.name.size() > 255 ? 255 : t.name.size());
  wire::PutU8(out, n);
  out->insert(out->end(), t.name.begin(), t.name.begin() + n);
}

bool GetProxyTarget(std::span<const uint8_t> w, ProxyTarget* out, size_t* used) {
  if (!out || w.size() < 4) return false;
  ProxyTarget t;
  t.kind = w[0];
  t.port = wire::GetBe16(w.data() + 1);
  size_t n = w[3];
  if (w.size() < 4 + n) return false;
  if (t.kind == sctp::kProxyAddr4) {
    if (n != 4) return false;
    for (size_t i = 0; i < 4; ++i) t.v4.o[i] = w[4 + i];
  } else if (t.kind == sctp::kProxyAddr6) {
    if (n != 16) return false;
    for (size_t i = 0; i < 16; ++i) t.ipv6.o[i] = w[4 + i];
  } else if (t.kind == sctp::kProxyAddrName) {
    t.name.assign(reinterpret_cast<const char*>(w.data() + 4), n);
  } else {
    return false;
  }
  *out = std::move(t);
  if (used) *used = 4 + n;
  return true;
}

std::vector<uint8_t> EncodeProxyFrame(const ProxyFrame& f) {
  std::vector<uint8_t> out;
  out.reserve(kProxyHeaderSize + f.payload.size());
  wire::PutU8(&out, f.version);
  wire::PutU8(&out, f.type);
  wire::PutBe16(&out, f.channel);
  wire::PutU8(&out, f.flags);
  wire::PutU8(&out, f.proto);
  wire::PutBe16(&out, f.stream);
  wire::PutBe32(&out, f.seq);
  wire::PutBe32(&out, uint32_t(f.payload.size()));
  out.insert(out.end(), f.payload.begin(), f.payload.end());
  return out;
}

bool DecodeProxyFrame(std::span<const uint8_t> w, ProxyFrame* out) {
  if (!out || w.size() < kProxyHeaderSize) return false;
  ProxyFrame f;
  f.version = w[0];
  f.type = w[1];
  f.channel = wire::GetBe16(w.data() + 2);
  f.flags = w[4];
  f.proto = w[5];
  f.stream = wire::GetBe16(w.data() + 6);
  f.seq = wire::GetBe32(w.data() + 8);
  uint32_t n = wire::GetBe32(w.data() + 12);
  if (w.size() - kProxyHeaderSize < n) return false;
  f.payload.assign(w.begin() + kProxyHeaderSize, w.begin() + kProxyHeaderSize + n);
  *out = std::move(f);
  return true;
}

uint16_t ProxyMux::StreamFor(uint16_t channel) const {
  uint16_t slot = uint16_t(channel & 0x7FFF);
  uint16_t lane = uint16_t(((channel & sctp::kProxyServerBit) != 0) ? 1 : 0);
  return uint16_t(sctp::kStreamProxyMin + ((slot % kProxyStreamSpan) * 2) + lane);
}

ProxyChannel* ProxyMux::Find(uint16_t channel) {
  for (auto& c : channels_) {
    if (c.id == channel) return &c;
  }
  return nullptr;
}

const ProxyChannel* ProxyMux::Find(uint16_t channel) const {
  for (const auto& c : channels_) {
    if (c.id == channel) return &c;
  }
  return nullptr;
}

size_t ProxyMux::live_count() const {
  size_t n = 0;
  for (const auto& c : channels_) {
    if (!c.done()) ++n;
  }
  return n;
}

size_t ProxyMux::Reap() {
  size_t before = channels_.size();
  std::vector<ProxyChannel> keep;
  keep.reserve(channels_.size());
  for (auto& c : channels_) {
    if (!c.done()) keep.push_back(std::move(c));
  }
  channels_ = std::move(keep);
  return before - channels_.size();
}

ProxyChannel* ProxyMux::Insert(uint16_t channel, uint16_t stream, uint8_t proto) {
  ProxyChannel c;
  c.id = channel;
  c.stream = stream;
  c.proto = proto;
  c.state = ProxyState::kOpening;
  channels_.push_back(std::move(c));
  return &channels_.back();
}

bool ProxyMux::Emit(const ProxyFrame& f, bool control, bool unordered) {
  if (!send_) return false;
  auto wire = EncodeProxyFrame(f);
  uint16_t sid = control ? sctp::kStreamControl : f.stream;
  uint32_t ppid = control ? sctp::kPpidProxyControl : sctp::kPpidProxyData;
  send_(sid, ppid, wire, unordered);
  return true;
}

uint16_t ProxyMux::Open(uint8_t proto, const ProxyTarget& target) {
  if (proto != sctp::kProxyTcp && proto != sctp::kProxyUdp) return 0;
  uint16_t id = 0;
  for (uint32_t tries = 0; tries < 0x8000u; ++tries) {
    uint16_t cand = uint16_t(next_id_ & 0x7FFF);
    next_id_ = uint16_t((next_id_ + 1) & 0x7FFF);
    if (next_id_ == 0) next_id_ = 1;
    if (cand == 0) continue;
    if (server_) cand = uint16_t(cand | sctp::kProxyServerBit);
    if (!Find(cand)) {
      id = cand;
      break;
    }
  }
  if (id == 0) return 0;

  auto* ch = Insert(id, StreamFor(id), proto);
  ch->local_open = true;
  ch->target = target;

  ProxyFrame f;
  f.type = sctp::kProxyOpen;
  f.channel = id;
  f.proto = proto;
  f.stream = ch->stream;
  PutProxyTarget(&f.payload, target);
  if (!Emit(f, true, false)) {
    ch->state = ProxyState::kClosed;
    return 0;
  }
  return id;
}

size_t ProxyMux::Writable(uint16_t channel) const {
  const auto* ch = Find(channel);
  if (!ch || ch->state != ProxyState::kOpen || ch->local_eof) return 0;
  if (ch->proto == sctp::kProxyUdp) return window_;
  uint64_t in_flight = ch->bytes_tx - ch->acked_tx;
  if (in_flight >= window_) return 0;
  return size_t(window_ - in_flight);
}

void ProxyMux::Consumed(uint16_t channel, size_t bytes) {
  auto* ch = Find(channel);
  if (!ch || ch->proto == sctp::kProxyUdp) return;
  ch->consumed_rx += bytes;
  // Acknowledge in quarter windows: often enough to keep the sender moving,
  // rare enough that the control stream stays quiet.
  uint64_t owed = ch->consumed_rx - ch->acked_rx;
  if (owed == 0) return;
  if (owed < window_ / 4 && ch->state != ProxyState::kClosed) return;
  ProxyFrame f;
  f.type = sctp::kProxyAck;
  f.channel = channel;
  f.proto = ch->proto;
  f.stream = ch->stream;
  wire::PutBe64(&f.payload, ch->consumed_rx);
  if (!Emit(f, true, false)) return;
  ch->acked_rx = ch->consumed_rx;
}

bool ProxyMux::Send(uint16_t channel, std::span<const uint8_t> data) {
  auto* ch = Find(channel);
  if (!ch || ch->state != ProxyState::kOpen || ch->local_eof) return false;
  if (ch->proto == sctp::kProxyUdp) return SendDatagram(channel, data, nullptr);
  // A stream is segmented so no single packet outgrows the path MTU.
  size_t off = 0;
  do {
    size_t n = data.size() - off;
    if (n > sctp::kProxySegment) n = sctp::kProxySegment;
    ProxyFrame f;
    f.type = sctp::kProxyData;
    f.channel = channel;
    f.proto = ch->proto;
    f.stream = ch->stream;
    f.seq = ch->local_seq++;
    f.payload.assign(data.begin() + std::ptrdiff_t(off),
                     data.begin() + std::ptrdiff_t(off + n));
    if (!Emit(f, false, false)) return false;
    ch->bytes_tx += n;
    off += n;
  } while (off < data.size());
  return true;
}

bool ProxyMux::SendDatagram(uint16_t channel, std::span<const uint8_t> data,
                            const ProxyTarget* peer) {
  auto* ch = Find(channel);
  if (!ch || ch->state != ProxyState::kOpen || ch->local_eof) return false;
  ProxyFrame f;
  f.type = sctp::kProxyDatagram;
  f.channel = channel;
  f.proto = ch->proto;
  f.stream = ch->stream;
  f.seq = ch->local_seq++;
  PutProxyTarget(&f.payload, peer ? *peer : ch->target);
  f.payload.insert(f.payload.end(), data.begin(), data.end());
  if (!Emit(f, false, true)) return false;
  ch->bytes_tx += data.size();
  return true;
}

bool ProxyMux::Close(uint16_t channel, uint8_t reason) {
  auto* ch = Find(channel);
  if (!ch || ch->state == ProxyState::kClosed) return false;
  if (reason == sctp::kProxyEof && ch->local_eof) return false;
  ProxyFrame f;
  f.type = sctp::kProxyClose;
  f.channel = channel;
  f.flags = reason;
  f.proto = ch->proto;
  f.stream = ch->stream;
  f.seq = ch->local_seq++;
  if (!Emit(f, true, false)) return false;
  if (reason == sctp::kProxyEof) {
    ch->local_eof = true;
    // A UDP flow has no half close; EOF from either side ends it.
    if (ch->remote_eof || ch->proto == sctp::kProxyUdp) ch->state = ProxyState::kClosed;
  } else {
    ch->state = ProxyState::kClosed;
  }
  return true;
}

void ProxyMux::Finish(ProxyChannel* ch, uint8_t reason) {
  if (on_close_) on_close_(ch, reason);
  ch->state = ProxyState::kClosed;
}

bool ProxyMux::OnWire(std::span<const uint8_t> user, uint16_t sid) {
  ProxyFrame f;
  if (!DecodeProxyFrame(user, &f)) return false;
  return OnFrame(f, sid);
}

bool ProxyMux::OnFrame(const ProxyFrame& f, uint16_t sid) {
  if (f.version != sctp::kProxyVersion || f.channel == 0) return false;

  if (f.type == sctp::kProxyOpen) {
    if (Find(f.channel)) return false;
    if (f.proto != sctp::kProxyTcp && f.proto != sctp::kProxyUdp) return false;
    ProxyTarget t;
    if (!GetProxyTarget(f.payload, &t, nullptr)) return false;
    uint16_t stream = sctp::IsProxyStream(f.stream) ? f.stream : StreamFor(f.channel);
    auto* ch = Insert(f.channel, stream, f.proto);
    ch->target = t;
    ch->bound = t;
    uint8_t status = on_open_ ? on_open_(ch) : uint8_t(sctp::kProxyErrPolicy);
    ch->status = status;

    ProxyFrame ack;
    ack.type = sctp::kProxyOpenAck;
    ack.channel = f.channel;
    ack.flags = status;
    ack.proto = ch->proto;
    ack.stream = ch->stream;
    PutProxyTarget(&ack.payload, ch->bound);
    Emit(ack, true, false);
    ch->state = status == sctp::kProxyOk ? ProxyState::kOpen : ProxyState::kClosed;
    return status == sctp::kProxyOk;
  }

  auto* ch = Find(f.channel);
  if (!ch) return false;

  if (f.type == sctp::kProxyOpenAck) {
    if (!ch->local_open || ch->state != ProxyState::kOpening) return false;
    ch->status = f.flags;
    ProxyTarget bound;
    if (GetProxyTarget(f.payload, &bound, nullptr)) ch->bound = bound;
    if (sctp::IsProxyStream(f.stream)) ch->stream = f.stream;
    ch->state = f.flags == sctp::kProxyOk ? ProxyState::kOpen : ProxyState::kClosed;
    if (on_ack_) on_ack_(ch);
    return f.flags == sctp::kProxyOk;
  }

  if (f.type == sctp::kProxyAck) {
    if (f.payload.size() < 8) return false;
    uint64_t acked = wire::GetBe64(f.payload.data());
    if (acked > ch->acked_tx && acked <= ch->bytes_tx) ch->acked_tx = acked;
    return true;
  }

  if (f.type == sctp::kProxyData || f.type == sctp::kProxyDatagram ||
      f.type == sctp::kProxyClose) {
    return AdmitOrdered(ch, f, sid);
  }

  return false;
}

bool ProxyMux::AdmitOrdered(ProxyChannel* ch, const ProxyFrame& f, uint16_t sid) {
  if (f.seq < ch->remote_next) return true;  // stale duplicate; already handled
  if (f.seq > ch->remote_next) {
    // Out of order: hold it until the gap in front of it fills. The
    // association's own reliability (sctp-flow.md) guarantees that gap does
    // eventually fill; this only ever reorders, never has to give up. Bounded
    // implicitly, not by an explicit cap here, by the sender's own byte-level
    // window (kProxyWindow / kSctpMtu segments -- low hundreds of frames at
    // the very most).
    for (const auto& held : ch->reorder) {
      if (held.seq == f.seq) return true;  // already buffered
    }
    ch->reorder.push_back(f);
    return true;
  }
  bool ok = DeliverOrdered(ch, f, sid);
  ++ch->remote_next;
  for (;;) {
    auto it = std::find_if(ch->reorder.begin(), ch->reorder.end(),
                           [ch](const ProxyFrame& p) { return p.seq == ch->remote_next; });
    if (it == ch->reorder.end()) break;
    ProxyFrame next = std::move(*it);
    ch->reorder.erase(it);
    ok = DeliverOrdered(ch, next, sid) && ok;
    ++ch->remote_next;
  }
  return ok;
}

bool ProxyMux::DeliverOrdered(ProxyChannel* ch, const ProxyFrame& f, uint16_t sid) {
  if (f.type == sctp::kProxyData) {
    if (ch->state != ProxyState::kOpen || ch->remote_eof) return false;
    if (sid != sctp::kStreamControl && sid != ch->stream) ch->stream = sid;
    ch->bytes_rx += f.payload.size();
    if (on_data_) on_data_(ch, f.payload);
    return true;
  }

  if (f.type == sctp::kProxyDatagram) {
    if (ch->state != ProxyState::kOpen || ch->remote_eof) return false;
    ProxyDatagram dg;
    size_t used = 0;
    if (!GetProxyTarget(f.payload, &dg.peer, &used)) return false;
    dg.payload.assign(f.payload.begin() + used, f.payload.end());
    ch->bytes_rx += dg.payload.size();
    if (on_dgram_) on_dgram_(ch, dg);
    return true;
  }

  if (f.type == sctp::kProxyClose) {
    if (ch->state == ProxyState::kClosed) return false;
    uint8_t reason = f.flags;
    if (reason == sctp::kProxyEof && ch->proto == sctp::kProxyTcp) {
      ch->remote_eof = true;
      if (on_close_) on_close_(ch, reason);
      if (ch->local_eof) ch->state = ProxyState::kClosed;
      return true;
    }
    ch->remote_eof = true;
    Finish(ch, reason);
    return true;
  }

  return false;
}

}  // namespace wns
