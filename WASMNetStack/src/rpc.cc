#include "wns/rpc.h"

#include "wire.h"

#include <cstddef>

namespace wns {

std::vector<uint8_t> EncodeRpcFrame(const RpcFrame& f) {
  std::vector<uint8_t> out;
  wire::PutU8(&out, f.version);
  wire::PutU8(&out, f.flags);
  wire::PutBe16(&out, 0);
  wire::PutBe64(&out, f.xid);
  wire::PutBe32(&out, f.service);
  wire::PutBe32(&out, f.method);
  wire::PutBe32(&out, uint32_t(f.payload.size()));
  out.insert(out.end(), f.payload.begin(), f.payload.end());
  return out;
}

bool DecodeRpcFrame(std::span<const uint8_t> wire, RpcFrame* out) {
  if (!out || wire.size() < 24) return false;
  RpcFrame f;
  f.version = wire[0];
  f.flags = wire[1];
  f.xid = wns::wire::GetBe64(wire.data() + 4);
  f.service = wns::wire::GetBe32(wire.data() + 12);
  f.method = wns::wire::GetBe32(wire.data() + 16);
  uint32_t plen = wns::wire::GetBe32(wire.data() + 20);
  if (wire.size() < 24 + plen) return false;
  f.payload.assign(wire.begin() + 24, wire.begin() + 24 + plen);
  *out = std::move(f);
  return true;
}

RpcEndpoint::RpcEndpoint(LocatorTable* locators, Classifier* classifier,
                         uint32_t local_node, uint32_t peer_node)
    : locators_(locators), classifier_(classifier), local_node_(local_node),
      peer_node_(peer_node) {
  (void)classifier_;
  (void)local_node_;
}

uint64_t RpcEndpoint::SendRequest(uint32_t service, uint32_t method,
                                  std::span<const uint8_t> payload) {
  uint64_t xid = next_xid_++;
  pending_.push_back(xid);
  RpcFrame f;
  f.flags = sctp::kRpcFlagReq;
  f.xid = xid;
  f.service = service;
  f.method = method;
  f.payload.assign(payload.begin(), payload.end());
  last_frame_ = EncodeRpcFrame(f);
  return xid;
}

bool RpcEndpoint::RecvFrame(const RpcFrame& f) {
  last_rx_ = f;
  have_rx_ = true;
  if (f.flags & sctp::kRpcFlagRes) Complete(f.xid);
  return true;
}

bool RpcEndpoint::Pending(uint64_t xid) const {
  for (auto p : pending_) {
    if (p == xid) return true;
  }
  return false;
}

bool RpcEndpoint::Complete(uint64_t xid) {
  for (size_t i = 0; i < pending_.size(); ++i) {
    if (pending_[i] == xid) {
      pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
  }
  return false;
}

ClassifyResult RpcEndpoint::path() const {
  if (!locators_) return {};
  if (auto* n = locators_->Find(peer_node_)) return n->path;
  return {};
}

void RpcEndpoint::OnPeerLocatorsChanged() {
  // XIDs stay valid across locator replacement.
}

}  // namespace wns
