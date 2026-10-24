#include "wns/sock.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using raw_socket = SOCKET;
static constexpr raw_socket kRawBad = INVALID_SOCKET;
#define WNS_CLOSE closesocket
#elif !defined(__wasi__)
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using raw_socket = int;
static constexpr raw_socket kRawBad = -1;
#define WNS_CLOSE close
#endif

namespace wns {
namespace {

std::string PortText(uint16_t port) {
  char b[8];
  std::snprintf(b, sizeof(b), "%u", unsigned(port));
  return b;
}

#if !defined(__wasi__)

raw_socket Raw(SockHandle h) { return static_cast<raw_socket>(h); }
SockHandle Wrap(raw_socket s) { return static_cast<SockHandle>(s); }

bool WouldBlock() {
#ifdef _WIN32
  int e = WSAGetLastError();
  return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

bool V4Mapped(const uint8_t* b) {
  for (int i = 0; i < 10; ++i) {
    if (b[i] != 0) return false;
  }
  return b[10] == 0xff && b[11] == 0xff;
}

// Endpoint -> sockaddr. Returns the used length, 0 on failure.
socklen_t ToSockaddr(const Endpoint& e, sockaddr_storage* ss) {
  std::memset(ss, 0, sizeof(*ss));
  if (e.v6) {
    auto* a = reinterpret_cast<sockaddr_in6*>(ss);
    a->sin6_family = AF_INET6;
    a->sin6_port = htons(e.port);
    std::memcpy(a->sin6_addr.s6_addr, e.ipv6.o, 16);
    return sizeof(sockaddr_in6);
  }
  auto* a = reinterpret_cast<sockaddr_in*>(ss);
  a->sin_family = AF_INET;
  a->sin_port = htons(e.port);
  a->sin_addr.s_addr = htonl(e.v4.ToU32());
  return sizeof(sockaddr_in);
}

bool FromSockaddr(const sockaddr_storage& ss, Endpoint* out) {
  if (ss.ss_family == AF_INET) {
    const auto* a = reinterpret_cast<const sockaddr_in*>(&ss);
    out->v6 = false;
    out->v4 = Ipv4::FromU32(ntohl(a->sin_addr.s_addr));
    out->port = ntohs(a->sin_port);
    return true;
  }
  if (ss.ss_family == AF_INET6) {
    const auto* a = reinterpret_cast<const sockaddr_in6*>(&ss);
    const uint8_t* b = a->sin6_addr.s6_addr;
    out->port = ntohs(a->sin6_port);
    if (V4Mapped(b)) {
      out->v6 = false;
      out->v4.o[0] = b[12];
      out->v4.o[1] = b[13];
      out->v4.o[2] = b[14];
      out->v4.o[3] = b[15];
      return true;
    }
    out->v6 = true;
    std::memcpy(out->ipv6.o, b, 16);
    return true;
  }
  return false;
}

bool LocalOf(raw_socket s, Endpoint* out) {
  sockaddr_storage ss{};
  socklen_t n = sizeof(ss);
  if (getsockname(s, reinterpret_cast<sockaddr*>(&ss), &n) != 0) return false;
  return FromSockaddr(ss, out);
}

raw_socket MakeSocket(bool v6, bool udp) {
  raw_socket s = ::socket(v6 ? AF_INET6 : AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM,
                          udp ? IPPROTO_UDP : IPPROTO_TCP);
  if (s == kRawBad) return kRawBad;
  if (v6) {
    int off = 0;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&off), sizeof(off));
  }
  return s;
}

#endif  // !__wasi__

}  // namespace

Endpoint Endpoint::V4(Ipv4 a, uint16_t port) {
  Endpoint e;
  e.v6 = false;
  e.v4 = a;
  e.port = port;
  return e;
}

Endpoint Endpoint::V6(Ipv6 a, uint16_t port) {
  Endpoint e;
  e.v6 = true;
  e.ipv6 = a;
  e.port = port;
  return e;
}

bool Endpoint::operator==(const Endpoint& x) const {
  if (v6 != x.v6 || port != x.port) return false;
  return v6 ? ipv6 == x.ipv6 : v4 == x.v4;
}

std::string Endpoint::Text() const {
  if (v6) return "[" + FormatIpv6(ipv6) + "]:" + PortText(port);
  return FormatIpv4(v4) + ":" + PortText(port);
}

ProxyTarget TargetOf(const Endpoint& e) {
  return e.v6 ? ProxyTarget::V6(e.ipv6, e.port) : ProxyTarget::V4(e.v4, e.port);
}

