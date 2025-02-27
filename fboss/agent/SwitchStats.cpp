/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/SwitchStats.h"

#include <folly/Memory.h>
#include "fboss/agent/PortStats.h"

using facebook::fb303::AVG;
using facebook::fb303::RATE;
using facebook::fb303::SUM;

namespace facebook::fboss {

// set to empty string, we'll prepend prefix when fbagent collects counters
std::string SwitchStats::kCounterPrefix = "";

SwitchStats::SwitchStats()
    : SwitchStats(fb303::ThreadCachedServiceData::get()->getThreadStats()) {}

SwitchStats::SwitchStats(ThreadLocalStatsMap* map)
    : trapPkts_(
          makeTLTimeseries(map, kCounterPrefix + "trapped.pkts", SUM, RATE)),
      trapPktDrops_(
          makeTLTimeseries(map, kCounterPrefix + "trapped.drops", SUM, RATE)),
      trapPktBogus_(
          makeTLTimeseries(map, kCounterPrefix + "trapped.bogus", SUM, RATE)),
      trapPktErrors_(
          makeTLTimeseries(map, kCounterPrefix + "trapped.error", SUM, RATE)),
      trapPktUnhandled_(makeTLTimeseries(
          map,
          kCounterPrefix + "trapped.unhandled",
          SUM,
          RATE)),
      trapPktToHost_(
          makeTLTimeseries(map, kCounterPrefix + "host.rx", SUM, RATE)),
      trapPktToHostBytes_(
          makeTLTimeseries(map, kCounterPrefix + "host.rx.bytes", SUM, RATE)),
      pktFromHost_(
          makeTLTimeseries(map, kCounterPrefix + "host.tx", SUM, RATE)),
      pktFromHostBytes_(
          makeTLTimeseries(map, kCounterPrefix + "host.tx.bytes", SUM, RATE)),
      trapPktArp_(
          makeTLTimeseries(map, kCounterPrefix + "trapped.arp", SUM, RATE)),
      arpUnsupported_(
          makeTLTimeseries(map, kCounterPrefix + "arp.unsupported", SUM, RATE)),
      arpNotMine_(
          makeTLTimeseries(map, kCounterPrefix + "arp.not_mine", SUM, RATE)),
      arpRequestsRx_(
          makeTLTimeseries(map, kCounterPrefix + "arp.request.rx", SUM, RATE)),
      arpRepliesRx_(
          makeTLTimeseries(map, kCounterPrefix + "arp.reply.rx", SUM, RATE)),
      arpRequestsTx_(
          makeTLTimeseries(map, kCounterPrefix + "arp.request.tx", SUM, RATE)),
      arpRepliesTx_(
          makeTLTimeseries(map, kCounterPrefix + "arp.reply.tx", SUM, RATE)),
      arpBadOp_(
          makeTLTimeseries(map, kCounterPrefix + "arp.bad_op", SUM, RATE)),
      trapPktNdp_(
          makeTLTimeseries(map, kCounterPrefix + "trapped.ndp", SUM, RATE)),
      ipv6NdpBad_(
          makeTLTimeseries(map, kCounterPrefix + "ipv6.ndp.bad", SUM, RATE)),
      ipv4Rx_(
          makeTLTimeseries(map, kCounterPrefix + "trapped.ipv4", SUM, RATE)),
      ipv4TooSmall_(
          makeTLTimeseries(map, kCounterPrefix + "ipv4.too_small", SUM, RATE)),
      ipv4WrongVer_(makeTLTimeseries(
          map,
          kCounterPrefix + "ipv4.wrong_version",
          SUM,
          RATE)),
      ipv4Nexthop_(
          makeTLTimeseries(map, kCounterPrefix + "ipv4.nexthop", SUM, RATE)),
      ipv4Mine_(makeTLTimeseries(map, kCounterPrefix + "ipv4.mine", SUM, RATE)),
      ipv4NoArp_(
          makeTLTimeseries(map, kCounterPrefix + "ipv4.no_arp", SUM, RATE)),
      ipv4TtlExceeded_(makeTLTimeseries(
          map,
          kCounterPrefix + "ipv4.ttl_exceeded",
          SUM,
          RATE)),
      ipv6HopExceeded_(makeTLTimeseries(
          map,
          kCounterPrefix + "ipv6.hop_exceeded",
          SUM,
          RATE)),
      udpTooSmall_(
          makeTLTimeseries(map, kCounterPrefix + "udp.too_small", SUM, RATE)),
      dhcpV4Pkt_(
          makeTLTimeseries(map, kCounterPrefix + "dhcpV4.pkt", SUM, RATE)),
      dhcpV4BadPkt_(
          makeTLTimeseries(map, kCounterPrefix + "dhcpV4.bad_pkt", SUM, RATE)),
      dhcpV4DropPkt_(
          makeTLTimeseries(map, kCounterPrefix + "dhcpV4.drop_pkt", SUM, RATE)),
      dhcpV6Pkt_(
          makeTLTimeseries(map, kCounterPrefix + "dhcpV6.pkt", SUM, RATE)),
      dhcpV6BadPkt_(
          makeTLTimeseries(map, kCounterPrefix + "dhcpV6.bad_pkt", SUM, RATE)),
      dhcpV6DropPkt_(
          makeTLTimeseries(map, kCounterPrefix + "dhcpV6.drop_pkt", SUM, RATE)),
      addRouteV4_(makeTLTimeseries(map, kCounterPrefix + "route.v4.add", RATE)),
      addRouteV6_(makeTLTimeseries(map, kCounterPrefix + "route.v6.add", RATE)),
      delRouteV4_(
          makeTLTimeseries(map, kCounterPrefix + "route.v4.delete", RATE)),
      delRouteV6_(
          makeTLTimeseries(map, kCounterPrefix + "route.v6.delete", RATE)),
      dstLookupFailureV4_(makeTLTimeseries(
          map,
          kCounterPrefix + "ipv4.dst_lookup_failure",
          SUM,
          RATE)),
      dstLookupFailureV6_(makeTLTimeseries(
          map,
          kCounterPrefix + "ipv6.dst_lookup_failure",
          SUM,
          RATE)),
      dstLookupFailure_(makeTLTimeseries(
          map,
          kCounterPrefix + "ip.dst_lookup_failure",
          SUM,
          RATE)),
      updateState_(makeTLTHistogram(
          map,
          kCounterPrefix + "state_update.us",
          50000,
          0,
          1000000)),
      routeUpdate_(map, kCounterPrefix + "route_update.us", 50, 0, 500),
      bgHeartbeatDelay_(makeTLTHistogram(
          map,
          kCounterPrefix + "bg_heartbeat_delay.ms",
          100,
          0,
          20000,
          AVG,
          50,
          100)),
      updHeartbeatDelay_(makeTLTHistogram(
          map,
          kCounterPrefix + "upd_heartbeat_delay.ms",
          100,
          0,
          20000,
          AVG,
          50,
          100)),
      packetTxHeartbeatDelay_(makeTLTHistogram(
          map,
          kCounterPrefix + "packetTx_heartbeat_delay.ms",
          100,
          0,
          20000,
          AVG,
          50,
          100)),
      lacpHeartbeatDelay_(makeTLTHistogram(
          map,
          kCounterPrefix + "lacp_heartbeat_delay.ms",
          100,
          0,
          20000,
          AVG,
          50,
          100)),
      neighborCacheHeartbeatDelay_(makeTLTHistogram(
          map,
          kCounterPrefix + "neighbor_cache_heartbeat_delay.ms",
          100,
          0,
          20000,
          AVG,
          50,
          100)),
      bgEventBacklog_(makeTLTHistogram(
          map,
          kCounterPrefix + "bg_event_backlog",
          1,
          0,
          200,
          AVG,
          50,
          100)),
      updEventBacklog_(makeTLTHistogram(
          map,
          kCounterPrefix + "upd_event_backlog",
          1,
          0,
          200,
          AVG,
          50,
          100)),
      packetTxEventBacklog_(makeTLTHistogram(
          map,
          kCounterPrefix + "packetTx_event_backlog",
          1,
          0,
          200,
          AVG,
          50,
          100)),
      lacpEventBacklog_(makeTLTHistogram(
          map,
          kCounterPrefix + "lacp_event_backlog",
          1,
          0,
          200,
          AVG,
          50,
          100)),
      neighborCacheEventBacklog_(makeTLTHistogram(
          map,
          kCounterPrefix + "neighborCache_event_backlog",
          1,
          0,
          200,
          AVG,
          50,
          100)),
      linkStateChange_(
          makeTLTimeseries(map, kCounterPrefix + "link_state.flap", SUM)),
      pcapDistFailure_(map, kCounterPrefix + "pcap_dist_failure.error"),
      updateStatsExceptions_(makeTLTimeseries(
          map,
          kCounterPrefix + "update_stats_exceptions",
          SUM)),
      trapPktTooBig_(makeTLTimeseries(
          map,
          kCounterPrefix + "trapped.packet_too_big",
          SUM,
          RATE)),
      LldpRecvdPkt_(
          makeTLTimeseries(map, kCounterPrefix + "lldp.recvd", SUM, RATE)),
      LldpBadPkt_(
          makeTLTimeseries(map, kCounterPrefix + "lldp.recv_bad", SUM, RATE)),
      LldpValidateMisMatch_(makeTLTimeseries(
          map,
          kCounterPrefix + "lldp.validate_mismatch",
          SUM,
          RATE)),
      LldpNeighborsSize_(
          makeTLTimeseries(map, kCounterPrefix + "lldp.neighbors_size", SUM)),
      LacpRxTimeouts_(
          makeTLTimeseries(map, kCounterPrefix + "lacp.rx_timeout", SUM)),
      LacpMismatchPduTeardown_(makeTLTimeseries(
          map,
          kCounterPrefix + "lacp.mismatched_pdu_teardown",
          SUM)),
      MkPduRecvdPkts_(
          makeTLTimeseries(map, kCounterPrefix + "mkpdu.recvd", SUM, RATE)),
      MkPduSendPkts_(
          makeTLTimeseries(map, kCounterPrefix + "mkpdu.send", SUM, RATE)),
      MkPduSendFailure_(makeTLTimeseries(
          map,
          kCounterPrefix + "mkpdu.err.send_failure",
          SUM,
          RATE)),
      MkPduPortNotRegistered_(makeTLTimeseries(
          map,
          kCounterPrefix + "mkpdu.err.port_not_regd",
          SUM,
          RATE)),
      MKAServiceSendFailure_(makeTLTimeseries(
          map,
          kCounterPrefix + "mka_service.err.send_failure",
          SUM,
          RATE)),
      MKAServiceSendSuccess_(makeTLTimeseries(
          map,
          kCounterPrefix + "mka_service.send",
          SUM,
          RATE)),
      MKAServiceRecvSuccess_(makeTLTimeseries(
          map,
          kCounterPrefix + "mka_service.recvd",
          SUM,
          RATE)),
      pfcDeadlockDetectionCount_(makeTLTimeseries(
          map,
          kCounterPrefix + "pfc_deadlock_detection",
          SUM)),
      pfcDeadlockRecoveryCount_(
          makeTLTimeseries(map, kCounterPrefix + "pfc_deadlock_recovery", SUM)),
      threadHeartbeatMissCount_(makeTLTimeseries(
          map,
          kCounterPrefix + "thread_heartbeat_miss",
          SUM)) {}

PortStats* FOLLY_NULLABLE SwitchStats::port(PortID portID) {
  auto it = ports_.find(portID);
  if (it != ports_.end()) {
    return it->second.get();
  }
  // Since PortStats needs portName from current switch state, let caller to
  // decide whether it needs createPortStats function.
  return nullptr;
}

AggregatePortStats* FOLLY_NULLABLE
SwitchStats::aggregatePort(AggregatePortID aggregatePortID) {
  auto it = aggregatePortIDToStats_.find(aggregatePortID);

  return it == aggregatePortIDToStats_.end() ? nullptr : it->second.get();
}

PortStats* SwitchStats::createPortStats(PortID portID, std::string portName) {
  auto rv = ports_.emplace(
      portID, std::make_unique<PortStats>(portID, portName, this));
  DCHECK(rv.second);
  const auto& it = rv.first;
  return it->second.get();
}

AggregatePortStats* SwitchStats::createAggregatePortStats(
    AggregatePortID id,
    std::string name) {
  auto [it, inserted] = aggregatePortIDToStats_.emplace(
      id, std::make_unique<AggregatePortStats>(id, name));
  CHECK(inserted);

  return it->second.get();
}

} // namespace facebook::fboss
