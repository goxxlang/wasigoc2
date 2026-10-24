#ifndef WNS_RPC_H_
#define WNS_RPC_H_

#include "wns/classify.h"
#include "wns/locator.h"
#include "wns/sctp.h"

#include <cstdint>
#include <span>
#include <vector>

namespace wns {

struct RpcFrame {
  uint8_t version = 1;
  uint8_t flags = sctp::kRpcFlagReq;
  uint64_t xid = 0;
  uint32_t service = 0;
  uint32_t method = 0;
  std::vector<uint8_t> payload;
};

std::vector<uint8_t> EncodeRpcFrame(const RpcFrame& f);
bool DecodeRpcFrame(std::span<const uint8_t> wire, RpcFrame* out);

class RpcEndpoint {
 public:
  RpcEndpoint(LocatorTable* locators, Classifier* classifier, uint32_t local_node,
              uint32_t peer_node);

  uint64_t SendRequest(uint32_t service, uint32_t method,
                       std::span<const uint8_t> payload);
  bool RecvFrame(const RpcFrame& f);
  const std::vector<uint8_t>& last_frame() const { return last_frame_; }
  const RpcFrame* last_rx() const { return have_rx_ ? &last_rx_ : nullptr; }
  bool Pending(uint64_t xid) const;
  bool Complete(uint64_t xid);

  uint32_t peer_node() const { return peer_node_; }
  ClassifyResult path() const;
  size_t pending_count() const { return pending_.size(); }

  // Locator update via gossip. MUST NOT drop XIDs.
  void OnPeerLocatorsChanged();

 private:
  LocatorTable* locators_ = nullptr;
  Classifier* classifier_ = nullptr;
  uint32_t local_node_ = 0;
  uint32_t peer_node_ = 0;
  uint64_t next_xid_ = 1;
  std::vector<uint64_t> pending_;
  std::vector<uint8_t> last_frame_;
  RpcFrame last_rx_;
  bool have_rx_ = false;
};

}  // namespace wns

#endif  // WNS_RPC_H_
