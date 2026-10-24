Network Working Group                                         J. Doe
Internet-Draft                                          Independent
Intended status: Experimental                     September 20, 2026
Expires: March 24, 2027


         SCTP Reliability and Flow Control (FLOW), Version 1
                     draft-doe-ums-sctp-flow-01

Abstract

   This document specifies FLOW version 1, the SACK-based reliability and
   flow control mechanism used by every SCTP DATA chunk in a UMS-SCTP
   association: retransmission on loss, a peer-advertised receive window
   (rwnd), and a slow-start / congestion-avoidance send window (cwnd).
   It sits directly below the association's DATA-chunk callers -- BGP,
   DoSCTP, WSS-SCTP, SCTP RPC, and PXY -- so all of them get reliable,
   paced delivery without change. It draws on RFC 9260 sections 6.2 and 7
   but is not a full implementation of them; section 9 states exactly
   where it diverges and why.

Status of This Memo

   This Internet-Draft is submitted in full conformance with the
   provisions of BCP 78 and BCP 79. It is a work in progress and MUST
   NOT be cited as anything else.

Table of Contents

   1. Introduction ................................................... 1
   2. Terminology .................................................... 2
   3. SACK Chunk Format .............................................. 2
   4. Sender Behavior ................................................ 3
   5. Receiver Behavior .............................................. 4
   6. Retransmission .................................................. 4
   7. Congestion Control .............................................. 5
   8. Interaction with INIT / INIT-ACK ................................ 6
   9. Scope and Simplifications ....................................... 6
   10. IANA Considerations ............................................ 7

1. Introduction

   Every earlier UMS-SCTP subsystem document (sctp-proto.md, sctp-wss.md,
   sctp-rpc.md, dosctp.md, sctp-ibgp.md, sctp-proxy.md) assumes SCTP DATA
   chunks simply arrive. Before this document, the reference
   implementation's association layer did not: it advertised a fixed
   a_rwnd in INIT and then ignored it, and it never retransmitted
   anything. A lost UDP datagram under real network conditions -- the
   deployment this stack's transport proxy (sctp-proxy.md) exists for --
   corrupted a stream permanently rather than being recovered.

   FLOW closes that gap at the association level, once, rather than each
   subsystem inventing its own retry logic. (The PXY channel window
   described in sctp-proxy.md section 6 remains; it is a different layer,
   pacing how fast a channel hands bytes to the association in the first
   place, not how the association gets those bytes to the peer.)

2. Terminology

   The key words "MUST", "MUST NOT", "SHOULD", and "MAY" are to be
   interpreted as described in BCP 14.

   - TSN: Transmission Sequence Number, the existing 32-bit per-chunk
     counter every SCTP DATA chunk in this stack already carries.
   - cwnd: the sender's own congestion window, in octets.
   - rwnd: the receiver's advertised buffer capacity, in octets.
   - RTO: retransmission timeout.

3. SACK Chunk Format

   Chunk type 0x03 (SACK), matching RFC 9260's own numbering so the
   existing type-0/1/2 (DATA/INIT/INIT-ACK) sequence extends naturally.

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |  Type = 3     |    Flags      |            Length              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     Cumulative TSN Ack                        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     Advertised Receiver Window                |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Number of Gap Ack Blocks    |   Number of Duplicate TSNs     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |  Gap Ack Block #1 Start (16)  |  Gap Ack Block #1 End (16)     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                   ...
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                  Duplicate TSN #1 (32)                        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                   ...

   A gap ack block [start, end] means every TSN from (Cumulative TSN Ack
   + start) through (Cumulative TSN Ack + end) inclusive has been
   received, though the cumulative point has not reached it -- something
   earlier is still missing. The duplicate TSN list carries TSNs that
   arrived again after already being accounted for, so the sender can
   tell a redundant retransmission from a still-missing one.

4. Sender Behavior

   Every DATA chunk a subsystem sends is handed to the sender side of
   FLOW instead of going straight to the wire. It is transmitted
   immediately if

     bytes_in_flight + chunk_size <= min(cwnd, peer_rwnd)

   and otherwise queued, strictly in the order it was handed in, so wire
   order always matches TSN order. Queued chunks are drained whenever a
   SACK either advances the cumulative point or simply raises peer_rwnd,
   even with no new bytes acked -- a receiver's window reopening is
   enough on its own to let a paused sender resume.

5. Receiver Behavior

   The receiver tracks a single cumulative TSN point plus a set of TSNs
   received above it out of order (the gap set). A chunk is delivered to
   the subsystem it belongs to immediately, in arrival order, whether or
   not it is contiguous with the cumulative point -- FLOW does not hold a
   chunk back waiting for an earlier one to fill a gap. This means it
   adds no reordering or reassembly behavior beyond what the caller
   already had (see section 9).

   A SACK is emitted immediately once two chunks have arrived unacked, or
   as soon as any chunk arrives out of order (the gap set is non-empty),
   and otherwise within kSctpSackDelayMs (200 ms) of the first unacked
   chunk. A duplicate TSN -- already cumulatively acked, or already in the
   gap set -- is dropped without being redelivered, and forces an
   immediate SACK carrying it in the duplicate list.