bool ParseEndpoint(std::string_view spec, Endpoint* out) {
  ProxyTarget t;
  if (!ParseProxyTarget(spec, &t)) return false;
  if (t.kind == sctp::kProxyAddr4) {
    *out = Endpoint::V4(t.v4, t.port);
    return true;
  }
  if (t.kind == sctp::kProxyAddr6) {
    *out = Endpoint::V6(t.ipv6, t.port);
    return true;
  }
  return false;
}

#if defined(__wasi__)

bool NetInit() { return false; }
bool ResolveTarget(const ProxyTarget&, bool, Endpoint*) { return false; }
void SockClose(SockHandle) {}
bool SockSetNonBlocking(SockHandle, bool) { return false; }
bool SockSetBuffers(SockHandle, int, int) { return false; }

TcpListener::~TcpListener() {}
bool TcpListener::Listen(const Endpoint&, int) { return false; }
SockHandle TcpListener::Accept(Endpoint*) { return kBadSock; }
void TcpListener::Close() {}

TcpConn::~TcpConn() {}
bool TcpConn::Connect(const Endpoint&) { return false; }
void TcpConn::Adopt(SockHandle, const Endpoint&) {}
IoStatus TcpConn::Recv(uint8_t*, size_t, size_t*) { return IoStatus::kError; }
void TcpConn::Queue(std::span<const uint8_t>) {}
IoStatus TcpConn::Flush() { return IoStatus::kError; }
void TcpConn::ShutdownWrite() {}
void TcpConn::Close() {}

UdpSocket::~UdpSocket() {}
bool UdpSocket::Bind(const Endpoint&) { return false; }
bool UdpSocket::Open(bool) { return false; }
IoStatus UdpSocket::SendTo(std::span<const uint8_t>, const Endpoint&) {
  return IoStatus::kError;
}
IoStatus UdpSocket::RecvFrom(uint8_t*, size_t, size_t*, Endpoint*) {
  return IoStatus::kError;
}
void UdpSocket::Close() {}

void Poller::Reset() { slots_.clear(); }
void Poller::Watch(SockHandle, bool, bool) {}
int Poller::Wait(int) { return -1; }
bool Poller::Readable(SockHandle) const { return false; }
bool Poller::Writable(SockHandle) const { return false; }
Poller::Slot* Poller::Get(SockHandle) { return nullptr; }
const Poller::Slot* Poller::Get(SockHandle) const { return nullptr; }

#else

bool NetInit() {
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

bool ResolveTarget(const ProxyTarget& t, bool want_udp, Endpoint* out) {
  if (!out) return false;
  if (t.kind == sctp::kProxyAddr4) {
    *out = Endpoint::V4(t.v4, t.port);
    return true;
  }
  if (t.kind == sctp::kProxyAddr6) {
    *out = Endpoint::V6(t.ipv6, t.port);
    return true;
  }
  if (!NetInit()) return false;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = want_udp ? SOCK_DGRAM : SOCK_STREAM;
  hints.ai_protocol = want_udp ? IPPROTO_UDP : IPPROTO_TCP;
  addrinfo* res = nullptr;
  if (getaddrinfo(t.name.c_str(), PortText(t.port).c_str(), &hints, &res) != 0) return false;
  bool ok = false;
  for (addrinfo* a = res; a && !ok; a = a->ai_next) {
    sockaddr_storage ss{};
    if (a->ai_addrlen > sizeof(ss)) continue;
    std::memcpy(&ss, a->ai_addr, a->ai_addrlen);
    ok = FromSockaddr(ss, out);
  }
  freeaddrinfo(res);
  if (ok) out->port = t.port;
  return ok;
}

void SockClose(SockHandle h) {
  if (h != kBadSock) WNS_CLOSE(Raw(h));
}

bool SockSetNonBlocking(SockHandle h, bool on) {
  if (h == kBadSock) return false;
#ifdef _WIN32
  u_long v = on ? 1 : 0;
  return ioctlsocket(Raw(h), FIONBIO, &v) == 0;
#else
  int f = fcntl(Raw(h), F_GETFL, 0);
  if (f < 0) return false;
  f = on ? (f | O_NONBLOCK) : (f & ~O_NONBLOCK);
  return fcntl(Raw(h), F_SETFL, f) == 0;
#endif
}

bool SockSetBuffers(SockHandle h, int send_bytes, int recv_bytes) {
  if (h == kBadSock) return false;
  bool ok = true;
  if (send_bytes > 0) {
    ok = setsockopt(Raw(h), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&send_bytes),
                    sizeof(send_bytes)) == 0 &&
         ok;
  }
  if (recv_bytes > 0) {
    ok = setsockopt(Raw(h), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&recv_bytes),
                    sizeof(recv_bytes)) == 0 &&
         ok;
  }
  return ok;
}

