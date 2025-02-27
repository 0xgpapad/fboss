// Copyright 2004-present Facebook. All Rights Reserved.

#include "fboss/agent/hw/switch_asics/Tomahawk4Asic.h"

DECLARE_int32(acl_gid);

namespace {
// On TH4, LOGICAL_TABLE_ID is 4 bit which will give 16 groups per pipe.
// From IFP point of view the device operate in 4 Pipes which will get
// 4*16 = 64 groups.
// However in older devices LOGICAL_TABLE_ID is 5 bit which will give you
// 128 groups.
// However SDK reserves Group 64, to update the group qset even when entries
// already installed in the group.
// So 63 is the largest group id we can get.
constexpr auto kDefaultACLGroupID = 63;
} // namespace

namespace facebook::fboss {

bool Tomahawk4Asic::isSupported(Feature feature) const {
  switch (feature) {
    case HwAsic::Feature::SPAN:
    case HwAsic::Feature::ERSPANv4:
    case HwAsic::Feature::SFLOWv4:
    case HwAsic::Feature::MPLS:
    case HwAsic::Feature::MPLS_ECMP:
    case HwAsic::Feature::ERSPANv6:
    case HwAsic::Feature::SFLOWv6:
    case HwAsic::Feature::HOT_SWAP:
    case HwAsic::Feature::HASH_FIELDS_CUSTOMIZATION:
    case HwAsic::Feature::QUEUE:
    case HwAsic::Feature::ECN:
    case HwAsic::Feature::L3_QOS:
    case HwAsic::Feature::SCHEDULER_PPS:
    case HwAsic::Feature::NEXTHOP_TTL_DECREMENT_DISABLE:
    case HwAsic::Feature::DEBUG_COUNTER:
    case HwAsic::Feature::RESOURCE_USAGE_STATS:
    case HwAsic::Feature::HSDK:
    case HwAsic::Feature::OBJECT_KEY_CACHE:
    case HwAsic::Feature::L3_EGRESS_MODE_AUTO_ENABLED:
    case HwAsic::Feature::PKTIO:
    case HwAsic::Feature::ACL_COPY_TO_CPU:
    case HwAsic::Feature::INGRESS_FIELD_PROCESSOR_FLEX_COUNTER:
    case HwAsic::Feature::OBM_COUNTERS:
    case HwAsic::Feature::BUFFER_POOL:
    case HwAsic::Feature::EGRESS_QUEUE_FLEX_COUNTER:
    case HwAsic::Feature::INGRESS_L3_INTERFACE:
    case HwAsic::Feature::DETAILED_L2_UPDATE:
    case HwAsic::Feature::TELEMETRY_AND_MONITORING:
    case HwAsic::Feature::ALPM_ROUTE_PROJECTION:
    case HwAsic::Feature::MAC_AGING:
    case HwAsic::Feature::SAI_PORT_SPEED_CHANGE: // CS00011784917
    case HwAsic::Feature::SFLOW_SHIM_VERSION_FIELD:
    case HwAsic::Feature::EGRESS_MIRRORING:
    case HwAsic::Feature::EGRESS_SFLOW:
    case HwAsic::Feature::DEFAULT_VLAN:
    case HwAsic::Feature::L2_LEARNING:
    case HwAsic::Feature::SAI_ACL_ENTRY_SRC_PORT_QUALIFIER:
    case HwAsic::Feature::TRAFFIC_HASHING:
    case HwAsic::Feature::ACL_TABLE_GROUP:
    case HwAsic::Feature::CPU_PORT:
    case HwAsic::Feature::VRF:
    case HwAsic::Feature::SAI_HASH_FIELDS_CLEAR_BEFORE_SET:
    case HwAsic::Feature::ROUTE_COUNTERS:
    case HwAsic::Feature::ROUTE_FLEX_COUNTERS:
    case HwAsic::Feature::BRIDGE_PORT_8021Q:
    case HwAsic::Feature::FEC_DIAG_COUNTERS:
    case HwAsic::Feature::PTP_TC:
    case HwAsic::Feature::PTP_TC_PCS:
      return true;
    // features only supported by B0 version, or any physical device
    // where used chip is always B0.
    case HwAsic::Feature::NON_UNICAST_HASH:
    case HwAsic::Feature::WEIGHTED_NEXTHOPGROUP_MEMBER:
      return getAsicMode() != AsicMode::ASIC_MODE_SIM || isSimB0();
    // features not working well with bcmsim
    case HwAsic::Feature::MIRROR_PACKET_TRUNCATION:
    case HwAsic::Feature::SFLOW_SAMPLING:
      return getAsicMode() != AsicMode::ASIC_MODE_SIM;
    case HwAsic::Feature::HOSTTABLE_FOR_HOSTROUTES:
    case HwAsic::Feature::QOS_MAP_GLOBAL:
    case HwAsic::Feature::QCM:
    case HwAsic::Feature::SMAC_EQUALS_DMAC_CHECK_ENABLED:
    case HwAsic::Feature::PORT_TTL_DECREMENT_DISABLE:
    case HwAsic::Feature::PORT_INTERFACE_TYPE:
    case HwAsic::Feature::SAI_ECN_WRED:
    case HwAsic::Feature::SWITCH_ATTR_INGRESS_ACL: // CS00011272352
    case HwAsic::Feature::HOSTTABLE:
    case HwAsic::Feature::PORT_TX_DISABLE:
    case HwAsic::Feature::ZERO_SDK_WRITE_WARMBOOT:
    case HwAsic::Feature::PENDING_L2_ENTRY:
    case HwAsic::Feature::PFC:
    case HwAsic::Feature::COUNTER_REFRESH_INTERVAL:
    case HwAsic::Feature::WIDE_ECMP:
    case HwAsic::Feature::REMOVE_PORTS_FOR_COLDBOOT: // CS00012066057
    case HwAsic::Feature::MACSEC:
    case HwAsic::Feature::SAI_MPLS_QOS:
    case HwAsic::Feature::EMPTY_ACL_MATCHER:
    case HwAsic::Feature::SAI_PORT_SERDES_FIELDS_RESET:
    case HwAsic::Feature::MULTIPLE_ACL_TABLES:
    case HwAsic::Feature::SAI_WEIGHTED_NEXTHOPGROUP_MEMBER:
    case HwAsic::Feature::SAI_ACL_TABLE_UPDATE:
    case HwAsic::Feature::PORT_EYE_VALUES:
    case HwAsic::Feature::SAI_MPLS_TTL_1_TRAP:
    case HwAsic::Feature::SAI_MPLS_LABEL_LOOKUP_FAIL_COUNTER:
    case HwAsic::Feature::SAI_SAMPLEPACKET_TRAP:
      return false;

    case HwAsic::Feature::SAI_LAG_HASH:
#if defined(SAI_VERSION_6_0_0_14_ODP)
      return true;
#else
      return false;
#endif
  }
  return false;
}

int Tomahawk4Asic::getDefaultACLGroupID() const {
  if (FLAGS_acl_gid > 0) {
    return FLAGS_acl_gid;
  } else {
    return kDefaultACLGroupID;
  }
}

int Tomahawk4Asic::getStationID(int intfId) const {
  int stationId = intfId;
  // station id should be smaller than 511 on tomahawk4
  if (intfId >= 4000) {
    stationId = intfId - 4000 + 400; // 400, 401, 402, ...
  } else if (intfId >= 2000) {
    stationId = intfId - 2000 + 200; // 200, 201, 202, ...
  } else if (intfId >= 1000) {
    // kBaseVlanId used in ConfigFactory for testing purpose is 1000
    stationId = intfId - 1000 + 100; // 100, 101, 102, ...
  }
  return stationId;
}

int Tomahawk4Asic::getNumLanesPerPhysicalPort() const {
  /*
    In each Blackhawk7 core, there are 4 phyiscal ports and (up to) 4 logical
    ports but 8 physical lanes. Therefore, when calculating the physical_port of
    bcm_port_resource_t when using flexing port logic, we need to use
    numLanesPerPhysicalPort to divide physical lanes, which is learned from
    PlatformMapping.
  */
  return 2;
}

int Tomahawk4Asic::getDefaultNumPortQueues(cfg::StreamType streamType, bool cpu)
    const {
  // 12 logical queues in total, same as tomahawk3
  switch (streamType) {
    case cfg::StreamType::UNICAST:
      if (cpu) {
        break;
      }
      return 8;
    case cfg::StreamType::MULTICAST:
      return cpu ? 10 : 4;
    case cfg::StreamType::ALL:
      break;
  }
  throw FbossError(
      "Unexpected, stream: ", streamType, " cpu: ", cpu, "combination");
}
} // namespace facebook::fboss
