#include "wns/flow.h"

#include "wns/chunk.h"
#include "wire.h"

#include <algorithm>
#include <cmath>

namespace wns {
namespace {

constexpr size_t kSackHeaderSize = 16;

}  // namespace

std::vector<uint8_t> EncodeSack(const SackChunk& s) {
  std::vector<uint8_t> out;
  uint16_t length =
      uint16_t(kSackHeaderSize + s.gaps.size() * 4 + s.duplicates.size() * 4);
  wire::PutU8(&out, sctp::kChunkSack);
  wire::PutU8(&out, 0);
  wire::PutBe16(&out, length);
  wire::PutBe32(&out, s.cum_tsn_ack);
  wire::PutBe32(&out, s.a_rwnd);
  wire::PutBe16(&out, uint16_t(s.gaps.size()));
  wire::PutBe16(&out, uint16_t(s.duplicates.size()));
  for (const auto& g : s.gaps) {
    wire::PutBe16(&out, g.start);
    wire::PutBe16(&out, g.end);
  }
  for (auto d : s.duplicates) wire::PutBe32(&out, d);
  wire::Pad4(&out);
  return out;
}

bool DecodeSack(std::span<const uint8_t> w, SackChunk* out) {
  if (!out || w.size() < kSackHeaderSize) return false;
  if (w[0] != sctp::kChunkSack) return false;
  uint16_t length = wire::GetBe16(w.data() + 2);
  if (length < kSackHeaderSize || length > w.size()) return false;
  SackChunk s;
  s.cum_tsn_ack = wire::GetBe32(w.data() + 4);
  s.a_rwnd = wire::GetBe32(w.data() + 8);
  uint16_t n_gaps = wire::GetBe16(w.data() + 12);
  uint16_t n_dups = wire::GetBe16(w.data() + 14);
  size_t p = kSackHeaderSize;
  if (p + size_t(n_gaps) * 4 + size_t(n_dups) * 4 > length) return false;
  s.gaps.reserve(n_gaps);
  for (uint16_t i = 0; i < n_gaps; ++i) {
    SackGapBlock g;
    g.start = wire::GetBe16(w.data() + p);
    g.end = wire::GetBe16(w.data() + p + 2);
    s.gaps.push_back(g);
    p += 4;
  }
  s.duplicates.reserve(n_dups);
  for (uint16_t i = 0; i < n_dups; ++i) {
    s.duplicates.push_back(wire::GetBe32(w.data() + p));
    p += 4;
  }
  *out = std::move(s);
  return true;
}

// --- send side ---------------------------------------------------------

void FlowController::Send(uint16_t sid, uint32_t ppid, std::span<const uint8_t> user,
                          bool unordered, uint64_t now_ms) {
  if (!assoc_) return;
  DataChunk d;
  d.flags = sctp::kDataB | sctp::kDataE;
  if (unordered) d.flags |= sctp::kDataU;
  d.tsn = assoc_->NextTsn();
  d.sid = sid;
  d.ssn = assoc_->NextSsn(sid);
  d.ppid = ppid;
  d.user.assign(user.begin(), user.end());
  auto wire_bytes = EncodeData(d);

  Outstanding o;
  o.tsn = d.tsn;
  o.size = wire_bytes.size();
  o.wire = std::move(wire_bytes);

  // TSN order must match wire-send order, so a chunk that cannot go out now
  // waits in the queue rather than letting a later one jump ahead of it.
  if (send_queue_.empty() &&
      bytes_in_flight_ + o.size <= std::min<uint64_t>(cwnd_, peer_rwnd_)) {
    TransmitOutstanding(&o, false, now_ms);
    in_flight_.push_back(std::move(o));
  } else {
    send_queue_.push_back(std::move(o.wire));
  }
}

void FlowController::TransmitOutstanding(Outstanding* o, bool is_retransmit,
                                         uint64_t now_ms) {
  if (!o) return;
  if (!is_retransmit) {
    o->first_sent_ms = now_ms;
    bytes_in_flight_ += o->size;
    ++stats_.chunks_sent;
  } else {
    o->sample_ok = false;  // Karn's algorithm
    ++o->retransmits;
    ++stats_.chunks_retransmitted;
  }
  o->sent_ms = now_ms;
  o->gap_acked = false;
  if (send_) send_(o->wire);
}

void FlowController::PumpSendQueue(uint64_t now_ms) {
  while (!send_queue_.empty()) {
    size_t size = send_queue_.front().size();
    if (bytes_in_flight_ + size > std::min<uint64_t>(cwnd_, peer_rwnd_)) break;
    Outstanding o;
    o.wire = std::move(send_queue_.front());
    send_queue_.pop_front();
    o.size = size;
    // TSN sits at byte offset 4 in an encoded DATA chunk (see EncodeData).
    o.tsn = wire::GetBe32(o.wire.data() + 4);
    TransmitOutstanding(&o, false, now_ms);
    in_flight_.push_back(std::move(o));
  }
}

void FlowController::OnRttSample(double sample_ms) {
  if (sample_ms < 0) return;
  if (srtt_ms_ < 0) {
    srtt_ms_ = sample_ms;
    rttvar_ms_ = sample_ms / 2;
  } else {
    // RFC 6298.
    rttvar_ms_ = 0.75 * rttvar_ms_ + 0.25 * std::fabs(srtt_ms_ - sample_ms);
    srtt_ms_ = 0.875 * srtt_ms_ + 0.125 * sample_ms;
  }
  double rto = srtt_ms_ + std::max(4.0 * rttvar_ms_, 1.0);
  rto_ms_ = uint32_t(std::clamp(rto, double(sctp::kSctpMinRtoMs), double(sctp::kSctpMaxRtoMs)));
}

void FlowController::OnLossEvent() {
  uint32_t floor = 4 * sctp::kSctpMtu;
  ssthresh_ = std::max(cwnd_ / 2, floor);
  cwnd_ = ssthresh_;
  partial_bytes_acked_ = 0;
}

FlowController::Outstanding* FlowController::FindByTsn(uint32_t tsn) {
  for (auto& o : in_flight_) {
    if (o.tsn == tsn) return &o;
  }
  return nullptr;
}

bool FlowController::OnSack(std::span<const uint8_t> chunk, uint64_t now_ms) {
  SackChunk s;
  if (!DecodeSack(chunk, &s)) return false;
  ++stats_.sacks_recv;
  peer_rwnd_ = s.a_rwnd;

  uint64_t bytes_newly_freed = 0;
  bool full_size_acked = false;
  bool saw_loss = false;

  // Cumulative progress: drop everything the peer now has contiguously.
  while (!in_flight_.empty() && in_flight_.front().tsn <= s.cum_tsn_ack) {
    auto& o = in_flight_.front();
    if (!o.gap_acked) {
      bytes_newly_freed += o.size;
      if (o.size >= sctp::kSctpMtu) full_size_acked = true;
      if (o.sample_ok) OnRttSample(double(now_ms) - double(o.first_sent_ms));
    }
    ++stats_.chunks_acked;
    in_flight_.pop_front();
  }

  // Selective acks: mark the gap-covered TSNs so they stop being "missing"
  // and their bytes come off the in-flight count, without removing them
  // (the cumulative point hasn't reached them yet).
  std::vector<uint32_t> acked_by_gap;
  for (const auto& g : s.gaps) {
    for (uint32_t tsn = s.cum_tsn_ack + g.start; tsn <= s.cum_tsn_ack + g.end; ++tsn) {
      acked_by_gap.push_back(tsn);
    }
  }
  for (uint32_t tsn : acked_by_gap) {
    for (auto& o : in_flight_) {
      if (o.tsn != tsn || o.gap_acked) continue;
      o.gap_acked = true;
      o.miss_reports = 0;
      bytes_newly_freed += o.size;
      if (o.size >= sctp::kSctpMtu) full_size_acked = true;
      if (o.sample_ok) OnRttSample(double(now_ms) - double(o.first_sent_ms));
      ++stats_.chunks_acked;
      break;
    }
  }

  // Anything still outstanding below the highest gap-acked TSN was skipped
  // over by the peer: count a miss and fast-retransmit at the threshold.
  // This is a decide-then-act split, not a single pass: TransmitOutstanding
  // calls the Sender, which on a synchronous transport can reenter this same
  // OnSack and mutate in_flight_ (see FindByTsn's comment), so nothing here
  // resends while still holding a live iterator into it.
  uint32_t highest_reported = s.cum_tsn_ack;
  for (const auto& g : s.gaps) highest_reported = std::max(highest_reported, s.cum_tsn_ack + g.end);
  std::vector<uint32_t> fast_retransmit_tsns;
  for (auto& o : in_flight_) {
    if (o.gap_acked || o.tsn >= highest_reported) continue;
    if (++o.miss_reports >= sctp::kSctpFastRetransmitMisses) {
      o.miss_reports = 0;
      saw_loss = true;
      fast_retransmit_tsns.push_back(o.tsn);
    }
  }
  for (uint32_t tsn : fast_retransmit_tsns) {
    Outstanding* o = FindByTsn(tsn);
    if (!o || o->gap_acked) continue;  // a reentrant SACK already resolved it
    ++stats_.fast_retransmits;
    TransmitOutstanding(o, true, now_ms);
  }

  if (bytes_newly_freed > 0) {
    bytes_in_flight_ -= std::min(bytes_in_flight_, bytes_newly_freed);
  }
  if (saw_loss) {
    OnLossEvent();
  } else if (bytes_newly_freed > 0) {
    if (cwnd_ < ssthresh_) {
      // Slow start: grow by what was acked, capped to one MTU per acked
      // full-sized chunk so a large cumulative jump cannot overshoot.
      cwnd_ += full_size_acked ? sctp::kSctpMtu : uint32_t(bytes_newly_freed);
    } else {
      // Congestion avoidance: roughly one MTU per window's worth acked.
      partial_bytes_acked_ += uint32_t(std::min<uint64_t>(bytes_newly_freed, 0xFFFFFFFFu));
      if (partial_bytes_acked_ >= cwnd_) {
        partial_bytes_acked_ -= cwnd_;
        cwnd_ += sctp::kSctpMtu;
      }
    }
  }

  PumpSendQueue(now_ms);
  return true;
}

// --- receive side --------------------------------------------------------

DataAdmit FlowController::OnData(std::span<const uint8_t> chunk, DataChunk* out,
                                 uint64_t now_ms) {
  DataChunk d;
  if (!DecodeData(chunk, &d)) return DataAdmit::kMalformed;

  bool is_dup = (recv_cum_tsn_ != 0 && d.tsn <= recv_cum_tsn_) || recv_gaps_.count(d.tsn) != 0;
  if (is_dup) {
    recv_dups_since_sack_.push_back(d.tsn);
    ++stats_.dup_data_dropped;
    ScheduleSack(true, now_ms);
    return DataAdmit::kDuplicate;
  }

  if (d.tsn == recv_cum_tsn_ + 1) {
    recv_cum_tsn_ = d.tsn;
    for (;;) {
      auto it = recv_gaps_.find(recv_cum_tsn_ + 1);
      if (it == recv_gaps_.end()) break;
      recv_cum_tsn_ = *it;
      recv_gaps_.erase(it);
    }
  } else {
    recv_gaps_.insert(d.tsn);
  }

  ++unacked_chunks_;
  ScheduleSack(unacked_chunks_ >= sctp::kSctpSackEvery || !recv_gaps_.empty(), now_ms);
  *out = std::move(d);
  return DataAdmit::kNew;
}

void FlowController::ScheduleSack(bool force_now, uint64_t now_ms) {
  if (force_now) {
    EmitSack();
    return;
  }
  if (!sack_pending_) {
    sack_pending_ = true;
    sack_deadline_ms_ = now_ms + sctp::kSctpSackDelayMs;
  }
}

void FlowController::EmitSack() {
  SackChunk s;
  s.cum_tsn_ack = recv_cum_tsn_;
  s.a_rwnd = local_rwnd_;
  uint32_t start = recv_cum_tsn_;
  for (auto it = recv_gaps_.begin(); it != recv_gaps_.end();) {
    uint32_t run_start = *it;
    uint32_t run_end = run_start;
    auto next = std::next(it);
    while (next != recv_gaps_.end() && *next == run_end + 1) {
      run_end = *next;
      ++next;
    }
    s.gaps.push_back({uint16_t(run_start - start), uint16_t(run_end - start)});
    it = next;
  }
  s.duplicates = recv_dups_since_sack_;
  recv_dups_since_sack_.clear();
  unacked_chunks_ = 0;
  sack_pending_ = false;
  ++stats_.sacks_sent;
  if (send_) send_(EncodeSack(s));
}

void FlowController::Tick(uint64_t now_ms) {
  if (sack_pending_ && now_ms >= sack_deadline_ms_) EmitSack();

  // Decide first, act second (see FindByTsn's comment): the retransmit calls
  // below can reenter this instance on a synchronous transport, so nothing
  // here resends while still iterating in_flight_ directly.
  std::vector<uint32_t> timed_out_tsns;
  for (auto& o : in_flight_) {
    if (o.gap_acked) continue;
    if (now_ms - o.sent_ms < rto_ms_) continue;
    // A chunk keeps retrying past kSctpMaxRetransmits rather than being
    // abandoned: giving up would let a hole in the TSN space block the
    // peer's cumulative ack forever, wedging every chunk queued behind it
    // (see sctp-flow.md, section 9). The threshold is only an observability
    // signal, counted once, that the path may be dead.
    if (!o.past_threshold && o.retransmits >= sctp::kSctpMaxRetransmits) {
      o.past_threshold = true;
      ++stats_.chunks_stalled;
    }
    timed_out_tsns.push_back(o.tsn);
    // Bound how much work one Tick does; a burst of timeouts still gets
    // serviced, just over more than one call.
    if (timed_out_tsns.size() >= 32) break;
  }

  bool timed_out = false;
  for (uint32_t tsn : timed_out_tsns) {
    Outstanding* o = FindByTsn(tsn);
    if (!o || o->gap_acked) continue;  // resolved by a reentrant SACK meanwhile
    TransmitOutstanding(o, true, now_ms);
    ++stats_.timeouts;
    timed_out = true;
  }
  if (timed_out) {
    rto_ms_ = uint32_t(std::min<uint64_t>(uint64_t(rto_ms_) * 2, sctp::kSctpMaxRtoMs));
    OnLossEvent();
    cwnd_ = sctp::kSctpMinCwnd;  // slow-start restart
  }
  PumpSendQueue(now_ms);
}

}  // namespace wns
