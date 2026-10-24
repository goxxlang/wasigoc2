Network Working Group J. Doe, Ed. Internet-Draft GLS-SCTP Design Group Intended status: Experimental September 15, 2026 Expires: March 19, 2027
Gossip-Routed Label-Switched SCTP (GLS-SCTP) Protocol draft-doe-tsvwg-gls-sctp-00.txt
Abstract
This document specifies the Gossip-Routed Label-Switched Stream Control Transmission Protocol (GLS-SCTP), an experimental protocol extension to the Stream Control Transmission Protocol (SCTP, RFC 4960). GLS-SCTP integrates epidemic gossip-based control-plane path discovery with explicit label-switched packet forwarding natively within SCTP associations. By leveraging SCTP multi-streaming and multi-homing, GLS-SCTP decouples control-plane topology propagation from high- throughput data-plane forwarding. This design provides resilient, decentralized path routing, rapid multi-homed fast-reroute (FRR), and flexible stream multiplexing over arbitrary mesh overlays.
Status of This Memo
This Internet-Draft is submitted in full conformance with the provisions of BCP 78 and BCP 79.
Internet-Drafts are working documents of the Internet Engineering Task Force (IETF). Note that other groups may also distribute working documents as Internet-Drafts. The list of current Internet- Drafts is at https://datatracker.ietf.org/drafts/current/.
Internet-Drafts are draft documents valid for a maximum of six months and may be updated, replaced, or obsoleted by other documents at any time. It is inappropriate to use Internet-Drafts as reference material or to cite them other than as "work in progress."
Copyright Notice
Copyright (c) 2026 IETF Trust and the persons identified as the document authors. All rights reserved.
Table of Contents
Introduction . . . . . . . . . . . . . . . . . . . . . . . . . 2 1.1. Motivation and Problem Statement . . . . . . . . . . . . . 2 1.2. Key Architectural Goals . . . . . . . . . . . . . . . . . 2 1.3. Terminology . . . . . . . . . . . . . . . . . . . . . . . 3
Protocol Architecture . . . . . . . . . . . . . . . . . . . . . 3 2.1. Control Plane / Data Plane Separation . . . . . . . . . . 3 2.2. SCTP Stream Allocation . . . . . . . . . . . . . . . . . . 3
Chunk Formats and Encoding . . . . . . . . . . . . . . . . . . 4 3.1. LABEL_DATA Chunk (Type 0xF0) . . . . . . . . . . . . . . . 4 3.2. GOSSIP_CONTROL Chunk (Type 0xF1) . . . . . . . . . . . . . 5
Control Plane Operations . . . . . . . . . . . . . . . . . . . 6 4.1. Epidemic State Dissemination . . . . . . . . . . . . . . . 6 4.2. Forwarding Equivalence Class (FEC) Mapping . . . . . . . . 6 4.3. Convergence and Loop Prevention . . . . . . . . . . . . . 7
Data Plane Operations . . . . . . . . . . . . . . . . . . . . . 7 5.1. Ingress Processing: Label Push . . . . . . . . . . . . . . 7 5.2. Intermediate Processing: Label Swap . . . . . . . . . . . 8 5.3. Egress Processing: Label Pop . . . . . . . . . . . . . . . 8
Multi-Homing and Fast Reroute (FRR) . . . . . . . . . . . . . . 8 6.1. Path Failure Detection via SCTP Heartbeats . . . . . . . . 8 6.2. Local Repair and Backup Label Stacking . . . . . . . . . . 9
Security Considerations . . . . . . . . . . . . . . . . . . . . 9 7.1. Threat Model . . . . . . . . . . . . . . . . . . . . . . . 9 7.2. Gossip Message Authentication . . . . . . . . . . . . . . 9 7.3. Frame Parsing and Buffer Boundary Verification . . . . . 10
IANA Considerations . . . . . . . . . . . . . . . . . . . . . . 10 8.1. SCTP Chunk Types . . . . . . . . . . . . . . . . . . . . . 10 8.2. Reserved Stream IDs . . . . . . . . . . . . . . . . . . . 10
References . . . . . . . . . . . . . . . . . . . . . . . . . . 10 9.1. Normative References . . . . . . . . . . . . . . . . . . . 10 9.2. Informative References . . . . . . . . . . . . . . . . . . 11
Introduction
1.1. Motivation and Problem Statement
Modern distributed overlay networks require flexible routing, fine- grained traffic engineering, and robust resilience against sudden link or node failures. Traditional Multi-Protocol Label Switching (MPLS, RFC 3031) relies on centralized or state-heavy signaling protocols such as the Label Distribution Protocol (LDP) or Resource Reservation Protocol with Traffic Engineering (RSVP-TE). In ad-hoc, multi-path, or dynamically changing overlay topologies, maintaining explicit LDP sessions introduces unacceptable control-plane convergence delays and single-point-of-failure vulnerabilities.
Conversely, epidemic gossip protocols excel at resilient, scale-free state synchronization across unreliable network meshes but lack standard primitives for high-performance explicit path forwarding.
The Stream Control Transmission Protocol (SCTP, RFC 4960) provides built-in multi-streaming, multi-homing, reliable or partially reliable delivery, and dynamic transport-layer congestion control. GLS-SCTP combines these primitives by embedding a lightweight MPLS-like label swapping data plane inside SCTP chunks while employing a low-overhead gossip protocol on a dedicated control stream for label mapping and topology distribution.
1.2. Key Architectural Goals
Decoupled Control and Data Planes: Control messages (gossip topology vectors and label bindings) operate on dedicated SCTP streams independently from payload streams.
Native Multi-Homing Exploitation: Path failovers occur at the transport layer without requiring re-signaling or global routing convergence.
Label-Switched Payload Chunks: Payload encapsulation uses stackable label headers directly within custom SCTP chunks.
Decentralized Autonomy: No centralized controller, LDP, or RSVP-TE daemon is required.
1.3. Terminology
The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "NOT RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in BCP 14 [RFC2119] [RFC8174] when, and only when, they appear in all capitals, as shown here.
In addition, the following terms are used in this document:
GLS Node: An endpoint or intermediate system supporting GLS-SCTP. FEC (Forwarding Equivalence Class): A group of IP packets or SCTP data chunks forwarded over the same path with the same handling. ILM (Incoming Label Map): Data structure mapping inbound label/stream pairs to outbound next-hop operations. NHLFE (Next Hop Label Forwarding Entry): Data structure defining label operations (PUSH, SWAP, POP) and egress interface/stream bindings.
Protocol Architecture
2.1. Control Plane / Data Plane Separation
GLS-SCTP establishes an explicit separation between control signaling and data delivery over a single or multi-peer mesh of SCTP associations:
+-----------------------------------------------------------------+ | GLS-SCTP Layer | | | | +-----------------------+ +-----------------------+ | | | Control Plane | | Data Plane | | | | (Epidemic Gossip Engine) | (Label Swap Engine) | | | +-----------+-----------+ +-----------+-----------+ | | | | | | v v | | Stream 0 (Reliable) Streams 1..N (Payload) | +---------------+---------------------------------+---------------+ | SCTP Transport Layer | | (Multi-Homing, Path Heartbeats, Congestion Control) | +-----------------------------------------------------------------+
2.2. SCTP Stream Allocation
SCTP Associations in a GLS-SCTP fabric allocate streams as follows:
Stream ID 0: MUST be reserved strictly for GLS Control Plane gossip messages (GOSSIP_CONTROL chunks). Data on Stream 0 MUST be sent with ordered, reliable delivery (SCTP SACK mandated).
Stream IDs 1 to 65535: Assigned to Data Plane payload delivery using LABEL_DATA chunks. These streams MAY utilize SCTP Partial Reliability Extension (PR-SCTP, RFC 3758) depending on user payload QoS profiles.
Chunk Formats and EncodingGLS-SCTP introduces two new SCTP Chunk Types defined under the IANA experimental range.
3.1. LABEL_DATA Chunk (Type 0xF0)
The LABEL_DATA chunk encapsulates label stacks and user payloads across switched data paths.
0 1 2 3 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Type = 0xF0 | Chunk Flags | Chunk Length | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Stack Depth | Reserved (MBZ)| Stream Sequence Number | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Label Entry 1 | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | ... | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Label Entry N | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | | / User Payload Data / | | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Chunk Fields:
Type (8 bits): 0xF0 identifying LABEL_DATA.
Chunk Flags (8 bits): Reserved for fragmentation and ordering control: * Bit 0 (U bit): Unordered bit (1 = Unordered, 0 = Ordered). * Bit 1 (B bit): Beginning fragment bit. * Bit 2 (E bit): Ending fragment bit.
Chunk Length (16 bits): Total size of chunk in bytes, including Header, Label Entries, and Payload.
Stack Depth (8 bits): Unsigned integer indicating the number of 32-bit Label Entries currently in the label stack (1 \le \text{Stack Depth} \le 16).
Reserved (8 bits): Must Be Zero (MBZ) on transmission; ignored on reception.
Stream Sequence Number (16 bits): Sequential identifier for payload reassembly within the stream.
Label Entry Format (32 bits per entry):
0 1 2 3 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Label Value (20 bits) | TC |S| TTL | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Label Value (20 bits): Unique routing identifier within the local peer domain.
TC (Traffic Class, 3 bits): Class of Service / QoS priority.
S (Bottom of Stack, 1 bit): Set to 1 for the innermost label; 0 otherwise.
TTL (Time to Live, 8 bits): Hop limit; decremented at each SWAP operation.
3.2. GOSSIP_CONTROL Chunk (Type 0xF1)
Gossip state exchange occurs via GOSSIP_CONTROL chunks delivered on Stream 0.
0 1 2 3 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Type = 0xF1 | Chunk Flags | Chunk Length | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Gossip Version | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Originator Node ID | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Sequence Number | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ / Gossip Vector Payload / +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Gossip Vector Payload contains TLV (Type-Length-Value) encoded elements. The primary gossip job is IPv4/IPv6 locator change: a node identity is stable, its addresses are not.

