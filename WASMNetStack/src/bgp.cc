#include "wns/bgp.h"

#include "wire.h"

#include <cstddef>

namespace wns {
namespace {

void PutPrefix4(std::vector<uint8_t>* o, const BgpPrefix4& p) {
  wire::PutU8(o, p.plen);
  int n = (p.plen + 7) / 8;
  for (int i = 0; i < n && i < 4; ++i) wire::PutU8(o, p.addr.o[i]);
}

bool GetPrefix4(std::span<const uint8_t> w, size_t* i, BgpPrefix4* p) {
  if (*i >= w.size()) return false;
  p->plen = w[*i];
  ++*i;
  int n = (p->plen + 7) / 8;
  if (*i + size_t(n) > w.size()) return false;
  for (int k = 0; k < n && k < 4; ++k) p->addr.o[k] = w[(*i)++];
  return true;
}

void PutPrefix6(std::vector<uint8_t>* o, const BgpPrefix6& p) {
  wire::PutU8(o, p.plen);
  int n = (p.plen + 7) / 8;
  for (int i = 0; i < n && i < 16; ++i) wire::PutU8(o, p.addr.o[i]);
}

bool GetPrefix6(std::span<const uint8_t> w, size_t* i, BgpPrefix6* p) {
  if (*i >= w.size()) return false;
  p->plen = w[*i];
  ++*i;
  int n = (p->plen + 7) / 8;
  if (*i + size_t(n) > w.size()) return false;
  for (int k = 0; k < n && k < 16; ++k) p->addr.o[k] = w[(*i)++];
  return true;
}

}  // namespace

std::vector<uint8_t> EncodeBgpOpen(const BgpOpen& o) {
  std::vector<uint8_t> out;
  wire::PutU8(&out, kBgpOpen);
  wire::PutBe16(&out, 10);
  wire::PutU8(&out, o.version);
  wire::PutBe16(&out, o.my_as);
  wire::PutBe16(&out, o.hold_time);
  wire::PutBe32(&out, o.bgp_id);
  wire::PutU8(&out, 0);
  return out;
}

bool DecodeBgpOpen(std::span<const uint8_t> wire, BgpOpen* out) {
  if (!out || wire.size() < 13 || wire[0] != kBgpOpen) return false;
  BgpOpen o;
  o.version = wire[3];
  o.my_as = wns::wire::GetBe16(wire.data() + 4);
  o.hold_time = wns::wire::GetBe16(wire.data() + 6);
  o.bgp_id = wns::wire::GetBe32(wire.data() + 8);
  *out = o;
  return true;
}

std::vector<uint8_t> EncodeBgpKeepalive() {
  std::vector<uint8_t> out;
  wire::PutU8(&out, kBgpKeepalive);
  wire::PutBe16(&out, 3);
  return out;
}

std::vector<uint8_t> EncodeBgpUpdate(const BgpUpdate& u) {
  std::vector<uint8_t> body;
  for (const auto& p : u.withdrawn4) PutPrefix4(&body, p);
  std::vector<uint8_t> nlri;
  for (const auto& p : u.nlri4) PutPrefix4(&nlri, p);
  for (const auto& p : u.nlri6) PutPrefix6(&nlri, p);
  std::vector<uint8_t> out;
  wire::PutU8(&out, kBgpUpdate);
  uint16_t len = uint16_t(1 + 2 + 2 + body.size() + 2 + nlri.size());
  wire::PutBe16(&out, len);
  wire::PutBe16(&out, uint16_t(body.size()));
  out.insert(out.end(), body.begin(), body.end());
  wire::PutBe16(&out, 0);  // path attributes empty
  out.insert(out.end(), nlri.begin(), nlri.end());
  return out;
}

bool DecodeBgpUpdate(std::span<const uint8_t> wire, BgpUpdate* out) {
  if (!out || wire.size() < 7 || wire[0] != kBgpUpdate) return false;
  BgpUpdate u;
  uint16_t wlen = wns::wire::GetBe16(wire.data() + 3);
  size_t i = 5;
  size_t wend = i + wlen;
  if (wend > wire.size()) return false;
  while (i < wend) {
    BgpPrefix4 p;
    if (!GetPrefix4(wire, &i, &p)) return false;
    u.withdrawn4.push_back(p);
  }
  if (i + 2 > wire.size()) return false;
  uint16_t alen = wns::wire::GetBe16(wire.data() + i);
  i += 2;
  if (i + alen > wire.size()) return false;
  i += alen;
  while (i < wire.size()) {
    // IPv6 prefixes in this codec start with plen; v4 vs v6: plen>32 or remaining
    // bytes > 4 after a v4 parse is ambiguous. nlri6 is encoded after nlri4.
    // Heuristic: if remaining looks like v6 (plen>32 or leftover > 5 after one v4).
    if (wire[i] > 32) {
      BgpPrefix6 p;
      if (!GetPrefix6(wire, &i, &p)) return false;
      u.nlri6.push_back(p);
    } else {
      BgpPrefix4 p;
      if (!GetPrefix4(wire, &i, &p)) return false;
      u.nlri4.push_back(p);
    }
  }
  *out = std::move(u);
  return true;
}

uint8_t BgpType(std::span<const uint8_t> wire) {
  return wire.empty() ? 0 : wire[0];
}

void BgpSpeaker::Configure(uint16_t my_as, uint32_t bgp_id) {
  my_as_ = my_as;
  bgp_id_ = bgp_id;
}

std::vector<uint8_t> BgpSpeaker::SendOpen() {
  BgpOpen o;
  o.my_as = my_as_;
  o.bgp_id = bgp_id_;
  state_ = BgpState::kOpenSent;
  return EncodeBgpOpen(o);
}

bool BgpSpeaker::RecvOpen(std::span<const uint8_t> wire) {
  BgpOpen o;
  if (!DecodeBgpOpen(wire, &o)) return false;
  peer_as_ = o.my_as;
  if (state_ == BgpState::kOpenSent) state_ = BgpState::kEstablished;
  else state_ = BgpState::kOpenSent;
  return true;
}

bool BgpSpeaker::Install(const BgpUpdate& u) {
  for (const auto& w : u.withdrawn4) {
    for (size_t i = 0; i < rib4_.size();) {
      if (rib4_[i].addr == w.addr && rib4_[i].plen == w.plen) {
        rib4_.erase(rib4_.begin() + static_cast<std::ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
  }
  for (const auto& p : u.nlri4) rib4_.push_back(p);
  for (const auto& p : u.nlri6) rib6_.push_back(p);
  return true;
}

}  // namespace wns
