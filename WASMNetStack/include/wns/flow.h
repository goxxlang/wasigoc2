#ifndef WNS_FLOW_H_
#define WNS_FLOW_H_

#include "wns/assoc.h"
#include "wns/chunk.h"
#include "wns/sctp.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <set>
#include <span>
#include <vector>

namespace wns {

// FLOW v1 SACK chunk. Gap ack blocks are offsets from cum_tsn_ack, matching
// RFC 9260's SACK layout: [start, end] means TSNs
// (cum_tsn_ack + start) .. (cum_tsn_ack + end) were received.
struct SackGapBlock {
  uint16_t start = 0;
  uint16_t end = 0;
};

struct SackChunk {
  uint32_t cum_tsn_ack = 0;
  uint32_t a_rwnd = sctp::kSctpDefaultRwnd;
  std::vector<SackGapBlock> gaps;
  std::vector<uint32_t> duplicates;
};

std::vector<uint8_t> EncodeSack(const SackChunk& s);
bool DecodeSack(std::span<const uint8_t> wire, SackChunk* out);

struct FlowStats {
  uint64_t chunks_sent = 0;
  uint64_t chunks_acked = 0;
  uint64_t chunks_retransmitted = 0;
  // A chunk's retransmit count crossed kSctpMaxRetransmits. Delivery is still
  // retried (see sctp-flow.md, section 9); this is a signal, not a give-up counter.
  uint64_t chunks_stalled = 0;
  uint64_t fast_retransmits = 0;
  uint64_t timeouts = 0;
  uint64_t sacks_sent = 0;
  uint64_t sacks_recv = 0;
  uint64_t dup_data_dropped = 0;
};

enum class DataAdmit : uint8_t {
  kNew = 0,        // deliver it
  kDuplicate = 1,  // already seen; a SACK was scheduled, nothing to deliver
  kMalformed = 2,  // not a decodable DATA chunk
};

// SCTP-level reliability and flow control for one association: SACK-based
// retransmission, RTT-driven RTO, slow-start/congestion-avoidance cwnd, and
// peer-rwnd enforcement on every DATA chunk. See sctp-flow.md for the wire
// format and the scope this implementation covers.
//
// This sits below the DATA-chunk callers (WSS, RPC, DoSCTP, BGP, PXY): they
// keep calling Stack::SendData exactly as before, and delivery order on
// arrival is unchanged (this does not add a reorder/reassembly buffer) —
// what changes is that every chunk now gets acknowledged, retransmitted on
// loss, and paced against the peer's window.
class FlowController {
 public:
  // Raw SCTP chunk bytes (DATA or SACK); the caller wraps them in a packet.
  using Sender = std::function<void(std::span<const uint8_t> chunk)>;

  explicit FlowController(Association* assoc) : assoc_(assoc) {}

  void SetSender(Sender s) { send_ = std::move(s); }

  // Builds a DATA chunk (allocating TSN/SSN from the association) and either
  // transmits it now or queues it behind the send window. `now_ms` is this
  // chunk's send timestamp for RTO purposes; every entry point takes time
  // from the caller rather than a wall clock, so a test can drive it exactly
  // and a stack that never calls Tick() simply never times anything out.
  void Send(uint16_t sid, uint32_t ppid, std::span<const uint8_t> user, bool unordered,
           uint64_t now_ms);

  // Feed an inbound SCTP chunk whose type is kChunkData. `out` is filled and
  // kNew returned when it should be delivered to the caller's DATA handler.
  DataAdmit OnData(std::span<const uint8_t> chunk, DataChunk* out, uint64_t now_ms);
  // Feed an inbound SCTP chunk whose type is kChunkSack.
  bool OnSack(std::span<const uint8_t> chunk, uint64_t now_ms);

  // Retransmission timeouts and the delayed SACK timer. Call periodically.
  void Tick(uint64_t now_ms);

  // From the peer's INIT / INIT-ACK a_rwnd field.
  void SetPeerRwnd(uint32_t rwnd) { peer_rwnd_ = rwnd; }
  // What this side advertises in its own INIT / INIT-ACK / SACK.
  uint32_t local_rwnd() const { return local_rwnd_; }
  void set_local_rwnd(uint32_t n) { local_rwnd_ = n ? n : sctp::kSctpDefaultRwnd; }

  uint32_t cwnd() const { return cwnd_; }
  uint32_t peer_rwnd() const { return peer_rwnd_; }
  uint64_t bytes_in_flight() const { return bytes_in_flight_; }
  size_t outstanding_count() const { return in_flight_.size(); }
  size_t queued_count() const { return send_queue_.size(); }
  uint32_t rto_ms() const { return rto_ms_; }
  const FlowStats& stats() const { return stats_; }

 private:
  struct Outstanding {
    uint32_t tsn = 0;
    std::vector<uint8_t> wire;  // the encoded DATA chunk, ready to resend
    size_t size = 0;
    uint64_t first_sent_ms = 0;
    uint64_t sent_ms = 0;
    uint32_t retransmits = 0;
    uint32_t miss_reports = 0;
    bool sample_ok = true;      // Karn's algorithm: no RTT sample after a resend
    bool gap_acked = false;     // selectively acked; not missing, not yet freed
    bool past_threshold = false;  // crossed kSctpMaxRetransmits; counted once
  };

  void EmitSack();
  void ScheduleSack(bool force_now, uint64_t now_ms);
  void TransmitOutstanding(Outstanding* o, bool is_retransmit, uint64_t now_ms);
  void PumpSendQueue(uint64_t now_ms);
  void OnRttSample(double sample_ms);
  void OnLossEvent();
  // Re-resolves a TSN right before acting on it. TransmitOutstanding() calls
  // the Sender callback, and on a synchronous transport (the in-process
  // fabric, or two FlowControllers cross-wired directly in a test) that can
  // reenter this same instance and mutate in_flight_ — pop entries off it,
  // push new ones on. A live iterator into in_flight_ would not survive
  // that, so every place that decides "retransmit these TSNs" collects TSN
  // values first, then looks each one up fresh via this helper in a second
  // pass, after the decision loop over in_flight_ has finished.
  Outstanding* FindByTsn(uint32_t tsn);

  Association* assoc_ = nullptr;
  Sender send_;

  // --- send side ---
  uint32_t cwnd_ = sctp::kSctpInitialCwnd;
  uint32_t ssthresh_ = 0xFFFFFFFFu;
  uint32_t peer_rwnd_ = sctp::kSctpDefaultRwnd;
  uint64_t bytes_in_flight_ = 0;
  uint32_t partial_bytes_acked_ = 0;
  double srtt_ms_ = -1;
  double rttvar_ms_ = 0;
  uint32_t rto_ms_ = sctp::kSctpInitialRtoMs;
  std::deque<Outstanding> in_flight_;         // ascending TSN order
  std::deque<std::vector<uint8_t>> send_queue_;  // encoded, waiting for room

  // --- receive side ---
  uint32_t local_rwnd_ = sctp::kSctpDefaultRwnd;
  uint32_t recv_cum_tsn_ = 0;  // highest contiguous TSN received; 0 = none
  std::set<uint32_t> recv_gaps_;
  std::vector<uint32_t> recv_dups_since_sack_;
  uint32_t unacked_chunks_ = 0;
  bool sack_pending_ = false;
  uint64_t sack_deadline_ms_ = 0;

  FlowStats stats_;
};

}  // namespace wns

#endif  // WNS_FLOW_H_
