// wasigocvm guest net bridge: gocvm.Call + poll() workers.
//
// Product gate is WASIGO_GOCVM (set by compile.bat / wasigocvm.bat / goldens), not stock
// __wasip2__. Isolation is ~/WASMSafeSpace + ~/WASMv8bindings in this
// module (exec child is EPT/TPT/CHPT; work is WASMWin32 wasi_call),
// not a companion gocvm_host process. POSIX sockets come from the
// wasigocvm sysroot. TLS is OpenSSL in this module (WASMLime TlsTransport:
// memory BIOs + SSL_do_handshake), not Schannel and not a host hop.
//
// Blocking recv()/accept() would freeze every goroutine. Until threads exist
// in our sysroot, the worker pool is poll() on non-blocking fds, integrated
// with Scheduler::run() the same way Chan waiters already are.
//
// Included from runtime.hpp under WASIGO_GOCVM && WASIGO_NEED_CORO.
// Needs -I <repo>/src on the clang++ line.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "wasigocvm_libc.hpp"
#include "wasigocvm_tls.hpp"

namespace gocvm {

using ::gocvm::exec_lookpath;
using ::gocvm::wasigocvm_try_libc;
#if defined(WASIGO_GOCVM_HAS_PTHREAD) && WASIGO_GOCVM_HAS_PTHREAD
using ::gocvm::ExecTable;
using ::gocvm::ExecProc;
#endif

class WasigocvmNetBridge : public AsyncHostBridge {
 public:
  uint64_t Submit(const std::string& topic, const std::string& payload) override {
    uint64_t id = ++next_id_;
    Completion c;
    c.id = id;
    bool parked = false;
    dispatch(id, topic, payload, &c, &parked);
    if (!parked) completions_.push_back(std::move(c));
    return id;
  }

  bool PollOne(Completion* out) override {
    if (completions_.empty()) pump(0);
    return pop(out);
  }

  void WaitOne(Completion* out) override {
    // Cap each park so a single orphan reader cannot freeze the
    // cooperative scheduler. After enough empty pumps, fail the oldest
    // pending op so goldens surface "timeout" instead of hanging ctest.
    int idle = 0;
    while (completions_.empty()) {
      if (pending_.empty()) {
        Completion c;
        c.ok = false;
        c.err = "wasigocvm: WaitOne with no pending work";
        *out = std::move(c);
        return;
      }
      size_t before = pending_.size();
      pump(50);
      if (completions_.empty() && pending_.size() == before) {
        if (++idle > 100) {  // ~5s
          auto it = pending_.begin();
          Completion c;
          c.id = it->first;
          complete_err(&c, "error: i/o timeout");
          completions_.push_back(std::move(c));
          pending_.erase(it);
          idle = 0;
        }
      } else {
        idle = 0;
      }
    }
    pop(out);
  }

 private:
  enum class Op { kConnect, kAccept, kRead, kWrite, kReadFrom, kWriteTo,
                  kTlsConnect, kTlsHandshake, kTlsRead, kTlsWrite,
                  kExecWait, kExecRead };

  struct Sock {
    int fd = -1;
    bool udp = false;
    bool listen = false;
  };

  struct Parked {
    uint64_t id = 0;
    Op op = Op::kRead;
    uint64_t handle = 0;
    int fd = -1;
    short events = 0;
    int maxlen = 0;
    std::string remaining;
    std::string to_addr;
  };

  uint64_t next_id_ = 0;
  uint64_t next_handle_ = 0;
  std::unordered_map<uint64_t, Sock> socks_;
  std::unordered_map<uint64_t, tls_stack::Sess> tls_;
  std::unordered_map<uint64_t, Parked> pending_;
  std::deque<Completion> completions_;
#if defined(WASIGO_GOCVM_HAS_PTHREAD) && WASIGO_GOCVM_HAS_PTHREAD
  ExecTable execs_;
#endif

  static bool would_block() {
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
  }

  static std::string syserr(const char* what) {
    return std::string("error: ") + what + ": errno " + std::to_string(errno);
  }

  static bool split_first(const std::string& s, char sep, std::string* a, std::string* b) {
    auto i = s.find(sep);
    if (i == std::string::npos) {
      *a = s;
      *b = "";
      return false;
    }
    *a = s.substr(0, i);
    *b = s.substr(i + 1);
    return true;
  }

