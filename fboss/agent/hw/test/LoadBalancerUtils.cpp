/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/test/LoadBalancerUtils.h"

#include <folly/IPAddress.h>

#include "fboss/agent/HwSwitch.h"
#include "fboss/agent/LoadBalancerConfigApplier.h"
#include "fboss/agent/Platform.h"
#include "fboss/agent/hw/switch_asics/HwAsic.h"
#include "fboss/agent/hw/test/HwSwitchEnsemble.h"
#include "fboss/agent/hw/test/HwTestPacketUtils.h"
#include "fboss/agent/packet/PktFactory.h"
#include "fboss/agent/state/LoadBalancer.h"
#include "fboss/agent/state/SwitchState.h"
#include "fboss/agent/test/ResourceLibUtil.h"
#include "folly/MacAddress.h"

#include <folly/gen/Base.h>
#include <sstream>

namespace facebook::fboss::utility {
namespace {
cfg::Fields getHalfHashFields() {
  cfg::Fields hashFields;
  hashFields.ipv4Fields_ref() = std::set<cfg::IPv4Field>(
      {cfg::IPv4Field::SOURCE_ADDRESS, cfg::IPv4Field::DESTINATION_ADDRESS});
  hashFields.ipv6Fields_ref() = std::set<cfg::IPv6Field>(
      {cfg::IPv6Field::SOURCE_ADDRESS, cfg::IPv6Field::DESTINATION_ADDRESS});

  return hashFields;
}

cfg::Fields getFullHashFields() {
  auto hashFields = getHalfHashFields();
  hashFields.transportFields_ref() = std::set<cfg::TransportField>(
      {cfg::TransportField::SOURCE_PORT,
       cfg::TransportField::DESTINATION_PORT});
  return hashFields;
}

cfg::LoadBalancer getHalfHashConfig(
    const Platform* platform,
    cfg::LoadBalancerID id) {
  cfg::LoadBalancer loadBalancer;
  *loadBalancer.id_ref() = id;
  if (platform->getAsic()->isSupported(
          HwAsic::Feature::HASH_FIELDS_CUSTOMIZATION)) {
    *loadBalancer.fieldSelection_ref() = getHalfHashFields();
  }
  *loadBalancer.algorithm_ref() = cfg::HashingAlgorithm::CRC16_CCITT;
  return loadBalancer;
}
cfg::LoadBalancer getFullHashConfig(
    const Platform* platform,
    cfg::LoadBalancerID id) {
  cfg::LoadBalancer loadBalancer;
  *loadBalancer.id_ref() = id;
  if (platform->getAsic()->isSupported(
          HwAsic::Feature::HASH_FIELDS_CUSTOMIZATION)) {
    *loadBalancer.fieldSelection_ref() = getFullHashFields();
  }
  *loadBalancer.algorithm_ref() = cfg::HashingAlgorithm::CRC16_CCITT;
  return loadBalancer;
}
cfg::LoadBalancer getTrunkHalfHashConfig(const Platform* platform) {
  return getHalfHashConfig(platform, cfg::LoadBalancerID::AGGREGATE_PORT);
}
cfg::LoadBalancer getTrunkFullHashConfig(const Platform* platform) {
  return getFullHashConfig(platform, cfg::LoadBalancerID::AGGREGATE_PORT);
}
} // namespace
cfg::LoadBalancer getEcmpHalfHashConfig(const Platform* platform) {
  return getHalfHashConfig(platform, cfg::LoadBalancerID::ECMP);
}
cfg::LoadBalancer getEcmpFullHashConfig(const Platform* platform) {
  return getFullHashConfig(platform, cfg::LoadBalancerID::ECMP);
}

std::vector<cfg::LoadBalancer> getEcmpFullTrunkHalfHashConfig(
    const Platform* platform) {
  return {getEcmpFullHashConfig(platform), getTrunkHalfHashConfig(platform)};
}
std::vector<cfg::LoadBalancer> getEcmpHalfTrunkFullHashConfig(
    const Platform* platform) {
  return {getEcmpHalfHashConfig(platform), getTrunkFullHashConfig(platform)};
}
std::vector<cfg::LoadBalancer> getEcmpFullTrunkFullHashConfig(
    const Platform* platform) {
  return {getEcmpFullHashConfig(platform), getTrunkFullHashConfig(platform)};
}

std::shared_ptr<SwitchState> setLoadBalancer(
    const Platform* platform,
    const std::shared_ptr<SwitchState>& inputState,
    const cfg::LoadBalancer& loadBalancerCfg) {
  return addLoadBalancers(platform, inputState, {loadBalancerCfg});
}

std::shared_ptr<SwitchState> addLoadBalancers(
    const Platform* platform,
    const std::shared_ptr<SwitchState>& inputState,
    const std::vector<cfg::LoadBalancer>& loadBalancerCfgs) {
  if (!platform->getAsic()->isSupported(
          HwAsic::Feature::HASH_FIELDS_CUSTOMIZATION)) {
    // configuring hash is not supported.
    XLOG(WARNING) << "load balancer configuration is not supported.";
    return inputState;
  }
  auto newState{inputState->clone()};
  auto lbMap = newState->getLoadBalancers()->clone();
  for (const auto& loadBalancerCfg : loadBalancerCfgs) {
    auto loadBalancer =
        LoadBalancerConfigParser(platform).parse(loadBalancerCfg);
    if (lbMap->getLoadBalancerIf(loadBalancer->getID())) {
      lbMap->updateLoadBalancer(loadBalancer);
    } else {
      lbMap->addLoadBalancer(loadBalancer);
    }
  }
  newState->resetLoadBalancers(lbMap);
  return newState;
}

void pumpTraffic(
    bool isV6,
    HwSwitch* hw,
    folly::MacAddress dstMac,
    VlanID vlan,
    std::optional<PortID> frontPanelPortToLoopTraffic,
    int hopLimit,
    std::optional<folly::MacAddress> srcMacAddr) {
  folly::MacAddress srcMac(
      srcMacAddr.has_value() ? *srcMacAddr
                             : MacAddressGenerator().get(dstMac.u64HBO() + 1));
  for (auto i = 0; i < 100; ++i) {
    auto srcIp = folly::IPAddress(
        folly::sformat(isV6 ? "1001::{}" : "100.0.0.{}", i + 1));
    for (auto j = 0; j < 100; ++j) {
      auto dstIp = folly::IPAddress(
          folly::sformat(isV6 ? "2001::{}" : "200.0.0.{}", j + 1));
      auto pkt = makeUDPTxPacket(
          hw,
          vlan,
          srcMac,
          dstMac,
          srcIp,
          dstIp,
          10000 + i,
          20000 + j,
          0,
          hopLimit);
      if (frontPanelPortToLoopTraffic) {
        hw->sendPacketOutOfPortSync(
            std::move(pkt), frontPanelPortToLoopTraffic.value());
      } else {
        hw->sendPacketSwitchedSync(std::move(pkt));
      }
    }
  }
}

/*
 * Generate traffic with random source ip, destination ip, source port and
 * destination port. every run will pump same random traffic as random number
 * generator is seeded with constant value. in an attempt to unify hash
 * configurations across switches in network, full hash is considered to be
 * present on all switches. this causes polarization in tests and vendor
 * recommends not to use traffic where source and destination fields (ip and
 * port) are only incremented by 1 but to use somewhat random traffic. however
 * random traffic should be deterministic. this function attempts to provide the
 * deterministic random traffic for experimentation and use in the load balancer
 * tests.
 */
void pumpDeterministicRandomTraffic(
    bool isV6,
    HwSwitch* hw,
    folly::MacAddress intfMac,
    VlanID vlan,
    std::optional<PortID> frontPanelPortToLoopTraffic,
    int hopLimit) {
  static uint32_t count = 0;
  uint32_t counter = 1;

  RandomNumberGenerator srcV4(0, 0, 0xFF);
  RandomNumberGenerator srcV6(0, 0, 0xFFFF);
  RandomNumberGenerator dstV4(1, 0, 0xFF);
  RandomNumberGenerator dstV6(1, 0, 0xFFFF);
  RandomNumberGenerator srcPort(2, 10001, 10100);
  RandomNumberGenerator dstPort(2, 20001, 20100);

  auto intToHex = [](auto i) {
    std::stringstream stream;
    stream << std::hex << i;
    return stream.str();
  };

  auto srcMac = MacAddressGenerator().get(intfMac.u64HBO() + 1);
  for (auto i = 0; i < 1000; ++i) {
    auto srcIp = isV6
        ? folly::IPAddress(folly::sformat("1001::{}", intToHex(srcV6())))
        : folly::IPAddress(folly::sformat("100.0.0.{}", srcV4()));
    for (auto j = 0; j < 100; ++j) {
      auto dstIp = isV6
          ? folly::IPAddress(folly::sformat("2001::{}", intToHex(dstV6())))
          : folly::IPAddress(folly::sformat("200.0.0.{}", dstV4()));

      auto pkt = makeUDPTxPacket(
          hw,
          vlan,
          srcMac,
          intfMac,
          srcIp,
          dstIp,
          srcPort(),
          dstPort(),
          0,
          hopLimit);
      if (frontPanelPortToLoopTraffic) {
        hw->sendPacketOutOfPortSync(
            std::move(pkt), frontPanelPortToLoopTraffic.value());
      } else {
        hw->sendPacketSwitchedSync(std::move(pkt));
      }
      count++;
      if (count % 1000 == 0) {
        XLOG(INFO) << counter << " . sent " << count << " packets";
        counter++;
      }
    }
  }
  XLOG(INFO) << "Sent total of " << count << " packets";
}

void pumpMplsTraffic(
    bool isV6,
    HwSwitch* hw,
    uint32_t label,
    folly::MacAddress intfMac,
    VlanID vlanId,
    std::optional<PortID> frontPanelPortToLoopTraffic) {
  MPLSHdr::Label mplsLabel{label, 0, true, 128};
  std::unique_ptr<TxPacket> pkt;
  for (auto i = 0; i < 100; ++i) {
    auto srcIp = folly::IPAddress(
        folly::sformat(isV6 ? "1001::{}" : "100.0.0.{}", i + 1));
    for (auto j = 0; j < 100; ++j) {
      auto dstIp = folly::IPAddress(
          folly::sformat(isV6 ? "2001::{}" : "200.0.0.{}", j + 1));

      auto frame = isV6 ? utility::getEthFrame(
                              intfMac,
                              intfMac,
                              {mplsLabel},
                              srcIp.asV6(),
                              dstIp.asV6(),
                              10000 + i,
                              20000 + j,
                              vlanId)
                        : utility::getEthFrame(
                              intfMac,
                              intfMac,
                              {mplsLabel},
                              srcIp.asV4(),
                              dstIp.asV4(),
                              10000 + i,
                              20000 + j,
                              vlanId);

      if (isV6) {
        pkt = frame.getTxPacket(hw);
      } else {
        pkt = frame.getTxPacket(hw);
      }

      if (frontPanelPortToLoopTraffic) {
        hw->sendPacketOutOfPortSync(
            std::move(pkt), frontPanelPortToLoopTraffic.value());
      } else {
        hw->sendPacketSwitchedSync(std::move(pkt));
      }
    }
  }
}

template <typename IdT>
bool isLoadBalancedImpl(
    const std::map<IdT, HwPortStats>& portIdToStats,
    const std::vector<NextHopWeight>& weights,
    int maxDeviationPct,
    bool noTrafficOk) {
  auto ecmpPorts = folly::gen::from(portIdToStats) |
      folly::gen::map([](const auto& portIdAndStats) {
                     return portIdAndStats.first;
                   }) |
      folly::gen::as<std::vector<IdT>>();

  auto portBytes = folly::gen::from(portIdToStats) |
      folly::gen::map([](const auto& portIdAndStats) {
                     return *portIdAndStats.second.outBytes__ref();
                   }) |
      folly::gen::as<std::set<uint64_t>>();

  auto lowest = *portBytes.begin();
  auto highest = *portBytes.rbegin();
  XLOG(DBG0) << " Highest bytes: " << highest << " lowest bytes: " << lowest;
  if (!lowest) {
    return !highest && noTrafficOk;
  }
  if (!weights.empty()) {
    auto maxWeight = *(std::max_element(weights.begin(), weights.end()));
    for (auto i = 0; i < portIdToStats.size(); ++i) {
      auto portOutBytes =
          *portIdToStats.find(ecmpPorts[i])->second.outBytes__ref();
      auto weightPercent = (static_cast<float>(weights[i]) / maxWeight) * 100.0;
      auto portOutBytesPercent =
          (static_cast<float>(portOutBytes) / highest) * 100.0;
      auto percentDev = std::abs(weightPercent - portOutBytesPercent);
      // Don't tolerate a deviation of more than maxDeviationPct
      XLOG(INFO) << "Percent Deviation: " << percentDev
                 << ", Maximum Deviation: " << maxDeviationPct;
      if (percentDev > maxDeviationPct) {
        return false;
      }
    }
  } else {
    auto percentDev = (static_cast<float>(highest - lowest) / lowest) * 100.0;
    // Don't tolerate a deviation of more than maxDeviationPct
    XLOG(INFO) << "Percent Deviation: " << percentDev
               << ", Maximum Deviation: " << maxDeviationPct;
    if (percentDev > maxDeviationPct) {
      return false;
    }
  }
  return true;
}

bool isLoadBalanced(
    const std::map<PortID, HwPortStats>& portStats,
    const std::vector<NextHopWeight>& weights,
    int maxDeviationPct,
    bool noTrafficOk) {
  return isLoadBalancedImpl(portStats, weights, maxDeviationPct, noTrafficOk);
}
bool isLoadBalanced(
    const std::map<PortID, HwPortStats>& portStats,
    int maxDeviationPct) {
  return isLoadBalanced(
      portStats, std::vector<NextHopWeight>(), maxDeviationPct);
}

bool isLoadBalanced(
    const std::map<std::string, HwPortStats>& portStats,
    const std::vector<NextHopWeight>& weights,
    int maxDeviationPct,
    bool noTrafficOk) {
  return isLoadBalancedImpl(portStats, weights, maxDeviationPct, noTrafficOk);
}

bool isLoadBalanced(
    const std::map<std::string, HwPortStats>& portStats,
    int maxDeviationPct) {
  return isLoadBalanced(
      portStats, std::vector<NextHopWeight>(), maxDeviationPct);
}

bool isLoadBalanced(
    const std::vector<PortDescriptor>& ecmpPorts,
    const std::vector<NextHopWeight>& weights,
    std::function<std::map<PortID, HwPortStats>(const std::vector<PortID>&)>
        getPortStatsFn,
    int maxDeviationPct,
    bool noTrafficOk) {
  auto portIDs = folly::gen::from(ecmpPorts) |
      folly::gen::map([](const auto& portDesc) {
                   CHECK(portDesc.isPhysicalPort());
                   return portDesc.phyPortID();
                 }) |
      folly::gen::as<std::vector<PortID>>();
  auto portIdToStats = getPortStatsFn(portIDs);
  return isLoadBalanced(portIdToStats, weights, maxDeviationPct, noTrafficOk);
}

bool isLoadBalanced(
    HwSwitchEnsemble* hwSwitchEnsemble,
    const std::vector<PortDescriptor>& ecmpPorts,
    const std::vector<NextHopWeight>& weights,
    int maxDeviationPct,
    bool noTrafficOk) {
  auto getPortStatsFn =
      [&](const std::vector<PortID>& portIds) -> std::map<PortID, HwPortStats> {
    return hwSwitchEnsemble->getLatestPortStats(portIds);
  };
  return isLoadBalanced(
      ecmpPorts, weights, getPortStatsFn, maxDeviationPct, noTrafficOk);
}

bool isLoadBalanced(
    HwSwitchEnsemble* hwSwitchEnsemble,
    const std::vector<PortDescriptor>& ecmpPorts,
    int maxDeviationPct) {
  return isLoadBalanced(
      hwSwitchEnsemble,
      ecmpPorts,
      std::vector<NextHopWeight>(),
      maxDeviationPct);
}

bool pumpTrafficAndVerifyLoadBalanced(
    std::function<void()> pumpTraffic,
    std::function<void()> clearPortStats,
    std::function<bool()> isLoadBalanced,
    int retries) {
  bool loadBalanced = false;
  for (auto i = 0; i < retries && !loadBalanced; i++) {
    clearPortStats();
    pumpTraffic();
    loadBalanced = isLoadBalanced();
  }
  return loadBalanced;
}

} // namespace facebook::fboss::utility
