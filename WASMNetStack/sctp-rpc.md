Network Working Group J. Doe, Ed. Internet-Draft September 2026 Intended status: Standards Track Expires: March 2027
Stream Control Transmission Protocol (SCTP) Remote Procedure Call (RPC) Protocol Version 1 draft-sctp-rpc-protocol-00.txt
Abstract
This document specifies the Stream Control Transmission Protocol (SCTP) Remote Procedure Call (RPC) protocol version 1. SCTP RPC is an application-layer protocol designed to execute remote procedure calls over SCTP transport. By leveraging SCTP's native features—such as multi-streaming, message framing, and multi-homing—this protocol eliminates head-of-line blocking across concurrent RPC transactions and ensures transport-level fault tolerance without requiring complex application-level session re-establishment logic.
Status of This Memo
This Internet-Draft is submitted in full conformance with the provisions of BCP 78 and BCP 79.
Internet-Drafts are working documents of the Internet Engineering Task Force (IETF). Note that other groups may also distribute working documents as Internet-Drafts. The list of current Internet-Drafts is at https://datatracker.ietf.org/drafts/current/.
Internet-Drafts are draft documents valid for a maximum of six months and may be updated, replaced, or obsoleted by other documents at any time. It is inappropriate to use Internet-Drafts as reference material or to cite them other than as "work in progress."
Copyright Notice
Copyright (c) 2026 IETF Trust and the persons identified as the document authors. All rights reserved.
Table of Contents
Introduction . . . . . . . . . . . . . . . . . . . . . . . . 2 1.1. Requirements Language . . . . . . . . . . . . . . . . . 2 1.2. Terminology . . . . . . . . . . . . . . . . . . . . . . 2
Protocol Overview . . . . . . . . . . . . . . . . . . . . . . 2
Message Formats . . . . . . . . . . . . . . . . . . . . . . . 3 3.1. SCTP RPC Frame Header . . . . . . . . . . . . . . . . . 3 3.2. Header Fields . . . . . . . . . . . . . . . . . . . . . 3 3.3. Flags Bitmask . . . . . . . . . . . . . . . . . . . . . 4
Transport Mapping & SCTP Integration . . . . . . . . . . . . 4 4.1. Payload Protocol Identifier (PPID) . . . . . . . . . . 4 4.2. Multi-Streaming & Channel Allocation . . . . . . . . . 5 4.3. Unordered Message Delivery . . . . . . . . . . . . . . 5 4.4. Multi-Homing & Path Resilience . . . . . . . . . . . . 5
RPC Operation Lifecycle . . . . . . . . . . . . . . . . . . . 6 5.1. Request-Response Pattern . . . . . . . . . . . . . . . 6 5.2. One-Way (Oneway) Pattern . . . . . . . . . . . . . . . 6 5.3. Error Handling . . . . . . . . . . . . . . . . . . . . 6
Security Considerations . . . . . . . . . . . . . . . . . . . 7
IANA Considerations . . . . . . . . . . . . . . . . . . . . . 7
References . . . . . . . . . . . . . . . . . . . . . . . . . 7
Introduction
Modern distributed systems rely heavily on Remote Procedure Call (RPC) frameworks for inter-service communication. Existing transports present distinct trade-offs: Transmission Control Protocol (TCP) provides strict ordering and reliability but suffers from Head-of-Line (HoL) blocking when multiplexing independent requests over a single connection; User Datagram Protocol (UDP) avoids HoL blocking but requires the application layer to re-implement sequence tracking, fragmentation, retransmission, and congestion control.
The Stream Control Transmission Protocol (SCTP), defined in RFC 9260, combines the message-oriented nature of UDP with the reliable, congestion- controlled capabilities of TCP, while adding native multi-streaming and multi-homing. This specification defines SCTP RPC Version 1, a lightweight framing protocol that optimizes remote procedure execution over SCTP.
1.1. Requirements Language
The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "NOT RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in BCP 14 [RFC2119] [RFC8174] when, and only when, they appear in all capitals, as shown here.
1.2. Terminology
Association: The SCTP connection between two endpoints.
Stream: A unidirectional logical channel established within an SCTP association.
Transaction Identifier (XID): A unique integer used to correlate a response message with its corresponding request message.
Payload Protocol Identifier (PPID): An integer value carried in the SCTP DATA chunk header indicating the protocol type of the user payload.
Protocol Overview
SCTP RPC operates directly over SCTP DATA chunks. Applications map logical call channels or concurrent worker threads to SCTP stream identifiers. When an RPC client invokes a remote method, it constructs an SCTP RPC frame containing target metadata, dispatches it over a selected SCTP stream, and awaits a response carrying a matching XID.
Key protocol guarantees include:
Elimination of cross-call HoL blocking via SCTP multi-streaming.
Reduced latency for independent requests using unordered delivery (U bit).
Native network path failover without breaking RPC connection context.
Message Formats
All multi-byte numeric fields MUST be transmitted in network byte order (big-endian).
3.1. SCTP RPC Frame Header
An SCTP RPC frame consists of a fixed 24-byte header followed by a variable- length payload.
0 1 2 3 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Version (8) | Flags (8) | Reserved (16) | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | |
Transaction ID (64) + | | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Service ID (32) | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Method ID (32) | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Payload Length (32) | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ | Payload Data (...) | | | +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
3.2. Header Fields
Version (8 bits): Protocol major version. MUST be set to 0x01 for this specification.
Flags (8 bits): Bitfield specifying frame control parameters (see Section 3.3).
Reserved (16 bits): Reserved for future extensions. MUST be set to 0x0000 on transmission and ignored on receipt.
Transaction ID (XID) (64 bits): A monotonically increasing or cryptographically random identifier assigned by the requester. Responses MUST echo the XID of the request.
Service ID (32 bits): Identifier representing the destination service or interface.
Method ID (32 bits): Identifier representing the specific procedure within the target service.
Payload Length (32 bits): Unsigned 32-bit integer indicating the size of the Payload Data field in bytes, excluding the 24-byte header.
Payload Data (Variable): The serialized method parameters (for requests) or return values/errors (for responses). Serialization format (e.g., Protobuf, CBOR) is out of scope for this transport specification.
3.3. Flags Bitmask
The Flags byte is structured as follows:
0 1 2 3 4 5 6 7 +---+---+---+---+---+---+---+---+ |REQ|RES|ONE|ERR| RESERVED | +---+---+---+---+---+---+---+---+
REQ (Bit 0, 0x01): Set if the frame represents an RPC Request.
RES (Bit 1, 0x02): Set if the frame represents an RPC Response.
ONE (Bit 2, 0x04): One-Way / Oneway flag. Indicates that no response frame is expected for this request.
ERR (Bit 3, 0x08): Error flag. Indicates that the payload contains an RPC engine or application error descriptor rather than standard return data.
Bits 4–7: Reserved for future use. MUST be 0.
Transport Mapping & SCTP Integration
4.1. Payload Protocol Identifier (PPID)
Implementations MUST set the SCTP PPID in all DATA chunk headers to the IANA- assigned value allocated for SCTP RPC (suggested value for testing: 70). Receivers MUST drop DATA chunks with unrecognized PPIDs unless explicit backward-compatibility handlers exist.
4.2. Multi-Streaming & Channel Allocation
SCTP associations negotiate a fixed number of inbound and outbound streams during initialization (INIT / INIT ACK).
Concurrent RPC Mapping: Implementations SHOULD distribute active RPC transactions across distinct Stream Identifiers (0 \le S < N_{streams}).
Stream Isolation: Loss of a frame on Stream A MUST NOT block delivery or processing of frames arriving on Stream B.
In-Order Operations: Sequential RPC calls requiring strict FIFO execution MUST be placed on the same Stream Identifier.
4.3. Unordered Message Delivery
For idempotent procedure calls or transactions where sequence ordering is not required, the sender MAY set the unordered delivery bit (U bit) in the SCTP DATA chunk header. This allows the receiving SCTP stack to pass the frame immediately to the application layer without holding it in reordering queues.
4.4. Multi-Homing & Path Resilience
SCTP endpoints MAY bind multiple IP addresses to a single association. If the primary network path fails, SCTP automatically re-routes DATA chunks over an alternate active path. SCTP RPC layers MUST rely on transport-level path heartbeats rather than implementing custom ping/pong heartbeats at the RPC application level.
4.5. IPv4/IPv6 Locator Tracking
SCTP RPC tracks IPv4 and IPv6 locators per Node ID. The RPC session is the SCTP association plus outstanding XIDs; addresses are locators.
Each RPC endpoint MUST retain a locator table: nodeId, ipv4[], ipv6[], and the primary address in use. Locators are learned from INIT/INIT-ACK and updated by GLS-SCTP GOSSIP_CONTROL IP_CHANGE TLVs (Chunk Type 0xF1).
On IP change the RPC layer MUST gossip IP_CHANGE immediately. It MUST NOT mint a new association, MUST NOT invalidate pending XIDs, and MUST NOT rebind Service ID / Method ID routing. Responses continue to echo the request XID on the same Stream ID after the path set is updated.
ASCONF mutates the local association path set. Gossip mutates the overlay locator table so non-adjacent RPC peers can reach the new IPv4 or IPv6 address without an application-level reconnect. Dual-stack REPLACE/DELETE of one family is not an RPC error while the other family remains reachable.
RPC Operation Lifecycle
5.1. Request-Response Pattern
The client selects an outbound SCTP Stream ID and generates a unique 64-bit XID.
The client constructs the header with Flags = 0x01 (REQ), serializes the payload, and transmits the frame using sctp_sendmsg().
The server receives the frame, dispatches the request to the procedure matching (Service ID, Method ID), and constructs a response.
The server transmits the response frame back to the client over the SAME SCTP Stream ID with Flags = 0x02 (RES) and matching XID.
5.2. One-Way (Oneway) Pattern
The client sends a request with Flags = 0x05 (REQ | ONE).
The server processes the request but MUST NOT generate a response frame.
The client immediately frees the pending XID tracking state upon transmission.
5.3. Error Handling
If the server encounters a transport-level serialization failure, unknown method error, or execution exception, it MUST return a response frame with the ERR flag set (Flags = 0x0A, corresponding to RES | ERR).
The payload of an error response frame SHOULD encapsulate a standardized error descriptor (e.g., numeric error code and string message).
Security Considerations
SCTP RPC frames are transmitted in cleartext by default. Applications requiring confidentiality, authentication, and integrity SHOULD run SCTP RPC over Datagram Transport Layer Security (DTLS) over SCTP, as specified in RFC 6083, or use IPsec (RFC 4301).
IANA Considerations
This document requests IANA to assign a dedicated SCTP Payload Protocol Identifier (PPID):
Value: TBD (suggested: 70) ID: SCTP_RPC Reference: RFC-XXXX
References
[RFC2119] Bradner, S., "Key words for use in RFCs to Indicate Requirement Levels", BCP 14, RFC 2119, DOI 10.17487/RFC2119, March 1997.
[RFC6083] Tuexen, M., Seggelmann, R., and E. Rescorla, "Datagram Transport Layer Security (DTLS) for Stream Control Transmission Protocol (SCTP)", RFC 6083, DOI 10.17487/RFC6083, January 2011.
[RFC8174] Leiba, B., "Ambiguity of Uppercase vs Lowercase in RFC 2119 Key Words", BCP 14, RFC 8174, DOI 10.17487/RFC8174, May 2017.
[RFC9260] Stewart, R., Tuexen, M., and K. Poon, "Stream Control Transmission Protocol", RFC 9260, DOI 10.17487/RFC9260, June 2022.