6. Retransmission

   Two independent triggers cause a retransmission, matching RFC 9260's
   two recovery paths:

   Fast retransmit: a TSN below the highest TSN any gap block reports as
   received -- something the peer has plainly gone past -- is counted as
   "missed" on every subsequent SACK that still shows it missing. At
   kSctpFastRetransmitMisses (3) such reports it is retransmitted at
   once, without waiting on the timer.

   Timeout: a chunk unacknowledged for at least the current RTO is
   retransmitted when Tick() is next called. RTO starts at 1 s and is
   estimated from round-trip samples per RFC 6298 (SRTT / RTTVAR),
   clamped to [200 ms, 60 s], and doubles on every timeout (Karn's
   algorithm: a retransmitted chunk's eventual ack is never used as an
   RTT sample, since it is ambiguous which attempt it acknowledges).

   Retransmission does not stop after any fixed number of attempts. A
   chunk keeps retrying, at the backed-off RTO, indefinitely; see section
   9 for why giving up was deliberately not implemented. Crossing
   kSctpMaxRetransmits (8) attempts is recorded as an observability
   counter only.

7. Congestion Control

   Slow start and congestion avoidance follow the usual shape: cwnd
   starts at 4380 octets (RFC 9260's min(4*MTU, max(2*MTU, 4380)) for a
   1200-octet segment), grows by the acknowledged byte count per ack
   while below ssthresh, and by roughly one segment per window's worth
   acked once at or above it. A loss event -- either trigger in section 6
   -- sets ssthresh to max(cwnd/2, 4*MTU) and cwnd to that same value; a
   timeout additionally restarts cwnd at one segment.

   This is one congestion window for the whole association, not one per
   destination address even though the association's Path list supports
   multihoming for failover. Per-path congestion control is not
   implemented.

8. Interaction with INIT / INIT-ACK

   The a_rwnd field INIT and INIT-ACK already carried, previously fixed
   at 65535 and read by neither side, now carries the real advertised
   window in both directions: SendInit() and the INIT-ACK built in
   RecvInit() fill it from the local FlowController, and RecvInit()
   seeds peer_rwnd from whichever side's message it just decoded --
   which is also how an endpoint that opens with SendInit() learns the
   window of a peer that never had to send its own INIT.

9. Scope and Simplifications

   This is not a complete implementation of RFC 9260 sections 6.2 and 7,
   and the gaps are deliberate rather than oversights:

   No Forward-TSN / partial reliability (RFC 3758 / 8842). A chunk that
   keeps failing forever is retried forever (section 6) rather than
   being abandoned, because abandoning it without a way to tell the
   receiver to skip past the resulting hole would stall the cumulative
   ack point permanently -- and with it every later chunk already queued
   behind it. Retrying forever is the safer default in the absence of
   that mechanism; a future revision that wants to give up on a stream
   deliberately should add Forward-TSN rather than silently drop it here.

   No reorder or reassembly buffer. A chunk is handed to its subsystem
   the moment it arrives, in arrival order, not TSN order. This matches
   what every existing subsystem already assumed before FLOW existed
   (none of them held chunks back for reordering), so it changes nothing
   about their behavior; it does mean FLOW's reliability guarantee is
   "every chunk eventually arrives, and arrives at most once," not "every
   chunk arrives in TSN order."

   No per-chunk bundling. Each chunk -- DATA or SACK -- becomes its own
   packet. Real SCTP implementations bundle several small chunks into one
   packet to amortize header overhead; this one does not, trading some
   efficiency for a simpler one-chunk-per-Transmit() path shared with
   every other chunk type in the stack (INIT, GOSSIP_CONTROL, LABEL_DATA).

   Reentrancy on a synchronous transport. The in-process fabric
   (Stack::Connect) and any other transport that delivers a packet
   synchronously, in the same call stack as the send that produced it,
   can cause a chunk's own transmission to trigger a SACK round trip that
   returns before that chunk has been recorded as outstanding. That SACK
   cannot acknowledge a chunk it does not know exists yet; the chunk is
   acknowledged normally on the next SACK instead. The practical
   consequence, exercised directly in tests/flow/reliability.cc, is an
   occasional spurious retransmission of a chunk that had in fact already
   arrived -- correctly recognized and dropped as a duplicate on the
   receiving end, the same benign event a delayed ack can provoke in real
   TCP or SCTP. This does not occur on a genuinely asynchronous transport
   (a real socket, as used by wns_proxy over UdpLink), where a send never
   has a receive nested inside it.

   No association-level abort. RFC 9260 aborts the whole association
   past certain failure thresholds (Association.Max.Retrans and
   similar). This implementation has no association-teardown concept at
   that layer at all; that decision is left to whatever owns the
   transport (see sctp-proxy.md's flow reclaim and idle timeout for the
   PXY layer's own answer to a dead path).

10. IANA Considerations

   This document requests chunk type assignment 3 (SACK), consistent with
   RFC 9260's own numbering for the chunk type this document's SACK plays
   the same role as.

Author's Address

   J. Doe
   Independent
