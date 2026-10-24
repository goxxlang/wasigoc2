#ifndef WNS_ANYCAST_H_
#define WNS_ANYCAST_H_

#include "wns/addr.h"
#include "wns/stack.h"

#include <string>
#include <string_view>
#include <vector>

namespace wns {

// P2P anycast plane. iBGP speaker prefixes and WASMDNS pins ride this
// stack's BGP UPDATE + DoSCTP A records. Does not replace the TunnelTug
// speaker or the wasmdns.Mesh — it fans the same VIP/name state to peers.
class Anycast {
 public:
  static constexpr const char* kDefaultVip = "203.0.113.53";

  Anycast();

  void Bind(Stack* local, Stack* peer);
  bool SetVip(std::string_view ip);
  Ipv4 vip() const { return vip_; }

  // iBGP Route: "203.0.113.53/32" via next-hop (TunnelTug speaker wire).
  bool Advertise(std::string_view cidr, std::string_view nexthop);
  bool Withdraw(std::string_view cidr);

  // WASMDNS Mesh.Pin: name → VIP A. Pushed to both stacks' DoSCTP zones.
  bool Pin(std::string_view name);
  bool Lookup(std::string_view name, Ipv4* out) const;

  void SeedCCC();

  const std::vector<std::string>& pins() const { return pins_; }
  const std::vector<BgpPrefix4>& rib() const { return advertised_; }

 private:
  bool ParseCidr4(std::string_view cidr, BgpPrefix4* out) const;
  void InstallLocal(const BgpPrefix4& p, bool withdraw);

  Stack* local_ = nullptr;
  Stack* peer_ = nullptr;
  Ipv4 vip_{};
  std::vector<std::string> pins_;
  std::vector<BgpPrefix4> advertised_;
};

}  // namespace wns

#endif  // WNS_ANYCAST_H_
