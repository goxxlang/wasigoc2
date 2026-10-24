#include "wns/dns.h"

#include "wire.h"

namespace wns {
namespace {

void PutName(std::vector<uint8_t>* o, std::string_view name) {
  size_t i = 0;
  while (i < name.size()) {
    size_t j = i;
    while (j < name.size() && name[j] != '.') ++j;
    auto lab = name.substr(i, j - i);
    wire::PutU8(o, uint8_t(lab.size()));
    o->insert(o->end(), lab.begin(), lab.end());
    i = j < name.size() ? j + 1 : j;
  }
  wire::PutU8(o, 0);
}

bool GetName(std::span<const uint8_t> w, size_t* i, std::string* name) {
  name->clear();
  int hops = 0;
  while (*i < w.size() && hops++ < 128) {
    uint8_t len = w[*i];
    if (len == 0) {
      ++*i;
      return true;
    }
    if ((len & 0xC0) == 0xC0) {
      if (*i + 1 >= w.size()) return false;
      size_t ptr = ((size_t(len & 0x3F) << 8) | w[*i + 1]);
      *i += 2;
      size_t saved = *i;
      *i = ptr;
      bool ok = GetName(w, i, name);
      *i = saved;
      return ok;
    }
    ++*i;
    if (*i + len > w.size()) return false;
    if (!name->empty()) name->push_back('.');
    name->append(reinterpret_cast<const char*>(w.data() + *i), len);
    *i += len;
  }
  return false;
}

}  // namespace

std::vector<uint8_t> EncodeDns(const DnsMessage& m) {
  std::vector<uint8_t> out;
  wire::PutBe16(&out, m.id);
  wire::PutBe16(&out, m.flags);
  wire::PutBe16(&out, uint16_t(m.questions.size()));
  wire::PutBe16(&out, uint16_t(m.answers.size()));
  wire::PutBe16(&out, 0);
  wire::PutBe16(&out, 0);
  for (const auto& q : m.questions) {
    PutName(&out, q.name);
    wire::PutBe16(&out, q.type);
    wire::PutBe16(&out, q.qclass);
  }
  for (const auto& rr : m.answers) {
    PutName(&out, rr.name);
    wire::PutBe16(&out, rr.type);
    wire::PutBe16(&out, rr.rclass);
    wire::PutBe32(&out, rr.ttl);
    if (rr.type == kDnsTypeA) {
      wire::PutBe16(&out, 4);
      out.insert(out.end(), rr.a.o, rr.a.o + 4);
    } else if (rr.type == kDnsTypeAaaa) {
      wire::PutBe16(&out, 16);
      out.insert(out.end(), rr.aaaa.o, rr.aaaa.o + 16);
    } else {
      wire::PutBe16(&out, 0);
    }
  }
  return out;
}

bool DecodeDns(std::span<const uint8_t> wire, DnsMessage* out) {
  if (!out || wire.size() < 12) return false;
  DnsMessage m;
  m.id = wns::wire::GetBe16(wire.data());
  m.flags = wns::wire::GetBe16(wire.data() + 2);
  uint16_t qd = wns::wire::GetBe16(wire.data() + 4);
  uint16_t an = wns::wire::GetBe16(wire.data() + 6);
  size_t i = 12;
  for (uint16_t n = 0; n < qd; ++n) {
    DnsQuestion q;
    if (!GetName(wire, &i, &q.name)) return false;
    if (i + 4 > wire.size()) return false;
    q.type = wns::wire::GetBe16(wire.data() + i);
    q.qclass = wns::wire::GetBe16(wire.data() + i + 2);
    i += 4;
    m.questions.push_back(std::move(q));
  }
  for (uint16_t n = 0; n < an; ++n) {
    DnsRr rr;
    if (!GetName(wire, &i, &rr.name)) return false;
    if (i + 10 > wire.size()) return false;
    rr.type = wns::wire::GetBe16(wire.data() + i);
    rr.rclass = wns::wire::GetBe16(wire.data() + i + 2);
    rr.ttl = wns::wire::GetBe32(wire.data() + i + 4);
    uint16_t rdlen = wns::wire::GetBe16(wire.data() + i + 8);
    i += 10;
    if (i + rdlen > wire.size()) return false;
    if (rr.type == kDnsTypeA && rdlen == 4) {
      rr.a.o[0] = wire[i];
      rr.a.o[1] = wire[i + 1];
      rr.a.o[2] = wire[i + 2];
      rr.a.o[3] = wire[i + 3];
    } else if (rr.type == kDnsTypeAaaa && rdlen == 16) {
      for (int b = 0; b < 16; ++b) rr.aaaa.o[b] = wire[i + b];
    }
    i += rdlen;
    m.answers.push_back(std::move(rr));
  }
  *out = std::move(m);
  return true;
}

DnsMessage DnsQuery(uint16_t id, std::string name, uint16_t type) {
  DnsMessage m;
  m.id = id;
  m.flags = 0x0100;
  DnsQuestion q;
  q.name = std::move(name);
  q.type = type;
  m.questions.push_back(std::move(q));
  return m;
}

DnsMessage DnsAnswerA(const DnsMessage& q, Ipv4 addr) {
  DnsMessage m = q;
  m.flags = 0x8180;
  DnsRr rr;
  if (!q.questions.empty()) {
    rr.name = q.questions[0].name;
    rr.type = kDnsTypeA;
  }
  rr.a = addr;
  m.answers.push_back(rr);
  return m;
}

DnsMessage DnsAnswerAaaa(const DnsMessage& q, Ipv6 addr) {
  DnsMessage m = q;
  m.flags = 0x8180;
  DnsRr rr;
  if (!q.questions.empty()) {
    rr.name = q.questions[0].name;
    rr.type = kDnsTypeAaaa;
  }
  rr.aaaa = addr;
  m.answers.push_back(rr);
  return m;
}

}  // namespace wns
