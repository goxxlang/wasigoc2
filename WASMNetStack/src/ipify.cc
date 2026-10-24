#include "wns/ipify.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static constexpr socket_t kInvalid = INVALID_SOCKET;
#define IPIFY_CLOSE closesocket
#elif !defined(__wasi__)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t kInvalid = -1;
#define IPIFY_CLOSE close
#endif

namespace wns {
namespace {

constexpr std::uintptr_t kClosed = static_cast<std::uintptr_t>(-1);

bool ExtractIpField(std::string_view json, std::string* out) {
  auto pos = json.find("\"ip\"");
  if (pos == std::string_view::npos) return false;
  pos = json.find(':', pos + 4);
  if (pos == std::string_view::npos) return false;
  pos = json.find('"', pos + 1);
  if (pos == std::string_view::npos) return false;
  ++pos;
  std::string value;
  for (; pos < json.size(); ++pos) {
    char c = json[pos];
    if (c == '\\' && pos + 1 < json.size()) {
      value.push_back(json[++pos]);
      continue;
    }
    if (c == '"') break;
    value.push_back(c);
  }
  *out = std::move(value);
  return !out->empty();
}

std::string Http(int status, const char* reason, const char* type, std::string_view body) {
  std::string o;
  char line[64];
  std::snprintf(line, sizeof(line), "HTTP/1.1 %d %s\r\n", status, reason);
  o += line;
  o += "Content-Type: ";
  o += type;
  o += "\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n";
  o += "Access-Control-Allow-Methods: GET, OPTIONS\r\nConnection: close\r\n";
  std::snprintf(line, sizeof(line), "Content-Length: %zu\r\n\r\n", body.size());
  o += line;
  o.append(body.data(), body.size());
  return o;
}

#if !defined(__wasi__)
bool EnsureNet() {
#ifdef _WIN32
  static bool once = false;
  static bool ok = false;
  if (!once) {
    WSADATA w;
    ok = WSAStartup(MAKEWORD(2, 2), &w) == 0;
    once = true;
  }
  return ok;
#else
  return true;
#endif
}

socket_t AsSock(std::uintptr_t h) { return static_cast<socket_t>(h); }

bool V4Mapped(const uint8_t* b) {
  for (int i = 0; i < 10; ++i) {
    if (b[i] != 0) return false;
  }
  return b[10] == 0xff && b[11] == 0xff;
}

bool PeerIp(socket_t s, IpifyReply* out) {
  sockaddr_storage ss{};
  socklen_t n = sizeof(ss);
  if (getpeername(s, reinterpret_cast<sockaddr*>(&ss), &n) != 0) return false;
  if (ss.ss_family == AF_INET) {
    auto* a = reinterpret_cast<sockaddr_in*>(&ss);
    out->v4 = Ipv4::FromU32(ntohl(a->sin_addr.s_addr));
    out->v6 = false;
    out->ip = FormatIpv4(out->v4);
    return true;
  }
  if (ss.ss_family == AF_INET6) {
    auto* a = reinterpret_cast<sockaddr_in6*>(&ss);
    const uint8_t* b = a->sin6_addr.s6_addr;
    if (V4Mapped(b)) {
      out->v4.o[0] = b[12];
      out->v4.o[1] = b[13];
      out->v4.o[2] = b[14];
      out->v4.o[3] = b[15];
      out->v6 = false;
      out->ip = FormatIpv4(out->v4);
      return true;
    }
    std::memcpy(out->ipv6.o, b, 16);
    out->v6 = true;
    out->ip = FormatIpv6(out->ipv6);
    return true;
  }
  return false;
}

bool Handle(socket_t c) {
  char buf[4096];
  int n = recv(c, buf, sizeof(buf) - 1, 0);
  if (n <= 0) return false;
  buf[n] = 0;
  std::string_view req(buf, static_cast<size_t>(n));
  std::string resp;
  if (req.find("OPTIONS") == 0) {
    resp = Http(204, "No Content", "text/plain", "");
  } else if (req.find("ipify.js") != std::string_view::npos) {
    resp = IpifyHttpJs();
  } else {
    IpifyReply r;
    if (!PeerIp(c, &r)) return false;
    resp = IpifyHttpJson(r.ip);
  }
  send(c, resp.data(), static_cast<int>(resp.size()), 0);
  return true;
}
#endif

}  // namespace

std::string EncodeIpifyJson(std::string_view ip) {
  std::string o = "{\"ip\":\"";
  o.append(ip.data(), ip.size());
  o += "\"}";
  return o;
}

bool ParseVisibleIp(std::string_view ip, IpifyReply* out) {
  if (!out || ip.empty()) return false;
  IpifyReply r;
  r.ip.assign(ip.data(), ip.size());
  if (ParseIpv4(ip, &r.v4)) {
    r.v6 = false;
    *out = std::move(r);
    return true;
  }
  if (ParseIpv6(ip, &r.ipv6)) {
    r.v6 = true;
    *out = std::move(r);
    return true;
  }
  return false;
}

bool DecodeIpifyJson(std::string_view json, IpifyReply* out) {
  if (!out) return false;
  std::string ip;
  if (!ExtractIpField(json, &ip)) return false;
  return ParseVisibleIp(ip, out);
}

std::string IpifyHttpJson(std::string_view ip) {
  auto json = EncodeIpifyJson(ip);
  return Http(200, "OK", "application/json", json);
}

std::string IpifyHttpJs() { return Http(200, "OK", "text/javascript; charset=utf-8", kIpifyClientJs); }

IpifyServer::~IpifyServer() { Close(); }

#if defined(__wasi__)
void IpifyServer::Close() {
  listen_ = kClosed;
  open_ = false;
  port_ = 0;
}

bool IpifyServer::Listen(const char*, uint16_t) { return false; }
bool IpifyServer::ServeOne() { return false; }
void IpifyServer::Serve() {}
#else
void IpifyServer::Close() {
  if (open_ && listen_ != kClosed) {
    IPIFY_CLOSE(AsSock(listen_));
  }
  listen_ = kClosed;
  open_ = false;
  port_ = 0;
}

bool IpifyServer::Listen(const char* host, uint16_t port) {
  if (!EnsureNet()) return false;
  Close();
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
  addrinfo* res = nullptr;
  char pbuf[16];
  std::snprintf(pbuf, sizeof(pbuf), "%u", port);
  const char* node = (host && host[0]) ? host : nullptr;
  int gai = getaddrinfo(node, pbuf, &hints, &res);
  if (gai != 0) {
    hints.ai_flags = AI_PASSIVE;
    gai = getaddrinfo(node, pbuf, &hints, &res);
    if (gai != 0) return false;
  }
  socket_t s = kInvalid;
  for (addrinfo* a = res; a; a = a->ai_next) {
    s = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (s == kInvalid) continue;
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on), sizeof(on));
    if (a->ai_family == AF_INET6) {
      int off = 0;
      setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&off), sizeof(off));
    }
    if (bind(s, a->ai_addr, static_cast<int>(a->ai_addrlen)) == 0 && listen(s, 16) == 0) {
      sockaddr_storage ss{};
      socklen_t sl = sizeof(ss);
      if (getsockname(s, reinterpret_cast<sockaddr*>(&ss), &sl) == 0) {
        if (ss.ss_family == AF_INET) {
          port_ = ntohs(reinterpret_cast<sockaddr_in*>(&ss)->sin_port);
        } else {
          port_ = ntohs(reinterpret_cast<sockaddr_in6*>(&ss)->sin6_port);
        }
      } else {
        port_ = port;
      }
      listen_ = static_cast<std::uintptr_t>(s);
      open_ = true;
      freeaddrinfo(res);
      return true;
    }
    IPIFY_CLOSE(s);
    s = kInvalid;
  }
  freeaddrinfo(res);
  return false;
}

bool IpifyServer::ServeOne() {
  if (!open_ || listen_ == kClosed) return false;
  sockaddr_storage ss{};
  socklen_t n = sizeof(ss);
  socket_t c = accept(AsSock(listen_), reinterpret_cast<sockaddr*>(&ss), &n);
  if (c == kInvalid) return false;
  bool ok = Handle(c);
  IPIFY_CLOSE(c);
  return ok;
}

void IpifyServer::Serve() {
  while (open_) {
    ServeOne();
  }
}
#endif

}  // namespace wns
