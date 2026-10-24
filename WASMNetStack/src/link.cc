#include "wns/link.h"

namespace wns {
namespace {

constexpr size_t kMtu = 65535;
// The association has no retransmission, so a burst that overruns a socket
// buffer is lost for good. Ask for room.
constexpr int kLinkBuf = 4 << 20;

PacketSrc SrcOf(const Endpoint& e) {
  return e.v6 ? PacketSrc::V6(e.ipv6) : PacketSrc::V4(e.v4);
}

}  // namespace

bool UdpLink::Bind(const Endpoint& at) {
  buf_.resize(kMtu);
  if (!sock_.Bind(at)) return false;
  SockSetBuffers(sock_.handle(), kLinkBuf, kLinkBuf);
  return true;
}

void UdpLink::SetPeer(const Endpoint& peer) {
  peer_ = peer;
  have_peer_ = true;
}

void UdpLink::Attach(Stack* stack) {
  stack_ = stack;
  if (!stack_) return;
  stack_->SetTransport([this](std::span<const uint8_t> packet) { Send(packet); });
}

bool UdpLink::Send(std::span<const uint8_t> packet) {
  if (!have_peer_ || !sock_.open()) return false;
  if (!out_.empty()) {
    FlushOut();
    if (!out_.empty()) {
      // Keep ordering: once anything is queued, everything queues behind it.
      if (out_.size() >= max_queue_) {
        ++dropped_;
        return false;
      }
      out_.emplace_back(packet.begin(), packet.end());
      return true;
    }
  }
  auto st = sock_.SendTo(packet, peer_);
  if (st == IoStatus::kOk) {
    ++tx_;
    return true;
  }
  if (st != IoStatus::kAgain) return false;
  if (out_.size() >= max_queue_) {
    ++dropped_;
    return false;
  }
  out_.emplace_back(packet.begin(), packet.end());
  return true;
}

size_t UdpLink::FlushOut() {
  if (!have_peer_ || !sock_.open()) return 0;
  size_t n = 0;
  while (!out_.empty()) {
    auto st = sock_.SendTo(out_.front(), peer_);
    if (st == IoStatus::kAgain) break;
    if (st == IoStatus::kOk) {
      ++tx_;
      ++n;
    } else {
      ++dropped_;
    }
    out_.erase(out_.begin());
  }
  return n;
}

bool UdpLink::PumpOnce() {
  if (!sock_.open()) return false;
  if (buf_.size() < kMtu) buf_.resize(kMtu);
  size_t got = 0;
  Endpoint from{};
  if (sock_.RecvFrom(buf_.data(), buf_.size(), &got, &from) != IoStatus::kOk) return false;
  if (got == 0) return false;
  if (!have_peer_ && learn_peer_) SetPeer(from);
  // A second speaker on the same port is not part of this association; the
  // datagram is consumed and dropped, so draining continues.
  if (have_peer_ && from != peer_) return true;
  ++rx_;
  if (stack_) stack_->Ingress(std::span<const uint8_t>(buf_.data(), got), SrcOf(from));
  return true;
}

size_t UdpLink::Pump(size_t max) {
  size_t n = 0;
  for (size_t i = 0; i < max; ++i) {
    if (!PumpOnce()) break;
    ++n;
  }
  return n;
}

void UdpLink::Close() {
  sock_.Close();
  out_.clear();
  if (stack_) stack_->SetTransport(nullptr);
  stack_ = nullptr;
}

}  // namespace wns