  static bool split_hostport(const std::string& hostport, std::string* host, std::string* port) {
    if (hostport.empty()) return false;
    if (hostport[0] == '[') {
      auto end = hostport.find("]:");
      if (end == std::string::npos) return false;
      *host = hostport.substr(1, end - 1);
      *port = hostport.substr(end + 2);
      return !port->empty();
    }
    auto i = hostport.rfind(':');
    if (i == std::string::npos) return false;
    *host = hostport.substr(0, i);
    *port = hostport.substr(i + 1);
    return !port->empty();
  }

  static bool is_udp(const std::string& network) {
    return network == "udp" || network == "udp4" || network == "udp6";
  }

  static bool fill_v4(const std::string& host, const std::string& port, bool bind,
                      sockaddr_in* out) {
    std::memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    int p = std::atoi(port.c_str());
    if (p <= 0 || p > 65535) return false;
    out->sin_port = htons(static_cast<uint16_t>(p));
    if (host.empty() || host == "*" || host == "0.0.0.0") {
      (void)bind;
      out->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      return true;
    }
    if (host == "localhost") {
      out->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      return true;
    }
    // Hard fork: numeric / loopback only. DNS is W2G_ALLOW_DNS on the host
    // (wasmtime ip-name-lookup); we do not call getaddrinfo from the guest
    // by default — it is ambient authority preview 1 also refused, and it
    // can block the one cooperative thread.
    return inet_pton(AF_INET, host.c_str(), &out->sin_addr) == 1;
  }

  static std::string fmt_v4(const sockaddr_in& a) {
    char buf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &a.sin_addr, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(a.sin_port));
  }

