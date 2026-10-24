#ifndef WNS_CLASSIFY_H_
#define WNS_CLASSIFY_H_

#include "wns/addr.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wns {

enum class PathClass : uint8_t {
  kUnknown = 0,
  kClearnet = 1,
  kVpn = 2,
  kTor = 3,
};

// SCTP INIT/INIT-ACK claims the real (on-box) addresses. The packet source
// the peer observes is the internet-visible locator (NAT mapping).
enum class PathScope : uint8_t {
  kUnknown = 0,
  kDirect = 1,
  kNat = 2,
  kLan = 3,
  kInternet = 4,
};

inline const char* PathScopeName(PathScope s) {
  switch (s) {
    case PathScope::kDirect:
      return "direct";
    case PathScope::kNat:
      return "nat";
    case PathScope::kLan:
      return "lan";
    case PathScope::kInternet:
      return "internet";
    default:
      return "unknown";
  }
}

struct RealInternet {
  PathScope scope = PathScope::kUnknown;
  bool natted = false;
  bool has_real = false;
  bool has_internet = false;
};

inline const char* PathClassName(PathClass c) {
  switch (c) {
    case PathClass::kClearnet:
      return "clearnet";
    case PathClass::kVpn:
      return "vpn";
    case PathClass::kTor:
      return "tor";
    default:
      return "unknown";
  }
}

struct ClassifyResult {
  PathClass path = PathClass::kUnknown;
  PathClass ipv4 = PathClass::kUnknown;
  PathClass ipv6 = PathClass::kUnknown;
  bool family_split = false;
  bool rapid_hop = false;
  bool ipv6_dropped = false;
};

// Prefix tables plus IP-change heuristics. Gossip delivers the new locator;
// this layer decides whether that locator is VPN, Tor, or clearnet.
class Classifier {
 public:
  void AddTorExit4(Ipv4 a, uint8_t plen = 32);
  void AddTorExit6(Ipv6 a, uint8_t plen = 128);
  void AddVpnCidr4(Ipv4 net, uint8_t plen);
  void AddVpnCidr6(Ipv6 net, uint8_t plen);
  bool AddTorExit(std::string_view cidr);
  bool AddVpnCidr(std::string_view cidr);

  PathClass Lookup4(const Ipv4& a) const;
  PathClass Lookup6(const Ipv6& a) const;

  ClassifyResult Classify(const std::vector<Ipv4>& v4, const std::vector<Ipv6>& v6,
                          const ClassifyResult* previous, uint32_t prev_seq,
                          uint32_t seq) const;

  static RealInternet DetectRealInternet(const std::vector<Ipv4>& claimed4,
                                         const std::vector<Ipv6>& claimed6,
                                         const std::vector<Ipv4>& observed4,
                                         const std::vector<Ipv6>& observed6);

 private:
  struct Cidr4 {
    Ipv4 net;
    uint8_t plen = 32;
    PathClass cls = PathClass::kUnknown;
  };
  struct Cidr6 {
    Ipv6 net;
    uint8_t plen = 128;
    PathClass cls = PathClass::kUnknown;
  };

  PathClass Longest4(const Ipv4& a) const;
  PathClass Longest6(const Ipv6& a) const;

  std::vector<Cidr4> v4_;
  std::vector<Cidr6> v6_;
};

}  // namespace wns

#endif  // WNS_CLASSIFY_H_