TcpListener::~TcpListener() { Close(); }

void TcpListener::Close() {
  SockClose(h_);
  h_ = kBadSock;
}

bool TcpListener::Listen(const Endpoint& at, int backlog) {
  if (!NetInit()) return false;
  Close();
  raw_socket s = MakeSocket(at.v6, false);
  if (s == kRawBad) return false;
  int on = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on), sizeof(on));
  sockaddr_storage ss{};
  socklen_t n = ToSockaddr(at, &ss);
  if (bind(s, reinterpret_cast<sockaddr*>(&ss), n) != 0 || listen(s, backlog) != 0) {
    WNS_CLOSE(s);
    return false;
  }
  local_ = at;
  LocalOf(s, &local_);
  h_ = Wrap(s);
  SockSetNonBlocking(h_, true);
  return true;
}

SockHandle TcpListener::Accept(Endpoint* peer) {
  if (h_ == kBadSock) return kBadSock;
  sockaddr_storage ss{};
  socklen_t n = sizeof(ss);
  raw_socket c = accept(Raw(h_), reinterpret_cast<sockaddr*>(&ss), &n);
  if (c == kRawBad) return kBadSock;
  if (peer) FromSockaddr(ss, peer);
  SockHandle h = Wrap(c);
  SockSetNonBlocking(h, true);
  int on = 1;
  setsockopt(c, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&on), sizeof(on));
  return h;
}

TcpConn::~TcpConn() { Close(); }

void TcpConn::Close() {
  SockClose(h_);
  h_ = kBadSock;
  out_.clear();
  sent_ = 0;
  wrote_eof_ = false;
}

bool TcpConn::Connect(const Endpoint& to) {
  if (!NetInit()) return false;
  Close();
  raw_socket s = MakeSocket(to.v6, false);
  if (s == kRawBad) return false;
  sockaddr_storage ss{};
  socklen_t n = ToSockaddr(to, &ss);
  // Blocking connect keeps the egress path simple; the socket goes
  // non-blocking for the pumping that follows.
  if (connect(s, reinterpret_cast<sockaddr*>(&ss), n) != 0) {
    WNS_CLOSE(s);
    return false;
  }
  int on = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&on), sizeof(on));
  h_ = Wrap(s);
  peer_ = to;
  LocalOf(s, &local_);
  SockSetNonBlocking(h_, true);
  return true;
}

void TcpConn::Adopt(SockHandle h, const Endpoint& peer) {
  Close();
  h_ = h;
  peer_ = peer;
  if (h_ != kBadSock) {
    LocalOf(Raw(h_), &local_);
    SockSetNonBlocking(h_, true);
  }
}

IoStatus TcpConn::Recv(uint8_t* buf, size_t n, size_t* got) {
  if (got) *got = 0;
  if (h_ == kBadSock || n == 0) return IoStatus::kError;
  int r = recv(Raw(h_), reinterpret_cast<char*>(buf), int(n), 0);
  if (r > 0) {
    if (got) *got = size_t(r);
    return IoStatus::kOk;
  }
  if (r == 0) return IoStatus::kEof;
  return WouldBlock() ? IoStatus::kAgain : IoStatus::kError;
}

void TcpConn::Queue(std::span<const uint8_t> data) {
  if (data.empty()) return;
  // Reclaim the drained prefix before growing.
  if (sent_ > 0 && sent_ == out_.size()) {
    out_.clear();
    sent_ = 0;
  } else if (sent_ > 4096) {
    out_.erase(out_.begin(), out_.begin() + std::ptrdiff_t(sent_));
    sent_ = 0;
  }
  out_.insert(out_.end(), data.begin(), data.end());
}

IoStatus TcpConn::Flush() {
  if (h_ == kBadSock) return IoStatus::kError;
  while (sent_ < out_.size()) {
    int r = send(Raw(h_), reinterpret_cast<const char*>(out_.data() + sent_),
                 int(out_.size() - sent_), 0);
    if (r > 0) {
      sent_ += size_t(r);
      continue;
    }
    return WouldBlock() ? IoStatus::kAgain : IoStatus::kError;
  }
  out_.clear();
  sent_ = 0;
  return IoStatus::kOk;
}