  static int set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
  }

  static int socktype(const std::string& network) {
    return is_udp(network) ? SOCK_DGRAM : SOCK_STREAM;
  }

  bool pop(Completion* out) {
    if (completions_.empty()) return false;
    *out = std::move(completions_.front());
    completions_.pop_front();
    return true;
  }

  uint64_t alloc(int fd, bool udp, bool listen) {
    uint64_t h = ++next_handle_;
    socks_[h] = Sock{fd, udp, listen};
    return h;
  }

  Sock* lookup(uint64_t h) {
    auto it = socks_.find(h);
    return it == socks_.end() ? nullptr : &it->second;
  }

  bool parse_handle(const std::string& s, uint64_t* out) {
    if (s.empty()) return false;
    for (char c : s) {
      if (c < '0' || c > '9') return false;
    }
    *out = static_cast<uint64_t>(std::strtoull(s.c_str(), nullptr, 10));
    return true;
  }

  void complete_ok(Completion* c, std::string reply) {
    c->ok = true;
    c->reply = std::move(reply);
    c->err.clear();
  }

  void complete_err(Completion* c, std::string err) {
    c->ok = true;  // gocvm.Call's (string, error): real backend failures are reply
    c->reply = std::move(err);
    c->err.clear();
  }

  void park(uint64_t id, Op op, uint64_t handle, int fd, short events, int maxlen,
            std::string remaining, std::string to_addr) {
    Parked p;
    p.id = id;
    p.op = op;
    p.handle = handle;
    p.fd = fd;
    p.events = events;
    p.maxlen = maxlen;
    p.remaining = std::move(remaining);
    p.to_addr = std::move(to_addr);
    pending_[id] = std::move(p);
  }

  void cancel_fd(int fd, const char* why) {
    std::vector<uint64_t> ids;
    for (auto& kv : pending_) {
      if (kv.second.fd == fd) ids.push_back(kv.first);
    }
    for (uint64_t id : ids) {
      Completion c;
      c.id = id;
      complete_err(&c, std::string("error: ") + why);
      completions_.push_back(std::move(c));
      pending_.erase(id);
    }
  }

  void dispatch(uint64_t id, const std::string& topic, const std::string& payload,
                Completion* c, bool* parked) {
    *parked = false;
    if (topic == "net.dial") {
      std::string network, address;
      split_first(payload, ' ', &network, &address);
      do_dial(id, network, address, c, parked);
      return;
    }
    if (topic == "net.listen") {
      std::string network, address;
      split_first(payload, ' ', &network, &address);
      do_listen(network, address, c);
      return;
    }
    if (topic == "net.accept") {
      do_accept(id, payload, c, parked);
      return;
    }
    if (topic == "net.io.read") {
      do_read(id, payload, c, parked);
      return;
    }
    if (topic == "net.io.write") {
      do_write(id, payload, c, parked);
      return;
    }
    if (topic == "net.io.readfrom") {
      do_readfrom(id, payload, c, parked);
      return;
    }
    if (topic == "net.io.writeto") {
      do_writeto(id, payload, c, parked);
      return;
    }
    if (topic == "net.io.close") {
      do_close(payload, c);
      return;
    }
    if (topic == "net.tcp.bind") {
      do_bind_probe("tcp", payload, c);
      return;
    }
    if (topic == "net.udp.bind") {
      do_bind_probe("udp", payload, c);
      return;
    }
    // libc ABI (getpid, getenv, uname, stat, clocks): stay in this wasm
    // module. Do not drop to a companion host.
    if (wasigocvm_try_libc(topic, payload, &c->reply)) {
      complete_ok(c, c->reply);
      return;
    }
    if (topic.rfind("os.exec", 0) == 0) {
      do_exec(id, topic, payload, c, parked);
      return;
    }
    if (topic == "tls.dial") {
      do_tls_dial(id, payload, c, parked);
      return;
    }
    if (topic == "tls.io.read") {
      do_tls_read(id, payload, c, parked);
      return;
    }
    if (topic == "tls.io.write") {
      do_tls_write(id, payload, c, parked);
      return;
    }
    if (topic == "tls.io.close") {
      do_tls_close(payload, c);
      return;
    }
    c->ok = false;
    c->err = "unsupported on wasigocvm";
  }

  void do_bind_probe(const std::string& network, const std::string& address, Completion* c) {
    int fd = -1;
    std::string err;
    if (!bind_fd(network, address, /*listen=*/false, &fd, &err)) {
      complete_err(c, err);
      return;
    }
    sockaddr_in bound{};
    socklen_t blen = sizeof(bound);
    getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &blen);
    close(fd);
    complete_ok(c, "ok bound=" + fmt_v4(bound));
  }

  bool bind_fd(const std::string& network, const std::string& address, bool do_listen, int* fd_out,
               std::string* err) {
    std::string host, port;
    if (!split_hostport(address, &host, &port)) {
      *err = "error: missing port in address " + address;
      return false;
    }
    sockaddr_in addr{};
    if (!fill_v4(host, port, /*bind=*/true, &addr)) {
      *err = "error: resolve " + address;
      return false;
    }
    int fd = socket(AF_INET, socktype(network), 0);
    if (fd < 0) {
      *err = syserr("socket");
      return false;
    }
    set_nonblock(fd);
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      *err = syserr("bind");
      close(fd);
      return false;
    }
    if (do_listen && !is_udp(network)) {
      if (listen(fd, SOMAXCONN) < 0) {
        *err = syserr("listen");
        close(fd);
        return false;
      }
    }
    *fd_out = fd;
    return true;
  }

  void do_listen(const std::string& network, const std::string& address, Completion* c) {
    int fd = -1;
    std::string err;
    if (!bind_fd(network, address, /*listen=*/true, &fd, &err)) {
      complete_err(c, err);
      return;
    }
    sockaddr_in bound{};
    socklen_t blen = sizeof(bound);
    getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &blen);
    uint64_t h = alloc(fd, is_udp(network), !is_udp(network));
    complete_ok(c, "ok handle=" + std::to_string(h) + " bound=" + fmt_v4(bound));
  }

  void do_dial(uint64_t id, const std::string& network, const std::string& address, Completion* c,
               bool* parked) {
    std::string host, port;
    if (!split_hostport(address, &host, &port)) {
      complete_err(c, "error: missing port in address " + address);
      return;
    }
    sockaddr_in addr{};
    if (!fill_v4(host, port, /*bind=*/false, &addr)) {
      complete_err(c, "error: resolve " + address);
      return;
    }
    int fd = socket(AF_INET, socktype(network), 0);
    if (fd < 0) {
      complete_err(c, syserr("socket"));
      return;
    }
    set_nonblock(fd);
    int rc = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc == 0 || (is_udp(network) && rc < 0 && would_block())) {
      // UDP connect is just an association; EINPROGRESS still means "set".
      finish_connect(c, fd, is_udp(network), addr);
      return;
    }
    if (rc < 0 && errno == EINPROGRESS) {
      uint64_t h = alloc(fd, is_udp(network), false);
      park(id, Op::kConnect, h, fd, POLLOUT, 0, "", fmt_v4(addr));
      *parked = true;
      return;
    }
    complete_err(c, syserr("connect"));
    close(fd);
  }

  void drop_fd(int fd) {
    for (auto it = socks_.begin(); it != socks_.end();) {
      if (it->second.fd == fd) it = socks_.erase(it);
      else ++it;
    }
  }

  void finish_connect(Completion* c, int fd, bool udp, const sockaddr_in& remote) {
    int soerr = 0;
    socklen_t elen = sizeof(soerr);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &elen);
    if (soerr != 0) {
      errno = soerr;
      complete_err(c, syserr("connect"));
      drop_fd(fd);
      close(fd);
      return;
    }
    sockaddr_in local{};
    socklen_t llen = sizeof(local);
    getsockname(fd, reinterpret_cast<sockaddr*>(&local), &llen);
    uint64_t h = 0;
    for (auto& kv : socks_) {
      if (kv.second.fd == fd) {
        h = kv.first;
        break;
      }
    }
    if (h == 0) h = alloc(fd, udp, false);
    complete_ok(c, "ok handle=" + std::to_string(h) + " local=" + fmt_v4(local) +
                       " remote=" + fmt_v4(remote));
  }

  void do_accept(uint64_t id, const std::string& handle, Completion* c, bool* parked) {
    uint64_t h = 0;
    if (!parse_handle(handle, &h)) {
      complete_err(c, "error: bad handle");
      return;
    }
    Sock* s = lookup(h);
    if (!s || s->fd < 0) {
      complete_err(c, "error: unknown handle " + handle);
      return;
    }
    sockaddr_in remote{};
    socklen_t rlen = sizeof(remote);
    int nfd = accept(s->fd, reinterpret_cast<sockaddr*>(&remote), &rlen);
    if (nfd >= 0) {
      set_nonblock(nfd);
      uint64_t nh = alloc(nfd, false, false);
      complete_ok(c, "ok handle=" + std::to_string(nh) + " remote=" + fmt_v4(remote));
      return;
    }
    if (would_block()) {
      park(id, Op::kAccept, h, s->fd, POLLIN, 0, "", "");
      *parked = true;
      return;
    }
    complete_err(c, syserr("accept"));
  }

  void do_read(uint64_t id, const std::string& payload, Completion* c, bool* parked) {
    std::string hs, maxs;
    split_first(payload, '\x1f', &hs, &maxs);
    uint64_t h = 0;
    if (!parse_handle(hs, &h)) {
      complete_err(c, "error: bad handle");
      return;
    }
    Sock* s = lookup(h);
    if (!s || s->fd < 0) {
      complete_err(c, "error: unknown handle " + hs);
      return;
    }
    int maxlen = std::atoi(maxs.c_str());
    if (maxlen < 0) maxlen = 0;
    if (maxlen > 65536) maxlen = 65536;
    std::string buf;
    buf.resize(static_cast<size_t>(maxlen));
    ssize_t n = recv(s->fd, buf.data(), buf.size(), 0);
    if (n >= 0) {
      buf.resize(static_cast<size_t>(n));
      complete_ok(c, std::move(buf));
      return;
    }
    if (would_block()) {
      park(id, Op::kRead, h, s->fd, POLLIN, maxlen, "", "");
      *parked = true;
      return;
    }
    complete_err(c, syserr("read"));
  }

  void do_write(uint64_t id, const std::string& payload, Completion* c, bool* parked) {
    std::string hs, data;
    split_first(payload, '\x1f', &hs, &data);
    uint64_t h = 0;
    if (!parse_handle(hs, &h)) {
      complete_err(c, "error: bad handle");
      return;
    }
    Sock* s = lookup(h);
    if (!s || s->fd < 0) {
      complete_err(c, "error: unknown handle " + hs);
      return;
    }
    if (!write_all(id, h, s->fd, data, c, parked)) return;
  }

  bool write_all(uint64_t id, uint64_t h, int fd, const std::string& data, Completion* c,
                 bool* parked) {
    size_t off = 0;
    while (off < data.size()) {
      ssize_t n = send(fd, data.data() + off, data.size() - off, 0);
      if (n > 0) {
        off += static_cast<size_t>(n);
        continue;
      }
      if (n < 0 && would_block()) {
        park(id, Op::kWrite, h, fd, POLLOUT, 0, data.substr(off), "");
        *parked = true;
        return false;
      }
      complete_err(c, syserr("write"));
      return false;
    }
    complete_ok(c, std::to_string(data.size()));
    return true;
  }

  void do_readfrom(uint64_t id, const std::string& payload, Completion* c, bool* parked) {
    std::string hs, maxs;
    split_first(payload, '\x1f', &hs, &maxs);
    uint64_t h = 0;
    if (!parse_handle(hs, &h)) {
      complete_err(c, "error: bad handle");
      return;
    }
    Sock* s = lookup(h);
    if (!s || s->fd < 0) {
      complete_err(c, "error: unknown handle " + hs);
      return;
    }
    int maxlen = std::atoi(maxs.c_str());
    if (maxlen < 0) maxlen = 0;
    if (maxlen > 65536) maxlen = 65536;
    std::string buf;
    buf.resize(static_cast<size_t>(maxlen));
    sockaddr_in from{};
    socklen_t flen = sizeof(from);
    ssize_t n = recvfrom(s->fd, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr*>(&from),
                           &flen);
    if (n >= 0) {
      buf.resize(static_cast<size_t>(n));
      complete_ok(c, fmt_v4(from) + "\x1f" + buf);
      return;
    }
    if (would_block()) {
      park(id, Op::kReadFrom, h, s->fd, POLLIN, maxlen, "", "");
      *parked = true;
      return;
    }
    complete_err(c, syserr("readfrom"));
  }

  void do_writeto(uint64_t id, const std::string& payload, Completion* c, bool* parked) {
    std::string hs, rest;
    split_first(payload, '\x1f', &hs, &rest);
    std::string addr, data;
    split_first(rest, '\x1f', &addr, &data);
    uint64_t h = 0;
    if (!parse_handle(hs, &h)) {
      complete_err(c, "error: bad handle");
      return;
    }
    Sock* s = lookup(h);
    if (!s || s->fd < 0) {
      complete_err(c, "error: unknown handle " + hs);
      return;
    }
    // Connected UDP (net.DialPacket): send() — sendto on a connected
    // wasi socket is flaky under some hosts.
    if (!s->listen && s->udp) {
      ssize_t n = send(s->fd, data.data(), data.size(), 0);
      if (n >= 0) {
        complete_ok(c, std::to_string(n));
        return;
      }
      if (would_block()) {
        park(id, Op::kWriteTo, h, s->fd, POLLOUT, 0, data, addr);
        *parked = true;
        return;
      }
      complete_err(c, syserr("writeto"));
      return;
    }
    std::string host, port;
    if (!split_hostport(addr, &host, &port)) {
      complete_err(c, "error: missing port in address " + addr);
      return;
    }
    sockaddr_in to{};
    if (!fill_v4(host, port, /*bind=*/false, &to)) {
      complete_err(c, "error: resolve " + addr);
      return;
    }
    ssize_t n = sendto(s->fd, data.data(), data.size(), 0, reinterpret_cast<sockaddr*>(&to),
                         sizeof(to));
    if (n >= 0) {
      complete_ok(c, std::to_string(n));
      return;
    }
    if (would_block()) {
      park(id, Op::kWriteTo, h, s->fd, POLLOUT, 0, data, addr);
      *parked = true;
      return;
    }
    complete_err(c, syserr("writeto"));
  }

  void do_close(const std::string& handle, Completion* c) {
    uint64_t h = 0;
    if (!parse_handle(handle, &h)) {
      complete_err(c, "error: bad handle");
      return;
    }
    Sock* s = lookup(h);
    if (!s) {
      complete_err(c, "error: unknown handle " + handle);
      return;
    }
    int fd = s->fd;
    cancel_fd(fd, "closed");
    auto tit = tls_.find(h);
    if (tit != tls_.end()) {
      tls_stack::close_sess(&tit->second);
      tls_.erase(tit);
    }
    if (fd >= 0) close(fd);
    socks_.erase(h);
    complete_ok(c, "ok");
  }

  void do_tls_dial(uint64_t id, const std::string& payload, Completion* c, bool* parked) {
    std::string address, sni;
    split_first(payload, '\x1f', &address, &sni);
    std::string host, port;
    if (!split_hostport(address, &host, &port)) {
      complete_err(c, "error: missing port in address " + address);
      return;
    }
    if (sni.empty()) sni = host;
    sockaddr_in addr{};
    if (!fill_v4(host, port, /*bind=*/false, &addr)) {
      complete_err(c, "error: resolve " + address);
      return;
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      complete_err(c, syserr("socket"));
      return;
    }
    set_nonblock(fd);
    int rc = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    uint64_t h = alloc(fd, false, false);
    if (rc == 0) {
      start_tls(id, h, sni, c, parked);
      return;
    }
    if (rc < 0 && errno == EINPROGRESS) {
      park(id, Op::kTlsConnect, h, fd, POLLOUT, 0, sni, fmt_v4(addr));
      *parked = true;
      return;
    }
    tls_.erase(h);
    socks_.erase(h);
    complete_err(c, syserr("connect"));
    close(fd);
  }

  void start_tls(uint64_t id, uint64_t h, const std::string& sni, Completion* c, bool* parked) {
    Sock* s = lookup(h);
    if (!s || s->fd < 0) {
      complete_err(c, "error: unknown handle");
      return;
    }
    tls_stack::Sess sess;
    std::string err;
    if (!tls_stack::open_client(&sess, sni, &err)) {
      complete_err(c, err);
      drop_fd(s->fd);
      close(s->fd);
      socks_.erase(h);
      return;
    }
    tls_[h] = std::move(sess);
    continue_tls_handshake(id, h, c, parked);
  }

  void continue_tls_handshake(uint64_t id, uint64_t h, Completion* c, bool* parked) {
    Sock* s = lookup(h);
    auto it = tls_.find(h);
    if (!s || it == tls_.end()) {
      complete_err(c, "error: tls: no session");
      return;
    }
    short want = 0;
    std::string err;
    int r = tls_stack::handshake(&it->second, s->fd, &want, &err);
    if (r == 1) {
      complete_ok(c, "ok handle=" + std::to_string(h));
      return;
    }
    if (r == 0) {
      park(id, Op::kTlsHandshake, h, s->fd, want ? want : POLLIN, 0, "", "");
      *parked = true;
      return;
    }
    tls_stack::close_sess(&it->second);
    tls_.erase(it);
    drop_fd(s->fd);
    close(s->fd);
    socks_.erase(h);
    complete_err(c, err);
  }

  void do_tls_read(uint64_t id, const std::string& payload, Completion* c, bool* parked) {
    std::string hs, maxs;
    split_first(payload, '\x1f', &hs, &maxs);
    uint64_t h = 0;
    if (!parse_handle(hs, &h)) {
      complete_err(c, "error: bad handle");
      return;
    }
    Sock* s = lookup(h);
    auto it = tls_.find(h);
    if (!s || it == tls_.end()) {
      complete_err(c, "error: unknown tls handle");
      return;
    }
    int maxlen = std::atoi(maxs.c_str());
    if (maxlen < 0) maxlen = 0;
    if (maxlen > 65536) maxlen = 65536;
    std::string buf;
    buf.resize(static_cast<size_t>(maxlen));
    short want = 0;
    std::string err;
    int n = tls_stack::read_app(&it->second, s->fd, buf.data(), maxlen, &want, &err);
    if (n > 0) {
      buf.resize(static_cast<size_t>(n));
      complete_ok(c, std::move(buf));
      return;
    }
    if (n == 0 && want != 0) {
      park(id, Op::kTlsRead, h, s->fd, want, maxlen, "", "");
      *parked = true;
      return;
    }
    if (n == 0) {
      complete_ok(c, "");
      return;
    }
    complete_err(c, err);
  }

  void do_tls_write(uint64_t id, const std::string& payload, Completion* c, bool* parked) {
    std::string hs, data;
    split_first(payload, '\x1f', &hs, &data);
    uint64_t h = 0;
    if (!parse_handle(hs, &h)) {
      complete_err(c, "error: bad handle");
      return;
    }
    Sock* s = lookup(h);
    auto it = tls_.find(h);
    if (!s || it == tls_.end()) {
      complete_err(c, "error: unknown tls handle");
      return;
    }
    short want = 0;
    std::string err;
    int n = tls_stack::write_app(&it->second, s->fd, data.data(), static_cast<int>(data.size()),
                                 &want, &err);
    if (n > 0) {
      complete_ok(c, std::to_string(n));
      return;
    }
    if (n == 0) {
      park(id, Op::kTlsWrite, h, s->fd, want ? want : POLLOUT, 0, data, "");
      *parked = true;
      return;
    }
    complete_err(c, err);
  }

  void do_tls_close(const std::string& handle, Completion* c) { do_close(handle, c); }

