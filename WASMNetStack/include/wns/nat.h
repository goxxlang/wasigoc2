#ifndef WNS_NAT_H_
#define WNS_NAT_H_

#include "wns/addr.h"
#include "wns/classify.h"

#include <vector>

namespace wns {

// NatStack: Real vs Internet is an SCTP association check, not HTTP.
// Real  = INIT / INIT-ACK address list (what the host claims it binds).
// Internet = packet source the peer actually sees (NAT mapping / ipify gossip).
class NatStack {
 public:
  static RealInternet Scope(const std::vector<Ipv4>& init4, const std::vector<Ipv6>& init6,
                            const std::vector<Ipv4>& wire4, const std::vector<Ipv6>& wire6);
};

}  // namespace wns

#endif  // WNS_NAT_H_
