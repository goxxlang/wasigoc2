#ifndef WNS_PROXY_H_
#define WNS_PROXY_H_

#include "wns/addr.h"
#include "wns/sctp.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wns {

// Where a channel terminates. A name is resolved by the egress node, so an
// ingress node never has to see the DNS answer.
struct ProxyTarget {
  uint8_t kind = sctp::kProxyAddrName;
  std::string name;
  Ipv4 v4{};
  Ipv6 ipv6{};
  uint16_t port = 0;

  static ProxyTarget Name(std::string host, uint16_t port);
  static ProxyTarget V4(Ipv4 a, uint16_t port);
  static ProxyTarget V6(Ipv6 a, uint16_t port);

  bool operator==(const ProxyTarget& x) const;
  bool operator!=(const ProxyTarget& x) const { return !(*this == x); }
  std::string Text() const;
};

bool ParseProxyTarget(std::string_view spec, ProxyTarget* out);

// PXY v1 frame. 16-octet header, big endian, then the payload.
//
//  0                   1                   2                   3
//  +-------+-------+---------------+-------+-------+---------------+
//  |  ver  | type  |    channel    | flags | proto |    stream     |
//  +-------+-------+---------------+-------+-------+---------------+
//  |                        per-direction seq                      |
//  +---------------------------------------------------------------+
//  |                         payload length                        |
//  +---------------------------------------------------------------+
//
// `seq` orders DATA, DGRAM and CLOSE within one direction of one channel: the
// association delivers whatever stream they ride on arrival order, not TSN
// order, and control frames (stream 0) and a channel's own data (its own
// stream) can plainly race each other over a real, reordering-capable
// transport. `seq` is what lets the receiving end put them back in the order
// they were sent despite that. Each side of a channel keeps its own counter,
// starting at 0 with the first DATA/DGRAM/CLOSE it sends after OPEN or
// OPEN_ACK; OPEN, OPEN_ACK and ACK are not part of this sequence (see proxy.cc
// for why each is safe without it).
struct ProxyFrame {
  uint8_t version = sctp::kProxyVersion;
  uint8_t type = sctp::kProxyData;
  uint16_t channel = 0;
  uint8_t flags = 0;
  uint8_t proto = sctp::kProxyTcp;
  uint16_t stream = 0;
  uint32_t seq = 0;
  std::vector<uint8_t> payload;
};

inline constexpr size_t kProxyHeaderSize = 16;

std::vector<uint8_t> EncodeProxyFrame(const ProxyFrame& f);
bool DecodeProxyFrame(std::span<const uint8_t> wire, ProxyFrame* out);

// OPEN and DATAGRAM payloads carry a target; the encoding is shared.
void PutProxyTarget(std::vector<uint8_t>* out, const ProxyTarget& t);
bool GetProxyTarget(std::span<const uint8_t> wire, ProxyTarget* out, size_t* used);

enum class ProxyState : uint8_t {
  kIdle = 0,
  kOpening = 1,
  kOpen = 2,
  kClosed = 3,
};

struct ProxyChannel {
  uint16_t id = 0;
  uint16_t stream = 0;
  uint8_t proto = sctp::kProxyTcp;
  ProxyState state = ProxyState::kIdle;
  bool local_open = false;   // this node sent the OPEN
  bool local_eof = false;    // this node will not write again
  bool remote_eof = false;   // the peer will not write again
  uint8_t status = sctp::kProxyOk;
  ProxyTarget target;
  ProxyTarget bound;         // what the egress actually reached
  uint64_t bytes_tx = 0;
  uint64_t bytes_rx = 0;
  uint64_t acked_tx = 0;     // bytes the peer has handed downstream
  uint64_t consumed_rx = 0;  // bytes this node has handed downstream
  uint64_t acked_rx = 0;     // high water of what we have acknowledged
  std::uintptr_t sock = static_cast<std::uintptr_t>(-1);  // owned by the caller

  // Per-direction ordering (see ProxyFrame's `seq`). `local_seq` is the next
  // value this node assigns; `remote_next` is the next one expected back.
  // `reorder` holds frames that arrived ahead of `remote_next`, small by
  // construction since the sender's own byte-level window bounds how far
  // ahead of an ack it can ever get.
  uint32_t local_seq = 0;
  uint32_t remote_next = 0;
  std::vector<ProxyFrame> reorder;

  bool done() const { return state == ProxyState::kClosed; }
};

// One received datagram plus the peer it came from or goes to.
struct ProxyDatagram {
  ProxyTarget peer;
  std::vector<uint8_t> payload;
};

