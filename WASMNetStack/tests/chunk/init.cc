#include "test.h"

#include "wns/chunk.h"

TEST(InitAddressParamsRoundtrip) {
  wns::InitChunk i;
  i.initiate_tag = 0x11111111;
  i.a_rwnd = 65535;
  i.outbound = 16;
  i.inbound = 8;
  i.initial_tsn = 1;
  wns::Ipv4 a;
  wns::Ipv6 b;
  EXPECT(wns::ParseIpv4("192.168.1.10", &a));
  EXPECT(wns::ParseIpv6("fd00::1", &b));
  i.ipv4.push_back(a);
  i.ipv6.push_back(b);
  auto wire = wns::EncodeInit(i);
  EXPECT_EQ(wire[0], wns::sctp::kChunkInit);
  wns::InitChunk back;
  EXPECT(wns::DecodeInit(wire, &back));
  EXPECT(!back.ack);
  EXPECT_EQ(back.initiate_tag, 0x11111111u);
  EXPECT_EQ(back.ipv4.size(), 1u);
  EXPECT(back.ipv4[0] == a);
  EXPECT_EQ(back.ipv6.size(), 1u);
  EXPECT(back.ipv6[0] == b);
}

TEST(InitAckRequiresCookie) {
  wns::InitChunk i;
  i.ack = true;
  i.initiate_tag = 7;
  auto wire = wns::EncodeInit(i);
  EXPECT_EQ(wire[0], wns::sctp::kChunkInitAck);
  wns::InitChunk back;
  EXPECT(wns::DecodeInit(wire, &back));
  EXPECT(back.ack);
  EXPECT(!back.cookie.empty());
}
