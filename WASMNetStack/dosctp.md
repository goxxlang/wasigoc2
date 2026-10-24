Network Working Group                                         J. Doe
Internet-Draft                                          Independent
Intended status: Standards Track                      March 15, 2026
Expires: September 16, 2026


              DNS over Stream Control Transmission Protocol (DoSCTP)
                         draft-ietf-dnsop-dox-sctp-00

Abstract

   This document specifies the encapsulation and operational parameters for
   transporting Domain Name System (DNS) messages over the Stream Control
   Transmission Protocol (SCTP). By utilizing SCTP, DNS over SCTP (DoSCTP)
   eliminates head-of-line (HoL) blocking across concurrent queries, avoids
   the need for artificial byte-framing prefixes, and provides native transport
   multihoming resilience for DNS infrastructure. Security mechanisms via
   Datagram Transport Layer Security (DTLS) over SCTP are also defined.

Status of This Memo

   This Internet-Draft is submitted in full conformance with the provisions of
   BCP 78 and BCP 79.

   Internet-Drafts are working documents of the Internet Engineering Task
   Force (IETF). Note that other groups may also distribute working documents
   as Internet-Drafts. The list of current Internet-Drafts is at
   https://datatracker.ietf.org/drafts/current/.

   Internet-Drafts are draft documents valid for a maximum of six months and
   may be updated, replaced, or obsoleted by other documents at any time. It
   is inappropriate to use Internet-Drafts as reference material or to cite
   them other than as "work in progress."

Copyright Notice

   Copyright (c) 2026 IETF Trust and the persons identified as the document
   authors. All rights reserved.

Table of Contents

   1. Introduction .................................................... 2
   2. Terminology .................................................... 2
   3. Protocol Architecture & Transport Binding ....................... 3
      3.1. SCTP Payload Protocol Identifier (PPID) .................... 3
      3.2. Message Framing and Boundary Preservation .................. 3
      3.3. Stream Demultiplexing and Mapping ......................... 3
      3.4. Unordered Delivery (Head-of-Line Blocking Mitigation) ...... 4
   4. Security Considerations ........................................ 4
      4.1. Security Layer Binding via DTLS over SCTP .................. 4
      4.2. SCTP-AUTH Extension Support ............................... 5
      4.3. Amplification and Reflection Defense ....................... 5
   5. Multihoming and Session Management .............................. 5
      5.1. Association Setup and Path Validation ..................... 5
      5.2. Transparent Failover and Heartbeat Monitoring ............. 6
   6. IANA Considerations ............................................ 6
      6.1. SCTP PPID Allocation ...................................... 6
      6.2. Port Assignment ........................................... 6
   7. References ..................................................... 7
      7.1. Normative References ...................................... 7
      7.2. Informative References .................................... 7
   Authors' Addresses ................................................. 8

1. Introduction

   Traditional DNS transports rely either on UDP [RFC1035], which lacks congestion
   control and multi-path resilience, or TCP [RFC7766], which suffers from
   head-of-line (HoL) blocking under packet loss and requires dynamic stream
   multiplexing layers such as HTTP/2 or HTTP/3 (DoH/DoH3).

   The Stream Control Transmission Protocol (SCTP) [RFC9260] provides a message-oriented,
   multi-streamed, and multihomed transport service natively within the transport
   layer. This document defines DNS over SCTP (DoSCTP), establishing a standard mapping
   for DNS wire-format messages directly over SCTP association streams.

2. Terminology

   The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD",
   "SHOULD NOT", "RECOMMENDED", "NOT RECOMMENDED", "MAY", and "OPTIONAL" in this
   document are to be interpreted as described in BCP 14 [RFC2119] [RFC8174] when,
   and only when, they appear in all capitals, as shown here.

   - DoSCTP: DNS over Stream Control Transmission Protocol.
   - Association: An SCTP connection between two endpoints.
   - Stream: A uni-directional or bi-directional logical channel within an SCTP association.
   - PPID: Payload Protocol Identifier used in SCTP DATA chunks.

3. Protocol Architecture & Transport Binding

3.1. SCTP Payload Protocol Identifier (PPID)

   DoSCTP DATA chunks MUST contain a designated Payload Protocol Identifier (PPID)
   assigned by IANA for DNS payload demultiplexing. A network stack or middlebox
   MUST NOT inspect upper-layer DNS headers to determine protocol type; the PPID
   serves as the explicit protocol indicator.

3.2. Message Framing and Boundary Preservation

   Unlike DNS over TCP [RFC7766] which requires a 2-octet length field preceding
   each DNS message to establish boundaries within a continuous byte stream,
   DoSCTP leverages native SCTP message framing. 

   Each DNS message MUST be encapsulated directly into one or more SCTP DATA chunks
   where the Beginning ('B') and Ending ('E') flags delineate the boundaries of the
   DNS wire-format payload. The 2-octet length prefix MUST NOT be present in DoSCTP
   payloads.

3.3. Stream Demultiplexing and Mapping

   SCTP associations support up to $2^{16}-1$ independent streams. DoSCTP endpoints MAY
   assign concurrent DNS query-response transactions to distinct SCTP streams. 

   - Stream 0 IS RESERVED for control operations and dynamic capability negotiations.
   - Streams $1 \dots N$ MAY be assigned dynamically to client queries.
   - An endpoint MAY multiplex multiple sequential queries over a single stream, or assign
     each pending query to a unique stream ID to enforce strict transaction separation.