#if defined(WASIGO_GOCVM_HAS_PTHREAD) && WASIGO_GOCVM_HAS_PTHREAD
  void do_exec(uint64_t id, const std::string& topic, const std::string& payload,
               Completion* c, bool* parked) {
    if (topic == "os.exec.lookpath") {
      std::string reply;
      exec_lookpath(payload, &reply);
      if (reply.rfind("error:", 0) == 0) complete_err(c, reply);
      else complete_ok(c, reply);
      return;
    }
    if (topic == "os.exec") {
      uint64_t h = 0;
      std::string err;
      if (!execs_.spawn(payload, &h, &err)) {
        complete_err(c, err);
        return;
      }
      park(id, Op::kExecWait, h, -1, 0, 0, "combined", "");
      *parked = true;
      return;
    }
    if (topic == "os.exec.start") {
      uint64_t h = 0;
      std::string err;
      if (!execs_.spawn(payload, &h, &err)) {
        complete_err(c, err);
        return;
      }
      complete_ok(c, "ok handle=" + std::to_string(h));
      return;
    }
    if (topic == "os.exec.wait") {
      uint64_t h = 0;
      if (!parse_handle(payload, &h) || !execs_.get(h)) {
        complete_err(c, "error: unknown handle " + payload);
        return;
      }
      if (!execs_.done(h)) {
        park(id, Op::kExecWait, h, -1, 0, 0, "wait", "");
        *parked = true;
        return;
      }
      finish_exec_wait_only(h, c);
      return;
    }
    if (topic == "os.exec.stdout.read" || topic == "os.exec.stderr.read") {
      std::string hs, ms;
      split_first(payload, '\x1f', &hs, &ms);
      uint64_t h = 0;
      if (!parse_handle(hs, &h) || !execs_.get(h)) {
        complete_err(c, "error: unknown handle " + hs);
        return;
      }
      if (!execs_.readable(h)) {
        park(id, Op::kExecRead, h, -1, 0, std::atoi(ms.c_str()), "", "");
        *parked = true;
        return;
      }
      finish_exec_read(h, std::atoi(ms.c_str()), c);
      return;
    }
    if (topic == "os.exec.stdin.write") {
      complete_err(c, "error: stdin not piped");
      return;
    }
    if (topic == "os.exec.stdin.close") {
      complete_ok(c, "ok");
      return;
    }
    complete_err(c, "error: unknown os.exec topic " + topic);
  }

  void finish_exec_wait(uint64_t h, Completion* c) {
    ExecProc* p = execs_.get(h);
    if (!p) {
      complete_err(c, "error: unknown handle");
      return;
    }
    execs_.join_done(p);
    int code = 0;
    std::string out;
    {
      std::lock_guard<std::mutex> lk(p->mu);
      code = p->exit_code;
      out = execs_.take_out(p);
      c->worker_thread = p->tid;
    }
    complete_ok(c, "exit=" + std::to_string(code) + "\n" + out);
  }

  void finish_exec_wait_only(uint64_t h, Completion* c) {
    ExecProc* p = execs_.get(h);
    if (!p) {
      complete_err(c, "error: unknown handle");
      return;
    }
    execs_.join_done(p);
    int code = 0;
    {
      std::lock_guard<std::mutex> lk(p->mu);
      code = p->exit_code;
      c->worker_thread = p->tid;
    }
    complete_ok(c, "exit=" + std::to_string(code));
  }

  void finish_exec_read(uint64_t h, int maxlen, Completion* c) {
    ExecProc* p = execs_.get(h);
    if (!p) {
      complete_err(c, "error: unknown handle");
      return;
    }
    if (maxlen <= 0 || maxlen > 60000) maxlen = 60000;
    std::lock_guard<std::mutex> lk(p->mu);
    complete_ok(c, execs_.read_out(p, maxlen));
  }

  void resume_exec(Parked& p) {
    Completion c;
    c.id = p.id;
    if (p.op == Op::kExecWait) {
      // Combined os.exec parks as kExecWait and wants exit+output;
      // os.exec.wait wants exit only. Distinguish by remaining empty
      // vs we could use maxlen: maxlen==0 and remaining empty for wait-only
      // if we set remaining="combined" for CombinedOutput.
      if (p.remaining == "wait") finish_exec_wait_only(p.handle, &c);
      else finish_exec_wait(p.handle, &c);
    } else {
      finish_exec_read(p.handle, p.maxlen, &c);
    }
    completions_.push_back(std::move(c));
  }

  bool drain_exec_parked() {
    std::vector<uint64_t> ready;
    for (auto& kv : pending_) {
      if (kv.second.op != Op::kExecWait && kv.second.op != Op::kExecRead) continue;
      uint64_t h = kv.second.handle;
      if (kv.second.op == Op::kExecWait) {
        if (execs_.done(h)) ready.push_back(kv.first);
      } else if (execs_.readable(h)) {
        ready.push_back(kv.first);
      }
    }
    for (uint64_t id : ready) {
      auto it = pending_.find(id);
      if (it == pending_.end()) continue;
      Parked p = std::move(it->second);
      pending_.erase(it);
      resume_exec(p);
    }
    return !ready.empty();
  }
