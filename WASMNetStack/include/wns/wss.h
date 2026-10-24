#ifndef WNS_WSS_H_
#define WNS_WSS_H_

#include "wns/addr.h"
#include "wns/classify.h"
#include "wns/locator.h"
#include "wns/sctp.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wns {

enum class WssReady : uint8_t {
  kConnecting = 0,
  kOpen = 1,
  kClosing = 2,
  kClosed = 3,
};

struct WssConnect {
  std::string resource;
  std::string host;
  std::vector<std::string> protocols;
  uint16_t requested_streams = 16;
  std::vector<Ipv4> ipv4;
  std::vector<Ipv6> ipv6;
};

struct WssConnected {
  uint16_t status = 200;
  std::string selected_protocol;
  uint16_t stream_min = sctp::kStreamWssMin;
  uint16_t stream_max = sctp::kStreamWssMin + 15;
};

struct WssFrame {
  uint16_t stream_id = sctp::kStreamWssMin;
  bool fin = true;
  uint8_t opcode = sctp::kWssOpText;
  uint8_t flags = 0;
  std::vector<uint8_t> payload;
};

std::string EncodeWssConnect(const WssConnect& c);
bool DecodeWssConnect(std::string_view json, WssConnect* out);
std::string EncodeWssConnected(const WssConnected& c);
bool DecodeWssConnected(std::string_view json, WssConnected* out);
std::vector<uint8_t> EncodeWssFrame(const WssFrame& f);
bool DecodeWssFrame(std::span<const uint8_t> wire, WssFrame* out);

class WssSocket {
 public:
  WssSocket(LocatorTable* locators, Classifier* classifier, uint32_t local_node,
            uint32_t peer_node);

  bool Connect(const WssConnect& req);
  bool Accept(const WssConnect& req, uint16_t stream_min, uint16_t stream_max);
  bool OnConnected(const WssConnected& c);

  WssReady ready_state() const { return ready_; }
  uint16_t stream_min() const { return stream_min_; }
  uint16_t stream_max() const { return stream_max_; }
  uint32_t peer_node() const { return peer_node_; }
  ClassifyResult path() const;
  const WssConnect& connect() const { return connect_; }

  void PushRx(WssFrame f);
  bool TakeRx(WssFrame* out);
  size_t rx_count() const { return rx_.size(); }

  void OnPeerLocatorsChanged();

 private:
  LocatorTable* locators_ = nullptr;
  Classifier* classifier_ = nullptr;
  uint32_t local_node_ = 0;
  uint32_t peer_node_ = 0;
  WssReady ready_ = WssReady::kClosed;
  uint16_t stream_min_ = 0;
  uint16_t stream_max_ = 0;
  uint16_t requested_streams_ = 0;
  WssConnect connect_;
  std::vector<WssFrame> rx_;
};

}  // namespace wns

#endif  // WNS_WSS_H_
