#include "wns/gossip.h"

#include "wire.h"

namespace wns {
namespace {

size_t AddrBytes(const IpChangeTlv& t) {
  if (t.family == sctp::kFamily4) return t.ipv4.size() * 4;
  return t.ipv6.size() * 16;
}

uint8_t AddrCount(const IpChangeTlv& t) {
  if (t.family == sctp::kFamily4) return uint8_t(t.ipv4.size());
  return uint8_t(t.ipv6.size());
}

void ApplyTlv(LocatorTable* table, uint32_t node, const IpChangeTlv& t) {
  if (t.family == sctp::kFamily4) {
    switch (t.op) {
      case sctp::kOpAdd:
        for (auto a : t.ipv4) table->Add4(node, a);
        break;
      case sctp::kOpDelete:
        for (auto a : t.ipv4) table->Delete4(node, a);
        break;
      case sctp::kOpSetPrimary:
        if (!t.ipv4.empty()) table->SetPrimary4(node, t.ipv4.front());
        break;
      case sctp::kOpReplace:
        table->Replace4(node, t.ipv4);
        break;
      default:
        break;
    }
    bool real = (t.flags & sctp::kIpChangeReal) != 0;
    bool inet = (t.flags & sctp::kIpChangeInternet) != 0;
    if (real) {
      if (t.op == sctp::kOpReplace) table->ReplaceReal4(node, t.ipv4);
      else if (t.op != sctp::kOpDelete)
        for (auto a : t.ipv4) table->AddReal4(node, a);
    }
    if (inet) {
      if (t.op == sctp::kOpReplace) table->ReplaceInternet4(node, t.ipv4);
      else if (t.op != sctp::kOpDelete)
        for (auto a : t.ipv4) table->AddInternet4(node, a);
    }
    return;
  }
  switch (t.op) {
    case sctp::kOpAdd:
      for (auto a : t.ipv6) table->Add6(node, a);
      break;
    case sctp::kOpDelete:
      for (auto a : t.ipv6) table->Delete6(node, a);
      break;
    case sctp::kOpSetPrimary:
      if (!t.ipv6.empty()) table->SetPrimary6(node, t.ipv6.front());
      break;
    case sctp::kOpReplace:
      table->Replace6(node, t.ipv6);
      break;
    default:
      break;
  }
  bool real = (t.flags & sctp::kIpChangeReal) != 0;
  bool inet = (t.flags & sctp::kIpChangeInternet) != 0;
  if (real) {
    if (t.op == sctp::kOpReplace) table->ReplaceReal6(node, t.ipv6);
    else if (t.op != sctp::kOpDelete)
      for (auto a : t.ipv6) table->AddReal6(node, a);
  }
  if (inet) {
    if (t.op == sctp::kOpReplace) table->ReplaceInternet6(node, t.ipv6);
    else if (t.op != sctp::kOpDelete)
      for (auto a : t.ipv6) table->AddInternet6(node, a);
  }
}

}  // namespace

std::vector<uint8_t> EncodeGossipControl(const GossipControl& g) {
  std::vector<uint8_t> tlv;
  for (const auto& c : g.changes) {
    uint16_t length = uint16_t(8 + AddrBytes(c));
    wire::PutU8(&tlv, uint8_t(sctp::kTlvIpChange));
    wire::PutBe16(&tlv, length);
    wire::PutU8(&tlv, c.flags);
    wire::PutU8(&tlv, c.family);
    wire::PutU8(&tlv, c.op);
    wire::PutU8(&tlv, AddrCount(c));
    wire::PutU8(&tlv, 0);
    if (c.family == sctp::kFamily4) {
      for (auto a : c.ipv4) {
        tlv.insert(tlv.end(), a.o, a.o + 4);
      }
    } else {
      for (auto a : c.ipv6) {
        tlv.insert(tlv.end(), a.o, a.o + 16);
      }
    }
    wire::Pad4(&tlv);
  }

  std::vector<uint8_t> out;
  uint16_t unpadded = uint16_t(16 + tlv.size());
  wire::PutU8(&out, sctp::kChunkGossipControl);
  wire::PutU8(&out, 0);
  wire::PutBe16(&out, unpadded);
  wire::PutBe32(&out, g.version);
  wire::PutBe32(&out, g.originator);
  wire::PutBe32(&out, g.seq);
  out.insert(out.end(), tlv.begin(), tlv.end());
  wire::Pad4(&out);
  return out;
}

bool DecodeGossipControl(std::span<const uint8_t> wire, GossipControl* out) {
  if (!out || wire.size() < 16) return false;
  if (wire[0] != sctp::kChunkGossipControl) return false;
  uint16_t clen = wns::wire::GetBe16(wire.data() + 2);
  if (clen < 16 || clen > wire.size()) return false;
  GossipControl g;
  g.version = uint8_t(wns::wire::GetBe32(wire.data() + 4));
  g.originator = wns::wire::GetBe32(wire.data() + 8);
  g.seq = wns::wire::GetBe32(wire.data() + 12);
  size_t i = 16;
  while (i + 8 <= clen) {
    uint8_t type = wire[i];
    uint16_t length = wns::wire::GetBe16(wire.data() + i + 1);
    uint8_t flags = wire[i + 3];
    if (length < 8 || i + length > clen) return false;
    if (type != uint8_t(sctp::kTlvIpChange)) {
      i += length;
      while (i % 4) ++i;
      continue;
    }
    IpChangeTlv t;
    t.flags = flags;
    t.family = wire[i + 4];
    t.op = wire[i + 5];
    uint8_t count = wire[i + 6];
    size_t payload = i + 8;
    if (t.family == sctp::kFamily4) {
      if (payload + size_t(count) * 4 > i + length) return false;
      for (uint8_t n = 0; n < count; ++n) {
        Ipv4 a;
        a.o[0] = wire[payload + n * 4];
        a.o[1] = wire[payload + n * 4 + 1];
        a.o[2] = wire[payload + n * 4 + 2];
        a.o[3] = wire[payload + n * 4 + 3];
        t.ipv4.push_back(a);
      }
    } else if (t.family == sctp::kFamily6) {
      if (payload + size_t(count) * 16 > i + length) return false;
      for (uint8_t n = 0; n < count; ++n) {
        Ipv6 a;
        for (int b = 0; b < 16; ++b) a.o[b] = wire[payload + n * 16 + b];
        t.ipv6.push_back(a);
      }
    } else {
      return false;
    }
    g.changes.push_back(std::move(t));
    i += length;
    while (i % 4) ++i;
  }
  *out = std::move(g);
  return true;
}

GossipResult ApplyGossip(LocatorTable* table, const GossipControl& g) {
  if (!table || g.originator == 0) return GossipResult::kBad;
  for (const auto& t : g.changes) {
    if ((t.flags & sctp::kIpChangeAuth) == 0) return GossipResult::kUnauth;
  }
  auto* existing = table->Find(g.originator);
  if (existing && g.seq <= existing->seq) return GossipResult::kDuplicate;
  for (const auto& t : g.changes) ApplyTlv(table, g.originator, t);
  table->SetSeq(g.originator, g.seq);
  return GossipResult::kApplied;
}

}  // namespace wns