#else
  void do_exec(uint64_t, const std::string&, const std::string&, Completion* c, bool*) {
    c->ok = false;
    c->err = "unsupported on wasigocvm (std::thread fork needs -pthread)";
  }
  bool drain_exec_parked() { return false; }
  void resume_exec(Parked& p) {
    Completion c;
    c.id = p.id;
    complete_err(&c, "error: no std::thread");
    completions_.push_back(std::move(c));
  }
#endif

  void resume(Parked& p) {
    if (p.op == Op::kExecWait || p.op == Op::kExecRead) {
      resume_exec(p);
      return;
    }
    Completion c;
    c.id = p.id;
    bool parked = false;
    Sock* s = lookup(p.handle);
    if (!s || s->fd < 0) {
      complete_err(&c, "error: unknown handle");
      completions_.push_back(std::move(c));
      return;
    }
    switch (p.op) {
      case Op::kConnect: {
        sockaddr_in remote{};
        std::string host, port;
        if (split_hostport(p.to_addr, &host, &port)) fill_v4(host, port, false, &remote);
        finish_connect(&c, p.fd, s->udp, remote);
        break;
      }
      case Op::kTlsConnect: {
        int soerr = 0;
        socklen_t elen = sizeof(soerr);
        getsockopt(p.fd, SOL_SOCKET, SO_ERROR, &soerr, &elen);
        if (soerr != 0) {
          errno = soerr;
          complete_err(&c, syserr("connect"));
          drop_fd(p.fd);
          close(p.fd);
          break;
        }
        start_tls(p.id, p.handle, p.remaining, &c, &parked);
        break;
      }
      case Op::kTlsHandshake:
        continue_tls_handshake(p.id, p.handle, &c, &parked);
        break;
      case Op::kTlsRead:
        do_tls_read(p.id, std::to_string(p.handle) + "\x1f" + std::to_string(p.maxlen), &c, &parked);
        break;
      case Op::kTlsWrite:
        do_tls_write(p.id, std::to_string(p.handle) + "\x1f" + p.remaining, &c, &parked);
        break;
      case Op::kAccept:
        do_accept(p.id, std::to_string(p.handle), &c, &parked);
        break;
      case Op::kRead:
        do_read(p.id, std::to_string(p.handle) + "\x1f" + std::to_string(p.maxlen), &c, &parked);
        break;
      case Op::kWrite:
        write_all(p.id, p.handle, p.fd, p.remaining, &c, &parked);
        break;
      case Op::kReadFrom:
        do_readfrom(p.id, std::to_string(p.handle) + "\x1f" + std::to_string(p.maxlen), &c,
                    &parked);
        break;
      case Op::kWriteTo: {
        std::string payload = std::to_string(p.handle) + "\x1f" + p.to_addr + "\x1f" + p.remaining;
        do_writeto(p.id, payload, &c, &parked);
        break;
      }
      case Op::kExecWait:
      case Op::kExecRead:
        resume_exec(p);
        parked = true;
        break;
    }
    if (!parked) completions_.push_back(std::move(c));
  }

  void pump(int timeout_ms) {
    if (drain_exec_parked()) return;
    if (pending_.empty()) return;
    std::vector<pollfd> fds;
    std::vector<uint64_t> ids;
    bool exec_waiting = false;
    fds.reserve(pending_.size());
    ids.reserve(pending_.size());
    for (auto& kv : pending_) {
      if (kv.second.op == Op::kExecWait || kv.second.op == Op::kExecRead) {
        exec_waiting = true;
        continue;
      }
      if (kv.second.fd < 0) continue;
      pollfd p{};
      p.fd = kv.second.fd;
      p.events = kv.second.events;
      fds.push_back(p);
      ids.push_back(kv.first);
    }
    if (fds.empty()) {
      if (exec_waiting && timeout_ms > 0) {
        unsigned us = static_cast<unsigned>(timeout_ms) * 1000u;
        if (us) usleep(us);
        drain_exec_parked();
      }
      return;
    }
    int n = poll(fds.data(), fds.size(), timeout_ms);
    drain_exec_parked();
    if (n <= 0) return;
    std::vector<uint64_t> ready;
    for (size_t i = 0; i < fds.size(); ++i) {
      if (fds[i].revents != 0) ready.push_back(ids[i]);
    }
    for (uint64_t id : ready) {
      auto it = pending_.find(id);
      if (it == pending_.end()) continue;
      Parked p = std::move(it->second);
      pending_.erase(it);
      resume(p);
    }
  }
};

inline void install_wasigocvm_net_bridge() {
  static WasigocvmNetBridge bridge;
  RegisterAsyncHostBridge(&bridge);
}

}  // namespace gocvm
