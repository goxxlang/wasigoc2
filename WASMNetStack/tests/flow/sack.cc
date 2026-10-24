#include "test.h"

#include "wns/flow.h"

TEST(SackWireRoundtrip) {
  wns::SackChunk s;
  s.cum_tsn_ack = 41;
  s.a_rwnd = 90000;
  s.gaps = {{2, 2}, {5, 7}};
  s.duplicates = {39, 39, 44};

  auto w = wns::EncodeSack(s);
  EXPECT(w[0] == wns::sctp::kChunkSack);

  wns::SackChunk back;
  EXPECT(wns::DecodeSack(w, &back));
  EXPECT_EQ(back.cum_tsn_ack, 41u);
  EXPECT_EQ(back.a_rwnd, 90000u);
  EXPECT_EQ(back.gaps.size(), 2u);
  EXPECT_EQ(back.gaps[0].start, 2u);
  EXPECT_EQ(back.gaps[0].end, 2u);
  EXPECT_EQ(back.gaps[1].start, 5u);
  EXPECT_EQ(back.gaps[1].end, 7u);
  EXPECT_EQ(back.duplicates.size(), 3u);
  EXPECT_EQ(back.duplicates[2], 44u);
}

TEST(SackWireNoGapsOrDups) {
  wns::SackChunk s;
  s.cum_tsn_ack = 1;
  s.a_rwnd = 65535;
  auto w = wns::EncodeSack(s);
  EXPECT_EQ(w.size(), 16u);  // header only, no gap/dup entries

  wns::SackChunk back;
  EXPECT(wns::DecodeSack(w, &back));
  EXPECT(back.gaps.empty());
  EXPECT(back.duplicates.empty());
}

TEST(SackWireRejectsTruncated) {
  wns::SackChunk s;
  s.cum_tsn_ack = 1;
  s.gaps = {{1, 1}};
  auto w = wns::EncodeSack(s);
  wns::SackChunk back;
  // Claims a gap block the buffer doesn't actually carry.
  w.resize(w.size() - 4);
  EXPECT(!wns::DecodeSack(w, &back));
  EXPECT(!wns::DecodeSack(std::span<const uint8_t>(w.data(), 3), &back));
}

TEST(SackWireWrongChunkTypeRejected) {
  auto w = wns::EncodeSack(wns::SackChunk{});
  w[0] = wns::sctp::kChunkData;
  wns::SackChunk back;
  EXPECT(!wns::DecodeSack(w, &back));
}
