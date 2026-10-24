#include "wns/demux.h"

#include "wns/sctp.h"

namespace wns {

ControlKind ClassifyControl(uint8_t chunk_type, uint32_t ppid) {
  if (chunk_type == sctp::kChunkGossipControl) return ControlKind::kGossip;
  if (chunk_type == sctp::kChunkInit || chunk_type == sctp::kChunkInitAck) {
    return ControlKind::kInit;
  }
  if (chunk_type != sctp::kChunkData) return ControlKind::kReject;
  switch (ppid) {
    case sctp::kPpidBgpControl:
      return ControlKind::kBgp;
    case sctp::kPpidWssControl:
      return ControlKind::kWss;
    case sctp::kPpidDosctp:
      return ControlKind::kDosctp;
    default:
      return ControlKind::kReject;
  }
}

}  // namespace wns
