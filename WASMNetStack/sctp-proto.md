Unified Multi-Service SCTP (UMS-SCTP) Transport & Protocol Integration Specification
Abstract
This specification defines the Unified Multi-Service Stream Control Transmission Protocol (UMS-SCTP) architecture. UMS-SCTP integrates BGP-4 over SCTP, SCTP RPC v1, DNS over SCTP (DoSCTP), Gossip-Routed Label-Switched SCTP (GLS-SCTP), WebSocket over SCTP (WSS-SCTP), PXY v1, and WASMTTY (WTTY terminal sessions) into a consolidated, high-performance transport fabric. By standardizing global Stream Identifier (SID) block partitioning, Payload Protocol Identifier (PPID) demultiplexing, custom chunk parsing, and unified Stream 0 control-plane dispatching, UMS-SCTP enables inter-domain routing, service RPCs, infrastructure DNS queries, overlay label switching, and browser real-time streaming to coexist securely over single or mesh-connected SCTP associations without Head-of-Line (HoL) blocking or control-channel collisions.
1. Architectural Layering & System Stack
The UMS-SCTP framework establishes a multi-tiered architecture that separates overlay path selection, transport demultiplexing, and application payload handling.
+-----------------------------------------------------------------------------------+
|                            APPLICATION LAYER PAYLOADS                             |
|  +------------------+  +----------------+  +--------------+  +------------------+ |
|  |  BGP-4 Routing   |  |  SCTP RPC v1   |  | DoSCTP Wire  |  |   WSS Payload    | |
|  | (AFI/SAFI Streams|  | (XID Services) |  | (DNS Query)  |  | (Text / Binary)  | |
|  +--------+---------+  +-------+--------+  +------+-------+  +--------+---------+ |
+-----------|--------------------|------------------|-------------------|-----------+
|           v                    v                  v                   v           |
|  +-----------------------------------------------------------------------------+  |
|  |                   UMS-SCTP Unified Framing & Demux Layer                     |  |
|  |   (PPID Mapping, Stream Block Allocation, Stream 0 Control Dispatcher)      |  |
|  +-------------------------------------+---------------------------------------+  |
|                                        |                                          |
|                                        v                                          |
|  +-----------------------------------------------------------------------------+  |
|  |                 GLS-SCTP Overlay & Label Swap Subsystem                     |  |
|  |       (Custom Chunks: 0xF0 LABEL_DATA, 0xF1 GOSSIP_CONTROL Engine)          |  |
|  +-------------------------------------+---------------------------------------+  |
+----------------------------------------|------------------------------------------+
|                                        v                                          |
|  +-----------------------------------------------------------------------------+  |
|  |                 Stream Control Transmission Protocol (SCTP)                 |  |
|  |          (RFC 9260 / PR-SCTP RFC 3758 / Stream Interleaving RFC 8260)        |  |
|  +-------------------------------------+---------------------------------------+  |
|                                        |                                          |
|                                        v                                          |
|  +-----------------------------------------------------------------------------+  |
|  |                 Datagram Transport Layer Security (DTLS 1.3)                |  |
|  |                      and/or SCTP-AUTH (RFC 4895)                            |  |
|  +-------------------------------------+---------------------------------------+  |
|                                        |                                          |
|                                        v                                          |
|  +-----------------------------------------------------------------------------+  |
|  |                                  IP / UDP                                   |  |
|  +-----------------------------------------------------------------------------+  |


2. Global Stream Identifier (SID) Partitioning
To prevent stream collision across active protocols while avoiding application-layer framing overhead, UMS-SCTP partitions the 16-bit SCTP Stream Identifier space (0 \le S \le 65535) into dedicated functional blocks.
2.1 Global Stream Allocation Table
Stream ID Range (S)
Sub-System Allocation
Operational Description & Protocol Binding
0
UMS Control Channel
Unified Stream 0 Control Dispatcher (Multiplexes BGP Control, DoSCTP Negotiator, WSS Control, GLS Gossip).
1 \le S \le 63
BGP-4 Routing Streams
Address family isolation: Stream 1 (IPv4), Stream 2 (IPv6), Stream 3 (EVPN), Stream 4 (FlowSpec/DetNet), 5–63 (Telemetry/Bulk).
64 \le S \le 1023
DoSCTP Query Streams
Parallel DNS query streams. Supports unordered delivery ('U' bit).
1024 \le S \le 8191
WSS-SCTP Channels
Web-browser data channels (Text, Binary payloads, custom sub-protocols).
8192 \le S \le 16383
SCTP RPC v1 Pool
Concurrent microservice RPC calls mapped dynamically to worker streams.
16384 \le S \le 32767
PXY v1 Proxy Channels
Proxied TCP connections and UDP flows, one channel per stream (see sctp-proxy.md).
32768 \le S \le 57343
GLS Overlay Switched Paths
Label-switched data paths (LABEL\_DATA chunk forwarding).
57344 \le S \le 65535
WASMTTY Sessions
One ordered stream per terminal. Payload is a WTTY frame from WASMTTY, unchanged: type, flags, uint16 length, payload. Hello payload is the magic WTTY, version, role, cols, rows. The lower node id opens streams 57344–61439; the higher node id opens 61440–65535.

3. PPID and Custom Chunk Registry Integration
UMS-SCTP combines standard SCTP DATA chunk demultiplexing via Payload Protocol Identifiers (PPIDs) with experimental raw chunk types introduced by GLS-SCTP.
3.1 Complete Protocol Demultiplexing Matrix
Dispatch Identifier
Identifier Type
Target Sub-System
Purpose / Encoding
PPID 18
PPID
BGP-4
PPID_BGP_CONTROL (Session management: OPEN, KEEPALIVE, NOTIFICATION)
PPID 58
PPID
WSS-SCTP
Payload-WSS-Control (WebSocket Handshake, PING/PONG, CLOSE)
PPID 59
PPID
WSS-SCTP
Payload-WSS-Text (UTF-8 encoded WebSocket text frame)
PPID 60
PPID
WSS-SCTP
Payload-WSS-Binary (Raw binary WebSocket frame)
PPID 70
PPID
SCTP RPC v1
SCTP_RPC (Fixed 24-byte RPC header + payload)
PPID 78
PPID
BGP-4
PPID_BGP_COMMON_UPDATE (Standard IPv4 Unicast updates)
PPID 79
PPID
BGP-4
PPID_BGP_MULTIPROTOCOL (MP-BGP updates: IPv6, EVPN, FlowSpec)
PPID 80
PPID
BGP-4
PPID_BGP_BULK_TELEMETRY (High-volume non-routing telemetry updates)
PPID 81
PPID
BGP-4
PPID_BGP_FRAG_CONTINUE (Upper-layer BGP message fragment)
PPID 100
PPID
DoSCTP
DoSCTP_PAYLOAD (Raw DNS wire-format payload without 2-byte prefix)
PPID 120
PPID
PXY v1
PROXY_CONTROL (Channel OPEN, OPEN_ACK, CLOSE, ACK on Stream 0)
PPID 121
PPID
PXY v1
PROXY_DATA (Channel DATA and DGRAM frames on a proxy stream)
PPID 130
PPID
WASMTTY
WTTY (WASMTTY frame on the session stream: hello, stdin, stdout, stderr, resize, exit, ping, pong)
Chunk Type 0xF0
SCTP Chunk
GLS-SCTP
LABEL_DATA (Overlay label stack + payload encapsulation)
Chunk Type 0xF1
SCTP Chunk
GLS-SCTP
GOSSIP_CONTROL (IPv4/IPv6 IP_CHANGE for WSS/RPC locator tracking; optional topology & label TLVs)

4. Stream 0 Unified Control-Plane Dispatcher
4.1 The Stream 0 Collision Problem
Four of the integrated protocols (BGP-4, DoSCTP, WSS-SCTP, and GLS-SCTP) mandate Stream 0 for operational control:
BGP-4 requires Stream 0 with PPID 18.
WSS-SCTP specifies Stream 0 for JSON connection handshakes with PPID 58.
DoSCTP reserves Stream 0 for capabilities.
GLS-SCTP delivers GOSSIP_CONTROL (Chunk 0xF1) on Stream 0.
4.2 Unified Stream 0 Demultiplexing Logic
UMS-SCTP resolves control-plane collisions at the transport interface by implementing a zero-overhead Stream 0 demultiplexer:
                          [ Incoming Stream 0 Chunk ]
                                       |
                   +-------------------+-------------------+
                   |                                       |
        Is Chunk Type 0xF1?                      Is Standard DATA Chunk?
                   |                                       |
                  YES                                     YES
                   |                                       |
        Dispatch to GLS-SCTP                     Inspect DATA Chunk PPID
       Gossip Engine Subsystem                             |
                                       +---------+---------+---------+
                                       |         |         |         |
                                    PPID 18   PPID 58   PPID 100  Others
                                       |         |         |         |
                                       v         v         v         v
                                     BGP-4      WSS      DoSCTP    Reject
                                    Control   Control   Control   (Cease)


Chunk-Type Inspection: If the incoming chunk on Stream 0 has Chunk Type = 0xF1, it is passed directly to the GLS-SCTP Gossip Control Engine. Chunk Type = 0x03 (SACK) is passed to the FLOW reliability and flow control engine (see sctp-flow.md) rather than the PPID dispatcher below; it carries no PPID of its own.
PPID Inspection: If the chunk is a standard SCTP DATA chunk (Type = 0x00), the receiver evaluates the 32-bit PPID header field:
PPID 18: Dispatched to the BGP Finite State Machine (FSM).
PPID 58: Dispatched to the WSS Protocol Handshake Manager.
PPID 100: Dispatched to the DoSCTP Capability Negotiator.
PPID 120: Dispatched to the PXY Channel Control Engine (see sctp-proxy.md).
PPID 130 is not a Stream 0 control PPID. A WTTY frame rides its session stream (57344–65535).
Validation & Security: Any unrecognized PPID or unexpected chunk type arriving on Stream 0 MUST trigger an SCTP Operational Error or BGP NOTIFICATION with subcode Connection Unsynchronized.
5. Unified Framing & Encapsulation Hierarchy
When payloads travel across a UMS-SCTP fabric, they undergo structural encapsulation depending on whether they are routed directly to local endpoints or label-switched over an intermediate GLS overlay mesh.
5.1 End-to-End Encapsulation Headers
+----------------------------------------------------------------------------------+
|                           SCTP Common Header (12 Bytes)                          |
+----------------------------------------------------------------------------------+
|                   SCTP Chunk Header (Type, Flags, Length, TS N)                   |
+----------------------------------------------------------------------------------+
|  OPTIONAL: GLS-SCTP Header (Type 0xF0: Stack Depth + Label Entries [4..64 Bytes]) |
+----------------------------------------------------------------------------------+
|  Service Framing Layer (Selected by PPID / Stream ID):                          |
|   - BGP-4: Optional Upper-Layer Frag Header (PPID 81)                            |
|   - SCTP RPC v1: Fixed 24-Byte Header (Version, XID, Service ID, Method ID)      |
|   - WSS-SCTP: Fixed 64-Bit Header (FIN, Opcode, Flags, Payload Length)            |
|   - DoSCTP: Raw DNS Wire-Format Header                                           |
|   - WASMTTY: WTTY frame (type, flags, uint16 length, payload)                    |
+----------------------------------------------------------------------------------+
|                                  Payload Data                                    |
+----------------------------------------------------------------------------------+


5.2 Layered Encapsulation Examples
Case A: WSS-SCTP Direct Endpoint Payload (Stream 1024, PPID 59)
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Type = 0x00 (DATA) | Flags (U/B/E) |          Length          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                              TSN                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Stream ID = 1024          |     Stream Seq Num     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           PPID = 59                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|F| RSV | Opcode|     Flags     |            Reserved           |  <-- WSS Header
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        UTF-8 Text Data                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


Case B: SCTP RPC Tunneled over GLS Label-Switched Path (Chunk 0xF0)
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Type = 0xF0 (LABEL_DATA) | Flags   |          Length          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Stack Depth = 2| Reserved (MBZ)|    Stream Seq Num (Stream N) |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Label Entry 1: Outer Path Label (20)             | TC |0| TTL |  <-- GLS Stack
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Label Entry 2: Service Egress Label (20)         | TC |1| TTL |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Version (8)  |   Flags (8)   |         Reserved (16)       |  <-- RPC v1 Header
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Transaction ID (64)                     |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Service ID (32)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Method ID (32)                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Payload Length (32)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Serialized Payload                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


6. Integrated Transport Delivery Semantics & Reliability Matrix
UMS-SCTP maps the QoS requirements of each sub-system to the underlying transport capabilities of SCTP (Partial Reliability Extension RFC 3758, Stream Interleaving RFC 8260, and Unordered Delivery).
6.1 Quality of Service Configuration Matrix
Protocol Payload Type
Target Stream Range
Ordering (U bit)
Reliability Policy (PR-SCTP)
Interleaving Allowed
BGP-4 Session Control
Stream 0
Ordered (U=0)
Fully Reliable (R_{max}=\infty)
No
BGP-4 Unicast Updates
Streams 1–2
Ordered (U=0)
Fully Reliable (R_{max}=\infty)
Yes (via PPID 81)
BGP Bulk Telemetry
Stream 5+
Unordered (U=1)
Timed PR (T_{max}=500\text{ ms})
Yes
SCTP RPC Request/Response
Streams 8192+
Dynamic
Fully Reliable or Retransmit-Limited
Yes
SCTP RPC Oneway
Streams 8192+
Unordered (U=1)
Partial Reliable (R_{max}=1)
Yes
DoSCTP Standard Query
Streams 64–1023
Unordered (U=1)
Fully Reliable (R_{max}=\infty)
Yes
WSS Telemetry / State
Streams 1024+
Unordered (U=1)
Loss-Tolerant (R_{max}=0)
Yes
WSS Media Stream
Streams 1024+
Unordered (U=1)
Timed PR (T_{max}=50\text{ ms})
Yes
GLS Gossip Sync
Stream 0
Ordered (U=0)
Fully Reliable (R_{max}=\infty)
No
GLS Switched Payload
Streams 32768–57343
Per Inner Label
Inherited from Outer Stack TC
Yes
WASMTTY session
Streams 57344–65535
Ordered (U=0)
Fully Reliable (R_{max}=\infty)
No

7. Cross-Layer Interaction & End-to-End Operational Lifecycle
 [ Client / Edge Node ]                                            [ Transit / Mesh Node ]                                           [ Target Service Node ]
           |                                                                  |                                                                 |
           |=========================== 1. SCTP Association Setup & DTLS 1.3 Handshake ===========================|
           |                                                                  |                                                                 |
           |--- (Stream 0, Chunk 0xF1) GLS Gossip State Exchange ------------>|                                                                 |
           |<-- (Stream 0, Chunk 0xF1) Label Allocation Vector (FEC: RPC) -----|                                                                 |
           |                                                                  |                                                                 |
           |--- (Stream 0, PPID 18) BGP OPEN Exchange ----------------------->|                                                                 |
           |--- (Stream 0, PPID 58) WSS CONNECT Handshake ---------------------------------------------------------------------------------->|
           |<-- (Stream 0, PPID 58) WSS 201 CREATED (Alloc Streams 1024-1039) ------------------------------------------------------------------|
           |                                                                  |                                                                 |
           |=== 2. Concurrent Multi-Service Data Transmission                 |                                                                 |
           |                                                                  |                                                                 |
           |--- (Stream 64, PPID 100, U=1) DoSCTP DNS Query ------------------->|                                                                 |
           |<-- (Stream 64, PPID 100, U=1) DoSCTP DNS Answer ------------------|                                                                 |
           |                                                                  |                                                                 |
           |--- (Chunk 0xF0, Label Stack [L1, L2]) RPC Request --------------->|                                                                 |
           |                                                                  |--- (SWAP L1->L3, Decrement TTL) Chunk 0xF0 Forwarding --------->|
           |                                                                  |                                  (POP Label L2, Reassemble RPC) |
           |                                                                  |                                  Execute Target Method          |
           |<---------------------------------------------------------------------------------------------------- (Stream 8192, PPID 70) RPC Response|
           |                                                                  |                                                                 |
           |--- (Stream 1024, PPID 60, Rmax=0) WSS Real-Time Sensor Data ------------------------------------------------------------------------>|
           |                                                                  |                                                                 |


7.1 Detailed Phase Breakdown
Association Setup & Overlay Discovery:
The endpoints execute the SCTP 4-way cookie handshake (INIT, INIT-ACK, COOKIE-ECHO, COOKIE-ACK) over destination port 179 or standard service ports, securing the transport with DTLS 1.3.
Endpoints initiate GLS-SCTP gossip exchange on Stream 0 using Chunk Type 0xF1. The first TLVs are IP_CHANGE locator sets (IPv4 and IPv6) for each Node ID so WSS and RPC can bind sessions to identity rather than a single address. Optional follow-on TLVs assign incoming label maps (ILMs) to target Forwarding Equivalence Classes (FECs).
Session Initialization:
BGP-4: Exchanges PPID 18 OPEN messages on Stream 0, negotiating BGP Capabilities and address family mappings for Streams 1–4.
WSS-SCTP: Transmits a JSON CONNECT frame on Stream 0 with PPID 58. The server responds with 201 CREATED and allocates Stream Range 1024..1039.
Multi-Service Data Execution:
DNS Lookups: Dispatched on Streams 64–1023 using PPID 100 with the U bit set to 1. If a packet drop occurs, it does not delay WebSocket or BGP data.
Routed RPC Calls: An RPC client encapsulates a 24-byte RPC header (PPID 70) into a LABEL_DATA chunk (0xF0). Intermediate mesh nodes swap labels based on ILM tables without parsing the RPC header.
Real-Time Web Streaming: Browser clients stream high-frequency sensor readings on Stream 1024 using PPID 60 with R_{max}=0. Under loss, outdated sensor packets are discarded instantly.
7.2 IPv4/IPv6 Tracking and Gossip IP Change
WSS-SCTP and SCTP RPC capture dual-stack locators (IPv4 and IPv6) as session metadata keyed by Node ID. Stream allocations, XIDs, and WebSocket readyState do not depend on a specific address.
GLS-SCTP GOSSIP_CONTROL (Chunk Type 0xF1) is the overlay IP-change plane. When a WSS or RPC endpoint gains, loses, or promotes an IPv4 or IPv6 address, it pushes an IP_CHANGE TLV on Stream 0 immediately. Receivers update locator tables and NHLFE next-hop IPs. They MUST NOT re-handshake WSS CONNECT or drop RPC XIDs.
SCTP ASCONF remains association-local (add-ip / delete-ip / set-primary on the existing peer). Gossip is fabric-wide: nodes that are not on that association learn the new locator so they can open or retarget paths.
BGP Stream 1 (IPv4) and Stream 2 (IPv6) continue to carry unicast NLRI. They are not the WSS/RPC locator channel.
8. Multi-Homing Failover & Path Resilience
UMS-SCTP relies on transport-native SCTP multi-homing to provide fault tolerance across all embedded application protocols.
       Primary Path [IP_A1 <-> IP_B1] (ACTIVE)
     +-----------------------------------------+
     |                                         |
[ Endpoint A ]                             [ Endpoint B ]
     |                                         |
     +-----------------------------------------+
      Secondary Path [IP_A2 <-> IP_B2] (HEARTBEAT Only -> FAST REROUTE)


Continuous Path Validation:
The underlying SCTP stack sends HEARTBEAT and HEARTBEAT-ACK chunks across secondary IP paths (IP_A2 to IP_B2) at configured intervals.
Instant Transport Failover:
If Path.Max.Retrans threshold is exceeded on the primary interface, SCTP marks IP_A1 as INACTIVE.
Future DATA chunks for all sub-systems (BGP updates, RPC requests, DNS queries, WSS messages) are immediately re-routed to IP_A2.
Application Transparency:
BGP-4: The peer Finite State Machine stays in Established state without resetting timers.
WSS-SCTP: Browser client WebSockets remain open without executing application-level re-handshakes.
SCTP RPC: Pending Transaction IDs (XIDs) remain valid; responses return over the new active path.
GLS-SCTP: Pre-computed Backup NHLFE entries activate instantly, pushing alternate label stacks to bypass failed links.
9. Security Considerations
Unified Encryption Layer: All UMS-SCTP associations operating over public networks MUST deploy DTLS 1.3 over SCTP (RFC 6083) or IPsec (RFC 4301).
SCTP-AUTH Integrity: Endpoints MUST enforce SCTP Authentication (RFC 4895) for critical control chunks, specifically preventing malicious injection of 0xF1 GOSSIP_CONTROL or ASCONF dynamic address reconfigurations.
DoS Mitigation: The standard SCTP 4-way cookie handshake defers memory allocations on state initialization, shielding all high-layer services (BGP, DNS, RPC, WSS) from SYN/INIT flooding attacks.
Label Validation & Stack Limits: Intermediate GLS nodes MUST enforce strict boundary checks on LABEL_DATA chunks (Type 0xF0), dropping frames with \text{Stack Depth} > 16 or invalid S-bit framing to prevent buffer overflow exploits.
10. IANA & Protocol Registration Summary
This specification requires unified reservation of PPIDs and Chunk Types under standard registries:
SCTP Payload Protocol Identifiers (PPID) Registry:
+---------+--------------------------+--------------------+
| Value   | Identifier Name          | Target Sub-System  |
+---------+--------------------------+--------------------+
| 18      | BGP (Control)            | BGP-4              |
| 58      | Payload-WSS-Control      | WSS-SCTP           |
| 59      | Payload-WSS-Text         | WSS-SCTP           |
| 60      | Payload-WSS-Binary       | WSS-SCTP           |
| 70      | SCTP_RPC                 | SCTP RPC v1        |
| 78      | BGP-Common-Update        | BGP-4              |
| 79      | BGP-Multiprotocol        | BGP-4              |
| 80      | BGP-Bulk-Telemetry       | BGP-4              |
| 81      | BGP-Frag-Continue        | BGP-4              |
| 100     | DoSCTP_PAYLOAD           | DoSCTP             |
| 120     | PROXY_CONTROL            | PXY v1             |
| 121     | PROXY_DATA               | PXY v1             |
| 130     | WTTY                     | WASMTTY            |
+---------+--------------------------+--------------------+

SCTP Standard and Experimental Chunk Types Registry:
+---------+--------------------------+--------------------+
| Value   | Chunk Name               | Target Sub-System  |
+---------+--------------------------+--------------------+
| 0x03    | SACK                     | FLOW v1            |
| 0xF0    | LABEL_DATA               | GLS-SCTP           |
| 0xF1    | GOSSIP_CONTROL (IP_CHANGE) | GLS-SCTP         |
+---------+--------------------------+--------------------+


