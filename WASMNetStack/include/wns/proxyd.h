#ifndef WNS_PROXYD_H_
#define WNS_PROXYD_H_

#include "wns/link.h"
#include "wns/proxy.h"
#include "wns/sock.h"
#include "wns/stack.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wns {

// What an egress node is willing to reach on a peer's behalf.
struct ProxyPolicy {
  bool egress = false;
  bool allow_tcp = true;
  bool allow_udp = true;
  // Empty allows any target. A rule with port 0 matches every port on the host.
  std::vector<ProxyTarget> allow;

  bool Permits(uint8_t proto, const ProxyTarget& t) const;
};

// A local listener whose accepted flows are forwarded to `target` across the
// association.
struct ProxyIngress {
  bool udp = false;
  Endpoint listen{};
  ProxyTarget target;
};

struct ProxyStats {
  uint64_t opened = 0;
  uint64_t refused = 0;
  uint64_t closed = 0;
  uint64_t bytes_to_peer = 0;
  uint64_t bytes_from_peer = 0;
};

// Drives real TCP and UDP sockets against a Stack's proxy mux: the
// TCP/UDP <-> SCTP <-> TCP/UDP datapath. One node can be ingress, egress, or
// both at once.
class ProxyNode {
 public:
  explicit ProxyNode(Stack* stack);
  ~ProxyNode();
  ProxyNode(const ProxyNode&) = delete;
  ProxyNode& operator=(const ProxyNode&) = delete;

  // Ingress: listen locally, forward to `target` via the peer.
  bool AddTcpIngress(const Endpoint& at, const ProxyTarget& target);
  bool AddUdpIngress(const Endpoint& at, const ProxyTarget& target);
  // Port 0 binds an ephemeral port; this reports what was taken.
  const std::vector<ProxyIngress>& ingress() const { return ingress_; }

  // Egress: admission control for OPENs arriving from the peer.
  void SetPolicy(ProxyPolicy p) { policy_ = std::move(p); }
  const ProxyPolicy& policy() const { return policy_; }

  // The SCTP association's packet transport, watched in the same select pass.
  // Null keeps the in-process fabric.
  void AttachLink(UdpLink* link) { link_ = link; }

  // Idle UDP flows are torn down after this long. 0 disables.
  void set_udp_idle_ms(uint64_t ms) { udp_idle_ms_ = ms; }

  // One pass: wait for readiness, then move whatever is ready in both
  // directions. Returns the number of sockets serviced.
  int Poll(int timeout_ms);
  // Moves bytes without waiting; useful when the association is in-process.
  int Drain() { return Poll(0); }

  size_t flow_count() const { return flows_.size(); }
  const ProxyStats& stats() const { return stats_; }
  Stack* stack() const { return stack_; }

 private:
  struct Flow {
    uint16_t channel = 0;
    bool udp = false;
    bool ingress = false;
    bool sock_eof = false;
    bool dead = false;
    // The peer said it's done writing, but this node still owed origin some
    // queued-but-unsent bytes when that arrived (Flush() hit a non-blocking
    // socket that wasn't ready). ShutdownWrite() is deferred until the
    // backlog actually drains, so a burst near EOF is never silently
    // dropped by a shutdown that outraces it.
    bool close_pending = false;
    uint64_t last_ms = 0;
    std::unique_ptr<TcpConn> tcp;
    std::unique_ptr<UdpSocket> sock;  // egress UDP: our own socket
    UdpSocket* reply = nullptr;       // ingress UDP: the listener's socket
    Endpoint client{};                // ingress UDP: who to reply to
    Endpoint remote{};                // egress: where datagrams go
    // Datagrams a client sent before the peer accepted the channel. A UDP
    // client that sends once and waits would otherwise never be answered.
    std::vector<std::vector<uint8_t>> queued;
    size_t queued_bytes = 0;
  };

  struct TcpRule {
    std::unique_ptr<TcpListener> sock;
    ProxyTarget target;
  };
  struct UdpRule {
    std::unique_ptr<UdpSocket> sock;
    ProxyTarget target;
  };

  void WireHandlers();
  Flow* Find(uint16_t channel);
  Flow* FindUdpClient(UdpSocket* reply, const Endpoint& client);
  Flow* Add(uint16_t channel, bool udp, bool ingress);
  void Kill(Flow* f, uint8_t reason, bool tell_peer);
  void FlushQueued(Flow* f);
  void Sweep();

  uint8_t OnOpen(ProxyChannel* ch);
  void OnAck(ProxyChannel* ch);
  void OnData(ProxyChannel* ch, std::span<const uint8_t> data);
  void OnDatagram(ProxyChannel* ch, const ProxyDatagram& dg);
  void OnClose(ProxyChannel* ch, uint8_t reason);

  bool LinkCongested() const;
  int ServiceListeners(Poller* p);
  int ServiceFlows(Poller* p);

  Stack* stack_ = nullptr;
  UdpLink* link_ = nullptr;
  ProxyPolicy policy_;
  ProxyStats stats_;
  uint64_t udp_idle_ms_ = 60000;
  std::vector<ProxyIngress> ingress_;
  std::vector<TcpRule> tcp_rules_;
  std::vector<UdpRule> udp_rules_;
  std::vector<std::unique_ptr<Flow>> flows_;
  std::vector<uint8_t> buf_;
  Poller poller_;
};

}  // namespace wns

#endif  // WNS_PROXYD_H_
