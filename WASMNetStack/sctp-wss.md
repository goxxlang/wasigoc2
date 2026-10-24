W3C Specification Draft: WebSocket over SCTP Transport (WSS-SCTP)
1. Abstract
This specification defines WSS-over-SCTP, an application and transport layer protocol enabling standard WebSocket semantics (wss://) over Stream Control Transmission Protocol (SCTP) associations within browser runtimes. By leveraging SCTP multi-streaming, out-of-order delivery, and partial reliability, WSS-over-SCTP eliminates Head-of-Line (HoL) blocking inherent in TCP-based WebSockets while providing low-framing overhead for high-frequency and real-time web applications.
2. Protocol Architecture & Stack
The WSS-SCTP protocol operates above DTLS-encapsulated SCTP, such as that provided by native SCTP implementations or WebRTC RTCDataChannel infrastructures.
+-------------------------------------------------------------+
|                WSS Application Payload                      |
|             (UTF-8 Text Data / Binary Blobs)                |
+-------------------------------------------------------------+
|                WSS-over-SCTP Encapsulation                  |
|         (Framing, Stream Mapping, Reliability Flags)        |
+-------------------------------------------------------------+
|             Stream Control Transmission Protocol            |
|       (SCTP Association - RFC 4960 / WebRTC SCTP Stack)     |
+-------------------------------------------------------------+
|               Datagram Transport Layer Security             |
|                          (DTLS 1.3)                         |
+-------------------------------------------------------------+
|                           UDP / IP                          |
+-------------------------------------------------------------+


3. SCTP Stream Allocation & Multiplexing
Rather than multiplexing multiple logical channels over a single byte stream using application-level framing headers, WSS-SCTP maps discrete logical WebSocket channels directly to SCTP streams within an established SCTP association.
3.1 Stream ID Conventions
Stream 0 (Control Channel): Reserved exclusively for protocol negotiation, sub-protocol handshakes, stream state maintenance, global PING/PONG keepalives, and association-wide CLOSE frames.
Streams 1 to 65535 (Data Channels): Allocated to independent, concurrent WebSocket data streams.
3.2 Payload Protocol Identifier (PPID) Mapping
To enable network middleboxes and native transport stacks to categorize payload types without inspecting application headers, the following dedicated PPIDs are assigned:
PPID
Identifier Name
Purpose / Payload Type
58
Payload-WSS-Control
Control frames, handshake messages, stream setup signals
59
Payload-WSS-Text
UTF-8 encoded text payloads
60
Payload-WSS-Binary
Raw binary payloads (TypedArrays, ArrayBuffers, Blobs)

4. Frame Format Header Specification
Because the underlying SCTP stack guarantees message boundary preservation, maximum transmission unit (MTU) discovery, and payload fragmentation, WSS-SCTP replaces the variable 2-to-10 byte RFC 6455 header with a fixed 64-bit header.
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|F| RSV | Opcode|     Flags     |          Reserved             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Payload Data                          |
|                             ...                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


4.1 Header Field Definitions
FIN (1 bit): Indicates that this frame is the final fragment of a logical application message.
RSV (3 bits): Reserved for extension negotiation (e.g., per-frame compression). Must be 000 unless negotiated.
Opcode (4 bits): Interpretation of payload data:
0x0: Continuation Frame
0x1: Text Frame (UTF-8)
0x2: Binary Frame
0x8: Close Frame
0x9: Ping
0xA: Pong
Flags (8 bits): Transport hint flags applied per frame:
Bit 0 (UNORDERED): 1 = Bypass SCTP in-order sequence buffering for immediate delivery.
Bit 1 (PARTIAL_RELIABLE): 1 = Allow packet drop under loss conditions according to partial-reliability parameters.
Bits 2–7: Reserved for future flags.
Reserved (16 bits): Set to 0x0000.
Payload Length (32 bits): Unsigned integer defining the length of the payload in bytes N, where 0 \le N \le 2^{32}-1.
5. Handshake & Stream Lifecycle
5.1 Connection Sequence
Client                                                  Server
  |                                                       |
  |========= DTLS / SCTP Association Established =========|
  |                                                       |
  |--- (Stream 0, PPID 58) WSS CONNECT Handshake -------->|
  |<-- (Stream 0, PPID 58) 201 CREATED / Stream Alloc ----|
  |                                                       |
  |========= Active Multi-Stream Communication ===========|
  |                                                       |
  |--- (Stream 1, PPID 59) Text Data Frame -------------->|
  |<-- (Stream 2, PPID 60) Binary Frame (Unordered) ------|
  |                                                       |


5.2 Control Stream Handshake Payload (Stream 0)
Client connection requests are transmitted on Stream 0 as a serialized JSON control structure:
{
  "type": "CONNECT",
  "resource": "/v1/telemetry",
  "host": "api.example.com",
  "protocols": ["json-rpc", "sensor-v2"],
  "requestedStreams": 16,
  "locators": {
    "ipv4": ["203.0.113.10"],
    "ipv6": ["2001:db8::10"]
  }
}


Server response on Stream 0:
{
  "type": "CONNECTED",
  "status": 200,
  "selectedProtocol": "json-rpc",
  "streamRange": {
    "min": 1,
    "max": 16
  }
}


6. Delivery Policies & Reliability Modes
WSS-over-SCTP exposes granular delivery semantics on a per-frame or per-stream basis.
Let R_{max} denote maximum retransmission attempts, and T_{max} denote maximum lifetime in milliseconds.
Fully Reliable In-Order (Default WebSocket Equivalence): \text{Ordered} = \text{True}, \quad R_{max} = \infty, \quad T_{max} = \infty Guarantees standard WebSocket message delivery and sequence preservation.
Unordered Loss-Tolerant (High-Frequency Real-Time Payload): \text{Ordered} = \text{False}, \quad R_{max} = 0 Eliminates Head-of-Line blocking completely; frames arriving out-of-order are dispatched to application memory immediately.
Timed Partial Reliability (Bounded Latency Telemetry): \text{Ordered} = \text{False}, \quad T_{max} = 50\text{ ms} Stops retransmission attempts if the packet frame cannot be acknowledged within T_{max}, protecting real-time state synchronization pipelines.
7. Web API Specification (Browser Binding)
The browser runtime interface extends the standard WebSocket API surface to expose SCTP multi-streaming and partial reliability configuration.
interface SctpWebSocketInit {
  protocols?: string | string[];
  maxStreams?: number;
  defaultOrdered?: boolean;
  defaultMaxRetransmits?: number;
}

interface SctpSendOptions {
  streamId?: number;
  ordered?: boolean;
  maxRetransmits?: number;
  maxPacketLifeTime?: number;
}

declare class SctpWebSocket extends EventTarget {
  constructor(url: string, options?: SctpWebSocketInit);

  readonly readyState: number;
  readonly bufferedAmount: number;
  readonly extensions: string;
  readonly protocol: string;

  // Stream Management
  allocateStream(): number;
  closeStream(streamId: number): void;

  // Data Transmission
  send(data: string | Blob | ArrayBufferView | ArrayBuffer, options?: SctpSendOptions): void;
  close(code?: number, reason?: string): void;

  // Event Handlers
  onopen: ((this: SctpWebSocket, ev: Event) => any) | null;
  onmessage: ((this: SctpWebSocket, ev: SctpMessageEvent) => any) | null;
  onerror: ((this: SctpWebSocket, ev: Event) => any) | null;
  onclose: ((this: SctpWebSocket, ev: CloseEvent) => any) | null;
}

interface SctpMessageEvent extends MessageEvent {
  readonly streamId: number;
  readonly unordered: boolean;
}


8. Security Considerations
Mandatory Encryption (DTLS): All WSS-SCTP associations MUST run over DTLS 1.3 to ensure confidentiality, integrity, and anti-replay protections.
Client-to-Server Masking: Since SCTP framing handles payload boundary defense, client-side XOR masking (required by RFC 6455 over TCP) is omitted, reducing CPU overhead in browser runtimes.
Cross-Origin Protections: Standard Origin header verification MUST be executed during the Stream 0 handshake exchange before allocating stream IDs.
9. IPv4/IPv6 Locator Tracking
WSS-SCTP tracks dual-stack locators as session metadata, not as the session identity. The WebSocket remains bound to the SCTP association and allocated stream range; IPv4 and IPv6 addresses are locators that MAY change.
Each SctpWebSocket MUST retain:
nodeId: overlay identity of the peer
ipv4[] / ipv6[]: current locator sets learned from CONNECT locators, SCTP INIT/INIT-ACK addresses, and subsequent gossip
primaryFamily / primaryAddr: the path currently used for DATA
On local or observed remote IP change, the endpoint MUST emit a GLS-SCTP GOSSIP_CONTROL (Chunk Type 0xF1) IP_CHANGE TLV. It MUST NOT send a second CONNECT, MUST NOT allocate a new stream range, and MUST NOT fire onclose.
Peer IP_CHANGE handling: apply ADD/DELETE/SET_PRIMARY/REPLACE to the locator table; keep readyState OPEN; continue send() on the existing stream IDs. If the new locator is not yet in the SCTP association path set, the transport MAY ASCONF add-ip locally; overlay nodes that are not the direct peer learn the locator only via gossip.
Dual-stack: IPv4-only, IPv6-only, and dual-stack peers are valid. Withdrawal of one family (REPLACE with empty set, or DELETE of the last address) MUST NOT close the socket while the other family remains reachable.
