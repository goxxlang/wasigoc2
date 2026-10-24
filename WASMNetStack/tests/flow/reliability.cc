#include "test.h"

#include "wns/chunk.h"
#include "wns/flow.h"

#include <set>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> Bytes(const char* s) {
  std::vector<uint8_t> o;
  for (const char* p = s; *p; ++p) o.push_back(uint8_t(*p));
  return o;
}

std::string Text(std::span<const uint8_t> b) {
  return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

// Cross-wires two FlowControllers over a link that can drop specific TSNs by
// number, so a test can script exactly which chunk goes missing and prove the
// controller notices, gaps around it, and recovers it.
struct LossyPair {
  wns::Association assoc_a;
  wns::Association assoc_b;
  wns::FlowController a{&assoc_a};
  wns::FlowController b{&assoc_b};
  uint64_t now = 0;
  std::set<uint32_t> drop_a_to_b;
  std::set<uint32_t> drop_b_to_a;
  std::vector<std::string> a_delivered;
  std::vector<std::string> b_delivered;
  int a_to_b_data_frames = 0;
  int b_to_a_sack_frames = 0;

  LossyPair() {
    a.SetSender([this](std::span<const uint8_t> chunk) { FromA(chunk); });
    b.SetSender([this](std::span<const uint8_t> chunk) { FromB(chunk); });
  }

  void FromA(std::span<const uint8_t> chunk) {
    if (chunk.empty()) return;
    if (chunk[0] == wns::sctp::kChunkData) {
      ++a_to_b_data_frames;
      wns::DataChunk d;
      if (wns::DecodeData(chunk, &d) && drop_a_to_b.count(d.tsn)) return;  // lost in transit
      wns::DataChunk out;
      auto r = b.OnData(chunk, &out, now);
      if (r == wns::DataAdmit::kNew) b_delivered.push_back(Text(out.user));
      return;
    }
    if (chunk[0] == wns::sctp::kChunkSack) b.OnSack(chunk, now);
  }

  void FromB(std::span<const uint8_t> chunk) {
    if (chunk.empty()) return;
    if (chunk[0] == wns::sctp::kChunkData) {
      wns::DataChunk d;
      if (wns::DecodeData(chunk, &d) && drop_b_to_a.count(d.tsn)) return;
      wns::DataChunk out;
      auto r = a.OnData(chunk, &out, now);
      if (r == wns::DataAdmit::kNew) a_delivered.push_back(Text(out.user));
      return;
    }
    if (chunk[0] == wns::sctp::kChunkSack) {
      ++b_to_a_sack_frames;
      a.OnSack(chunk, now);
    }
  }
};

}  // namespace

TEST(FlowDeliversInOrderAndTracksPeerRwnd) {
  LossyPair p;
  p.b.set_local_rwnd(9000);
  p.a.Send(wns::sctp::kStreamWssMin, wns::sctp::kPpidWssBinary, Bytes("one"), false, p.now);
  p.a.Send(wns::sctp::kStreamWssMin, wns::sctp::kPpidWssBinary, Bytes("two"), false, p.now);
  p.a.Send(wns::sctp::kStreamWssMin, wns::sctp::kPpidWssBinary, Bytes("three"), false, p.now);

  EXPECT_EQ(p.b_delivered.size(), 3u);
  EXPECT(p.b_delivered[0] == "one");
  EXPECT(p.b_delivered[1] == "two");
  EXPECT(p.b_delivered[2] == "three");
  // b's SACKs carried its advertised window back to a.
  EXPECT_EQ(p.a.peer_rwnd(), 9000u);
  EXPECT(p.b_to_a_sack_frames > 0);
  // The last chunk's ack is still sitting behind b's delayed-SACK timer
  // (only two unacked chunks trigger an immediate one); flushing it is what
  // brings everything to a clean, fully-acked state.
  EXPECT(p.a.outstanding_count() > 0u);
  p.now += 200;
  p.b.Tick(p.now);
  EXPECT_EQ(p.a.outstanding_count(), 0u);
  EXPECT(p.a.stats().chunks_acked >= 3u);
  EXPECT_EQ(p.a.stats().chunks_retransmitted, 0u);
}

TEST(FlowDuplicateDataIsDroppedNotRedelivered) {
  LossyPair p;
  p.a.Send(1, 0, Bytes("x"), false, p.now);
  EXPECT_EQ(p.b_delivered.size(), 1u);
  EXPECT_EQ(p.a_to_b_data_frames, 1);

  // Replay the exact same DATA chunk a second time (TSN 1: associations start
  // TSN numbering at 1, and this was the only chunk sent so far).
  wns::DataChunk d;
  d.flags = wns::sctp::kDataB | wns::sctp::kDataE;
  d.tsn = 1;
  d.sid = 1;
  d.ssn = 0;
  d.ppid = 0;
  d.user = Bytes("x");
  auto wire = wns::EncodeData(d);

  wns::DataChunk out;
  auto r = p.b.OnData(wire, &out, p.now);
  EXPECT_EQ(int(r), int(wns::DataAdmit::kDuplicate));
  EXPECT_EQ(p.b_delivered.size(), 1u);  // still just the one delivery
  EXPECT_EQ(p.b.stats().dup_data_dropped, 1u);
}

