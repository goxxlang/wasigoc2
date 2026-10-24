#ifndef WNS_STATUS_H_
#define WNS_STATUS_H_

#include "wns/link.h"
#include "wns/proxyd.h"
#include "wns/sock.h"
#include "wns/stack.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wns {

// One JSON object describing everything a UI would want to poll from a
// running wns_proxy: node identity, link counters, the association's flow
// controller, and every PXY channel. Pure serialization, no I/O.
std::string BuildStatusJson(const std::string& label, Stack& stack, const ProxyNode& proxy,
                            const UdpLink& link);

// A tiny non-blocking HTTP server exposing BuildStatusJson() over
// GET /status (any other path gets the same body; this has exactly one
// resource). Meant to be pumped from the same loop as everything else --
// wns_proxy calls Pump() once per iteration, the way it already drains the
// association's link and its proxy flows.
class StatusServer {
 public:
  StatusServer() = default;
  StatusServer(const StatusServer&) = delete;
  StatusServer& operator=(const StatusServer&) = delete;

  bool Listen(const Endpoint& at);
  bool open() const { return listener_.open(); }
  const Endpoint& local() const { return listener_.local(); }

  // Accepts new connections and services any that are readable or still have
  // a response queued. `body` is recomputed by the caller each pass (it's
  // cheap -- a handful of counters -- and this way a slow client never sees a
  // stale snapshot frozen at accept time).
  void Pump(const std::string& body);

 private:
  struct Conn {
    std::unique_ptr<TcpConn> tcp;
    std::string in;
    bool responded = false;
  };

  TcpListener listener_;
  std::vector<Conn> conns_;
};

}  // namespace wns

#endif  // WNS_STATUS_H_
