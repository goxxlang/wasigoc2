#ifndef WNS_DNS_H_
#define WNS_DNS_H_

#include "wns/addr.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wns {

inline constexpr uint16_t kDnsTypeA = 1;
inline constexpr uint16_t kDnsTypeAaaa = 28;
inline constexpr uint16_t kDnsClassIn = 1;

struct DnsQuestion {
  std::string name;
  uint16_t type = kDnsTypeA;
  uint16_t qclass = kDnsClassIn;
};

struct DnsRr {
  std::string name;
  uint16_t type = kDnsTypeA;
  uint16_t rclass = kDnsClassIn;
  uint32_t ttl = 60;
  Ipv4 a{};
  Ipv6 aaaa{};
};

struct DnsMessage {
  uint16_t id = 0;
  uint16_t flags = 0;
  std::vector<DnsQuestion> questions;
  std::vector<DnsRr> answers;
};

std::vector<uint8_t> EncodeDns(const DnsMessage& m);
bool DecodeDns(std::span<const uint8_t> wire, DnsMessage* out);

DnsMessage DnsQuery(uint16_t id, std::string name, uint16_t type);
DnsMessage DnsAnswerA(const DnsMessage& q, Ipv4 addr);
DnsMessage DnsAnswerAaaa(const DnsMessage& q, Ipv6 addr);

}  // namespace wns

#endif  // WNS_DNS_H_
