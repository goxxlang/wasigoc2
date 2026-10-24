#include "wns/link.h"
#include "wns/proxy.h"
#include "wns/proxyd.h"
#include "wns/sock.h"
#include "wns/stack.h"
#include "wns/status.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Rule {
  bool udp = false;
  wns::Endpoint listen{};
  wns::ProxyTarget target;
};

void Usage() {
  std::fprintf(stderr,
               "usage: wns_proxy --link host:port [options]\n"
               "\n"
               "  --node <id>            local node id (default 1)\n"
               "  --peer-node <id>       peer node id (default 2)\n"
               "  --link <host:port>     bind the SCTP-over-UDP link here\n"
               "  --peer <host:port>     the peer link; omitted means learn it\n"
               "  --tcp <bind=target>    TCP ingress rule, repeatable\n"
               "  --udp <bind=target>    UDP ingress rule, repeatable\n"
               "  --egress               serve OPEN requests from the peer\n"
               "  --allow <host:port>    egress allow rule, port 0 matches any\n"
               "  --no-tcp, --no-udp     refuse that protocol on egress\n"
               "  --idle-ms <n>          UDP flow idle timeout (default 60000)\n"
               "  --rwnd <bytes>         advertised SCTP receive window (default 65535)\n"
               "  --stats-ms <n>         status line interval, 0 to silence\n"
               "  --http <host:port>     serve live JSON status on GET /status\n"
               "  --label <name>         name this instance reports in its status JSON\n"
               "\n"
               "example, client side:\n"
               "  wns_proxy --link 0.0.0.0:9899 --peer 198.51.100.7:9899 \\\n"
               "            --tcp 127.0.0.1:8080=origin.example:80\n"
               "example, exit side:\n"
               "  wns_proxy --node 2 --peer-node 1 --link 0.0.0.0:9899 --egress\n");
}

bool SplitRule(const char* spec, Rule* out) {
  const char* eq = std::strchr(spec, '=');
  if (!eq) return false;
  std::string left(spec, size_t(eq - spec));
  std::string right(eq + 1);
  return wns::ParseEndpoint(left, &out->listen) &&
         wns::ParseProxyTarget(right, &out->target);
}

