#ifndef WNS_LINK_H_
#define WNS_LINK_H_

#include "wns/sock.h"
#include "wns/stack.h"

#include <cstdint>
#include <span>
#include <vector>

namespace wns {

// SCTP packets over UDP, the encapsulation the architecture diagram puts under
// the association. One UdpLink carries one association toward one peer.
class UdpLink {
 public:
  UdpLink() = default;
  UdpLink(const UdpLink&) = delete;
  UdpLink& operator=(const UdpLink&) = delete;

  bool Bind(const Endpoint& at);
  void SetPeer(const Endpoint& peer);
  bool have_peer() const { return have_peer_; }
  const Endpoint& peer() const { return peer_; }
  const Endpoint& local() const { return sock_.local(); }
  SockHandle handle() const { return sock_.handle(); }
  bool open() const { return sock_.open(); }

  // Binds the stack's transport to this link and remembers it for ingress.
  void Attach(Stack* stack);
  Stack* stack() const { return stack_; }

  // When set, the first datagram's source becomes the peer. This is how a
  // listening node learns a client behind NAT.
  void set_learn_peer(bool on) { learn_peer_ = on; }

  // Queues when the socket buffer is full, so a burst is not silently lost.
  bool Send(std::span<const uint8_t> packet);
  // Retries queued packets; call it when the socket reports writable.
  size_t FlushOut();
  size_t pending() const { return out_.size(); }
  uint64_t packets_dropped() const { return dropped_; }
  // Packets held before the link starts dropping.
  void set_max_queue(size_t n) { max_queue_ = n; }
  // Reads one datagram and feeds it to the stack. False when nothing was
  // pending.
  bool PumpOnce();
  // Drains up to `max` datagrams; returns how many were read.
  size_t Pump(size_t max = 1024);

  uint64_t packets_tx() const { return tx_; }
  uint64_t packets_rx() const { return rx_; }
  void Close();

 private:
  UdpSocket sock_;
  Stack* stack_ = nullptr;
  Endpoint peer_{};
  bool have_peer_ = false;
  bool learn_peer_ = true;
  uint64_t tx_ = 0;
  uint64_t rx_ = 0;
  uint64_t dropped_ = 0;
  size_t max_queue_ = 4096;
  std::vector<std::vector<uint8_t>> out_;
  std::vector<uint8_t> buf_;
};

}  // namespace wns

#endif  // WNS_LINK_H_
