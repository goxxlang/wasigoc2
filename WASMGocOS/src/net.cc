#include "gocos/jitrt.hpp"

#include "wns/c/system.h"
#include "wns/stack.h"

#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace wasmgocos {
namespace {

std::vector<std::string> Fields(const std::string& args) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= args.size()) {
    size_t sep = args.find('\x1f', start);
    if (sep == std::string::npos) {
      out.push_back(args.substr(start));
      break;
    }
    out.push_back(args.substr(start, sep - start));
    start = sep + 1;
  }
  return out;
}

// Guest memory BIO on one stack, kernel core on the other. Bytes the
// sysroot BIO_reads from its write BIO are WssSend'd here. The kernel
// answers a core service name; anything else is carried back unchanged,
// which is what BIO_write into the read BIO consumes.
struct BioPipe {
  wns::Stack guest{1};
  wns::Stack core{2};
  uint16_t stream = 0;
  bool open = false;

  BioPipe() {
    guest.Connect(core);
    wns::WssConnect hello;
    hello.resource = "/gocos";
    hello.host = "kernel";
    hello.protocols.push_back("bio");
    wns::Ipv4 ip{};
    if (!wns::ParseIpv4("127.0.0.1", &ip)) return;
    hello.ipv4.push_back(ip);
    if (!guest.WssHandshake(hello)) return;
    wns::WssSocket* gs = guest.FindWss(2);
    wns::WssSocket* cs = core.FindWss(1);
    if (gs == nullptr || cs == nullptr) return;
    if (gs->ready_state() != wns::WssReady::kOpen) return;
    stream = gs->stream_min();
    open = true;
  }
};

BioPipe& Pipe() {
  static BioPipe pipe;
  return pipe;
}

std::string ReplyFor(const std::string& req) {
  std::string answered = jit_service(req.c_str());
  if (answered == "ipt miss" || answered.empty()) return req;
  return answered;
}

}  // namespace

std::string jit_bio(const char* args) {
  BioPipe& pipe = Pipe();
  if (!pipe.open) return "error: bio: handshake";
  std::vector<std::string> fields = Fields(args != nullptr ? args : "");
  if (fields.empty() || fields[0].empty()) return "error: bio: empty";
  if (fields[0] == "send") {
    if (fields.size() < 2 || fields[1].empty()) return "error: bio: empty";
    const std::string& body = fields[1];
    std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(body.data()), body.size());
    if (!pipe.guest.WssSend(pipe.stream, wns::sctp::kWssOpText, bytes)) {
      return "error: bio: send";
    }
    wns::WssSocket* core = pipe.core.FindWss(1);
    wns::WssFrame rx;
    if (core == nullptr || !core->TakeRx(&rx)) return "error: bio: empty";
    std::string req(reinterpret_cast<const char*>(rx.payload.data()), rx.payload.size());
    std::string reply = ReplyFor(req);
    std::span<const uint8_t> out(reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
    if (!pipe.core.WssSend(pipe.stream, wns::sctp::kWssOpText, out)) return "error: bio: send";
    return "sent";
  }
  if (fields[0] == "recv") {
    wns::WssSocket* guest = pipe.guest.FindWss(2);
    wns::WssFrame rx;
    if (guest == nullptr || !guest->TakeRx(&rx)) return "error: bio: empty";
    return std::string(reinterpret_cast<const char*>(rx.payload.data()), rx.payload.size());
  }
  return "error: bio: empty";
}

// The kernel's bridge onto ~/WASMNetStack is the C interface in
// wns/c/system.h. One handle is the resolver, one is the kernel. DNS
// put lands on the resolver; query and the answer are the kernel side.
struct NetLink {
  WnsHandle resolver = WNS_HANDLE_INVALID;
  WnsHandle kernel = WNS_HANDLE_INVALID;

  NetLink() {
    if (WnsInit() != WNS_RESULT_OK) return;
    resolver = WnsStackCreate(1);
    kernel = WnsStackCreate(2);
    if (resolver == WNS_HANDLE_INVALID || kernel == WNS_HANDLE_INVALID) return;
    if (WnsConnect(kernel, resolver) != WNS_RESULT_OK) {
      resolver = WNS_HANDLE_INVALID;
      kernel = WNS_HANDLE_INVALID;
    }
  }
};

NetLink& Link() {
  static NetLink link;
  return link;
}

// Plan 9 namespace on its own pair of stacks. fd, env, sh, jobs, rc, and
// walk are the verbs. The kernel side runs those functions; a caller on
// this side only has the hop.
int g_plan9_depth = 0;

struct Plan9Pipe {
  wns::Stack guest{3};
  wns::Stack core{4};
  uint16_t stream = 0;
  bool open = false;

  Plan9Pipe() {
    guest.Connect(core);
    wns::WssConnect hello;
    hello.resource = "/plan9";
    hello.host = "kernel";
    hello.protocols.push_back("plan9");
    wns::Ipv4 ip{};
    if (!wns::ParseIpv4("127.0.0.1", &ip)) return;
    hello.ipv4.push_back(ip);
    if (!guest.WssHandshake(hello)) return;
    wns::WssSocket* gs = guest.FindWss(4);
    wns::WssSocket* cs = core.FindWss(3);
    if (gs == nullptr || cs == nullptr) return;
    if (gs->ready_state() != wns::WssReady::kOpen) return;
    stream = gs->stream_min();
    open = true;
  }
};