3.4. Unordered Delivery (Head-of-Line Blocking Mitigation)

   To maximize throughput and minimize latency in lossy environments, DoSCTP endpoints
   SHOULD set the Unordered flag ('U' bit) to 1 on SCTP DATA chunks containing independent
   DNS queries or responses. 

   When the 'U' bit is set, the receiving SCTP stack bypasses stream sequence ordering,
   delivering the DNS message immediately to the application layer. This completely
   eliminates head-of-line blocking between independent DNS queries.

4. Security Considerations

4.1. Security Layer Binding via DTLS over SCTP

   To provide confidentiality, authentication, and replay protection, DoSCTP MUST
   be secured using Datagram Transport Layer Security (DTLS) over SCTP as specified
   in [RFC6083].



+-------------------------------------------------------+ | DNS Query / Response | +-------------------------------------------------------+ | DTLS Security Layer (RFC 6083) | +-------------------------------------------------------+ | SCTP Transport (Multihomed, Unordered Streams) | +-------------------------------------------------------+ | IPv4 / IPv6 | +-------------------------------------------------------+
When DTLS over SCTP is negotiated:
- The DTLS record layer handles encryption and integrity verification across streams.
- The underlying SCTP association maintains stream separation and transport framing.

4.2. SCTP-AUTH Extension Support

Endpoints MUST support the SCTP Authentication extension [RFC4895] to prevent
third-party chunk injection and unauthorized association reconfiguration (such
as malicious dynamic address reconfiguration via ASCONF).

4.3. Amplification and Reflection Defense

SCTP incorporates a 4-way handshake using `INIT`, `INIT-ACK`, `COOKIE-ECHO`, and
`COOKIE-ACK` chunks. Memory allocations on the server are deferred until verification
of the state cookie returned by the client in `COOKIE-ECHO`. Consequently, DoSCTP is
inherently immune to UDP-style IP spoofing and amplification reflection vectors.

5. Multihoming and Session Management

5.1. Association Setup and Path Validation

Endpoints establish an association via the standard SCTP 4-way handshake. Each
endpoint MAY supply multiple IPv4 and/or IPv6 addresses in the `INIT` or `INIT-ACK`
chunk.

Path validation and reachability for all listed destination addresses are managed
continuously by the transport layer using SCTP `HEARTBEAT` and `HEARTBEAT-ACK` chunks.

5.2. Transparent Failover and Heartbeat Monitoring

If a primary network interface or path fails:
1. The SCTP stack automatically re-routes pending and future DATA chunks to an
   established secondary IP address.
2. Ongoing DNS queries DO NOT suffer connection resets or require TLS/DTLS
   re-handshakes.
3. The application-layer DNS client experiences only transport retransmission
   delay rather than full session reconstruction.

6. IANA Considerations

6.1. SCTP PPID Allocation

IANA is requested to assign a dedicated SCTP Payload Protocol Identifier (PPID)
from the "SCTP PPID" registry:

+----------------+--------------------------+---------------+
| Value          | SCTP PPID Description    | Reference     |
+----------------+--------------------------+---------------+
| TBD1           | DNS over SCTP (DoSCTP)   | [This-Doc]    |
+----------------+--------------------------+---------------+

6.2. Port Assignment

IANA is requested to assign a service name and transport port for DoSCTP from the
"Service Name and Transport Protocol Port Number Registry":

- Service Name: `dns-sctp`
- Transport Protocol: `sctp`
- Assignee: IESG
- Contact: IETF Chair
- Description: DNS over SCTP
- Reference: [This-Doc]
- Port Number: TBD2 (e.g., 53 or 853)

7. References

7.1. Normative References

[RFC1035] Mockapetris, P., "Domain names - implementation and specification",
          STD 13, RFC 1035, DOI 10.17487/RFC1035, November 1987.

[RFC2119] Bradner, S., "Key words for use in RFCs to Indicate Requirement
          Levels", BCP 14, RFC 2119, DOI 10.17487/RFC2119, March 1997.

[RFC4895] Tuexen, M., Stewart, R., Lei, P., and E. Rescorla, "Authenticated
          Chunks for the Stream Control Transmission Protocol (SCTP)",
          RFC 4895, DOI 10.17487/RFC4895, August 2007.

[RFC6083] Tuexen, M., Seggelmann, R., and E. Rescorla, "Datagram Transport
          Layer Security (DTLS) for Stream Control Transmission Protocol (SCTP)",
          RFC 6083, DOI 10.17487/RFC6083, January 2011.

[RFC8174] Leiba, B., "Ambiguity of Uppercase vs Lowercase in RFC 2119 Key
          Words", BCP 14, RFC 8174, DOI 10.17487/RFC8174, May 2017.

[RFC9260] Stewart, R., Tuexen, M., and K. Poon, "Stream Control Transmission
          Protocol", RFC 9260, DOI 10.17487/RFC9260, June 2022.

7.2. Informative References

[RFC7766] Dickinson, J., Dickinson, S., Bellis, R., Mankin, A., and D.
          Wessels, "DNS Transport over TCP - Implementation Requirements",
          RFC 7766, DOI 10.17487/RFC7766, March 2016.

[RFC7858] Hu, Z., Zhu, L., Heidemann, J., Mankin, A., Wessels, D., and C.
          Sisalem, "Specification for DNS over Transport Layer Security (TLS)",
          RFC 7858, DOI 10.17487/RFC7858, May 2016.

Authors' Addresses

J. Doe
Independent
Email: j.doe@example.com


