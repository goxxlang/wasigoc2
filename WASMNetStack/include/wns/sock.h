#ifndef WNS_SOCK_H_
#define WNS_SOCK_H_

#include "wns/addr.h"
#include "wns/proxy.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wns {

using SockHandle = std::uintptr_t;
inline constexpr SockHandle kBadSock = static_cast<SockHandle>(-1);

enum class IoStatus : uint8_t {
  kOk = 0,     // progress was made
  kAgain = 1,  // would block, try later
  kEof = 2,    // orderly shutdown by the peer
  kError = 3,  // fatal for this socket
};

// A numeric transport address. ProxyTarget names are resolved into one of
// these by the egress node.
struct Endpoint {
  bool v6 = false;
  Ipv4 v4{};
  Ipv6 ipv6{};
  uint16_t port = 0;

  static Endpoint V4(Ipv4 a, uint16_t port);
  static Endpoint V6(Ipv6 a, uint16_t port);
  bool operator==(const Endpoint& x) const;
  bool operator!=(const Endpoint& x) const { return !(*this == x); }
  std::string Text() const;
};

// Idempotent; on Windows this is WSAStartup.
bool NetInit();

// Numeric "host:port" or "[v6]:port". No DNS.
bool ParseEndpoint(std::string_view spec, Endpoint* out);
// Numeric or DNS, via getaddrinfo.
bool ResolveTarget(const ProxyTarget& t, bool want_udp, Endpoint* out);
ProxyTarget TargetOf(const Endpoint& e);

void SockClose(SockHandle h);
bool SockSetNonBlocking(SockHandle h, bool on);
// Best effort; a kernel may grant less than asked. Pass 0 to leave one alone.
bool SockSetBuffers(SockHandle h, int send_bytes, int recv_bytes);

class TcpListener {
 public:
  TcpListener() = default;
  ~TcpListener();
  TcpListener(const TcpListener&) = delete;
  TcpListener& operator=(const TcpListener&) = delete;

  bool Listen(const Endpoint& at, int backlog = 64);
  // kBadSock when nothing is pending; the socket is already non-blocking.
  SockHandle Accept(Endpoint* peer);
  SockHandle handle() const { return h_; }
  const Endpoint& local() const { return local_; }
  bool open() const { return h_ != kBadSock; }
  void Close();

 private:
  SockHandle h_ = kBadSock;
  Endpoint local_{};
};

// Non-blocking TCP with an owned transmit queue, so a caller can hand over
// bytes without minding partial writes.
class TcpConn {
 public:
  TcpConn() = default;
  ~TcpConn();
  TcpConn(const TcpConn&) = delete;
  TcpConn& operator=(const TcpConn&) = delete;

  bool Connect(const Endpoint& to);
  void Adopt(SockHandle h, const Endpoint& peer);

  IoStatus Recv(uint8_t* buf, size_t n, size_t* got);
  void Queue(std::span<const uint8_t> data);
  IoStatus Flush();
  size_t pending() const { return out_.size() - sent_; }

  void ShutdownWrite();
  void Close();
  SockHandle handle() const { return h_; }
  bool open() const { return h_ != kBadSock; }
  const Endpoint& peer() const { return peer_; }
  const Endpoint& local() const { return local_; }

 private:
  SockHandle h_ = kBadSock;
  Endpoint peer_{};
  Endpoint local_{};
  std::vector<uint8_t> out_;
  size_t sent_ = 0;
  bool wrote_eof_ = false;
};

class UdpSocket {
 public:
  UdpSocket() = default;
  ~UdpSocket();
  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;

  bool Bind(const Endpoint& at);
  // Ephemeral port in the given family.
  bool Open(bool v6);

  IoStatus SendTo(std::span<const uint8_t> data, const Endpoint& to);
  IoStatus RecvFrom(uint8_t* buf, size_t n, size_t* got, Endpoint* from);

  void Close();
  SockHandle handle() const { return h_; }
  bool open() const { return h_ != kBadSock; }
  const Endpoint& local() const { return local_; }

 private:
  SockHandle h_ = kBadSock;
  Endpoint local_{};
};

// select() front end. Rebuilt each pass, so a caller may add and drop sockets
// freely between waits.
class Poller {
 public:
  void Reset();
  void Watch(SockHandle h, bool read, bool write);
  int Wait(int timeout_ms);
  bool Readable(SockHandle h) const;
  bool Writable(SockHandle h) const;

 private:
  struct Slot {
    SockHandle h = kBadSock;
    bool read = false;
    bool write = false;
    bool rd_ready = false;
    bool wr_ready = false;
  };
  Slot* Get(SockHandle h);
  const Slot* Get(SockHandle h) const;
  std::vector<Slot> slots_;
};

}  // namespace wns

#endif  // WNS_SOCK_H_
