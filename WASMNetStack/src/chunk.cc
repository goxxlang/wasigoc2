#include "wns/chunk.h"

#include "wire.h"

namespace wns {

uint32_t PackLabel(const Label& l) {
  uint32_t w = (l.value & 0xFFFFF) << 12;
  w |= (uint32_t(l.tc) & 0x7) << 9;
  if (l.bottom) w |= 1u << 8;
  w |= l.ttl;
  return w;
}

Label UnpackLabel(uint32_t w) {
  Label l;
  l.value = (w >> 12) & 0xFFFFF;
  l.tc = uint8_t((w >> 9) & 0x7);
  l.bottom = ((w >> 8) & 1) != 0;
  l.ttl = uint8_t(w);
  return l;
}

void PutParam(std::vector<uint8_t>* o, uint16_t type, std::span<const uint8_t> value) {
  uint16_t length = uint16_t(4 + value.size());
  wire::PutBe16(o, type);
  wire::PutBe16(o, length);
  o->insert(o->end(), value.begin(), value.end());
  wire::Pad4(o);
}

std::vector<uint8_t> EncodeInit(const InitChunk& i) {
  std::vector<uint8_t> params;
  for (auto a : i.ipv4) {
    PutParam(&params, sctp::kParamIpv4, std::span<const uint8_t>(a.o, 4));
  }
  for (auto a : i.ipv6) {
    PutParam(&params, sctp::kParamIpv6, std::span<const uint8_t>(a.o, 16));
  }
  if (i.ack) {
    std::vector<uint8_t> cookie = i.cookie;
    if (cookie.empty()) cookie = {'w', 'n', 's', 0};
    PutParam(&params, sctp::kParamCookie, cookie);
  }
  std::vector<uint8_t> out;
  uint16_t length = uint16_t(20 + params.size());
  wire::PutU8(&out, i.ack ? sctp::kChunkInitAck : sctp::kChunkInit);
  wire::PutU8(&out, 0);
  wire::PutBe16(&out, length);
  wire::PutBe32(&out, i.initiate_tag);
  wire::PutBe32(&out, i.a_rwnd);
  wire::PutBe16(&out, i.outbound);
  wire::PutBe16(&out, i.inbound);
  wire::PutBe32(&out, i.initial_tsn);
  out.insert(out.end(), params.begin(), params.end());
  wire::Pad4(&out);
  return out;
}

bool DecodeInit(std::span<const uint8_t> wire, InitChunk* out) {
  if (!out || wire.size() < 20) return false;
  uint8_t type = wire[0];
  if (type != sctp::kChunkInit && type != sctp::kChunkInitAck) return false;
  uint16_t length = wns::wire::GetBe16(wire.data() + 2);
  if (length < 20 || length > wire.size()) return false;
  InitChunk i;
  i.ack = type == sctp::kChunkInitAck;
  i.initiate_tag = wns::wire::GetBe32(wire.data() + 4);
  i.a_rwnd = wns::wire::GetBe32(wire.data() + 8);
  i.outbound = wns::wire::GetBe16(wire.data() + 12);
  i.inbound = wns::wire::GetBe16(wire.data() + 14);
  i.initial_tsn = wns::wire::GetBe32(wire.data() + 16);
  size_t p = 20;
  while (p + 4 <= length) {
    uint16_t ptype = wns::wire::GetBe16(wire.data() + p);
    uint16_t plen = wns::wire::GetBe16(wire.data() + p + 2);
    if (plen < 4 || p + plen > length) return false;
    if (ptype == sctp::kParamIpv4 && plen == 8) {
      Ipv4 a;
      a.o[0] = wire[p + 4];
      a.o[1] = wire[p + 5];
      a.o[2] = wire[p + 6];
      a.o[3] = wire[p + 7];
      i.ipv4.push_back(a);
    } else if (ptype == sctp::kParamIpv6 && plen == 20) {
      Ipv6 a;
      for (int b = 0; b < 16; ++b) a.o[b] = wire[p + 4 + b];
      i.ipv6.push_back(a);
    } else if (ptype == sctp::kParamCookie && plen >= 4) {
      i.cookie.assign(wire.begin() + p + 4, wire.begin() + p + plen);
    }
    p += plen;
    while (p % 4) ++p;
  }
  *out = std::move(i);
  return true;
}

std::vector<uint8_t> EncodeData(const DataChunk& d) {
  std::vector<uint8_t> out;
  uint16_t length = uint16_t(16 + d.user.size());
  wire::PutU8(&out, sctp::kChunkData);
  wire::PutU8(&out, d.flags);
  wire::PutBe16(&out, length);
  wire::PutBe32(&out, d.tsn);
  wire::PutBe16(&out, d.sid);
  wire::PutBe16(&out, d.ssn);
  wire::PutBe32(&out, d.ppid);
  out.insert(out.end(), d.user.begin(), d.user.end());
  wire::Pad4(&out);
  return out;
}

bool DecodeData(std::span<const uint8_t> wire, DataChunk* out) {
  if (!out || wire.size() < 16) return false;
  if (wire[0] != sctp::kChunkData) return false;
  uint16_t length = wns::wire::GetBe16(wire.data() + 2);
  if (length < 16 || length > wire.size()) return false;
  DataChunk d;
  d.flags = wire[1];
  d.tsn = wns::wire::GetBe32(wire.data() + 4);
  d.sid = wns::wire::GetBe16(wire.data() + 8);
  d.ssn = wns::wire::GetBe16(wire.data() + 10);
  d.ppid = wns::wire::GetBe32(wire.data() + 12);
  d.user.assign(wire.begin() + 16, wire.begin() + length);
  *out = std::move(d);
  return true;
}

std::vector<uint8_t> EncodeLabelData(const LabelData& d) {
  std::vector<uint8_t> out;
  uint8_t depth = uint8_t(d.stack.size());
  uint16_t length = uint16_t(8 + depth * 4 + d.user.size());
  wire::PutU8(&out, sctp::kChunkLabelData);
  wire::PutU8(&out, d.flags);
  wire::PutBe16(&out, length);
  wire::PutU8(&out, depth);
  wire::PutU8(&out, 0);
  wire::PutBe16(&out, d.ssn);
  for (const auto& l : d.stack) wire::PutBe32(&out, PackLabel(l));
  out.insert(out.end(), d.user.begin(), d.user.end());
  wire::Pad4(&out);
  return out;
}

bool DecodeLabelData(std::span<const uint8_t> wire, LabelData* out) {
  if (!out || wire.size() < 8) return false;
  if (wire[0] != sctp::kChunkLabelData) return false;
  uint16_t length = wns::wire::GetBe16(wire.data() + 2);
  uint8_t depth = wire[4];
  if (depth == 0 || depth > 16) return false;
  if (length < 8u + uint16_t(depth) * 4) return false;
  if (length > wire.size()) return false;
  LabelData d;
  d.flags = wire[1];
  d.ssn = wns::wire::GetBe16(wire.data() + 6);
  size_t off = 8;
  for (uint8_t i = 0; i < depth; ++i) {
    d.stack.push_back(UnpackLabel(wns::wire::GetBe32(wire.data() + off)));
    off += 4;
  }
  d.user.assign(wire.begin() + off, wire.begin() + length);
  *out = std::move(d);
  return true;
}

std::vector<uint8_t> EncodePacket(const SctpHeader& h, std::span<const uint8_t> chunk) {
  std::vector<uint8_t> out;
  wire::PutBe16(&out, h.src_port);
  wire::PutBe16(&out, h.dst_port);
  wire::PutBe32(&out, h.vtag);
  wire::PutBe32(&out, h.checksum);
  out.insert(out.end(), chunk.begin(), chunk.end());
  return out;
}

bool DecodePacket(std::span<const uint8_t> wire, SctpHeader* h,
                  std::span<const uint8_t>* chunk) {
  if (!h || !chunk || wire.size() < 12) return false;
  h->src_port = wns::wire::GetBe16(wire.data());
  h->dst_port = wns::wire::GetBe16(wire.data() + 2);
  h->vtag = wns::wire::GetBe32(wire.data() + 4);
  h->checksum = wns::wire::GetBe32(wire.data() + 8);
  *chunk = wire.subspan(12);
  return !chunk->empty();
}

uint8_t ChunkType(std::span<const uint8_t> chunk) {
  return chunk.empty() ? 0 : chunk[0];
}

}  // namespace wns
