#include "test.h"

#include "wns/chunk.h"

TEST(DataChunkRoundtrip) {
  wns::DataChunk d;
  d.flags = wns::sctp::kDataB | wns::sctp::kDataE | wns::sctp::kDataU;
  d.tsn = 9;
  d.sid = 1024;
  d.ssn = 2;
  d.ppid = wns::sctp::kPpidWssText;
  d.user = {'h', 'i'};
  auto wire = wns::EncodeData(d);
  wns::DataChunk back;
  EXPECT(wns::DecodeData(wire, &back));
  EXPECT_EQ(back.tsn, 9u);
  EXPECT_EQ(back.sid, 1024);
  EXPECT_EQ(back.ppid, wns::sctp::kPpidWssText);
  EXPECT_EQ(back.user.size(), 2u);
}

TEST(LabelDataRoundtrip) {
  wns::LabelData d;
  wns::Label l;
  l.value = 42;
  l.bottom = true;
  l.ttl = 16;
  d.stack.push_back(l);
  d.user = {1, 2, 3, 4};
  auto wire = wns::EncodeLabelData(d);
  EXPECT_EQ(wire[0], wns::sctp::kChunkLabelData);
  wns::LabelData back;
  EXPECT(wns::DecodeLabelData(wire, &back));
  EXPECT_EQ(back.stack.size(), 1u);
  EXPECT_EQ(back.stack[0].value, 42u);
  EXPECT(back.stack[0].bottom);
  EXPECT_EQ(back.user.size(), 4u);
}

TEST(PacketWrapsChunk) {
  wns::SctpHeader h;
  h.vtag = 0xabc;
  wns::DataChunk d;
  d.user = {9};
  auto pkt = wns::EncodePacket(h, wns::EncodeData(d));
  wns::SctpHeader back;
  std::span<const uint8_t> chunk;
  EXPECT(wns::DecodePacket(pkt, &back, &chunk));
  EXPECT_EQ(back.vtag, 0xabcu);
  EXPECT_EQ(wns::ChunkType(chunk), wns::sctp::kChunkData);
}
