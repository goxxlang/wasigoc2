#include "wns/wss.h"

#include "wns/sctp.h"

#include <cstdio>
#include <sstream>

namespace wns {
namespace {

std::string Escape(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') out.push_back('\\');
    out.push_back(c);
  }
  return out;
}

bool ExtractString(std::string_view json, std::string_view key, std::string* out) {
  std::string needle = "\"";
  needle.append(key);
  needle.append("\"");
  auto pos = json.find(needle);
  if (pos == std::string_view::npos) return false;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string_view::npos) return false;
  pos = json.find('"', pos + 1);
  if (pos == std::string_view::npos) return false;
  ++pos;
  std::string value;
  for (; pos < json.size(); ++pos) {
    char c = json[pos];
    if (c == '\\' && pos + 1 < json.size()) {
      value.push_back(json[++pos]);
      continue;
    }
    if (c == '"') break;
    value.push_back(c);
  }
  *out = std::move(value);
  return true;
}

bool ExtractU16(std::string_view json, std::string_view key, uint16_t* out) {
  std::string needle = "\"";
  needle.append(key);
  needle.append("\"");
  auto pos = json.find(needle);
  if (pos == std::string_view::npos) return false;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string_view::npos) return false;
  ++pos;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  unsigned v = 0;
  if (std::sscanf(json.data() + pos, "%u", &v) != 1) return false;
  *out = uint16_t(v);
  return true;
}

void ExtractIpArray(std::string_view json, std::string_view key, bool v6,
                    std::vector<Ipv4>* v4, std::vector<Ipv6>* v6out) {
  auto pos = json.find(key);
  if (pos == std::string_view::npos) return;
  pos = json.find('[', pos);
  if (pos == std::string_view::npos) return;
  ++pos;
  while (pos < json.size() && json[pos] != ']') {
    while (pos < json.size() && json[pos] != '"' && json[pos] != ']') ++pos;
    if (pos >= json.size() || json[pos] == ']') break;
    ++pos;
    std::string ip;
    while (pos < json.size() && json[pos] != '"') ip.push_back(json[pos++]);
    if (v6) {
      Ipv6 a;
      if (ParseIpv6(ip, &a)) v6out->push_back(a);
    } else {
      Ipv4 a;
      if (ParseIpv4(ip, &a)) v4->push_back(a);
    }
    if (pos < json.size()) ++pos;
  }
}

}  // namespace

std::string EncodeWssConnect(const WssConnect& c) {
  std::ostringstream os;
  os << "{\"type\":\"CONNECT\",\"resource\":\"" << Escape(c.resource)
     << "\",\"host\":\"" << Escape(c.host) << "\",\"protocols\":[";
  for (size_t i = 0; i < c.protocols.size(); ++i) {
    if (i) os << ',';
    os << '"' << Escape(c.protocols[i]) << '"';
  }
  os << "],\"requestedStreams\":" << c.requested_streams << ",\"locators\":{\"ipv4\":[";
  for (size_t i = 0; i < c.ipv4.size(); ++i) {
    if (i) os << ',';
    os << '"' << FormatIpv4(c.ipv4[i]) << '"';
  }
  os << "],\"ipv6\":[";
  for (size_t i = 0; i < c.ipv6.size(); ++i) {
    if (i) os << ',';
    os << '"' << FormatIpv6(c.ipv6[i]) << '"';
  }
  os << "]}}";
  return os.str();
}

bool DecodeWssConnect(std::string_view json, WssConnect* out) {
  if (!out) return false;
  WssConnect c;
  ExtractString(json, "resource", &c.resource);
  ExtractString(json, "host", &c.host);
  ExtractU16(json, "requestedStreams", &c.requested_streams);
  auto ppos = json.find("\"protocols\"");
  if (ppos != std::string_view::npos) {
    auto b = json.find('[', ppos);
    auto e = json.find(']', b);
    if (b != std::string_view::npos && e != std::string_view::npos) {
      auto slice = json.substr(b, e - b);
      size_t i = 0;
      while (i < slice.size()) {
        auto q = slice.find('"', i);
        if (q == std::string_view::npos) break;
        ++q;
        std::string proto;
        while (q < slice.size() && slice[q] != '"') proto.push_back(slice[q++]);
        if (!proto.empty()) c.protocols.push_back(proto);
        i = q + 1;
      }
    }
  }
  ExtractIpArray(json, "\"ipv4\"", false, &c.ipv4, &c.ipv6);
  ExtractIpArray(json, "\"ipv6\"", true, &c.ipv4, &c.ipv6);
  *out = std::move(c);
  return true;
}

