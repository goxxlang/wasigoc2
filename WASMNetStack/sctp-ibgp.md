BGP-4 Message Transport over Stream Control Transmission Protocol (SCTP)
Abstract
This document specifies the operational and protocol mechanisms for transporting Border Gateway Protocol version 4 (BGP-4) messages over the Stream Control Transmission Protocol (SCTP). BGP-4 traditionally relies on the Transmission Control Protocol (TCP) for reliable stream transport. However, TCP enforces strict byte-stream ordering across a single connection, introducing Head-of-Line (HoL) blocking across independent address families and lacking native multi-homing support at the transport layer. By utilizing SCTP, BGP-4 gains multi-streaming capabilities, explicit message-oriented framing, enhanced association resilience via multi-homing, and logical isolation between distinct Address Family Identifiers (AFIs) and Subsequent Address Family Identifiers (SAFIs). This specification defines the Payload Protocol Identifier (PPID) allocations, stream mapping rules, upper-layer fragmentation handling, and receiver reassembly procedures necessary for executing BGP-4 over SCTP associations.
Status of This Memo
This Internet-Draft is submitted in full conformance with the provisions of BCP 78 and BCP 79. Internet-Drafts are working documents of the Internet Engineering Task Force (IETF). Note that other groups may also distribute working documents as Internet-Drafts. The list of current Internet-Drafts is at https://datatracker.ietf.org/drafts/current/.
Internet-Drafts are draft documents valid for a maximum of six months and may be updated, replaced, or obsoleted by other documents at any time. It is inappropriate to use Internet-Drafts as reference material or to cite them other than as "work in progress."
Copyright Notice
Copyright (c) 2026 IETF Trust and the persons identified as the document authors. All rights reserved.
This document is subject to BCP 78 and the IETF Trust's Legal Provisions Relating to IETF Documents (https://trustee.ietf.org/license-info) in effect on the date of publication of this document. Please review these documents carefully, as they describe your rights and restrictions with respect to this document.
1. Introduction
The Border Gateway Protocol version 4 (BGP-4) serves as the core inter-domain routing protocol across the global Internet infrastructure. Historically, BGP-4 has relied on TCP as its underlying transport layer, binding to well-known destination port 179. While TCP provides reliable ordered delivery, its single sequence space enforces total ordering across all transmitted octets. As Multiprotocol Extensions for BGP-4 (MP-BGP) expanded BGP usage to carry diverse routing and non-routing telemetry data—such as IPv4 Unicast, IPv6 Unicast, Ethernet Virtual Private Networks (EVPN), BGP Link-State (BGP-LS), Deterministic Networking (DetNet) flow mapping, and Flow Specification—a single lost TCP segment stalls the processing of all subsequent Network Layer Reachability Information (NLRI) streams regardless of their address family. This vulnerability, known as Head-of-Line (HoL) blocking, severely degrades convergence times for critical unicast routes during period of link congestion or bulk update dissemination.
Historical architectural proposals, including exploratory specifications such as draft-zhiyfang-fecai-bgp-over-sctp and structural separation concepts like draft-raszuk-ti-bgp, highlighted the operational necessity of decoupling routing-critical updates from non-routing bulk telemetry. The Stream Control Transmission Protocol (SCTP), standardized in RFC 9260, provides a feature set designed to resolve these transport limitations. SCTP natively supports multi-streaming, explicit message-oriented boundaries, transport-level multi-homing for path redundancy, and enhanced protection against denial-of-service state exhaustion through cryptographic cookie handshakes.
This document formalizes the architecture for executing BGP-4 over SCTP transport. By assigning distinct AFIs and SAFIs to independent SCTP streams and utilizing dedicated Payload Protocol Identifiers (PPIDs), network operators can isolate core Internet unicast updates from high-volume extension updates. This approach eliminates cross-family fate sharing, streamlines buffer management, and optimizes overall control plane performance.
2. Requirements Language
The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "NOT RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in BCP 14 [RFC2119] [RFC8174] when, and only when, they appear in all capitals, as shown here.
3. Protocol Architecture and Transport Capabilities
Transporting BGP-4 over SCTP provides explicit operational advantages over traditional TCP transport. The table below contrasts the technical characteristics of TCP and SCTP transport layers in the context of BGP-4 control plane operations.
Transport Feature
Traditional TCP Transport
SCTP Transport (This Specification)
Primary Framing Unit
Unstructured Byte Stream
Explicit Message Chunks
Stream Multiplexing
Single Stream (Subject to HoL Blocking)
Multi-Streaming (Independent Ordering per Stream)
Path Redundancy
Single Path per Connection (Requires Application Failover)
Native Multi-Homing per Association
Connection Handshake
3-Way Handshake (Vulnerable to SYN Flooding)
4-Way Cookie-Based Handshake
Payload Identification
Implicit (Port-Based / Protocol Handshake)
Explicit 32-bit PPID Header Field
Message Interleaving
Not Supported
Supported via RFC 8260 Stream Interleaving

