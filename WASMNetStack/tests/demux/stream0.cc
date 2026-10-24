#include "test.h"

#include "wns/demux.h"
#include "wns/sctp.h"

TEST(Stream0Demux) {
  EXPECT_EQ(wns::ClassifyControl(wns::sctp::kChunkGossipControl, 0),
            wns::ControlKind::kGossip);
  EXPECT_EQ(wns::ClassifyControl(wns::sctp::kChunkData, wns::sctp::kPpidBgpControl),
            wns::ControlKind::kBgp);
  EXPECT_EQ(wns::ClassifyControl(wns::sctp::kChunkData, wns::sctp::kPpidWssControl),
            wns::ControlKind::kWss);
  EXPECT_EQ(wns::ClassifyControl(wns::sctp::kChunkData, wns::sctp::kPpidDosctp),
            wns::ControlKind::kDosctp);
  EXPECT_EQ(wns::ClassifyControl(wns::sctp::kChunkData, 1), wns::ControlKind::kReject);
  EXPECT_EQ(wns::ClassifyControl(wns::sctp::kChunkInit, 0), wns::ControlKind::kInit);
  EXPECT_EQ(wns::ClassifyControl(wns::sctp::kChunkInitAck, 0), wns::ControlKind::kInit);
}