std::string EncodeWssConnected(const WssConnected& c) {
  std::ostringstream os;
  os << "{\"type\":\"CONNECTED\",\"status\":" << c.status << ",\"selectedProtocol\":\""
     << Escape(c.selected_protocol) << "\",\"streamRange\":{\"min\":" << c.stream_min
     << ",\"max\":" << c.stream_max << "}}";
  return os.str();
}

bool DecodeWssConnected(std::string_view json, WssConnected* out) {
  if (!out) return false;
  WssConnected c;
  ExtractU16(json, "status", &c.status);
  ExtractString(json, "selectedProtocol", &c.selected_protocol);
  ExtractU16(json, "min", &c.stream_min);
  ExtractU16(json, "max", &c.stream_max);
  *out = c;
  return true;
}

std::vector<uint8_t> EncodeWssFrame(const WssFrame& f) {
  std::vector<uint8_t> out;
  uint8_t b0 = uint8_t(f.opcode & 0x0F);
  if (f.fin) b0 |= sctp::kWssFin;
  out.push_back(b0);
  out.push_back(f.flags);
  out.push_back(0);
  out.push_back(0);
  uint32_t n = uint32_t(f.payload.size());
  out.push_back(uint8_t(n >> 24));
  out.push_back(uint8_t(n >> 16));
  out.push_back(uint8_t(n >> 8));
  out.push_back(uint8_t(n));
  out.insert(out.end(), f.payload.begin(), f.payload.end());
  return out;
}

bool DecodeWssFrame(std::span<const uint8_t> wire, WssFrame* out) {
  if (!out || wire.size() < 8) return false;
  WssFrame f;
  f.fin = (wire[0] & sctp::kWssFin) != 0;
  f.opcode = wire[0] & 0x0F;
  f.flags = wire[1];
  uint32_t n = (uint32_t(wire[4]) << 24) | (uint32_t(wire[5]) << 16) |
               (uint32_t(wire[6]) << 8) | uint32_t(wire[7]);
  if (wire.size() < 8 + n) return false;
  f.payload.assign(wire.begin() + 8, wire.begin() + 8 + n);
  *out = std::move(f);
  return true;
}

WssSocket::WssSocket(LocatorTable* locators, Classifier* classifier, uint32_t local_node,
                     uint32_t peer_node)
    : locators_(locators), classifier_(classifier), local_node_(local_node),
      peer_node_(peer_node) {}

bool WssSocket::Connect(const WssConnect& req) {
  if (!locators_) return false;
  locators_->Replace4(local_node_, req.ipv4);
  locators_->Replace6(local_node_, req.ipv6);
  if (classifier_) {
    locators_->SetPath(local_node_,
                       classifier_->Classify(req.ipv4, req.ipv6, nullptr, 0, 0));
  }
  requested_streams_ = req.requested_streams;
  connect_ = req;
  ready_ = WssReady::kConnecting;
  return true;
}

bool WssSocket::Accept(const WssConnect& req, uint16_t stream_min, uint16_t stream_max) {
  if (!locators_) return false;
  locators_->Replace4(peer_node_, req.ipv4);
  locators_->Replace6(peer_node_, req.ipv6);
  if (classifier_) {
    locators_->SetPath(peer_node_,
                       classifier_->Classify(req.ipv4, req.ipv6, nullptr, 0, 0));
  }
  stream_min_ = stream_min ? stream_min : sctp::kStreamWssMin;
  stream_max_ = stream_max ? stream_max : uint16_t(stream_min_ + req.requested_streams - 1);
  requested_streams_ = req.requested_streams;
  connect_ = req;
  ready_ = WssReady::kOpen;
  return true;
}

bool WssSocket::OnConnected(const WssConnected& c) {
  stream_min_ = c.stream_min;
  stream_max_ = c.stream_max;
  ready_ = WssReady::kOpen;
  return true;
}

void WssSocket::PushRx(WssFrame f) { rx_.push_back(std::move(f)); }

bool WssSocket::TakeRx(WssFrame* out) {
  if (!out || rx_.empty()) return false;
  *out = std::move(rx_.front());
  rx_.erase(rx_.begin());
  return true;
}

ClassifyResult WssSocket::path() const {
  if (!locators_) return {};
  if (auto* n = locators_->Find(peer_node_)) return n->path;
  return {};
}

void WssSocket::OnPeerLocatorsChanged() {
  // Identity is the association + stream range, not the locator.
}

}  // namespace wns
