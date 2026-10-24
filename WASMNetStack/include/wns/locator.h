#ifndef WNS_LOCATOR_H_
#define WNS_LOCATOR_H_

#include "wns/addr.h"
#include "wns/classify.h"

#include <cstdint>
#include <vector>

namespace wns {

struct NodeLocators {
  uint32_t node_id = 0;
  uint32_t seq = 0;
  std::vector<Ipv4> ipv4;
  std::vector<Ipv6> ipv6;
  std::vector<Ipv4> real4;
  std::vector<Ipv6> real6;
  std::vector<Ipv4> internet4;
  std::vector<Ipv6> internet6;
  Ipv4 primary4{};
  Ipv6 primary6{};
  bool has_primary4 = false;
  bool has_primary6 = false;
  ClassifyResult path;
  RealInternet scope;
};

class LocatorTable {
 public:
  NodeLocators* Find(uint32_t node_id);
  const NodeLocators* Find(uint32_t node_id) const;
  NodeLocators& Ensure(uint32_t node_id);

  bool Add4(uint32_t node_id, Ipv4 a);
  bool Add6(uint32_t node_id, Ipv6 a);
  bool Delete4(uint32_t node_id, Ipv4 a);
  bool Delete6(uint32_t node_id, Ipv6 a);
  bool SetPrimary4(uint32_t node_id, Ipv4 a);
  bool SetPrimary6(uint32_t node_id, Ipv6 a);
  bool Replace4(uint32_t node_id, const std::vector<Ipv4>& addrs);
  bool Replace6(uint32_t node_id, const std::vector<Ipv6>& addrs);
  bool AddReal4(uint32_t node_id, Ipv4 a);
  bool AddReal6(uint32_t node_id, Ipv6 a);
  bool AddInternet4(uint32_t node_id, Ipv4 a);
  bool AddInternet6(uint32_t node_id, Ipv6 a);
  bool ReplaceReal4(uint32_t node_id, const std::vector<Ipv4>& addrs);
  bool ReplaceReal6(uint32_t node_id, const std::vector<Ipv6>& addrs);
  bool ReplaceInternet4(uint32_t node_id, const std::vector<Ipv4>& addrs);
  bool ReplaceInternet6(uint32_t node_id, const std::vector<Ipv6>& addrs);

  void SetSeq(uint32_t node_id, uint32_t seq);
  void SetPath(uint32_t node_id, ClassifyResult path);
  void SetScope(uint32_t node_id, RealInternet scope);

  const std::vector<NodeLocators>& All() const { return nodes_; }

 private:
  std::vector<NodeLocators> nodes_;
};

}  // namespace wns

#endif  // WNS_LOCATOR_H_
