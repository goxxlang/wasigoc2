#include "wns/c/system.h"

#include "wns/anycast.h"
#include "wns/proxy.h"
#include "wns/stack.h"
#include "wns/ua.h"

#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<WnsHandle, std::unique_ptr<wns::Stack>>& Stacks() {
  static std::unordered_map<WnsHandle, std::unique_ptr<wns::Stack>> t;
  return t;
}

WnsHandle& NextHandle() {
  static WnsHandle n = 1;
  return n;
}

wns::Stack* Get(WnsHandle h) {
  auto it = Stacks().find(h);
  return it == Stacks().end() ? nullptr : it->second.get();
}

std::unordered_map<WnsHandle, std::unique_ptr<wns::Anycast>>& Planes() {
  static std::unordered_map<WnsHandle, std::unique_ptr<wns::Anycast>> t;
  return t;
}

wns::Anycast* GetPlane(WnsHandle h) {
  auto it = Planes().find(h);
  return it == Planes().end() ? nullptr : it->second.get();
}

// Proxy channels reach the C ABI without callbacks, so arriving payload is
// parked per channel until WnsProxyRecv drains it.
struct ProxyInbox {
  bool serve = false;
  std::unordered_map<uint32_t, std::deque<uint8_t>> rx;
};

std::unordered_map<WnsHandle, ProxyInbox>& Inboxes() {
  static std::unordered_map<WnsHandle, ProxyInbox> t;
  return t;
}

void EnsureProxy(WnsHandle h, wns::Stack* s) {
  if (Inboxes().count(h)) return;
  Inboxes()[h];
  auto& mux = s->proxy();
  mux.SetOpenHandler([h](wns::ProxyChannel*) -> uint8_t {
    auto it = Inboxes().find(h);
    if (it == Inboxes().end() || !it->second.serve) return wns::sctp::kProxyErrPolicy;
    return wns::sctp::kProxyOk;
  });
  mux.SetDataHandler([h](wns::ProxyChannel* ch, std::span<const uint8_t> d) {
    auto it = Inboxes().find(h);
    if (it == Inboxes().end()) return;
    auto& q = it->second.rx[ch->id];
    q.insert(q.end(), d.begin(), d.end());
  });
  mux.SetDatagramHandler([h](wns::ProxyChannel* ch, const wns::ProxyDatagram& dg) {
    auto it = Inboxes().find(h);
    if (it == Inboxes().end()) return;
    auto& q = it->second.rx[ch->id];
    q.insert(q.end(), dg.payload.begin(), dg.payload.end());
  });
}

}  // namespace

