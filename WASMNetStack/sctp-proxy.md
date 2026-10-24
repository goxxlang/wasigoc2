Network Working Group                                         J. Doe
Internet-Draft                                          Independent
Intended status: Experimental                     September 20, 2026
Expires: March 24, 2027


        Transport Proxy Channels over SCTP (PXY), Version 1
                     draft-doe-ums-sctp-proxy-01

Abstract

   This document specifies PXY version 1, a channel multiplexing protocol
   that carries TCP connections and UDP flows across a single SCTP
   association. An ingress node accepts local TCP connections or UDP
   datagrams, maps each flow to a PXY channel bound to one SCTP stream, and
   an egress node terminates that channel against the requested target.
   The result is a TCP/UDP to SCTP to TCP/UDP datapath in which the overlay
   association, not the client, owns multihoming, path classification, and
   locator changes.

Status of This Memo

   This Internet-Draft is submitted in full conformance with the provisions
   of BCP 78 and BCP 79. It is a work in progress and MUST NOT be cited as
   anything else.

Table of Contents

   1. Introduction ................................................... 1
   2. Terminology .................................................... 2
   3. Transport Binding .............................................. 2
   4. Frame Format ................................................... 3
   5. Channel Lifecycle .............................................. 4
   6. Flow Control ................................................... 5
   7. UDP Mapping .................................................... 6
   8. Egress Policy .................................................. 6
   9. Operational Notes and Limitations .............................. 7
   10. IANA Considerations ........................................... 7

1. Introduction

   A UMS-SCTP association already multiplexes routing, naming, web socket,
   and RPC traffic over one multihomed transport. Applications that speak
   plain TCP or UDP cannot use it without a shim. PXY is that shim: it
   presents an ordinary socket on the ingress node and reproduces it on the
   egress node, while the bytes in between ride SCTP streams.

   PXY is deliberately smaller than SOCKS5 or CONNECT. There is no
   authentication exchange, no address negotiation round trip beyond the
   OPEN, and no per-flow connection setup on the wire: an association that
   is already up carries a new channel in a single frame.

2. Terminology

   The key words "MUST", "MUST NOT", "REQUIRED", "SHOULD", "SHOULD NOT",
   and "MAY" are to be interpreted as described in BCP 14.

   - Channel: one proxied TCP connection or UDP flow, identified by a
     16-bit channel id that is unique within the association.
   - Ingress: the node that accepts the local socket and sends OPEN.
   - Egress: the node that reaches the target on the ingress node's behalf.
   - Target: the host and port the channel terminates against, carried as
     a literal address or as a name the egress resolves.

3. Transport Binding

3.1. Stream Allocation

   PXY occupies stream identifiers 16384 through 32767, carved from the
   upper half of the former SCTP RPC v1 pool. The RPC pool is therefore
   8192 through 16383 in this revision.

   Channel setup and teardown (OPEN, OPEN_ACK, CLOSE, ACK) ride stream 0,
   the UMS control dispatcher, so that the control plane for a channel is
   ordered against the rest of the association's control traffic.

   Payload (DATA, DGRAM) rides the channel's own stream. The stream is
   derived from the channel id and carried explicitly in every frame:

     stream = 16384 + ((channel & 0x7FFF) mod 8192) * 2 + lane

   where lane is 0 for channels opened by the client half of the
   association and 1 for those opened by the server half. A responder MUST
   set bit 15 of every channel id it allocates, so both ends may open
   channels concurrently without collision and without negotiation.

3.2. Payload Protocol Identifiers

   PPID 120 (PROXY_CONTROL) marks channel control frames on stream 0.
   PPID 121 (PROXY_DATA) marks payload frames on a proxy stream.

   A receiver MUST demultiplex on the PPID first and on the stream
   identifier only as a fallback, so an implementation that renumbers its
   stream blocks stays interoperable.

4. Frame Format

   Every PXY frame begins with a 16-octet header in network byte order.

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |    Version    |     Type      |           Channel             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     Flags     |    Proto      |           Stream              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       Sequence Number                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        Payload Length                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        Payload (variable)                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Version is 1. Channel MUST NOT be zero. Proto carries the IP protocol
   number of the proxied transport: 6 for TCP, 17 for UDP.

   Type takes these values:

     0x01 OPEN       payload is a Target
     0x02 OPEN_ACK   flags is a status, payload is the bound Target
     0x03 DATA       payload is stream bytes
     0x04 DGRAM      payload is a Target then datagram bytes
     0x05 CLOSE      flags is a reason
     0x06 ACK        payload is a 64-bit consumed byte count

   Sequence Number orders DATA, DGRAM and CLOSE within one direction of one
   channel. It MUST NOT be interpreted for OPEN, OPEN_ACK or ACK: OPEN and
   OPEN_ACK each happen at most once and need no ordering of their own; ACK
   carries an already-cumulative watermark (section 6) that is safe to
   apply out of order. Each side of a channel keeps its own counter for the
   frames it sends, starting at 0 with the first DATA, DGRAM or CLOSE after
   OPEN or OPEN_ACK and incrementing by one per frame after that; the two
   directions of one channel are numbered independently, the same way a
   TCP connection's two directions each have their own sequence space.

   A receiver MUST hold a DATA, DGRAM or CLOSE frame whose Sequence Number
   is ahead of the next one it expects for that direction, and deliver it
   only once every earlier one has arrived and been delivered. This is not
   optional: the association below (sctp-flow.md) delivers whatever stream
   a frame rides on arrival order, not send order, and OPEN's control
   traffic (stream 0) and a channel's own data (its own stream) can
   plainly race each other over a transport that reorders packets, even
   though a well-behaved sender emits them in the right order to begin
   with. Held frames resolve on their own once sctp-flow.md's reliability
   delivers what they were waiting on; a receiver MUST NOT time out or
   give up on a gap.

   A Target is encoded as:

     +---------+-----------------+---------+------------------+
     | Kind(8) |     Port(16)    | Len(8)  |  Address (Len)   |
     +---------+-----------------+---------+------------------+

   Kind 4 carries a 4-octet IPv4 address, kind 6 a 16-octet IPv6 address,
   and kind 0 a UTF-8 host name of Len octets. A name MUST be resolved by
   the egress node, which keeps the ingress node from having to see, or to
   be trusted with, the resolution.

   OPEN_ACK status values: 0 ok, 1 refused, 2 unreachable, 3 forbidden by
   policy, 4 out of resources, 5 protocol error.

   CLOSE reason values: 0 end of data (half close), 1 reset, 2 timeout.