void TcpConn::ShutdownWrite() {
  if (h_ == kBadSock || wrote_eof_) return;
  wrote_eof_ = true;
#ifdef _WIN32
  shutdown(Raw(h_), SD_SEND);
#else
  shutdown(Raw(h_), SHUT_WR);
#endif
}

UdpSocket::~UdpSocket() { Close(); }

void UdpSocket::Close() {
  SockClose(h_);
  h_ = kBadSock;
}

bool UdpSocket::Bind(const Endpoint& at) {
  if (!NetInit()) return false;
  Close();
  raw_socket s = MakeSocket(at.v6, true);
  if (s == kRawBad) return false;
  int on = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on), sizeof(on));
  sockaddr_storage ss{};
  socklen_t n = ToSockaddr(at, &ss);
  if (bind(s, reinterpret_cast<sockaddr*>(&ss), n) != 0) {
    WNS_CLOSE(s);
    return false;
  }
  local_ = at;
  LocalOf(s, &local_);
  h_ = Wrap(s);
  SockSetNonBlocking(h_, true);
  return true;
}

bool UdpSocket::Open(bool v6) {
  Endpoint any;
  any.v6 = v6;
  any.port = 0;
  return Bind(any);
}

IoStatus UdpSocket::SendTo(std::span<const uint8_t> data, const Endpoint& to) {
  if (h_ == kBadSock) return IoStatus::kError;
  sockaddr_storage ss{};
  socklen_t n = ToSockaddr(to, &ss);
  int r = sendto(Raw(h_), reinterpret_cast<const char*>(data.data()), int(data.size()), 0,
                 reinterpret_cast<sockaddr*>(&ss), n);
  if (r >= 0) return IoStatus::kOk;
  return WouldBlock() ? IoStatus::kAgain : IoStatus::kError;
}

IoStatus UdpSocket::RecvFrom(uint8_t* buf, size_t n, size_t* got, Endpoint* from) {
  if (got) *got = 0;
  if (h_ == kBadSock || n == 0) return IoStatus::kError;
  sockaddr_storage ss{};
  socklen_t sl = sizeof(ss);
  int r = recvfrom(Raw(h_), reinterpret_cast<char*>(buf), int(n), 0,
                   reinterpret_cast<sockaddr*>(&ss), &sl);
  if (r >= 0) {
    if (got) *got = size_t(r);
    if (from) FromSockaddr(ss, from);
    return IoStatus::kOk;
  }
  return WouldBlock() ? IoStatus::kAgain : IoStatus::kError;
}

void Poller::Reset() { slots_.clear(); }

Poller::Slot* Poller::Get(SockHandle h) {
  for (auto& s : slots_) {
    if (s.h == h) return &s;
  }
  return nullptr;
}

const Poller::Slot* Poller::Get(SockHandle h) const {
  for (const auto& s : slots_) {
    if (s.h == h) return &s;
  }
  return nullptr;
}

void Poller::Watch(SockHandle h, bool read, bool write) {
  if (h == kBadSock) return;
  if (auto* s = Get(h)) {
    s->read = s->read || read;
    s->write = s->write || write;
    return;
  }
  Slot s;
  s.h = h;
  s.read = read;
  s.write = write;
  slots_.push_back(s);
}

int Poller::Wait(int timeout_ms) {
  fd_set rd, wr;
  FD_ZERO(&rd);
  FD_ZERO(&wr);
  raw_socket max_fd = 0;
  int watched = 0;
  for (auto& s : slots_) {
    s.rd_ready = false;
    s.wr_ready = false;
    raw_socket fd = Raw(s.h);
    if (s.read) {
      FD_SET(fd, &rd);
      ++watched;
    }
    if (s.write) {
      FD_SET(fd, &wr);
      ++watched;
    }
    if (fd > max_fd) max_fd = fd;
  }
  if (watched == 0) return 0;
  timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  int n = select(int(max_fd) + 1, &rd, &wr, nullptr, timeout_ms < 0 ? nullptr : &tv);
  if (n <= 0) return n;
  for (auto& s : slots_) {
    raw_socket fd = Raw(s.h);
    s.rd_ready = s.read && FD_ISSET(fd, &rd);
    s.wr_ready = s.write && FD_ISSET(fd, &wr);
  }
  return n;
}

bool Poller::Readable(SockHandle h) const {
  const auto* s = Get(h);
  return s && s->rd_ready;
}

bool Poller::Writable(SockHandle h) const {
  const auto* s = Get(h);
  return s && s->wr_ready;
}

#endif  // __wasi__

}  // namespace wns
