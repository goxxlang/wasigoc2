#ifndef WNS_ASSOC_H_
#define WNS_ASSOC_H_

#include "wns/addr.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace wns {

struct PacketSrc {
  bool has4 = false;
  bool has6 = false;
  Ipv4 v4{};
  Ipv6 v6{};

  static PacketSrc V4(Ipv4 a) {
    PacketSrc s;
    s.has4 = true;
    s.v4 = a;
    return s;
  }
  static PacketSrc V6(Ipv6 a) {
    PacketSrc s;
    s.has6 = true;
    s.v6 = a;
    return s;
  }
};

struct Path {
  bool v6 = false;
  Ipv4 v4{};
  Ipv6 ipv6{};
  bool active = true;
  bool primary = false;
};

struct Association {
  uint16_t local_port = 179;
  uint16_t peer_port = 179;
  uint32_t local_vtag = 0;
  uint32_t peer_vtag = 0;
  uint32_t peer_node = 0;
  uint32_t tsn = 1;
  std::vector<Path> paths;
  std::vector<std::pair<uint16_t, uint16_t>> ssn;

  uint32_t NextTsn() { return tsn++; }
  uint16_t NextSsn(uint16_t sid);
  bool AddPath4(Ipv4 a, bool primary);
  bool AddPath6(Ipv6 a, bool primary);
  bool FailPrimary();
  const Path* Primary() const;
};

uint32_t MakeVtag(uint32_t node_id);

}  // namespace wns

#endif  // WNS_ASSOC_H_
