#ifndef WNS_STACK_H_
#define WNS_STACK_H_

#include "wns/assoc.h"
#include "wns/bgp.h"
#include "wns/classify.h"
#include "wns/dns.h"
#include "wns/flow.h"
#include "wns/gls.h"
#include "wns/gossip.h"
#include "wns/locator.h"
#include "wns/proxy.h"
#include "wns/rpc.h"
#include "wns/tty.h"
#include "wns/wss.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wns {

class Stack {
 public:
  using RpcHandler = std::function<std::vector<uint8_t>(
      uint32_t service, uint32_t method, std::span<const uint8_t> payload)>;
  // Sink for finished SCTP packets when the association is not an in-process
  // fabric. The link layer binds this to a socket.
  using Transport = std::function<void(std::span<const uint8_t> packet)>;

  explicit Stack(uint32_t node_id);
  Stack(const Stack&) = delete;
  Stack& operator=(const Stack&) = delete;

  uint32_t node_id() const { return node_id_; }
  LocatorTable& locators() { return locators_; }
  const LocatorTable& locators() const { return locators_; }
  Classifier& classifier() { return classifier_; }
  Association& assoc() { return assoc_; }
  Gls& gls() { return gls_; }
  BgpSpeaker& bgp() { return bgp_; }
  ProxyMux& proxy() { return proxy_; }
  const ProxyMux& proxy() const { return proxy_; }
  TtyMux& tty() { return tty_; }
  const TtyMux& tty() const { return tty_; }
  FlowController& flow() { return flow_; }
  const FlowController& flow() const { return flow_; }

  void Connect(Stack& peer);
  // Association with a peer reached over a real transport instead of the
  // in-process fabric. Vtags are settled by INIT / INIT-ACK.
  void ConnectRemote(uint32_t peer_node, uint16_t peer_port = sctp::kPort);
  void SetTransport(Transport t);
  bool has_transport() const { return peer_ != nullptr || static_cast<bool>(transport_); }

  // Drives SACK-delay and retransmission-timeout timers. A caller that never
  // calls this (the synchronous in-process fabric in most tests) still gets
  // reliable, windowed delivery — it just never sees a timeout fire, which is
  // correct since nothing is ever actually lost on that fabric.
  void Tick(uint64_t now_ms);

  WssSocket& OpenWss(uint32_t peer_node);
  RpcEndpoint& OpenRpc(uint32_t peer_node);
  WssSocket* FindWss(uint32_t peer_node);
  RpcEndpoint* FindRpc(uint32_t peer_node);

  std::vector<uint8_t> Announce(uint8_t family, uint8_t op,
                                const std::vector<Ipv4>& v4,
                                const std::vector<Ipv6>& v6, uint8_t extra_flags = 0);
  GossipResult RecvGossip(std::span<const uint8_t> wire);
  bool Ingress(std::span<const uint8_t> wire, PacketSrc src = {});

  bool AddLocal4(Ipv4 a, bool gossip);
  bool ApplyIpifyJson(std::string_view json, bool gossip);
  std::string VisibleJson() const;
  bool FailPrimary();

  bool SendInit(PacketSrc appear_as = {});

  bool WssHandshake(const WssConnect& req);
  bool WssSend(uint16_t stream, uint8_t opcode, std::span<const uint8_t> payload);

  uint64_t RpcCall(uint32_t service, uint32_t method, std::span<const uint8_t> payload,
                   bool via_gls);
  void SetRpcHandler(RpcHandler h) { rpc_handler_ = std::move(h); }

  bool DnsQuerySend(std::string name, uint16_t type);
  void DnsPutA(std::string name, Ipv4 addr);
  const DnsMessage* last_dns() const { return have_dns_ ? &last_dns_ : nullptr; }

  bool BgpOpen();
  bool BgpAdvertise(const BgpUpdate& u);

  void BindLabel(uint32_t in_label, LabelOp op, uint32_t out_label, uint16_t stream);

  // TCP/UDP <-> SCTP <-> TCP/UDP. The mux carries the channel state; these are
  // the association-side shorthands.
  uint16_t ProxyOpen(uint8_t proto, const ProxyTarget& target);
  bool ProxySend(uint16_t channel, std::span<const uint8_t> data);
  bool ProxySendDatagram(uint16_t channel, std::span<const uint8_t> data,
                         const ProxyTarget* peer = nullptr);
  bool ProxyClose(uint16_t channel, uint8_t reason);

  // WASMTTY session. Open sends a listener WTTY hello on a TTY stream.
  uint16_t TtyOpen(uint16_t cols, uint16_t rows);
  bool TtyStdin(uint16_t stream, std::span<const uint8_t> data);
  bool TtyStdout(uint16_t stream, std::span<const uint8_t> data);
  bool TtyResize(uint16_t stream, uint16_t cols, uint16_t rows);

  ClassifyResult PathOf(uint32_t node_id) const;
  RealInternet ScopeOf(uint32_t node_id) const;

 private:
  void RefreshClass(uint32_t node_id, const ClassifyResult* prev, uint32_t prev_seq,
                    uint32_t seq);
  void RefreshScope(uint32_t node_id);
  void NotifyPeers(uint32_t node_id);
  void Transmit(std::span<const uint8_t> chunk);
  void Transmit(std::span<const uint8_t> chunk, uint32_t vtag, PacketSrc src);
  void SendData(uint16_t sid, uint32_t ppid, std::span<const uint8_t> user, bool unord);
  bool RecvChunk(std::span<const uint8_t> chunk);
  bool RecvInit(std::span<const uint8_t> chunk);
  bool RecvData(const DataChunk& d);
  bool RecvLabel(const LabelData& ld);
  bool RecvWssControl(std::span<const uint8_t> json);
  bool RecvRpc(const RpcFrame& f, uint16_t sid);
  bool RecvProxy(const DataChunk& d);
  bool RecvTty(const DataChunk& d);

  uint32_t node_id_ = 0;
  uint32_t seq_ = 0;
  uint16_t dns_id_ = 1;
  uint16_t rpc_stream_ = sctp::kStreamRpcMin;
  LocatorTable locators_;
  Classifier classifier_;
  Association assoc_;
  Gls gls_;
  BgpSpeaker bgp_;
  ProxyMux proxy_;
  TtyMux tty_;
  FlowController flow_;
  Stack* peer_ = nullptr;
  Transport transport_;
  uint64_t now_ms_ = 0;
  std::vector<std::unique_ptr<WssSocket>> wss_;
  std::vector<std::unique_ptr<RpcEndpoint>> rpc_;
  RpcHandler rpc_handler_;
  DnsMessage last_dns_;
  bool have_dns_ = false;
  std::vector<std::pair<std::string, Ipv4>> zone_;
  PacketSrc appear_as_{};
  PacketSrc last_src_{};
};

}  // namespace wns

#endif  // WNS_STACK_H_