TEST(FlowGapThenRtoRetransmitRecoversLoss) {
  LossyPair p;
  // TSNs are allocated in Send() call order starting at 1: chunk2 (tsn 2) is
  // the one that goes missing.
  p.drop_a_to_b.insert(2);
  p.a.Send(1, 0, Bytes("first"), false, p.now);
  p.a.Send(1, 0, Bytes("second"), false, p.now);
  p.a.Send(1, 0, Bytes("third"), false, p.now);

  // "first" arrived and was cumulatively acked; "third" arrived out of order
  // and sits in b's gap set; "second" never showed up. "third" itself is
  // still outstanding too: the SACK its own arrival triggered went out while
  // "third" was mid-transmission, before it was recorded as in flight, so it
  // couldn't be gap-acked by that same round trip — it catches up on the
  // next SACK instead.
  EXPECT_EQ(p.b_delivered.size(), 2u);
  EXPECT(p.b_delivered[0] == "first");
  EXPECT(p.b_delivered[1] == "third");
  EXPECT_EQ(p.a.outstanding_count(), 2u);  // second (missing) + third (unacked)

  uint32_t rto = p.a.rto_ms();
  EXPECT(rto > 0);
  // The retry now gets through; only the first attempt was lost.
  p.drop_a_to_b.clear();
  p.now += rto;
  p.a.Tick(p.now);

  EXPECT(p.a.stats().timeouts >= 1u);
  EXPECT(p.a.stats().chunks_retransmitted >= 1u);
  EXPECT_EQ(p.b_delivered.size(), 3u);
  bool have_second = false;
  for (auto& s : p.b_delivered) have_second = have_second || s == "second";
  EXPECT(have_second);
  // The cumulative ack point caught up all the way once the hole filled.
  EXPECT_EQ(p.a.outstanding_count(), 0u);
  // "third" had also timed out (never acked yet, same reason as above) and
  // was retransmitted in this same Tick; by the time its retry lands, the
  // earlier retransmit of "second" has already made it cumulatively old, so
  // it is correctly recognized and dropped as a duplicate rather than
  // delivered twice — a real, harmless spurious retransmission, the same
  // kind a delayed ack can provoke in TCP or real SCTP.
  EXPECT_EQ(p.b.stats().dup_data_dropped, 1u);
}

TEST(FlowFastRetransmitBeatsTimeout) {
  LossyPair p;
  p.drop_a_to_b.insert(2);
  p.a.Send(1, 0, Bytes("a"), false, p.now);
  p.a.Send(1, 0, Bytes("b"), false, p.now);  // dropped
  // Neither has been acked by anything yet; no SACK has come back at all.
  EXPECT_EQ(p.a.outstanding_count(), 2u);

  // Three more chunks arrive after the hole; each one's SACK reports TSN 2
  // missing. The third report crosses the fast-retransmit threshold, well
  // before the RTO (left untouched, and much larger) would have fired. The
  // retry is allowed through only once fast retransmit is expected to have
  // fired, so this proves recovery happened via that path, not a timeout.
  EXPECT(p.a.rto_ms() > 100);
  p.a.Send(1, 0, Bytes("c"), false, p.now);
  p.a.Send(1, 0, Bytes("d"), false, p.now);
  p.drop_a_to_b.clear();
  p.a.Send(1, 0, Bytes("e"), false, p.now);

  EXPECT_EQ(p.a.stats().fast_retransmits, 1u);
  EXPECT_EQ(p.a.stats().timeouts, 0u);  // recovered without waiting on the RTO
  bool have_b = false;
  for (auto& s : p.b_delivered) have_b = have_b || s == "b";
  EXPECT(have_b);
}

TEST(FlowSendQueuesBehindPeerRwndAndDrainsOnCredit) {
  LossyPair p;
  p.a.SetPeerRwnd(4);  // smaller than any chunk below
  p.a.Send(1, 0, Bytes("waiting"), false, p.now);
  EXPECT_EQ(p.a.outstanding_count(), 0u);
  EXPECT_EQ(p.a.queued_count(), 1u);
  EXPECT_EQ(p.b_delivered.size(), 0u);

  // The peer's rwnd opening up, even with nothing new cumulatively acked,
  // is enough to drain the queue.
  wns::SackChunk credit;
  credit.cum_tsn_ack = 0;
  credit.a_rwnd = 65535;
  auto wire = wns::EncodeSack(credit);
  p.a.OnSack(wire, p.now);

  EXPECT_EQ(p.a.queued_count(), 0u);
  EXPECT_EQ(p.a.outstanding_count(), 1u);
  EXPECT_EQ(p.b_delivered.size(), 1u);
  EXPECT(p.b_delivered[0] == "waiting");
}

TEST(FlowMalformedDataChunkRejected) {
  LossyPair p;
  uint8_t junk[3] = {wns::sctp::kChunkData, 0, 0};
  wns::DataChunk out;
  auto r = p.b.OnData(junk, &out, p.now);
  EXPECT_EQ(int(r), int(wns::DataAdmit::kMalformed));
}
