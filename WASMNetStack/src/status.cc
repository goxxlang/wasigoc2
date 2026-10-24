#include "wns/status.h"

#include <cstdio>

namespace wns {
namespace {

void PutJsonString(std::string* out, const std::string& s) {
  out->push_back('"');
  for (char c : s) {
    switch (c) {
      case '"': out->append("\\\""); break;
      case '\\': out->append("\\\\"); break;
      case '\n': out->append("\\n"); break;
      case '\r': out->append("\\r"); break;
      case '\t': out->append("\\t"); break;
      default:
        if (uint8_t(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", unsigned(c));
          out->append(buf);
        } else {
          out->push_back(c);
        }
    }
  }
  out->push_back('"');
}

void PutKV(std::string* out, const char* key, const std::string& val, bool comma) {
  if (comma) out->push_back(',');
  PutJsonString(out, key);
  out->push_back(':');
  PutJsonString(out, val);
}

template <typename T>
void PutNum(std::string* out, const char* key, T val, bool comma) {
  if (comma) out->push_back(',');
  PutJsonString(out, key);
  out->push_back(':');
  out->append(std::to_string(val));
}

void PutBool(std::string* out, const char* key, bool val, bool comma) {
  if (comma) out->push_back(',');
  PutJsonString(out, key);
  out->append(val ? ":true" : ":false");
}

const char* ProxyStateName(ProxyState s) {
  switch (s) {
    case ProxyState::kIdle: return "idle";
    case ProxyState::kOpening: return "opening";
    case ProxyState::kOpen: return "open";
    case ProxyState::kClosed: return "closed";
  }
  return "unknown";
}

const char* ProtoName(uint8_t proto) {
  return proto == sctp::kProxyUdp ? "udp" : "tcp";
}

}  // namespace

std::string BuildStatusJson(const std::string& label, Stack& stack, const ProxyNode& proxy,
                            const UdpLink& link) {
  std::string out;
  out.push_back('{');
  PutKV(&out, "label", label, false);
  PutNum(&out, "node", stack.node_id(), true);
  PutNum(&out, "peer_node", stack.assoc().peer_node, true);

  out.append(",\"link\":{");
  PutKV(&out, "local", link.local().Text(), false);
  PutBool(&out, "have_peer", link.have_peer(), true);
  PutKV(&out, "peer", link.have_peer() ? link.peer().Text() : std::string(), true);
  PutNum(&out, "tx", link.packets_tx(), true);
  PutNum(&out, "rx", link.packets_rx(), true);
  PutNum(&out, "dropped", link.packets_dropped(), true);
  PutNum(&out, "queue", link.pending(), true);
  out.push_back('}');

  const auto& flow = stack.flow();
  const auto& fs = flow.stats();
  out.append(",\"flow\":{");
  PutNum(&out, "cwnd", flow.cwnd(), false);
  PutNum(&out, "peer_rwnd", flow.peer_rwnd(), true);
  PutNum(&out, "local_rwnd", flow.local_rwnd(), true);
  PutNum(&out, "bytes_in_flight", flow.bytes_in_flight(), true);
  PutNum(&out, "rto_ms", flow.rto_ms(), true);
  PutNum(&out, "outstanding", flow.outstanding_count(), true);
  PutNum(&out, "queued", flow.queued_count(), true);
  PutNum(&out, "chunks_sent", fs.chunks_sent, true);
  PutNum(&out, "chunks_acked", fs.chunks_acked, true);
  PutNum(&out, "retransmitted", fs.chunks_retransmitted, true);
  PutNum(&out, "stalled", fs.chunks_stalled, true);
  PutNum(&out, "fast_retransmits", fs.fast_retransmits, true);
  PutNum(&out, "timeouts", fs.timeouts, true);
  PutNum(&out, "sacks_sent", fs.sacks_sent, true);
  PutNum(&out, "sacks_recv", fs.sacks_recv, true);
  PutNum(&out, "dup_dropped", fs.dup_data_dropped, true);
  out.push_back('}');

  const auto& ps = proxy.stats();
  out.append(",\"proxy\":{");
  PutNum(&out, "opened", ps.opened, false);
  PutNum(&out, "refused", ps.refused, true);
  PutNum(&out, "closed", ps.closed, true);
  PutNum(&out, "bytes_to_peer", ps.bytes_to_peer, true);
  PutNum(&out, "bytes_from_peer", ps.bytes_from_peer, true);
  PutNum(&out, "live_flows", proxy.flow_count(), true);
  out.push_back('}');

  const auto& pol = proxy.policy();
  out.append(",\"policy\":{");
  PutBool(&out, "egress", pol.egress, false);
  PutBool(&out, "allow_tcp", pol.allow_tcp, true);
  PutBool(&out, "allow_udp", pol.allow_udp, true);
  out.append(",\"allow\":[");
  for (size_t i = 0; i < pol.allow.size(); ++i) {
    if (i) out.push_back(',');
    out.push_back('{');
    PutKV(&out, "target", pol.allow[i].Text(), false);
    out.push_back('}');
  }
  out.append("]}");

  out.append(",\"ingress\":[");
  const auto& ing = proxy.ingress();
  for (size_t i = 0; i < ing.size(); ++i) {
    if (i) out.push_back(',');
    out.push_back('{');
    PutKV(&out, "proto", ing[i].udp ? "udp" : "tcp", false);
    PutKV(&out, "listen", ing[i].listen.Text(), true);
    PutKV(&out, "target", ing[i].target.Text(), true);
    out.push_back('}');
  }
  out.push_back(']');

  out.append(",\"channels\":[");
  const auto& chans = stack.proxy().channels();
  bool first = true;
  for (const auto& ch : chans) {
    if (!first) out.push_back(',');
    first = false;
    out.push_back('{');
    PutNum(&out, "id", ch.id, false);
    PutKV(&out, "proto", ProtoName(ch.proto), true);
    PutNum(&out, "stream", ch.stream, true);
    PutKV(&out, "state", ProxyStateName(ch.state), true);
    PutKV(&out, "target", ch.target.Text(), true);
    PutKV(&out, "bound", ch.bound.Text(), true);
    PutNum(&out, "bytes_tx", ch.bytes_tx, true);
    PutNum(&out, "bytes_rx", ch.bytes_rx, true);
    PutBool(&out, "local_eof", ch.local_eof, true);
    PutBool(&out, "remote_eof", ch.remote_eof, true);
    PutBool(&out, "local_open", ch.local_open, true);
    out.push_back('}');
  }
  out.push_back(']');

  out.push_back('}');
  return out;
}

bool StatusServer::Listen(const Endpoint& at) { return listener_.Listen(at); }

void StatusServer::Pump(const std::string& body) {
  if (!listener_.open()) return;

  for (;;) {
    Endpoint peer{};
    SockHandle h = listener_.Accept(&peer);
    if (h == kBadSock) break;
    Conn c;
    c.tcp = std::make_unique<TcpConn>();
    c.tcp->Adopt(h, peer);
    conns_.push_back(std::move(c));
  }

  std::string response;
  response.reserve(body.size() + 256);
  response.append("HTTP/1.1 200 OK\r\n");
  response.append("Content-Type: application/json\r\n");
  response.append("Cache-Control: no-store\r\n");
  response.append("Access-Control-Allow-Origin: *\r\n");
  response.append("Connection: close\r\n");
  response.append("Content-Length: ").append(std::to_string(body.size())).append("\r\n\r\n");
  response.append(body);

  for (auto it = conns_.begin(); it != conns_.end();) {
    Conn& c = *it;
    if (!c.tcp->open()) {
      it = conns_.erase(it);
      continue;
    }
    if (!c.responded) {
      uint8_t buf[1024];
      size_t got = 0;
      auto st = c.tcp->Recv(buf, sizeof(buf), &got);
      if (st == IoStatus::kOk) {
        c.in.append(reinterpret_cast<char*>(buf), got);
        // Enough of a request line to answer; don't wait for a body no one
        // sends. A stray OPTIONS preflight gets the same JSON body back,
        // which is harmless and keeps this to one code path.
        if (c.in.find("\r\n\r\n") != std::string::npos || c.in.size() > 4) {
          c.tcp->Queue(std::span<const uint8_t>(
              reinterpret_cast<const uint8_t*>(response.data()), response.size()));
          c.responded = true;
        }
      } else if (st == IoStatus::kEof || st == IoStatus::kError) {
        c.tcp->Close();
        it = conns_.erase(it);
        continue;
      }
    }
    if (c.responded) {
      c.tcp->Flush();
      if (c.tcp->pending() == 0) {
        c.tcp->Close();
        it = conns_.erase(it);
        continue;
      }
    }
    ++it;
  }
}

}  // namespace wns
