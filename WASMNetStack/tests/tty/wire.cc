#include "test.h"

#include "wns/stack.h"
#include "wns/tty.h"

#include <string>

TEST(TtyFrameRoundtrip) {
  wns::TtyFrame f;
  f.type = wns::sctp::kTtyStdout;
  f.flags = 1;
  f.payload = {'h', 'e', 'l', 'l', 'o'};
  auto w = wns::EncodeTtyFrame(f);
  EXPECT_EQ(w.size(), wns::sctp::kTtyHeaderSize + 5);
  EXPECT_EQ(w[0], wns::sctp::kTtyStdout);
  EXPECT_EQ(w[1], 1);
  EXPECT_EQ(w[2], 0);
  EXPECT_EQ(w[3], 5);

  wns::TtyFrame g;
  EXPECT(wns::DecodeTtyFrame(w, &g));
  EXPECT_EQ(g.type, f.type);
  EXPECT_EQ(g.flags, f.flags);
  EXPECT_EQ(g.payload.size(), 5u);
  EXPECT_EQ(g.payload[0], 'h');

  w.resize(w.size() - 1);
  EXPECT(!wns::DecodeTtyFrame(w, &g));
}

TEST(TtyDecoderSplit) {
  wns::TtyFrame a;
  a.type = wns::sctp::kTtyStdin;
  a.payload = {'a', 'b'};
  wns::TtyFrame b;
  b.type = wns::sctp::kTtyStdout;
  b.payload = {'c', 'd'};
  auto wa = wns::EncodeTtyFrame(a);
  auto wb = wns::EncodeTtyFrame(b);
  std::vector<uint8_t> joined = wa;
  joined.insert(joined.end(), wb.begin(), wb.end());

  wns::TtyDecoder d;
  auto g0 = d.Push(std::span<const uint8_t>(joined.data(), 3));
  auto g1 = d.Push(std::span<const uint8_t>(joined.data() + 3, 4));
  auto g2 = d.Push(std::span<const uint8_t>(joined.data() + 7, joined.size() - 7));
  EXPECT(g0.empty());
  EXPECT_EQ(g1.size(), 1u);
  EXPECT_EQ(g1[0].type, wns::sctp::kTtyStdin);
  EXPECT_EQ(g1[0].payload.size(), 2u);
  EXPECT_EQ(g1[0].payload[0], 'a');
  EXPECT_EQ(g2.size(), 1u);
  EXPECT_EQ(g2[0].type, wns::sctp::kTtyStdout);
  EXPECT_EQ(g2[0].payload[1], 'd');
}

TEST(TtyHelloResizeExit) {
  wns::TtyHello h;
  h.version = wns::sctp::kTtyVersion;
  h.role = wns::sctp::kTtyRoleListener;
  h.cols = 120;
  h.rows = 40;
  auto raw = wns::EncodeTtyHello(h);
  EXPECT_EQ(raw.size(), 10u);
  EXPECT_EQ(raw[0], 'W');
  EXPECT_EQ(raw[1], 'T');
  EXPECT_EQ(raw[2], 'T');
  EXPECT_EQ(raw[3], 'Y');
  wns::TtyHello back;
  EXPECT(wns::DecodeTtyHello(raw, &back));
  EXPECT_EQ(back.role, wns::sctp::kTtyRoleListener);
  EXPECT_EQ(back.cols, 120);
  EXPECT_EQ(back.rows, 40);
  raw[0] = 'X';
  EXPECT(!wns::DecodeTtyHello(raw, &back));

  uint16_t c = 0, r = 0;
  EXPECT(wns::DecodeTtyResize(wns::EncodeTtyResize(80, 24), &c, &r));
  EXPECT_EQ(c, 80);
  EXPECT_EQ(r, 24);
  int32_t code = 0;
  EXPECT(wns::DecodeTtyExit(wns::EncodeTtyExit(3), &code));
  EXPECT_EQ(code, 3);
  EXPECT(wns::DecodeTtyExit(wns::EncodeTtyExit(-1), &code));
  EXPECT_EQ(code, -1);
}

TEST(TtyStreamBlockIsDisjoint) {
  EXPECT(wns::sctp::IsTtyStream(wns::sctp::kStreamTtyMin));
  EXPECT(wns::sctp::IsTtyStream(wns::sctp::kStreamTtyMax));
  EXPECT(!wns::sctp::IsTtyStream(wns::sctp::kStreamGlsMax));
  EXPECT(!wns::sctp::IsGlsStream(wns::sctp::kStreamTtyMin));
  EXPECT(!wns::sctp::IsProxyStream(wns::sctp::kStreamTtyMin));
  EXPECT(!wns::sctp::IsRpcStream(wns::sctp::kStreamTtyMin));
  EXPECT(wns::sctp::kPpidTty == 130u);
}

TEST(TtySessionOnFabric) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  uint16_t sid = a.TtyOpen(120, 40);
  EXPECT_EQ(sid, wns::sctp::kStreamTtyMin);
  auto* bs = b.tty().Find(sid);
  EXPECT(bs != nullptr);
  EXPECT(bs->have_hello);
  EXPECT_EQ(bs->cols, 120);
  EXPECT_EQ(bs->rows, 40);
  EXPECT_EQ(bs->role, wns::sctp::kTtyRoleListener);

  auto* as = a.tty().Find(sid);
  EXPECT(as != nullptr);
  EXPECT(as->have_hello);
  EXPECT_EQ(as->role, wns::sctp::kTtyRoleReceiver);

  uint8_t line[] = {'e', 'c', 'h', 'o', ' ', 'h', 'i', '\r'};
  EXPECT(a.TtyStdin(sid, line));
  EXPECT_EQ(bs->stdin_bytes.size(), sizeof(line));
  EXPECT_EQ(bs->stdin_bytes[0], 'e');

  uint8_t out[] = {'h', 'i', '\n'};
  EXPECT(b.TtyStdout(sid, out));
  EXPECT_EQ(as->stdout_bytes.size(), 3u);
  EXPECT_EQ(as->stdout_bytes[0], 'h');

  EXPECT(a.tty().Ping(sid));
  EXPECT_EQ(bs->pings, 1u);
  EXPECT_EQ(as->pongs, 1u);

  EXPECT(b.TtyResize(sid, 80, 24));
  EXPECT_EQ(as->cols, 80);
  EXPECT_EQ(as->rows, 24);
}