BGP-4 nodes supporting SCTP transport listen for incoming association requests on destination port 179. During the SCTP four-way handshake—comprising INIT, INIT-ACK, COOKIE-ECHO, and COOKIE-ACK chunks—the initiating host requests a number of outbound streams sufficient to allocate dedicated channels for all locally configured address families.
An SCTP implementation supporting BGP-4 enforces standard Verification Tag (VTag) validation rules specified in RFC 9260 to ensure that incoming packets belong to an active association and to prevent insertion attacks. When multi-homing is enabled, both endpoints exchange lists of reachable IPv4 or IPv6 addresses within the INIT and INIT-ACK chunks, permitting dynamic transport path switching without resetting the BGP peering state.
4. Payload Protocol Identifier (PPID) Allocations and Stream Mapping
SCTP DATA chunks contain a mandatory 32-bit Payload Protocol Identifier (PPID) field. The PPID identifies the structure and encoding of the payload contained within the DATA chunk, allowing the receiver to route messages to appropriate upper-layer processing threads.
PPID Value
Mnemonic / Purpose
Description
18
PPID_BGP_CONTROL
Reserved for session control messages: OPEN, KEEPALIVE, NOTIFICATION, and ROUTE-REFRESH.
78
PPID_BGP_COMMON_UPDATE
Standard BGP-4 UPDATE messages carrying legacy IPv4 Unicast routes.
79
PPID_BGP_MULTIPROTOCOL
MP-BGP UPDATE messages containing MP_REACH_NLRI / MP_UNREACH_NLRI attributes.
80
PPID_BGP_BULK_TELEMETRY
Non-routing critical data, large flow specification tables, or internal telemetry.
81
PPID_BGP_FRAG_C[span_43](start_span)[span_43](end_span)ONTINUE
Intermediate fragment of a segmented upper-layer BGP user message.

