#ifndef WNS_DEMUX_H_
#define WNS_DEMUX_H_

#include <cstdint>

namespace wns {

enum class ControlKind : uint8_t {
  kReject = 0,
  kGossip = 1,
  kBgp = 2,
  kWss = 3,
  kDosctp = 4,
  kInit = 5,
};

// Stream 0: chunk type 0xF1 -> gossip; DATA demuxed by PPID.
ControlKind ClassifyControl(uint8_t chunk_type, uint32_t ppid);

}  // namespace wns

#endif  // WNS_DEMUX_H_