// Multiplexes TCP connections and UDP flows onto SCTP streams. The mux is pure
// state: it never touches a socket. `Sender` hands finished DATA chunks to the
// association, and the callbacks let the socket layer act on peer events.
class ProxyMux {
 public:
  using Sender = std::function<void(uint16_t stream, uint32_t ppid,
                                    std::span<const uint8_t> user, bool unordered)>;
  // Egress side. Return an OPEN_ACK status; kProxyOk accepts the channel.
  using OpenHandler = std::function<uint8_t(ProxyChannel* ch)>;
  using AckHandler = std::function<void(ProxyChannel* ch)>;
  using DataHandler = std::function<void(ProxyChannel* ch, std::span<const uint8_t> data)>;
  using DatagramHandler = std::function<void(ProxyChannel* ch, const ProxyDatagram& dg)>;
  using CloseHandler = std::function<void(ProxyChannel* ch, uint8_t reason)>;

  explicit ProxyMux(bool server = false) : server_(server) {}

  void SetSender(Sender s) { send_ = std::move(s); }
  void SetOpenHandler(OpenHandler h) { on_open_ = std::move(h); }
  void SetAckHandler(AckHandler h) { on_ack_ = std::move(h); }
  void SetDataHandler(DataHandler h) { on_data_ = std::move(h); }
  void SetDatagramHandler(DatagramHandler h) { on_dgram_ = std::move(h); }
  void SetCloseHandler(CloseHandler h) { on_close_ = std::move(h); }

  void set_server(bool server) { server_ = server; }
  bool server() const { return server_; }

  // Ingress side: allocate a channel and send OPEN. Returns 0 on exhaustion.
  uint16_t Open(uint8_t proto, const ProxyTarget& target);

  // Stream payload for a TCP channel; ordered, segmented to the path MTU.
  // It does not clamp to the window, so a caller feeding a socket reads at
  // most Writable() bytes before calling this.
  bool Send(uint16_t channel, std::span<const uint8_t> data);
  // One datagram for a UDP channel; unordered, `peer` overrides the target.
  bool SendDatagram(uint16_t channel, std::span<const uint8_t> data,
                    const ProxyTarget* peer = nullptr);
  // Half close (kProxyEof) or teardown (kProxyReset).
  bool Close(uint16_t channel, uint8_t reason);

  // Bytes the channel may still send before the window closes. UDP channels
  // are never windowed.
  size_t Writable(uint16_t channel) const;
  // Report bytes handed downstream, which is what reopens the peer's window.
  void Consumed(uint16_t channel, size_t bytes);
  void set_window(uint32_t bytes) { window_ = bytes ? bytes : sctp::kProxyWindow; }
  uint32_t window() const { return window_; }

  // Feed a decoded PXY frame in. `sid` is the stream it arrived on.
  bool OnFrame(const ProxyFrame& f, uint16_t sid);
  bool OnWire(std::span<const uint8_t> user, uint16_t sid);

  ProxyChannel* Find(uint16_t channel);
  const ProxyChannel* Find(uint16_t channel) const;
  std::vector<ProxyChannel>& channels() { return channels_; }
  const std::vector<ProxyChannel>& channels() const { return channels_; }
  size_t live_count() const;
  // Drops closed channels; returns how many were removed.
  size_t Reap();

  uint16_t StreamFor(uint16_t channel) const;

 private:
  ProxyChannel* Insert(uint16_t channel, uint16_t stream, uint8_t proto);
  bool Emit(const ProxyFrame& f, bool control, bool unordered);
  void Finish(ProxyChannel* ch, uint8_t reason);
  // Sequence-gates a DATA/DGRAM/CLOSE frame: delivers it immediately if it is
  // the next one expected, buffers it if it arrived early, and drains
  // whatever else that unblocks. OPEN, OPEN_ACK and ACK skip this gate
  // entirely (see ProxyFrame's `seq` doc comment for why each is safe to).
  bool AdmitOrdered(ProxyChannel* ch, const ProxyFrame& f, uint16_t sid);
  // The actual per-type effect of a DATA/DGRAM/CLOSE frame, called by
  // AdmitOrdered once a frame's turn has come.
  bool DeliverOrdered(ProxyChannel* ch, const ProxyFrame& f, uint16_t sid);

  bool server_ = false;
  uint16_t next_id_ = 1;
  uint32_t window_ = sctp::kProxyWindow;
  Sender send_;
  OpenHandler on_open_;
  AckHandler on_ack_;
  DataHandler on_data_;
  DatagramHandler on_dgram_;
  CloseHandler on_close_;
  std::vector<ProxyChannel> channels_;
};

}  // namespace wns

#endif  // WNS_PROXY_H_