bool Number(const char* s, uint64_t* out) {
  if (!s || !*s) return false;
  char* end = nullptr;
  unsigned long long v = std::strtoull(s, &end, 10);
  if (!end || *end) return false;
  *out = uint64_t(v);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t node = 1;
  uint32_t peer_node = 2;
  wns::Endpoint link_at{};
  wns::Endpoint peer_at{};
  bool have_link = false;
  bool have_peer = false;
  uint64_t idle_ms = 60000;
  uint64_t stats_ms = 5000;
  uint64_t rwnd = 0;  // 0 = leave the association's default
  wns::Endpoint http_at{};
  bool have_http = false;
  std::string label;
  std::vector<Rule> rules;
  wns::ProxyPolicy policy;

  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    const char* v = (i + 1 < argc) ? argv[i + 1] : nullptr;
    uint64_t n = 0;
    if (std::strcmp(a, "--node") == 0 && v && Number(v, &n)) {
      node = uint32_t(n);
      ++i;
    } else if (std::strcmp(a, "--peer-node") == 0 && v && Number(v, &n)) {
      peer_node = uint32_t(n);
      ++i;
    } else if (std::strcmp(a, "--link") == 0 && v) {
      have_link = wns::ParseEndpoint(v, &link_at);
      if (!have_link) {
        std::fprintf(stderr, "bad --link %s\n", v);
        return 1;
      }
      ++i;
    } else if (std::strcmp(a, "--peer") == 0 && v) {
      have_peer = wns::ParseEndpoint(v, &peer_at);
      if (!have_peer) {
        std::fprintf(stderr, "bad --peer %s\n", v);
        return 1;
      }
      ++i;
    } else if ((std::strcmp(a, "--tcp") == 0 || std::strcmp(a, "--udp") == 0) && v) {
      Rule r;
      r.udp = std::strcmp(a, "--udp") == 0;
      if (!SplitRule(v, &r)) {
        std::fprintf(stderr, "bad %s %s, want bind=target\n", a, v);
        return 1;
      }
      rules.push_back(std::move(r));
      ++i;
    } else if (std::strcmp(a, "--egress") == 0) {
      policy.egress = true;
    } else if (std::strcmp(a, "--allow") == 0 && v) {
      wns::ProxyTarget t;
      if (!wns::ParseProxyTarget(v, &t)) {
        std::fprintf(stderr, "bad --allow %s\n", v);
        return 1;
      }
      policy.allow.push_back(std::move(t));
      ++i;
    } else if (std::strcmp(a, "--no-tcp") == 0) {
      policy.allow_tcp = false;
    } else if (std::strcmp(a, "--no-udp") == 0) {
      policy.allow_udp = false;
    } else if (std::strcmp(a, "--idle-ms") == 0 && v && Number(v, &idle_ms)) {
      ++i;
    } else if (std::strcmp(a, "--stats-ms") == 0 && v && Number(v, &stats_ms)) {
      ++i;
    } else if (std::strcmp(a, "--rwnd") == 0 && v && Number(v, &rwnd)) {
      ++i;
    } else if (std::strcmp(a, "--http") == 0 && v) {
      have_http = wns::ParseEndpoint(v, &http_at);
      if (!have_http) {
        std::fprintf(stderr, "bad --http %s\n", v);
        return 1;
      }
      ++i;
    } else if (std::strcmp(a, "--label") == 0 && v) {
      label = v;
      ++i;
    } else {
      Usage();
      return 1;
    }
  }

  if (!have_link || (rules.empty() && !policy.egress)) {
    Usage();
    return 1;
  }
  if (!wns::NetInit()) {
    std::fprintf(stderr, "network init failed\n");
    return 1;
  }

  wns::Stack stack(node);
  if (rwnd) stack.flow().set_local_rwnd(uint32_t(rwnd));
  wns::UdpLink link;
  if (!link.Bind(link_at)) {
    std::fprintf(stderr, "cannot bind link on %s\n", link_at.Text().c_str());
    return 1;
  }
  link.Attach(&stack);
  if (have_peer) link.SetPeer(peer_at);
  stack.ConnectRemote(peer_node);

  wns::ProxyNode proxy(&stack);
  proxy.AttachLink(&link);
  proxy.SetPolicy(policy);
  proxy.set_udp_idle_ms(idle_ms);

  for (const auto& r : rules) {
    bool ok = r.udp ? proxy.AddUdpIngress(r.listen, r.target)
                    : proxy.AddTcpIngress(r.listen, r.target);
    if (!ok) {
      std::fprintf(stderr, "cannot bind %s ingress on %s\n", r.udp ? "udp" : "tcp",
                   r.listen.Text().c_str());
      return 1;
    }
  }

  std::printf("wns proxy node %u link %s\n", node, link.local().Text().c_str());
  if (have_peer) std::printf("  peer node %u at %s\n", peer_node, peer_at.Text().c_str());
  else std::printf("  peer node %u, waiting for its first packet\n", peer_node);
  for (const auto& in : proxy.ingress()) {
    std::printf("  %s %s -> %s\n", in.udp ? "udp" : "tcp", in.listen.Text().c_str(),
                in.target.Text().c_str());
  }
  if (policy.egress) {
    std::printf("  egress on, %zu allow rule(s)%s%s\n", policy.allow.size(),
                policy.allow_tcp ? "" : ", tcp refused", policy.allow_udp ? "" : ", udp refused");
  }

  wns::StatusServer status;
  if (label.empty()) label = "node-" + std::to_string(node);
  if (have_http) {
    if (!status.Listen(http_at)) {
      std::fprintf(stderr, "cannot bind --http on %s\n", http_at.Text().c_str());
      return 1;
    }
    std::printf("  status json on http://%s/status\n", status.local().Text().c_str());
  }
  std::fflush(stdout);

  bool sent_init = false;
  uint64_t polls = 0;
  for (;;) {
    proxy.Poll(200);
    if (have_http) status.Pump(wns::BuildStatusJson(label, stack, proxy, link));
    if (!sent_init && link.have_peer()) {
      stack.SendInit();
      sent_init = true;
    }
    if (stats_ms && ++polls % (stats_ms / 200 + 1) == 0) {
      const auto& s = proxy.stats();
      const auto& fs = stack.flow().stats();
      std::printf("flows %zu open %llu refused %llu closed %llu up %llu B down %llu B "
                  "pkts %llu/%llu linkq %zu dropped %llu\n",
                  proxy.flow_count(), (unsigned long long)s.opened,
                  (unsigned long long)s.refused, (unsigned long long)s.closed,
                  (unsigned long long)s.bytes_to_peer,
                  (unsigned long long)s.bytes_from_peer,
                  (unsigned long long)link.packets_tx(),
                  (unsigned long long)link.packets_rx(), link.pending(),
                  (unsigned long long)link.packets_dropped());
      std::printf("  assoc cwnd %u rwnd %u inflight %llu rto %ums sacks %llu/%llu "
                  "retx %llu fastretx %llu timeouts %llu stalled %llu dup %llu\n",
                  stack.flow().cwnd(), stack.flow().peer_rwnd(),
                  (unsigned long long)stack.flow().bytes_in_flight(), stack.flow().rto_ms(),
                  (unsigned long long)fs.sacks_sent, (unsigned long long)fs.sacks_recv,
                  (unsigned long long)fs.chunks_retransmitted,
                  (unsigned long long)fs.fast_retransmits, (unsigned long long)fs.timeouts,
                  (unsigned long long)fs.chunks_stalled, (unsigned long long)fs.dup_data_dropped);
      std::fflush(stdout);
    }
  }
}