To eliminate Head-of-Line blocking across independent routing contexts, BGP-4 implementations map session management and specific address families to designated SCTP Stream Identifiers (SIDs). The primary session management tasks and critical control frames are bound to Stream 0.
Specifically, Stream 0 is designated as the Control Stream. All BGP OPEN, KEEPALIVE, NOTIFICATION, and ROUTE-REFRESH messages are transmitted on Stream 0 using PPID_BGP_CONTROL. Unordered delivery is prohibited on Stream 0 to preserve strict state machine sequencing.
Stream 1 is allocated for IPv4 Unicast routing reachability. Standard BGP-4 UPDATE messages conveying legacy IPv4 Unicast NLRI are transmitted on Stream 1 using PPID_BGP_COMMON_UPDATE.
Stream 2 handles IPv6 Unicast reachability. MP-BGP UPDATE messages carrying IPv6 Unicast attributes (AFI 2, SAFI 1) are assigned to Stream 2 using PPID_BGP_MULTIPROTOCOL.
Stream 3 is designated for Layer 2 VPN and EVPN reachability. Updates carrying EVPN NLRI (AFI 25, SAFI 70) are transmitted on Stream 3 using PPID_BGP_MULT[span_46](start_span)[span_46](end_span)IPROTOCOL.
Stream 4 is reserved for BGP Flow Specification rules and Deterministic Networking (DetNet) flow mapping structures. These updates utilize PPID_BGP_MULTIPROTOCOL and remain completely isolated from unicast forwarding tables.
Stream N (where N \ge 5) accommodates non-routing bulk data, internal domain telemetry, or specialized SAFIs. Isolating bulk updates on high-index SIDs with PPID_BGP_BULK_TELEMETRY ensures that high-volume telemetry floods cannot delay critical inter-domain routing updates.
Implementations adhere to this default stream assignment unless an alternative mapping is explicitly negotiated via an extended BGP Stream Mapping Capability exchanged during session initialization on Stream 0.
5. Sender and Receiver Operational Procedures
5.1. Sender Procedures and Upper-Layer Fragmentation
An upper-layer BGP-4 implementation sending messages over SCTP constructs update frames in accordance with the configured stream mapping rules. When a BGP message exceeds the Maximum Segment Size (MSS) or path Maximum Transmission Unit (MTU), relying solely on transport-level SCTP fragmentation can limit message interleaving. To enable stream interleaving across concurrent address families as defined in RFC 8260, an upper-layer sender SHOULD perform upper-layer message fragmentation.
When upper-layer fragmentation is utilized, the sender divides a large BGP user message into multiple fragments sized to avoid transport-level IP fragmentation. For every message fragment except the final one, the sender attaches PPID_BGP_FRAG_CONTINUE within the SCTP DATA chunk header. For the final fragment, the sender attaches the target operational PPID matching the payload type, such as PPID_BGP_MULTIPROTOCOL or PPID_BGP_COMMON_UPDATE. All fragments belonging to a single logical BGP message are transmitted sequentially on the same SCTP stream with ordered delivery enabled. Fragments originating from different address families on distinct streams MAY be freely interleaved at the transport layer, preventing high-volume updates from blocking urgent control frames.
### 5.2. Receiver Processing and Reassembly
A receiving BGP-4 node processes incoming SCTP DATA chunks on a per-stream basis independently. The receiver maintains separate reassembly contexts for each active Stream Identifier. A processing delay or buffer congestion on Stream K MUST NOT impede the reception, decoding, or processing of DATA chunks arriving on Stream J (J \neq K).
Upon receiving a DATA chunk, the receiver inspects the PPID field. If the chunk carries PPID_BGP_FRAG_CONTINUE, the payload is appended to the stream's logical reassembly buffer. When a DATA chunk carrying the final operational PPID arrives, the receiver marks the boundary of the complete BGP message, flushes the reassembly buffer, and passes the fully decoded message to the corresponding BGP finite state machine or Routing Information Base (RIB) process.
If an unrecognized PPID is received, or if a message arrives on an unallocated stream index exceeding local limits, the receiving BGP speaker MUST generate a NOTIFICATION message on Stream 0 carrying the Error Code Cease and Subcode Connection Unsynchronized, followed by terminating the SCTP association.
6. Network Address Translation (NAT) and Multi-Homing Considerations
Deploying BGP-4 over SCTP across Network Address and Port Translation (NAPT) middleboxes requires SCTP-aware NAT capabilities. Standard legacy NAT devices designed exclusively for TCP/UDP fail to translate SCTP headers correctly or maintain state across multi-homed association paths.
An SCTP-aware NAT function maintains state lookup tables mapping internal source IP addresses, internal port numbers, external IP addresses, external port numbers, and both association Verification Tags (VTags). BGP endpoints positioned behind NAT functions MUST validate that incoming chunks reflect the correct external VTags assigned during association setup.
If multiple internal BGP speakers behind a single NAT initiate SCTP associations to the same remote peer using identical source ports, the NAT function resolves port collisions by tracking internal VTags dynamically rather than modifying transport port numbers. This preserves standard port bindings while maintaining association separation. Additionally, when a multi-homed BGP node experiences an interface address modification or failover event, the endpoint utilizes dynamic address reconfiguration chunks to update active path lists without interrupting the active BGP peering state.
7. Operational Security and Route Leak Prevention
Transporting BGP-4 over SCTP preserves all standard BGP path selection semantics and policy enforcement mechanisms. Network operators deploying BGP-4 over SCTP MUST implement route leak prevention mechanisms, including BGP Roles and the Only to Customer (OTC) path attribute defined in RFC 9234.
During session establishment on Stream 0, External BGP (eBGP) peers negotiate BGP Roles describing their contractual relationship, such as Provider, Customer, Peer, Route Server (RS), or RS-Client. The OTC attribute is attached to routes announced to customers, peers, or RS-clients, and MUST be preserved across all transit propagation steps. If an intermediate Autonomous System (AS) strips or drops the OTC attribute, receiving speakers running BGP Roles detect the violation and reject the leaked route, mitigating unintended traffic misdirection.
At the transport layer, SCTP provides native resistance against denial-of-service state exhaustion attacks through its four-way cookie handshake. The receiving host allocates no control blocks or memory structures until the initiating peer verifies its IP address ownership by returning the signed State Cookie in a COOKIE-ECHO chunk. For end-to-end data integrity and peer authentication, operators SHOULD deploy SCTP Authentication (SCTP-AUTH) or Datagram Transport Layer Security (DTLS) over SCTP.
8. IANA Considerations
IANA maintains assignments within the "SCTP Payload Protocol Identifiers" registry and the "Service Name and Transport Protocol Port Number Registry".
### 8.1. SCTP Payload Protocol Identifier Assignments
This specification requests the formal assignment and reservation of the following PPID values within the "SCTP Payload Protocol Identifiers" registry:
Value
Identifier
Reference
18
BGP (Control / Historical)
[RFC9260], [This Document]
78
BGP-Common-Update
[This Document]
79
BGP-Multiprotocol
[This Document]
80
BGP-Bulk-Telemetry
[This Document]
81
BGP-Frag-Continue
[This Document]