extern "C" {

WnsResult WnsInit(void) { return WNS_RESULT_OK; }

void WnsShutdown(void) {
  Planes().clear();
  Inboxes().clear();
  Stacks().clear();
}

WnsHandle WnsStackCreate(uint32_t node_id) {
  if (node_id == 0) return WNS_HANDLE_INVALID;
  WnsHandle h = NextHandle()++;
  Stacks()[h] = std::make_unique<wns::Stack>(node_id);
  return h;
}

WnsResult WnsConnect(WnsHandle a, WnsHandle b) {
  auto* sa = Get(a);
  auto* sb = Get(b);
  if (!sa || !sb) return WNS_RESULT_NOT_FOUND;
  sa->Connect(*sb);
  return WNS_RESULT_OK;
}

WnsResult WnsClose(WnsHandle handle) {
  if (Planes().erase(handle)) return WNS_RESULT_OK;
  Inboxes().erase(handle);
  return Stacks().erase(handle) ? WNS_RESULT_OK : WNS_RESULT_NOT_FOUND;
}

WnsResult WnsAddTorExit(WnsHandle stack, const char* cidr) {
  auto* s = Get(stack);
  if (!s || !cidr) return WNS_RESULT_INVALID_ARGUMENT;
  return s->classifier().AddTorExit(cidr) ? WNS_RESULT_OK : WNS_RESULT_INVALID_ARGUMENT;
}

WnsResult WnsAddVpnCidr(WnsHandle stack, const char* cidr) {
  auto* s = Get(stack);
  if (!s || !cidr) return WNS_RESULT_INVALID_ARGUMENT;
  return s->classifier().AddVpnCidr(cidr) ? WNS_RESULT_OK : WNS_RESULT_INVALID_ARGUMENT;
}

WnsResult WnsAnnounce4(WnsHandle stack, uint8_t op, const char* ip) {
  auto* s = Get(stack);
  if (!s || !ip) return WNS_RESULT_INVALID_ARGUMENT;
  wns::Ipv4 a;
  if (!wns::ParseIpv4(ip, &a)) return WNS_RESULT_INVALID_ARGUMENT;
  s->Announce(wns::sctp::kFamily4, op, {a}, {});
  return WNS_RESULT_OK;
}

WnsResult WnsAddLocal4(WnsHandle stack, const char* ip, uint32_t gossip) {
  auto* s = Get(stack);
  if (!s || !ip) return WNS_RESULT_INVALID_ARGUMENT;
  wns::Ipv4 a;
  if (!wns::ParseIpv4(ip, &a)) return WNS_RESULT_INVALID_ARGUMENT;
  return s->AddLocal4(a, gossip != 0) ? WNS_RESULT_OK : WNS_RESULT_INVALID_ARGUMENT;
}

WnsResult WnsApplyIpify(WnsHandle stack, const char* json, uint32_t gossip) {
  auto* s = Get(stack);
  if (!s || !json) return WNS_RESULT_INVALID_ARGUMENT;
  return s->ApplyIpifyJson(json, gossip != 0) ? WNS_RESULT_OK : WNS_RESULT_INVALID_ARGUMENT;
}

WnsResult WnsVisibleJson(WnsHandle stack, char* out, size_t out_n) {
  auto* s = Get(stack);
  if (!s || !out || out_n == 0) return WNS_RESULT_INVALID_ARGUMENT;
  auto js = s->VisibleJson();
  if (js.empty()) return WNS_RESULT_NOT_FOUND;
  size_t n = js.size() < out_n - 1 ? js.size() : out_n - 1;
  std::memcpy(out, js.data(), n);
  out[n] = 0;
  return WNS_RESULT_OK;
}

WnsResult WnsPathClass(WnsHandle stack, uint32_t node_id, uint32_t* out) {
  auto* s = Get(stack);
  if (!s || !out) return WNS_RESULT_INVALID_ARGUMENT;
  *out = static_cast<uint32_t>(s->PathOf(node_id).path);
  return WNS_RESULT_OK;
}

WnsResult WnsPathScope(WnsHandle stack, uint32_t node_id, uint32_t* out) {
  auto* s = Get(stack);
  if (!s || !out) return WNS_RESULT_INVALID_ARGUMENT;
  *out = static_cast<uint32_t>(s->ScopeOf(node_id).scope);
  return WNS_RESULT_OK;
}

WnsResult WnsSendInit(WnsHandle stack, const char* appear_as) {
  auto* s = Get(stack);
  if (!s) return WNS_RESULT_NOT_FOUND;
  wns::PacketSrc src;
  if (appear_as && appear_as[0]) {
    wns::Ipv4 a;
    wns::Ipv6 b;
    if (wns::ParseIpv4(appear_as, &a)) src = wns::PacketSrc::V4(a);
    else if (wns::ParseIpv6(appear_as, &b)) src = wns::PacketSrc::V6(b);
    else return WNS_RESULT_INVALID_ARGUMENT;
  }
  return s->SendInit(src) ? WNS_RESULT_OK : WNS_RESULT_FAILED_PRECONDITION;
}

WnsResult WnsWssHandshake(WnsHandle stack, const char* resource, const char* host) {
  auto* s = Get(stack);
  if (!s || !resource || !host) return WNS_RESULT_INVALID_ARGUMENT;
  wns::WssConnect c;
  c.resource = resource;
  c.host = host;
  c.requested_streams = 16;
  return s->WssHandshake(c) ? WNS_RESULT_OK : WNS_RESULT_FAILED_PRECONDITION;
}

WnsResult WnsWssReady(WnsHandle stack, uint32_t* out) {
  auto* s = Get(stack);
  if (!s || !out) return WNS_RESULT_INVALID_ARGUMENT;
  auto* sock = s->FindWss(s->assoc().peer_node);
  *out = sock ? static_cast<uint32_t>(sock->ready_state()) : WNS_WSS_CLOSED;
  return WNS_RESULT_OK;
}

WnsResult WnsRpcCall(WnsHandle stack, uint32_t service, uint32_t method, uint64_t* xid) {
  auto* s = Get(stack);
  if (!s || !xid) return WNS_RESULT_INVALID_ARGUMENT;
  uint8_t empty[1] = {0};
  *xid = s->RpcCall(service, method, empty, false);
  return WNS_RESULT_OK;
}

WnsResult WnsRpcPending(WnsHandle stack, uint64_t xid, uint32_t* out) {
  auto* s = Get(stack);
  if (!s || !out) return WNS_RESULT_INVALID_ARGUMENT;
  auto* ep = s->FindRpc(s->assoc().peer_node);
  *out = ep && ep->Pending(xid) ? 1u : 0u;
  return WNS_RESULT_OK;
}

WnsResult WnsDnsPutA(WnsHandle stack, const char* name, const char* ip) {
  auto* s = Get(stack);
  if (!s || !name || !ip) return WNS_RESULT_INVALID_ARGUMENT;
  wns::Ipv4 a;
  if (!wns::ParseIpv4(ip, &a)) return WNS_RESULT_INVALID_ARGUMENT;
  s->DnsPutA(name, a);
  return WNS_RESULT_OK;
}

WnsResult WnsDnsQuery(WnsHandle stack, const char* name) {
  auto* s = Get(stack);
  if (!s || !name) return WNS_RESULT_INVALID_ARGUMENT;
  return s->DnsQuerySend(name, wns::kDnsTypeA) ? WNS_RESULT_OK
                                               : WNS_RESULT_FAILED_PRECONDITION;
}

WnsResult WnsDnsAnswerIp(WnsHandle stack, char* out, size_t out_n) {
  auto* s = Get(stack);
  if (!s || !out || out_n == 0) return WNS_RESULT_INVALID_ARGUMENT;
  auto* m = s->last_dns();
  if (!m || m->answers.empty()) return WNS_RESULT_NOT_FOUND;
  auto ip = wns::FormatIpv4(m->answers[0].a);
  size_t n = ip.size() < out_n - 1 ? ip.size() : out_n - 1;
  std::memcpy(out, ip.data(), n);
  out[n] = 0;
  return WNS_RESULT_OK;
}

WnsResult WnsProxyServe(WnsHandle stack, uint32_t allow) {
  auto* s = Get(stack);
  if (!s) return WNS_RESULT_NOT_FOUND;
  EnsureProxy(stack, s);
  Inboxes()[stack].serve = allow != 0;
  return WNS_RESULT_OK;
}

WnsResult WnsProxyOpen(WnsHandle stack, uint32_t proto, const char* target,
                       uint32_t* channel) {
  auto* s = Get(stack);
  if (!s || !target || !channel) return WNS_RESULT_INVALID_ARGUMENT;
  if (proto != WNS_PROXY_TCP && proto != WNS_PROXY_UDP) return WNS_RESULT_INVALID_ARGUMENT;
  wns::ProxyTarget t;
  if (!wns::ParseProxyTarget(target, &t)) return WNS_RESULT_INVALID_ARGUMENT;
  EnsureProxy(stack, s);
  uint16_t ch = s->ProxyOpen(uint8_t(proto), t);
  if (ch == 0) return WNS_RESULT_FAILED_PRECONDITION;
  *channel = ch;
  return WNS_RESULT_OK;
}

WnsResult WnsProxySend(WnsHandle stack, uint32_t channel, const uint8_t* data,
                       size_t len) {
  auto* s = Get(stack);
  if (!s || (!data && len) || channel > 0xFFFFu) return WNS_RESULT_INVALID_ARGUMENT;
  std::span<const uint8_t> payload(data, len);
  bool ok = s->ProxySend(uint16_t(channel), payload);
  return ok ? WNS_RESULT_OK : WNS_RESULT_FAILED_PRECONDITION;
}

WnsResult WnsProxyRecv(WnsHandle stack, uint32_t channel, uint8_t* out, size_t out_n,
                       size_t* got) {
  auto* s = Get(stack);
  if (!s || !out || !got || channel > 0xFFFFu) return WNS_RESULT_INVALID_ARGUMENT;
  *got = 0;
  auto it = Inboxes().find(stack);
  if (it == Inboxes().end()) return WNS_RESULT_NOT_FOUND;
  auto q = it->second.rx.find(channel);
  if (q == it->second.rx.end()) return WNS_RESULT_NOT_FOUND;
  size_t n = q->second.size() < out_n ? q->second.size() : out_n;
  for (size_t i = 0; i < n; ++i) out[i] = q->second[i];
  q->second.erase(q->second.begin(), q->second.begin() + std::ptrdiff_t(n));
  *got = n;
  // Draining the inbox is what reopens the peer's window.
  s->proxy().Consumed(uint16_t(channel), n);
  return WNS_RESULT_OK;
}

WnsResult WnsProxyClose(WnsHandle stack, uint32_t channel, uint32_t reason) {
  auto* s = Get(stack);
  if (!s || channel > 0xFFFFu) return WNS_RESULT_INVALID_ARGUMENT;
  bool ok = s->ProxyClose(uint16_t(channel), uint8_t(reason));
  auto it = Inboxes().find(stack);
  if (it != Inboxes().end() && reason != WNS_PROXY_EOF) it->second.rx.erase(channel);
  return ok ? WNS_RESULT_OK : WNS_RESULT_FAILED_PRECONDITION;
}

WnsResult WnsProxyState(WnsHandle stack, uint32_t channel, uint32_t* out) {
  auto* s = Get(stack);
  if (!s || !out || channel > 0xFFFFu) return WNS_RESULT_INVALID_ARGUMENT;
  auto* ch = s->proxy().Find(uint16_t(channel));
  if (!ch) return WNS_RESULT_NOT_FOUND;
  *out = uint32_t(ch->state);
  return WNS_RESULT_OK;
}

WnsResult WnsProxyTarget(WnsHandle stack, uint32_t channel, char* out, size_t out_n) {
  auto* s = Get(stack);
  if (!s || !out || out_n == 0 || channel > 0xFFFFu) return WNS_RESULT_INVALID_ARGUMENT;
  auto* ch = s->proxy().Find(uint16_t(channel));
  if (!ch) return WNS_RESULT_NOT_FOUND;
  auto text = ch->bound.port ? ch->bound.Text() : ch->target.Text();
  if (text.size() + 1 > out_n) return WNS_RESULT_INVALID_ARGUMENT;
  std::memcpy(out, text.c_str(), text.size() + 1);
  return WNS_RESULT_OK;
}

WnsResult WnsBgpOpen(WnsHandle stack) {
  auto* s = Get(stack);
  if (!s) return WNS_RESULT_NOT_FOUND;
  return s->BgpOpen() ? WNS_RESULT_OK : WNS_RESULT_FAILED_PRECONDITION;
}

WnsResult WnsBgpState(WnsHandle stack, uint32_t* out) {
  auto* s = Get(stack);
  if (!s || !out) return WNS_RESULT_INVALID_ARGUMENT;
  *out = static_cast<uint32_t>(s->bgp().state());
  return WNS_RESULT_OK;
}

WnsResult WnsRouteUserAgent(const char* ua, uint32_t* engine, uint32_t* flavor,
                            uint32_t* occupancy, uint32_t* flags, char* wasm,
                            size_t wasm_n) {
  auto r = wns::RouteUserAgent(ua ? ua : "");
  if (engine) *engine = static_cast<uint32_t>(r.engine);
  if (flavor) *flavor = static_cast<uint32_t>(r.flavor);
  if (occupancy) *occupancy = static_cast<uint32_t>(r.occupancy);
  if (flags) {
    *flags = 0;
    if (r.holepunch_mojo) *flags |= WNS_UA_HOLEPUNCH_MOJO;
    if (r.mothman_chrome) *flags |= WNS_UA_MOTHMAN_CHROME;
  }
  if (wasm && wasm_n > 0) {
    size_t n = std::strlen(r.wasm);
    if (n >= wasm_n) n = wasm_n - 1;
    std::memcpy(wasm, r.wasm, n);
    wasm[n] = 0;
  }
  return WNS_RESULT_OK;
}

WnsHandle WnsAnycastCreate(WnsHandle local, WnsHandle peer) {
  auto* sa = Get(local);
  if (!sa) return WNS_HANDLE_INVALID;
  WnsHandle h = NextHandle()++;
  auto plane = std::make_unique<wns::Anycast>();
  plane->Bind(sa, Get(peer));
  Planes()[h] = std::move(plane);
  return h;
}

WnsResult WnsAnycastAdvertise(WnsHandle plane, const char* cidr, const char* nexthop) {
  auto* p = GetPlane(plane);
  if (!p || !cidr) return WNS_RESULT_INVALID_ARGUMENT;
  return p->Advertise(cidr, nexthop ? nexthop : "") ? WNS_RESULT_OK
                                                    : WNS_RESULT_INVALID_ARGUMENT;
}

WnsResult WnsAnycastWithdraw(WnsHandle plane, const char* cidr) {
  auto* p = GetPlane(plane);
  if (!p || !cidr) return WNS_RESULT_INVALID_ARGUMENT;
  return p->Withdraw(cidr) ? WNS_RESULT_OK : WNS_RESULT_INVALID_ARGUMENT;
}

WnsResult WnsAnycastPin(WnsHandle plane, const char* name) {
  auto* p = GetPlane(plane);
  if (!p || !name) return WNS_RESULT_INVALID_ARGUMENT;
  return p->Pin(name) ? WNS_RESULT_OK : WNS_RESULT_INVALID_ARGUMENT;
}

WnsResult WnsAnycastLookup(WnsHandle plane, const char* name, char* out, size_t out_n) {
  auto* p = GetPlane(plane);
  if (!p || !name || !out || out_n == 0) return WNS_RESULT_INVALID_ARGUMENT;
  wns::Ipv4 ip;
  if (!p->Lookup(name, &ip)) return WNS_RESULT_NOT_FOUND;
  auto s = wns::FormatIpv4(ip);
  size_t n = s.size() < out_n - 1 ? s.size() : out_n - 1;
  std::memcpy(out, s.data(), n);
  out[n] = 0;
  return WNS_RESULT_OK;
}

WnsResult WnsAnycastSeedCCC(WnsHandle plane) {
  auto* p = GetPlane(plane);
  if (!p) return WNS_RESULT_NOT_FOUND;
  p->SeedCCC();
  return WNS_RESULT_OK;
}

}  // extern "C"