5. Channel Lifecycle

   The ingress node allocates a channel id, sends OPEN on stream 0, and
   holds the local socket unread until OPEN_ACK arrives. That hold is the
   only admission backpressure in the protocol, and an implementation MUST
   apply it: bytes read before the target is known have nowhere to go.

   The egress node reaches the target, then answers with OPEN_ACK. A
   non-zero status closes the channel immediately; the ingress node MUST
   then drop the local socket rather than leaving a client connected to
   nothing.

   For a TCP channel, CLOSE with reason 0 is a half close: the sender will
   write no more, the reverse direction stays open, and the receiver SHOULD
   propagate it as a FIN on its own socket. The channel ends when both
   directions have half closed, or at once on reason 1 or 2.

   Propagating that FIN MUST wait for every byte already handed to this
   channel to actually leave the local socket first. A non-blocking local
   socket can accept a write into an application-level buffer without that
   buffer draining immediately; issuing the FIN before it drains discards
   whatever was still queued; the receiver has no way to detect that
   silent loss, since from its side the connection simply closed a little
   short. An implementation MUST defer the FIN until its own local send
   buffer for that direction is empty, not merely until CLOSE has been
   received and acted on.

   For a UDP channel there is no half close. CLOSE in either direction ends
   the channel, and an idle channel SHOULD be reclaimed by the ingress node
   after a local timeout.

6. Flow Control

   A TCP channel is windowed. The sender MUST NOT have more than W
   unacknowledged bytes in flight, where W defaults to 131072. The receiver
   emits ACK carrying the cumulative count of bytes it has handed
   downstream, and SHOULD do so once per quarter window to keep the control
   stream quiet.

   The count is bytes handed downstream, not bytes received. An egress node
   whose target socket has stopped draining therefore stops acknowledging,
   the ingress window shuts, the ingress node stops reading its client, and
   the client's own TCP stack applies backpressure at the far edge. The
   window is what makes the chain end to end.

   Payload is segmented so that one DATA frame plus the SCTP and outer
   headers fits a common path MTU; 1200 octets is the default segment.

   UDP channels are not windowed.

7. UDP Mapping

   An ingress node maps each distinct local source address and port to its
   own channel, and replies to exactly that source. The DGRAM target field
   carries the datagram's peer: on the ingress side it is the channel
   target, and on the egress side it is the source the reply actually came
   from, so a resolver that answers from a different address stays visible
   to the client.

   Datagrams a client sends before OPEN_ACK arrives SHOULD be held and
   flushed once the channel opens, bounded by a small queue. A one-shot
   client, such as a stub resolver that sends a single query and waits,
   otherwise never receives an answer.

8. Egress Policy

   An egress node MUST refuse every OPEN unless it has been configured to
   serve them. A node that does serve them SHOULD carry an allow list of
   targets, matched on host and port, with port 0 matching every port on a
   host, and SHOULD be able to refuse TCP or UDP wholesale.

   Nothing in PXY authenticates the ingress node. The association's own
   admission -- who is allowed to establish it and what locators it accepts
   -- is the trust boundary; an unauthenticated association that serves
   egress is an open relay.

9. Operational Notes and Limitations

   PXY assumes the association below it delivers what it accepts. The
   windowing in Section 6 bounds what is in flight, but this revision of
   the underlying stack performs no retransmission: a packet lost on the
   path is lost to the channel, and a TCP channel that loses a segment is
   corrupt rather than delayed. PXY is therefore specified for associations
   over paths that do not drop, pending SACK and retransmission at the SCTP
   layer.

   A channel carries no idle keepalive of its own. Liveness is the
   association's business, and a channel outlives locator changes on either
   end because it is bound to a node id, not to an address.

10. IANA Considerations

   This document requests PPID assignments 120 (PROXY_CONTROL) and 121
   (PROXY_DATA), and records the stream block 16384-32767 in the UMS-SCTP
   stream allocation registry.

Author's Address

   J. Doe
   Independent
