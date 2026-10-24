#ifndef WNS_SCTP_H_
#define WNS_SCTP_H_

#include <cstdint>

namespace wns {
namespace sctp {

inline constexpr uint8_t kChunkData = 0x00;
inline constexpr uint8_t kChunkInit = 0x01;
inline constexpr uint8_t kChunkInitAck = 0x02;
inline constexpr uint8_t kChunkSack = 0x03;
inline constexpr uint8_t kChunkLabelData = 0xF0;
inline constexpr uint8_t kChunkGossipControl = 0xF1;

inline constexpr uint16_t kParamIpv4 = 5;
inline constexpr uint16_t kParamIpv6 = 6;
inline constexpr uint16_t kParamCookie = 7;

inline constexpr uint32_t kPpidBgpControl = 18;
inline constexpr uint32_t kPpidWssControl = 58;
inline constexpr uint32_t kPpidWssText = 59;
inline constexpr uint32_t kPpidWssBinary = 60;
inline constexpr uint32_t kPpidRpc = 70;
inline constexpr uint32_t kPpidBgpCommonUpdate = 78;
inline constexpr uint32_t kPpidBgpMultiprotocol = 79;
inline constexpr uint32_t kPpidBgpBulkTelemetry = 80;
inline constexpr uint32_t kPpidBgpFragContinue = 81;
inline constexpr uint32_t kPpidDosctp = 100;
inline constexpr uint32_t kPpidProxyControl = 120;
inline constexpr uint32_t kPpidProxyData = 121;
// WASMTTY session payload. The user data is one WTTY frame, unchanged.
inline constexpr uint32_t kPpidTty = 130;

inline constexpr uint16_t kTlvIpChange = 0x0001;

inline constexpr uint8_t kGossipVersion = 1;

inline constexpr uint8_t kFamily4 = 4;
inline constexpr uint8_t kFamily6 = 6;

inline constexpr uint8_t kOpAdd = 0x01;
inline constexpr uint8_t kOpDelete = 0x02;
inline constexpr uint8_t kOpSetPrimary = 0x03;
inline constexpr uint8_t kOpReplace = 0x04;

inline constexpr uint8_t kIpChangeAuth = 0x01;
inline constexpr uint8_t kIpChangeInternet = 0x02;
inline constexpr uint8_t kIpChangeReal = 0x04;

inline constexpr uint16_t kStreamControl = 0;
inline constexpr uint16_t kStreamBgpMin = 1;
inline constexpr uint16_t kStreamBgpMax = 63;
inline constexpr uint16_t kStreamBgp4 = 1;
inline constexpr uint16_t kStreamBgp6 = 2;
inline constexpr uint16_t kStreamDnsMin = 64;
inline constexpr uint16_t kStreamDnsMax = 1023;
inline constexpr uint16_t kStreamWssMin = 1024;
inline constexpr uint16_t kStreamWssMax = 8190;
// One stream at the top of the old WSS block. WTTY frames (PPID 140).
inline constexpr uint16_t kStreamTty = 8191;
inline constexpr uint16_t kStreamRpcMin = 8192;
inline constexpr uint16_t kStreamRpcMax = 16383;
// Transport proxy channels carve the upper half of the former RPC pool so a
// TCP/UDP flow owns a stream and cannot head-of-line block RPC workers.
inline constexpr uint16_t kStreamProxyMin = 16384;
inline constexpr uint16_t kStreamProxyMax = 32767;
inline constexpr uint16_t kStreamGlsMin = 32768;
inline constexpr uint16_t kStreamGlsMax = 57343;
// WASMTTY sessions. One ordered stream per terminal. The higher node id
// allocates the upper half so both ends can open without colliding.
inline constexpr uint16_t kStreamTtyMin = 57344;
inline constexpr uint16_t kStreamTtyServer = 61440;
inline constexpr uint16_t kStreamTtyMax = 65535;

inline constexpr uint8_t kDataI = 0x08;
inline constexpr uint8_t kDataU = 0x04;
inline constexpr uint8_t kDataB = 0x02;
inline constexpr uint8_t kDataE = 0x01;

inline constexpr uint8_t kWssFin = 0x80;
inline constexpr uint8_t kWssOpText = 0x1;
inline constexpr uint8_t kWssOpBinary = 0x2;
inline constexpr uint8_t kWssOpClose = 0x8;
inline constexpr uint8_t kWssOpPing = 0x9;
inline constexpr uint8_t kWssOpPong = 0xA;
inline constexpr uint8_t kWssUnordered = 0x01;
inline constexpr uint8_t kWssPartialReliable = 0x02;

inline constexpr uint16_t kPort = 179;

inline constexpr bool IsBgpStream(uint16_t s) { return s >= kStreamBgpMin && s <= kStreamBgpMax; }
inline constexpr bool IsDnsStream(uint16_t s) { return s >= kStreamDnsMin && s <= kStreamDnsMax; }
inline constexpr bool IsWssStream(uint16_t s) { return s >= kStreamWssMin && s <= kStreamWssMax; }
inline constexpr bool IsRpcStream(uint16_t s) { return s >= kStreamRpcMin && s <= kStreamRpcMax; }
inline constexpr bool IsProxyStream(uint16_t s) {
  return s >= kStreamProxyMin && s <= kStreamProxyMax;
}
inline constexpr bool IsGlsStream(uint16_t s) {
  return s >= kStreamGlsMin && s <= kStreamGlsMax;
}
inline constexpr bool IsTtyStream(uint16_t s) {
  return s >= kStreamTtyMin && s <= kStreamTtyMax;
}

inline constexpr uint8_t kRpcFlagReq = 0x01;
inline constexpr uint8_t kRpcFlagRes = 0x02;
inline constexpr uint8_t kRpcFlagOne = 0x04;
inline constexpr uint8_t kRpcFlagErr = 0x08;

// Occupancy APIs (UA, anycast, WASMDNS, shape, deeplink, occupy, drive)
// ride this service instead of REST HTTP. Tor Browser has no UDP punch;
// frames still use this RPC wire on the HTTP/SCTP occupancy hop (no occupy WSS).
// Command/response is two-way on service 1:
//   proxy  HTTP occupancy fetch
//   hook   occupy polls pending operator commands
//   reply  occupy completes a Call (payload → voodoo shell)
//   call   operator fires and waits for reply
inline constexpr uint32_t kRpcOccupancy = 1;
inline constexpr uint32_t kRpcOccProxy = 0;
inline constexpr uint32_t kRpcOccHook = 1;
inline constexpr uint32_t kRpcOccReply = 2;
inline constexpr uint32_t kRpcOccCall = 3;

// Association-level reliability and flow control (FLOW v1): SACK-based
// retransmission, a peer-advertised receive window (rwnd), and a slow-start /
// congestion-avoidance send window (cwnd) gate every SCTP DATA chunk, the way
// RFC 9260 sections 6.2 and 7 describe. See sctp-flow.md for the wire detail
// and the simplifications this implementation makes.

// One SCTP DATA chunk's payload ceiling, chosen so a chunk plus its SCTP and
// UDP-encapsulation headers still fits a common path MTU.
inline constexpr uint16_t kSctpMtu = 1200;

// RFC 9260 4.2.1: cwnd = min(4*MTU, max(2*MTU, 4380)).
inline constexpr uint32_t kSctpInitialCwnd = 4380;
// A DATA chunk's floor after a retransmit timeout (slow-start restart).
inline constexpr uint32_t kSctpMinCwnd = kSctpMtu;

// Retransmission timeout bounds and the estimator's starting point, before a
// round-trip sample exists (RFC 6298 with SCTP's usual 1 s initial RTO).
inline constexpr uint32_t kSctpInitialRtoMs = 1000;
inline constexpr uint32_t kSctpMinRtoMs = 200;
inline constexpr uint32_t kSctpMaxRtoMs = 60000;

// Consecutive SACKs reporting a TSN missing before it is fast-retransmitted,
// ahead of the RTO timer (RFC 9260 7.2.4's Max.Burst-style dupack count).
inline constexpr uint32_t kSctpFastRetransmitMisses = 3;
// Retransmit attempts for one chunk before this implementation gives up on
// it rather than aborting the whole association (see sctp-flow.md ยง9).
inline constexpr uint32_t kSctpMaxRetransmits = 8;

// The advertised receive window (a_rwnd) when nothing narrower is configured.
inline constexpr uint32_t kSctpDefaultRwnd = 65535;
// A DATA chunk arrival triggers an immediate SACK after this many chunks
// have gone unacknowledged, or after the delay below, whichever comes first.
inline constexpr uint32_t kSctpSackEvery = 2;
inline constexpr uint32_t kSctpSackDelayMs = 200;

// Transport proxy (PXY v1): TCP/UDP <-> SCTP <-> TCP/UDP. OPEN/OPEN_ACK/CLOSE
// ride stream 0 with PPID_PROXY_CONTROL; DATA/DGRAM ride the channel's own
// stream in the proxy block with PPID_PROXY_DATA.
inline constexpr uint8_t kProxyVersion = 1;

inline constexpr uint8_t kProxyOpen = 0x01;
inline constexpr uint8_t kProxyOpenAck = 0x02;
inline constexpr uint8_t kProxyData = 0x03;
inline constexpr uint8_t kProxyDatagram = 0x04;
inline constexpr uint8_t kProxyClose = 0x05;
inline constexpr uint8_t kProxyAck = 0x06;

// Proto field values match the IP protocol numbers they carry.
inline constexpr uint8_t kProxyTcp = 6;
inline constexpr uint8_t kProxyUdp = 17;

// OPEN_ACK status, carried in the flags octet.
inline constexpr uint8_t kProxyOk = 0x00;
inline constexpr uint8_t kProxyErrRefused = 0x01;
inline constexpr uint8_t kProxyErrUnreachable = 0x02;
inline constexpr uint8_t kProxyErrPolicy = 0x03;
inline constexpr uint8_t kProxyErrResources = 0x04;
inline constexpr uint8_t kProxyErrProto = 0x05;

// CLOSE reason, carried in the flags octet. kProxyEof is a half close: the
// sender will not write again, the reverse direction stays up.
inline constexpr uint8_t kProxyEof = 0x00;
inline constexpr uint8_t kProxyReset = 0x01;
inline constexpr uint8_t kProxyTimeout = 0x02;

// Target address kinds.
inline constexpr uint8_t kProxyAddrName = 0;
inline constexpr uint8_t kProxyAddr4 = 4;
inline constexpr uint8_t kProxyAddr6 = 6;

// Bytes a TCP channel may have in flight before the peer acknowledges what it
// has handed downstream. This is application-level backpressure, distinct
// from the association's own SCTP-level flow control (kSctp* above): it caps
// how fast a channel hands bytes to the association in the first place, so a
// PXY egress socket that stops draining can still shut the ingress socket's
// read side even though the association underneath is reliable now.
inline constexpr uint32_t kProxyWindow = 128u << 10;

// Largest PXY DATA payload in one chunk. Kept equal to the association's own
// segment size so one proxy write becomes exactly one SCTP DATA chunk.
inline constexpr uint16_t kProxySegment = kSctpMtu;

// Channel ids are owner scoped: the responder sets the high bit so both ends
// can open channels on one association without colliding.
inline constexpr uint16_t kProxyServerBit = 0x8000;

// WTTY frame, the WASMTTY wire. 4-octet header, then payload.
//   type (8) | flags (8) | length (16) | payload
inline constexpr uint8_t kTtyHello = 0x01;
inline constexpr uint8_t kTtyStdin = 0x02;
inline constexpr uint8_t kTtyStdout = 0x03;
inline constexpr uint8_t kTtyStderr = 0x04;
inline constexpr uint8_t kTtyResize = 0x05;
inline constexpr uint8_t kTtyExit = 0x06;
inline constexpr uint8_t kTtyPing = 0x07;
inline constexpr uint8_t kTtyPong = 0x08;

inline constexpr uint8_t kTtyRoleListener = 1;
inline constexpr uint8_t kTtyRoleReceiver = 2;
inline constexpr uint8_t kTtyVersion = 1;
inline constexpr uint32_t kTtyHeaderSize = 4;
inline constexpr uint32_t kTtyMaxPayload = 16 * 1024;

}  // namespace sctp
}  // namespace wns

#endif  // WNS_SCTP_H_
