#ifndef WNS_C_TYPES_H_
#define WNS_C_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t WnsHandle;
typedef uint32_t WnsResult;

#define WNS_HANDLE_INVALID ((WnsHandle)0)

#define WNS_RESULT_OK 0u
#define WNS_RESULT_INVALID_ARGUMENT 3u
#define WNS_RESULT_NOT_FOUND 5u
#define WNS_RESULT_FAILED_PRECONDITION 9u

#define WNS_PATH_UNKNOWN 0u
#define WNS_PATH_CLEARNET 1u
#define WNS_PATH_VPN 2u
#define WNS_PATH_TOR 3u

#define WNS_SCOPE_UNKNOWN 0u
#define WNS_SCOPE_DIRECT 1u
#define WNS_SCOPE_NAT 2u
#define WNS_SCOPE_LAN 3u
#define WNS_SCOPE_INTERNET 4u

#define WNS_WSS_CONNECTING 0u
#define WNS_WSS_OPEN 1u
#define WNS_WSS_CLOSING 2u
#define WNS_WSS_CLOSED 3u

#define WNS_ENGINE_UNKNOWN 0u
#define WNS_ENGINE_WEBKIT 1u
#define WNS_ENGINE_GECKO 2u
#define WNS_ENGINE_CHROMIUM 3u

#define WNS_FLAVOR_NONE 0u
#define WNS_FLAVOR_CHROME 1u
#define WNS_FLAVOR_EDGE 2u
#define WNS_FLAVOR_OPERA 3u
#define WNS_FLAVOR_BRAVE 4u
#define WNS_FLAVOR_VIVALDI 5u
#define WNS_FLAVOR_FIREFOX 6u
#define WNS_FLAVOR_SAFARI 7u
#define WNS_FLAVOR_TOR 8u

#define WNS_OCCUPANCY_NONE 0u
#define WNS_OCCUPANCY_YETI 1u
#define WNS_OCCUPANCY_MOTHMAN 2u
#define WNS_OCCUPANCY_HOLEPUNCH 3u

#define WNS_PROXY_TCP 6u
#define WNS_PROXY_UDP 17u

#define WNS_PROXY_IDLE 0u
#define WNS_PROXY_OPENING 1u
#define WNS_PROXY_OPEN 2u
#define WNS_PROXY_CLOSED 3u

#define WNS_PROXY_EOF 0u
#define WNS_PROXY_RESET 1u

#define WNS_UA_HOLEPUNCH_MOJO 1u
#define WNS_UA_MOTHMAN_CHROME 2u

#ifdef __cplusplus
}
#endif

#endif  // WNS_C_TYPES_H_
