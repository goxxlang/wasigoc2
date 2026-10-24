#include "test.h"

#include "wns/ipify.h"
#include "wns/stack.h"

#include <cstring>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static constexpr socket_t kInvalid = INVALID_SOCKET;
#define IPIFY_CLOSE closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t kInvalid = -1;
#define IPIFY_CLOSE close
#endif

namespace {

bool GetJson(uint16_t port, std::string* body, std::string* raw) {
#ifdef _WIN32
  WSADATA w;
  WSAStartup(MAKEWORD(2, 2), &w);
#endif
  socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == kInvalid) return false;
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
    IPIFY_CLOSE(s);
    return false;
  }
  const char* req = "GET /?format=json HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
  send(s, req, static_cast<int>(std::strlen(req)), 0);
  std::string all;
  char buf[1024];
  for (;;) {
    int n = recv(s, buf, sizeof(buf), 0);
    if (n <= 0) break;
    all.append(buf, static_cast<size_t>(n));
  }
  IPIFY_CLOSE(s);
  if (raw) *raw = all;
  auto sep = all.find("\r\n\r\n");
  if (sep == std::string::npos) return false;
  if (body) *body = all.substr(sep + 4);
  return true;
}

}  // namespace

TEST(IpifyEchoesPeerJson) {
  wns::IpifyServer srv;
  EXPECT(srv.Listen("127.0.0.1", 0));
  EXPECT(srv.port() != 0);
  std::thread t([&] { EXPECT(srv.ServeOne()); });
  std::string body, raw;
  EXPECT(GetJson(srv.port(), &body, &raw));
  t.join();
  EXPECT(raw.find("Cache-Control: no-store") != std::string::npos);
  wns::IpifyReply r;
  EXPECT(wns::DecodeIpifyJson(body, &r));
  EXPECT(!r.v6);
  EXPECT(r.ip == "127.0.0.1");
  wns::Stack st(9);
  EXPECT(st.ApplyIpifyJson(body, false));
  EXPECT(st.VisibleJson() == "{\"ip\":\"127.0.0.1\"}");
}

TEST(IpifyServesOwnClientJs) {
  std::string js = wns::IpifyHttpJs();
  EXPECT(js.find("api.ipify.org") == std::string::npos);
  EXPECT(js.find("fetch(\"/?format=json\"") != std::string::npos);
}
