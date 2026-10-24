#ifndef WNS_C_SYSTEM_H_
#define WNS_C_SYSTEM_H_

#include "wns/c/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

WnsResult WnsInit(void);
void WnsShutdown(void);

WnsHandle WnsStackCreate(uint32_t node_id);
WnsResult WnsConnect(WnsHandle a, WnsHandle b);
WnsResult WnsClose(WnsHandle handle);

WnsResult WnsAddTorExit(WnsHandle stack, const char* cidr);
WnsResult WnsAddVpnCidr(WnsHandle stack, const char* cidr);
WnsResult WnsAnnounce4(WnsHandle stack, uint8_t op, const char* ip);
WnsResult WnsAddLocal4(WnsHandle stack, const char* ip, uint32_t gossip);
WnsResult WnsApplyIpify(WnsHandle stack, const char* json, uint32_t gossip);
WnsResult WnsVisibleJson(WnsHandle stack, char* out, size_t out_n);
WnsResult WnsPathClass(WnsHandle stack, uint32_t node_id, uint32_t* out);
WnsResult WnsPathScope(WnsHandle stack, uint32_t node_id, uint32_t* out);
WnsResult WnsSendInit(WnsHandle stack, const char* appear_as);

WnsResult WnsWssHandshake(WnsHandle stack, const char* resource, const char* host);
WnsResult WnsWssReady(WnsHandle stack, uint32_t* out);

WnsResult WnsRpcCall(WnsHandle stack, uint32_t service, uint32_t method, uint64_t* xid);
WnsResult WnsRpcPending(WnsHandle stack, uint64_t xid, uint32_t* out);

WnsResult WnsDnsPutA(WnsHandle stack, const char* name, const char* ip);
WnsResult WnsDnsQuery(WnsHandle stack, const char* name);
WnsResult WnsDnsAnswerIp(WnsHandle stack, char* out, size_t out_n);

// TCP/UDP <-> SCTP <-> TCP/UDP channels. A node that wants to serve OPENs
// calls WnsProxyServe first; `target` is "host:port" or "[v6]:port".
WnsResult WnsProxyServe(WnsHandle stack, uint32_t allow);
WnsResult WnsProxyOpen(WnsHandle stack, uint32_t proto, const char* target,
                       uint32_t* channel);
WnsResult WnsProxySend(WnsHandle stack, uint32_t channel, const uint8_t* data,
                       size_t len);
WnsResult WnsProxyRecv(WnsHandle stack, uint32_t channel, uint8_t* out, size_t out_n,
                       size_t* got);
WnsResult WnsProxyClose(WnsHandle stack, uint32_t channel, uint32_t reason);
WnsResult WnsProxyState(WnsHandle stack, uint32_t channel, uint32_t* out);
WnsResult WnsProxyTarget(WnsHandle stack, uint32_t channel, char* out, size_t out_n);

WnsResult WnsBgpOpen(WnsHandle stack);
WnsResult WnsBgpState(WnsHandle stack, uint32_t* out);

WnsResult WnsRouteUserAgent(const char* ua, uint32_t* engine, uint32_t* flavor,
                            uint32_t* occupancy, uint32_t* flags, char* wasm,
                            size_t wasm_n);

WnsHandle WnsAnycastCreate(WnsHandle local, WnsHandle peer);
WnsResult WnsAnycastAdvertise(WnsHandle plane, const char* cidr, const char* nexthop);
WnsResult WnsAnycastWithdraw(WnsHandle plane, const char* cidr);
WnsResult WnsAnycastPin(WnsHandle plane, const char* name);
WnsResult WnsAnycastLookup(WnsHandle plane, const char* name, char* out, size_t out_n);
WnsResult WnsAnycastSeedCCC(WnsHandle plane);

#ifdef __cplusplus
}
#endif

#endif  // WNS_C_SYSTEM_H_
