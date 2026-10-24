#ifndef WNS_GOSSIP_H_
#define WNS_GOSSIP_H_

#include "wns/addr.h"
#include "wns/locator.h"
#include "wns/sctp.h"

#include <cstdint>
#include <span>
#include <vector>

namespace wns {

struct IpChangeTlv {
  uint8_t family = sctp::kFamily4;
  uint8_t op = sctp::kOpReplace;
  uint8_t flags = sctp::kIpChangeAuth;
  std::vector<Ipv4> ipv4;
  std::vector<Ipv6> ipv6;
};

struct GossipControl {
  uint8_t version = sctp::kGossipVersion;
  uint32_t originator = 0;
  uint32_t seq = 0;
  std::vector<IpChangeTlv> changes;
};

enum class GossipResult : uint8_t {
  kApplied = 0,
  kDuplicate = 1,
  kUnauth = 2,
  kBad = 3,
};

std::vector<uint8_t> EncodeGossipControl(const GossipControl& g);
bool DecodeGossipControl(std::span<const uint8_t> wire, GossipControl* out);
GossipResult ApplyGossip(LocatorTable* table, const GossipControl& g);

}  // namespace wns

#endif  // WNS_GOSSIP_H_
