#include "test.h"

#include "wns/sctp.h"
#include "wns/stack.h"

#include <string>

TEST(OccupancyRpcServiceConstants) {
  EXPECT_EQ(wns::sctp::kRpcOccupancy, 1u);
  EXPECT_EQ(wns::sctp::kRpcOccProxy, 0u);
  EXPECT_EQ(wns::sctp::kRpcOccHook, 1u);
  EXPECT_EQ(wns::sctp::kRpcOccReply, 2u);
  EXPECT_EQ(wns::sctp::kRpcOccCall, 3u);
}

TEST(OccupancyRpcProxyOverSctp) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  b.SetRpcHandler([](uint32_t svc, uint32_t method, std::span<const uint8_t> p) {
    EXPECT_EQ(svc, wns::sctp::kRpcOccupancy);
    EXPECT_EQ(method, wns::sctp::kRpcOccProxy);
    std::string body(p.begin(), p.end());
    EXPECT(body.find("/api/rbi/shape") != std::string::npos);
    std::string out = R"({"s":200,"b":"{\"ok\":true,\"via\":\"sctp-rpc\"}"})";
    return std::vector<uint8_t>(out.begin(), out.end());
  });
  std::string req = R"({"m":"POST","p":"/api/rbi/shape","b":"{\"dest\":\"https://gemini.google.com/app\"}"})";
  uint64_t xid =
      a.RpcCall(wns::sctp::kRpcOccupancy, wns::sctp::kRpcOccProxy,
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(req.data()),
                                         req.size()),
                false);
  auto* ep = a.FindRpc(2);
  EXPECT(ep != nullptr);
  EXPECT(!ep->Pending(xid));
  auto* rx = ep->last_rx();
  EXPECT(rx != nullptr);
  EXPECT_EQ(rx->xid, xid);
  EXPECT_EQ(rx->service, wns::sctp::kRpcOccupancy);
  EXPECT_EQ(rx->method, wns::sctp::kRpcOccProxy);
  std::string got(rx->payload.begin(), rx->payload.end());
  EXPECT(got.find("sctp-rpc") != std::string::npos);
}

TEST(OccupancyRpcCallAndReply) {
  wns::Stack a(1);
  wns::Stack b(2);
  a.Connect(b);
  b.SetRpcHandler([](uint32_t svc, uint32_t method, std::span<const uint8_t> p) {
    EXPECT_EQ(svc, wns::sctp::kRpcOccupancy);
    std::string body(p.begin(), p.end());
    if (method == wns::sctp::kRpcOccCall) {
      EXPECT(body.find("decodeBitmap") != std::string::npos);
      std::string out =
          R"({"s":200,"b":"{\"ok\":true,\"payload\":\"occupy-pong\",\"is_response\":true,\"queued\":false}"})";
      return std::vector<uint8_t>(out.begin(), out.end());
    }
    if (method == wns::sctp::kRpcOccReply) {
      EXPECT(body.find("occupy-pong") != std::string::npos);
      std::string out = R"({"s":200,"b":"{\"ok\":true,\"matched\":true}"})";
      return std::vector<uint8_t>(out.begin(), out.end());
    }
    if (method == wns::sctp::kRpcOccHook) {
      std::string out =
          R"({"s":200,"b":"{\"ok\":true,\"commands\":[{\"ordinal\":1691693543,\"name\":\"decodeBitmap\"}]}"})";
      return std::vector<uint8_t>(out.begin(), out.end());
    }
    return std::vector<uint8_t>{};
  });
  std::string call = R"({"ordinal":1691693543,"name":"decodeBitmap"})";
  uint64_t xid = a.RpcCall(
      wns::sctp::kRpcOccupancy, wns::sctp::kRpcOccCall,
      std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(call.data()), call.size()),
      false);
  auto* ep = a.FindRpc(2);
  EXPECT(ep != nullptr);
  EXPECT(!ep->Pending(xid));
  auto* rx = ep->last_rx();
  EXPECT(rx != nullptr);
  EXPECT_EQ(rx->method, wns::sctp::kRpcOccCall);
  std::string got(rx->payload.begin(), rx->payload.end());
  EXPECT(got.find("occupy-pong") != std::string::npos);

  std::string reply = R"({"ordinal":1691693543,"payload":"occupy-pong"})";
  xid = a.RpcCall(
      wns::sctp::kRpcOccupancy, wns::sctp::kRpcOccReply,
      std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(reply.data()), reply.size()),
      false);
  EXPECT(!ep->Pending(xid));
  rx = ep->last_rx();
  EXPECT(rx != nullptr);
  EXPECT_EQ(rx->method, wns::sctp::kRpcOccReply);
  got.assign(rx->payload.begin(), rx->payload.end());
  EXPECT(got.find("matched") != std::string::npos);

  std::string hook = R"({"sid":"dev-1"})";
  xid = a.RpcCall(
      wns::sctp::kRpcOccupancy, wns::sctp::kRpcOccHook,
      std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(hook.data()), hook.size()),
      false);
  EXPECT(!ep->Pending(xid));
  rx = ep->last_rx();
  EXPECT(rx != nullptr);
  EXPECT_EQ(rx->method, wns::sctp::kRpcOccHook);
  got.assign(rx->payload.begin(), rx->payload.end());
  EXPECT(got.find("decodeBitmap") != std::string::npos);
}