Plan9Pipe& Namespace() {
  static Plan9Pipe pipe;
  return pipe;
}

std::string Plan9Dispatch(const std::string& req) {
  size_t sep = req.find('\x1f');
  std::string verb = sep == std::string::npos ? req : req.substr(0, sep);
  std::string rest = sep == std::string::npos ? std::string() : req.substr(sep + 1);
  g_plan9_depth++;
  std::string reply = "error: net: plan9";
  if (verb == "fd") reply = jit_fd(rest.c_str());
  else if (verb == "env") reply = jit_env(rest.c_str());
  else if (verb == "sh") reply = jit_sh(rest.c_str());
  else if (verb == "jobs") reply = jit_jobs(rest.c_str());
  else if (verb == "rc") reply = jit_status(rest.c_str());
  else if (verb == "walk") reply = jit_walk(rest);
  g_plan9_depth--;
  return reply;
}

bool plan9_local() { return g_plan9_depth > 0; }

std::string plan9_hop(const char* verb, const char* args) {
  Plan9Pipe& pipe = Namespace();
  if (!pipe.open || verb == nullptr || verb[0] == 0) return "error: net: link";
  std::string body = verb;
  body.push_back('\x1f');
  if (args != nullptr) body += args;
  std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(body.data()), body.size());
  if (!pipe.guest.WssSend(pipe.stream, wns::sctp::kWssOpText, bytes)) return "error: net: link";
  wns::WssSocket* core = pipe.core.FindWss(3);
  wns::WssFrame rx;
  if (core == nullptr || !core->TakeRx(&rx)) return "error: net: link";
  std::string req(reinterpret_cast<const char*>(rx.payload.data()), rx.payload.size());
  std::string reply = Plan9Dispatch(req);
  std::span<const uint8_t> out(reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
  if (!pipe.core.WssSend(pipe.stream, wns::sctp::kWssOpText, out)) return "error: net: link";
  wns::WssSocket* guest = pipe.guest.FindWss(4);
  wns::WssFrame back;
  if (guest == nullptr || !guest->TakeRx(&back)) return "error: net: link";
  return std::string(reinterpret_cast<const char*>(back.payload.data()), back.payload.size());
}

std::string jit_net(const char* args) {
  NetLink& link = Link();
  if (link.resolver == WNS_HANDLE_INVALID || link.kernel == WNS_HANDLE_INVALID)
    return "error: net: link";
  std::vector<std::string> fields = Fields(args != nullptr ? args : "");
  if (fields.empty() || fields[0].empty() || fields[0] == "link") {
    return "net resolver=" + std::to_string(link.resolver) +
           " kernel=" + std::to_string(link.kernel);
  }
  if (fields[0] == "put" && fields.size() >= 3) {
    if (WnsDnsPutA(link.resolver, fields[1].c_str(), fields[2].c_str()) != WNS_RESULT_OK)
      return "error: net: put";
    return "put " + fields[1];
  }
  if (fields[0] == "query" && fields.size() >= 2 && !fields[1].empty()) {
    if (WnsDnsQuery(link.kernel, fields[1].c_str()) != WNS_RESULT_OK) return "error: net: query";
    char ip[64];
    if (WnsDnsAnswerIp(link.kernel, ip, sizeof(ip)) != WNS_RESULT_OK) return "dns miss";
    return ip;
  }
  if (fields[0] == "serve") {
    if (WnsProxyServe(link.resolver, 1) != WNS_RESULT_OK) return "error: net: serve";
    return "serve";
  }
  if (fields[0] == "open" && fields.size() >= 2 && !fields[1].empty()) {
    uint32_t channel = 0;
    if (WnsProxyOpen(link.kernel, WNS_PROXY_TCP, fields[1].c_str(), &channel) != WNS_RESULT_OK)
      return "error: net: open";
    return "channel " + std::to_string(channel);
  }
  if (fields[0] == "send" && fields.size() >= 3 && !fields[2].empty()) {
    uint32_t channel = static_cast<uint32_t>(std::strtoul(fields[1].c_str(), nullptr, 10));
    if (WnsProxySend(link.kernel, channel, reinterpret_cast<const uint8_t*>(fields[2].data()),
                     fields[2].size()) != WNS_RESULT_OK)
      return "error: net: send";
    return "sent " + fields[1];
  }
  if (fields[0] == "recv" && fields.size() >= 2 && !fields[1].empty()) {
    uint32_t channel = static_cast<uint32_t>(std::strtoul(fields[1].c_str(), nullptr, 10));
    uint8_t buf[256];
    size_t got = 0;
    if (WnsProxyRecv(link.resolver, channel, buf, sizeof(buf), &got) != WNS_RESULT_OK)
      return "net miss";
    return std::string(reinterpret_cast<char*>(buf), got);
  }
  return "net miss";
}

}  // namespace wasmgocos
