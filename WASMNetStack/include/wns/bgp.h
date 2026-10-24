#ifndef WNS_BGP_H_
#define WNS_BGP_H_

#include "wns/addr.h"

#include <cstdint>
#include <span>
#include <vector>

namespace wns {

inline constexpr uint8_t kBgpOpen = 1;
inline constexpr uint8_t kBgpUpdate = 2;
inline constexpr uint8_t kBgpNotification = 3;
inline constexpr uint8_t kBgpKeepalive = 4;

enum class BgpState : uint8_t { kIdle = 0, kOpenSent = 1, kEstablished = 2 };

struct BgpOpen {
  uint8_t version = 4;
  uint16_t my_as = 0;
  uint16_t hold_time = 90;
  uint32_t bgp_id = 0;
};

struct BgpPrefix4 {
  Ipv4 addr{};
  uint8_t plen = 32;
};

struct BgpPrefix6 {
  Ipv6 addr{};
  uint8_t plen = 128;
};

struct BgpUpdate {
  std::vector<BgpPrefix4> withdrawn4;
  std::vector<BgpPrefix4> nlri4;
  std::vector<BgpPrefix6> nlri6;
};

struct BgpNotification {
  uint8_t code = 0;
  uint8_t subcode = 0;
};

std::vector<uint8_t> EncodeBgpOpen(const BgpOpen& o);
bool DecodeBgpOpen(std::span<const uint8_t> wire, BgpOpen* out);
std::vector<uint8_t> EncodeBgpKeepalive();
std::vector<uint8_t> EncodeBgpUpdate(const BgpUpdate& u);
bool DecodeBgpUpdate(std::span<const uint8_t> wire, BgpUpdate* out);
uint8_t BgpType(std::span<const uint8_t> wire);

class BgpSpeaker {
 public:
  void Configure(uint16_t my_as, uint32_t bgp_id);
  BgpState state() const { return state_; }
  uint16_t peer_as() const { return peer_as_; }

  std::vector<uint8_t> SendOpen();
  bool RecvOpen(std::span<const uint8_t> wire);
  void Established() { state_ = BgpState::kEstablished; }
  bool configured() const { return my_as_ != 0; }

  bool Install(const BgpUpdate& u);
  const std::vector<BgpPrefix4>& rib4() const { return rib4_; }
  const std::vector<BgpPrefix6>& rib6() const { return rib6_; }

 private:
  uint16_t my_as_ = 0;
  uint16_t peer_as_ = 0;
  uint32_t bgp_id_ = 0;
  BgpState state_ = BgpState::kIdle;
  std::vector<BgpPrefix4> rib4_;
  std::vector<BgpPrefix6> rib6_;
};

}  // namespace wns

#endif  // WNS_BGP_H_