Required TLV:
IP_CHANGE — dual-stack locator mutation for an Originator Node ID (ADD, DELETE, SET_PRIMARY, REPLACE). WSS-SCTP and SCTP RPC consume this TLV to retarget sessions without CONNECT or XID reset.

Optional TLVs:
Node Reachability Matrices
FEC-to-Label Allocation Mappings (Local Inbound Label Assignments)
Link Performance Metrics (Latency, Jitter, Packet Loss Rate)

3.2.1 IP_CHANGE TLV
0 1 2 3 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Type = IP_CHANGE | Length | Flags | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Family | Op | Addr Count | Reserved | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Address[0] (4 octets IPv4 or 16 octets IPv6, network order) | / ... / +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Family: 4 = IPv4, 6 = IPv6. Mixed dual-stack updates MUST be two TLVs in the same GOSSIP_CONTROL chunk (one IPv4, one IPv6), sharing the chunk Sequence Number.
Op: 0x01 ADD, 0x02 DELETE, 0x03 SET_PRIMARY, 0x04 REPLACE (atomic drop of the previous set for that family, then install Address[0..N-1]).
Addr Count: number of addresses in this TLV. REPLACE with Addr Count = 0 withdraws the family.
Flags bit 0 (AUTH): MUST be 1; unauthenticated IP_CHANGE MUST be dropped.
A receiver updates the Node ID locator table, then the association path set and any NHLFE next-hop IP for that node. WSS and RPC MUST NOT close streams or drop pending XIDs on a successful IP_CHANGE.
Control Plane Operations
4.1. Epidemic State Dissemination
GLS-SCTP employs an anti-entropy epidemic gossip algorithm. IP_CHANGE is event-driven and MUST be pushed immediately (not deferred to the next gossip timer) so WSS/RPC locators converge before Path.Max.Retrans on the old address.
Periodicity: Every T_{\text{gossip}} interval (default: 500 ms), each node selects a random subset k (k = \lceil \log N \rceil) of active SCTP associations and anti-entropies IP_CHANGE high-watermarks (plus optional topology/label TLVs).
State Synchronization: The node transmits a GOSSIP_CONTROL summary vector containing high-watermark sequence numbers of its known locator state.
Reconciliation: Upon receipt, the remote peer compares vectors and requests missing IP_CHANGE TLVs via a delta pull request or pushes newer TLVs.
Local triggers that MUST emit IP_CHANGE: interface address add/remove, NAT mapping change observed on a WSS or RPC association, SCTP ASCONF add-ip/delete-ip/set-primary, and dual-stack family appearance or withdrawal.
4.2. Forwarding Equivalence Class (FEC) Mapping
Nodes allocate local labels for destinations or stream classes and gossip these bindings across the overlay network:
\text{Binding TLV} = \langle \text{FEC\_Prefix}, \text{Assigned\_Label}, \text{Node\_ID}, \text{Path\_Cost} \rangle
When Node B advertises label L_B for FEC F, upstream Node A records L_B in its Next-Hop Label Forwarding Entry (NHLFE) table for egress toward B.
4.3. Convergence and Loop Prevention
Path costs are computed using a composite metric incorporating hop count, latency, and SCTP path congestion status:
C_{\text{path}} = \sum_{i \in \text{hops}} \left( \alpha \cdot \text{RTT}_i + \beta \cdot \text{Loss}_i + \gamma \right)
Loop prevention is ensured via two complementary mechanisms:
Gossip Path Vectoring: Gossip updates carry an ordered path vector of visited Node IDs. Updates containing a receiving node's own ID are immediately discarded.
Data-Plane TTL Enforcement: Intermediate nodes decrement the top label's TTL field. Chunks with \text{TTL} = 0 are dropped and trigger an error log.
Data Plane Operations
5.1. Ingress Processing: Label Push
When an application submits data to an ingress GLS node for destination D:
The node queries its Forwarding Table to determine the matching FEC.
The NHLFE returns the corresponding label stack [L_1, L_2, \dots, L_k].
The ingress node constructs a LABEL_DATA chunk (Type 0xF0), sets Stack Depth to k, prepends the label entries with appropriate S-bits, and pushes the chunk onto the designated outbound SCTP stream.
5.2. Intermediate Processing: Label Swap
When an intermediate GLS node receives a LABEL_DATA chunk on inbound stream S_{in}:
Inspect top Label L_{in}.
Lookup L_{in} in the Incoming Label Map (ILM).
If action is SWAP($L_{out}$):
Replace L_{in} with L_{out}.
Decrement TTL: \text{TTL} \leftarrow \text{TTL} - 1.
Forward the chunk over outbound stream S_{out} to the corresponding next-hop SCTP association.
5.3. Egress Processing: Label Pop
If the ILM lookup returns action POP:
The top label is stripped, and Stack Depth is decremented by 1.
If S = 1 (Bottom of Stack reached):
The entire label header is removed.
The inner user payload is delivered to the target local application stream.
If Penultimate Hop Popping (PHP) is enabled, the egress-adjacent node strips the label and forwards a plain payload chunk to the destination endpoint.
Multi-Homing and Fast Reroute (FRR)
6.1. Path Failure Detection via SCTP Heartbeats
GLS-SCTP delegates primary physical path health detection to SCTP transport mechanisms:
SCTP HEARTBEAT and HEARTBEAT ACK chunks monitor secondary/alternate IP addresses for each multi-homed association.
If an IP path fails to respond after Path.Max.Retrans threshold, SCTP marks the path as INACTIVE.
6.2. Local Repair and Backup Label Stacking
When an active path is declared INACTIVE:
The local forwarding engine detects the transport path transition instantly.
Pre-computed Backup NHLFE entries (established via gossip path vectoring) are activated without sending control-plane teardown messages.
Inbound chunks matching affected labels are immediately rewritten with a pre-calculated bypass label stack and rerouted over an alternate active SCTP IP path binding.
+-----------------------------------------------------------------+ | SCTP Multi-Homed Node | | | | Primary Path: [192.0.2.1] ---> [FAILED] | | | | | v (Instant Transport Switch) | | Backup Path: [198.51.100.1] -> [ACTIVE - Backup Stack Used] | +-----------------------------------------------------------------+
Security Considerations
7.1. Threat Model
GLS-SCTP overlays face threats from both malicious transport-layer actors and compromised intermediate overlay nodes.
Gossip Poisoning: Attackers injecting false label mappings or loop vectors.
Frame Parsing Exploits: Maliciously crafted LABEL_DATA chunks with invalid Stack Depth or length offsets attempting buffer overflows or out-of-bounds reads.
DoS via Label Flooding: Rapid label allocation churn designed to exhaust node ILM/NHLFE memory allocations.
7.2. Gossip Message Authentication
All GOSSIP_CONTROL chunks MUST be authenticated using HMAC-SHA256 signatures or encapsulated within SCTP over DTLS (RFC 6083) or IPsec (RFC 3554).
Unauthenticated gossip packets MUST be dropped immediately prior to parsing TLV structures.
7.3. Frame Parsing and Buffer Boundary Verification
Implementations MUST perform strict input validation on LABEL_DATA chunks:
Verify \text{Chunk Length} \ge 8 + (\text{Stack Depth} \times 4).
Reject any chunk where \text{Stack Depth} > 16.
Ensure the innermost label (S=1) is present within the bounds specified by Stack Depth.
IANA Considerations
8.1. SCTP Chunk Types
This document requests IANA assignment of two experimental SCTP Chunk Types from the "SCTP Chunk Types" registry:
+------------+-------------------------------+---------------+ | Chunk Type | Chunk Name | Reference | +------------+-------------------------------+---------------+ | 0xF0 | LABEL_DATA | Section 3.1 | | 0xF1 | GOSSIP_CONTROL | Section 3.2 | +------------+-------------------------------+---------------+
8.2. Reserved Stream IDs
This document reserves Stream ID 0 in all GLS-SCTP associations exclusively for GOSSIP_CONTROL signaling messages.
References
9.1. Normative References
[RFC2119] Bradner, S., "Key words for use in RFCs to Indicate Requirement Levels", BCP 14, RFC 2119, DOI 10.17487/RFC2119, March 1997.
[RFC4960] Stewart, R., Ed., "Stream Control Transmission Protocol", RFC 4960, DOI 10.17487/RFC4960, September 2007.
[RFC8174] Leiba, B., "Ambiguity of Uppercase vs Lowercase in RFC 2119 Key Words", BCP 14, RFC 8174, DOI 10.17487/RFC8174, May 2017.
9.2. Informative References
[RFC3031] Rosen, E., Viswanathan, A., and R. Callon, "Multiprotocol Label Switching Architecture", RFC 3031, DOI 10.17487/RFC3031, January 2001.
[RFC3758] Stewart, R., Ramalho, M., Xie, Q., Tuexen, M., and P. Conrad, "Stream Control Transmission Protocol (SCTP) Partial Reliability Extension", RFC 3758, DOI 10.17487/RFC3758, May 2004.
[RFC6083] Tuexen, M., Seggelmann, R., and E. Rescorla, "Datagram Transport Layer Security (DTLS) for Stream Control Transmission Protocol (SCTP)", RFC 6083, DOI 10.17487/RFC6083, January 2011.