8.2. Transport Port Registration
BGP-4 over SCTP utilizes the registered well-known port 179 for SCTP association initiation, consistent with legacy TCP BGP transport assignments.
9. Comparative Operational Evaluation
Deploying BGP-4 over a single SCTP association provides structural performance improvements compared to both single-connection TCP and multi-session TCP deployment models. The table below summarizes key operational parameters across these transport modes.
Parameter
Single-TCP Session
Multi-Session TCP
Single-SCTP Association
Transport Connections
1 TCP Connection
N TCP Connections
1 SCTP Association
BGP Finite State Machines
1 Peer FSM
N Independent Peer FSMs
1 Integrated Peer FSM
Address Family Isolation
None (Shared Byte Queue)
Complete
Complete (Per-Stream Isolation)
Keepalive Processing Overhead
Low (1 Connection)
High (N \times Connection Overhead)
Low (Single Control Stream)
Multi-Homing Path Failover
Application-Driven (Slow)
Application-Driven (Slow)
Transport-Native (Sub-second)
Head-of-Line Blocking
Full Vulnerability
Isolated per Session
Fully Eliminated across Streams

By consolidating multiple logical routing channels into a single SCTP association, network operators achieve complete address family isolation without incurring the state processing, timer management, and keepalive message overhead associated with maintaining multiple parallel TCP sessions. Native multi-homing further enhances session stability by permitting seamless transport path failover in response to underlying physical link failures. Consequently, BGP-4 transport over SCTP establishes a scalable, resilient foundation for modern inter-domain routing architectures.
Works cited
1. draft-zhiyfang-fecai-bgp-over-sctp-00 - Datatracker - IETF, https://datatracker.ietf.org/doc/draft-zhiyfang-fecai-bgp-over-sctp/ 2. RFC 9260 - Stream Control Transmission Protocol - IETF Datatracker, https://datatracker.ietf.org/doc/rfc9260/ 3. RFC 4960: Stream Control Transmission Protocol, https://www.rfc-editor.org/info/rfc4960/ 4. draft-raszuk-ti-bgp-01 - IETF Datatracker, https://datatracker.ietf.org/doc/html/draft-raszuk-ti-bgp-01 5. Payload Protocol Identifier based Fragmentation and Reassembly, https://www.ietf.org/archive/id/draft-tuexen-tsvwg-sctp-ppid-frag-00.html 6. Stream Control Transmission Protocol (SCTP) Network Address, https://www.ietf.org/archive/id/draft-porfiri-tsvwg-sctp-natsupp-03.html 7. BGP Flow Specification for DetNet Flow Mapping - IETF Datatracker, https://datatracker.ietf.org/meeting/112/materials/slides-112-detnet-bgp-flow-specification-for-detnet-flow-mapping-00 8. Protocol Registries - Internet Assigned Numbers Authority, https://www.iana.org/protocols 9. doc: RFC 9260: Stream Control Transmission Protocol - hjp, https://www.hjp.at/doc/rfc/rfc9260.html 10. BGP Role model: tracking the adoption of RFC 9234 - Cloudflare Blog, https://blog.cloudflare.com/rfc9234-bgp-role-model/ 11. draft-ietf-teas-enhanced-vpn-07, https://datatracker.ietf.org/doc/html/draft-ietf-teas-enhanced-vpn-07 12. A YANG Data Model for Network Diagnosis using Scheduled, https://vlopezalvarez.github.io/draft-contreras-opsawg-scheduling-oam-tests/draft-ietf-opsawg-scheduling-oam-tests.html 13. doc: RFC 9341: Alternate-Marking Method - hjp, https://www.hjp.at/(en)/doc/rfc/rfc9341.html 14. Locator/ID Separation Protocol (LISP) Distinguished Name Encoding, https://www.rfc-editor.org/info/rfc9735/ 15. SCTP Log Fields - Palo Alto Networks | TechDocs, https://docs.paloaltonetworks.com/ngfw/administration/monitoring/use-syslog-for-monitoring/syslog-field-descriptions/sctp-log-fields 16. Document Search - IETF Datatracker, https://datatracker.ietf.org/doc/draft 17. IP Flow Information Export (IPFIX) Entities, https://www.iana.org/assignments/ipfix
