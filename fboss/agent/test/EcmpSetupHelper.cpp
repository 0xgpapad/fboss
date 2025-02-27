/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/test/EcmpSetupHelper.h"

#include <iterator>

#include "fboss/agent/RouteUpdateWrapper.h"
#include "fboss/agent/state/AggregatePort.h"
#include "fboss/agent/state/Interface.h"
#include "fboss/agent/state/Port.h"
#include "fboss/agent/state/PortDescriptor.h"
#include "fboss/agent/state/SwitchState.h"
#include "fboss/agent/state/Vlan.h"
#include "fboss/agent/types.h"

#include <folly/IPAddress.h>

using boost::container::flat_map;
using boost::container::flat_set;
using folly::IPAddress;
using folly::IPAddressV4;
using folly::IPAddressV6;
using folly::MacAddress;

namespace {

using namespace facebook::fboss;

std::optional<AggregatePortID> getAggPortID(
    const std::shared_ptr<SwitchState>& inputState,
    const PortID& portId) {
  for (auto aggPort : *inputState->getAggregatePorts().get()) {
    if (aggPort->isMemberPort(portId)) {
      return aggPort->getID();
    }
  }
  return std::nullopt;
}

flat_map<PortDescriptor, VlanID> computePortDesc2Vlan(
    const std::shared_ptr<SwitchState>& inputState) {
  boost::container::flat_map<PortDescriptor, VlanID> portDesc2Vlan;
  flat_set<PortID> portIds;
  for (const auto& port : *inputState->getPorts().get()) {
    portIds.insert(port->getID());
  }
  for (const auto& portId : portIds) {
    PortDescriptor portDesc = PortDescriptor(portId);
    auto aggId = getAggPortID(inputState, portId);
    if (aggId) {
      portDesc = PortDescriptor(*aggId);
    }
    auto port = inputState->getPorts()->getPort(portId);
    for (const auto& vlanMember : port->getVlans()) {
      portDesc2Vlan.insert(std::make_pair(portDesc, VlanID(vlanMember.first)));
    }
  }
  return portDesc2Vlan;
}

flat_map<VlanID, folly::CIDRNetwork> computeVlan2Subnet(
    const std::shared_ptr<SwitchState>& inputState,
    bool v6) {
  boost::container::flat_map<VlanID, folly::CIDRNetwork> vlan2Network;
  for (const auto& intf : *inputState->getInterfaces().get()) {
    for (const auto& cidrStr : intf->getAddresses()) {
      auto subnet = folly::IPAddress::createNetwork(cidrStr.first.str());
      if (!v6 && subnet.first.isV4()) {
        vlan2Network[intf->getVlanID()] = subnet;
      } else if (v6 && subnet.first.isV6() && !subnet.first.isLinkLocal()) {
        vlan2Network[intf->getVlanID()] = subnet;
      }
    }
  }
  return vlan2Network;
}
} // namespace

namespace facebook::fboss::utility {

boost::container::flat_set<PortDescriptor> getPortsWithExclusiveVlanMembership(
    const std::shared_ptr<SwitchState>& state) {
  boost::container::flat_set<PortDescriptor> ports;
  for (auto vlan : *state->getVlans()) {
    auto memberPorts = vlan->getPorts();
    if (memberPorts.size() == 1) {
      ports.insert(PortDescriptor{memberPorts.begin()->first});
    }
  }
  return ports;
}

template <typename AddrT, typename NextHopT>
BaseEcmpSetupHelper<AddrT, NextHopT>::BaseEcmpSetupHelper() {}

template <typename AddrT, typename NextHopT>
std::shared_ptr<SwitchState>
BaseEcmpSetupHelper<AddrT, NextHopT>::resolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    const boost::container::flat_set<PortDescriptor>& portDescs,
    bool useLinkLocal) const {
  return resolveNextHopsImpl(inputState, portDescs, true, useLinkLocal);
}

template <typename AddrT, typename NextHopT>
std::shared_ptr<SwitchState>
BaseEcmpSetupHelper<AddrT, NextHopT>::unresolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    const boost::container::flat_set<PortDescriptor>& portDescs,
    bool useLinkLocal) const {
  return resolveNextHopsImpl(inputState, portDescs, false, useLinkLocal);
}

template <typename AddrT, typename NextHopT>
std::shared_ptr<SwitchState>
BaseEcmpSetupHelper<AddrT, NextHopT>::resolveNextHopsImpl(
    const std::shared_ptr<SwitchState>& inputState,
    const boost::container::flat_set<PortDescriptor>& portDescs,
    bool resolve,
    bool useLinkLocal) const {
  auto outputState{inputState};
  for (auto nhop : nhops_) {
    if (portDescs.find(nhop.portDesc) != portDescs.end()) {
      outputState = resolve ? resolveNextHop(outputState, nhop, useLinkLocal)
                            : unresolveNextHop(outputState, nhop, useLinkLocal);
    }
  }
  return outputState;
}

template <typename AddrT, typename NextHopT>
std::shared_ptr<SwitchState>
BaseEcmpSetupHelper<AddrT, NextHopT>::resolveNextHop(
    const std::shared_ptr<SwitchState>& inputState,
    const NextHopT& nhop,
    bool useLinkLocal) const {
  auto outputState{inputState->clone()};
  auto vlanId = portDesc2Vlan_.find(nhop.portDesc)->second;
  auto vlan = outputState->getVlans()->getVlan(vlanId);
  auto nbrTable = vlan->template getNeighborEntryTable<AddrT>()->modify(
      vlanId, &outputState);
  auto nhopIp = useLinkLocal ? nhop.linkLocalNhopIp.value() : nhop.ip;
  if (nbrTable->getEntryIf(nhop.ip)) {
    nbrTable->updateEntry(
        nhopIp, nhop.mac, nhop.portDesc, vlan->getInterfaceID());
  } else {
    nbrTable->addEntry(nhopIp, nhop.mac, nhop.portDesc, vlan->getInterfaceID());
  }
  return outputState;
}

template <typename AddrT, typename NextHopT>
std::shared_ptr<SwitchState>
BaseEcmpSetupHelper<AddrT, NextHopT>::unresolveNextHop(
    const std::shared_ptr<SwitchState>& inputState,
    const NextHopT& nhop,
    bool useLinkLocal) const {
  auto outputState{inputState->clone()};
  auto vlanId = portDesc2Vlan_.find(nhop.portDesc)->second;
  auto vlan = outputState->getVlans()->getVlan(vlanId);
  auto nbrTable = vlan->template getNeighborEntryTable<AddrT>()->modify(
      vlanId, &outputState);
  auto nhopIp = useLinkLocal ? nhop.linkLocalNhopIp.value() : nhop.ip;
  nbrTable->removeEntry(nhopIp);
  return outputState;
}

template <typename AddrT, typename NextHopT>
std::vector<PortDescriptor> BaseEcmpSetupHelper<AddrT, NextHopT>::ecmpPortDescs(
    int width) const {
  std::vector<PortDescriptor> portDescs;
  for (auto i = 0; i < width; ++i) {
    portDescs.push_back(nhops_[i].portDesc);
  }
  return portDescs;
}

template <typename AddrT, typename NextHopT>
std::optional<VlanID> BaseEcmpSetupHelper<AddrT, NextHopT>::getVlan(
    const PortDescriptor& port) const {
  auto iter = portDesc2Vlan_.find(port);
  if (iter == portDesc2Vlan_.end()) {
    return std::nullopt;
  }
  return iter->second;
}

template <typename IPAddrT>
EcmpSetupTargetedPorts<IPAddrT>::EcmpSetupTargetedPorts(
    const std::shared_ptr<SwitchState>& inputState,
    std::optional<folly::MacAddress> nextHopMac,
    RouterID routerId)
    : BaseEcmpSetupHelper<IPAddrT, EcmpNextHopT>(), routerId_(routerId) {
  computeNextHops(inputState, nextHopMac);
}

template <typename IPAddrT>
void EcmpSetupTargetedPorts<IPAddrT>::computeNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    std::optional<folly::MacAddress> nextHopMac) {
  BaseEcmpSetupHelperT::portDesc2Vlan_ = computePortDesc2Vlan(inputState);
  auto vlan2Subnet =
      computeVlan2Subnet(inputState, BaseEcmpSetupHelperT::kIsV6);
  int offset = 0;
  // Locally administered, unicast MACs
  auto baseMac = folly::MacAddress("06:00:00:00:00:00").u64HBO();
  for (const auto& portDescAndVlan : BaseEcmpSetupHelperT::portDesc2Vlan_) {
    auto vlan = portDescAndVlan.second;
    auto ipAddrStr = vlan2Subnet[vlan].first.str();
    if (ipAddrStr.empty()) {
      // Ignore vlans without ip addresses
      continue;
    }
    auto subnetIp = IPAddrT(ipAddrStr);
    auto bytes = subnetIp.toByteArray();
    // Add a offset to compute next in subnet next hop IP.
    // Essentially for vlan/l3 intf with subnet X, we
    // would compute next hops by incrementing last octet
    // of subnet.
    int lastOctet = bytes[bytes.size() - 1] + (++offset);
    // Fail if we goto 255 at the last oct
    CHECK_GT(255, lastOctet);
    bytes[bytes.size() - 1] = static_cast<uint8_t>(lastOctet);
    BaseEcmpSetupHelperT::nhops_.push_back(EcmpNextHopT(
        IPAddrT(bytes),
        portDescAndVlan.first,
        nextHopMac ? MacAddress::fromHBO(nextHopMac.value().u64HBO())
                   : MacAddress::fromHBO(baseMac + offset),
        InterfaceID(vlan)));
  }
}

template <typename IPAddrT>
typename EcmpSetupTargetedPorts<IPAddrT>::EcmpNextHopT
EcmpSetupTargetedPorts<IPAddrT>::nhop(PortDescriptor portDesc) const {
  auto it = std::find_if(
      BaseEcmpSetupHelperT::nhops_.begin(),
      BaseEcmpSetupHelperT::nhops_.end(),
      [portDesc](const EcmpNextHopT& nh) { return nh.portDesc == portDesc; });
  if (it == BaseEcmpSetupHelperT::nhops_.end()) {
    throw FbossError("Could not find a nhop for: ", portDesc);
  }
  return *it;
}

template <typename IPAddrT>
void EcmpSetupTargetedPorts<IPAddrT>::programRoutes(
    std::unique_ptr<RouteUpdateWrapper> updater,
    const flat_set<PortDescriptor>& portDescriptors,
    const std::vector<RouteT>& prefixes,
    const std::vector<NextHopWeight>& weights,
    std::optional<RouteCounterID> counterID) const {
  if (prefixes.empty()) {
    return;
  }
  std::vector<NextHopWeight> hopWeights;
  if (!weights.size()) {
    for (auto i = 0; i < portDescriptors.size(); ++i) {
      hopWeights.push_back(ECMP_WEIGHT);
    }
  } else {
    hopWeights = weights;
  }

  CHECK_EQ(portDescriptors.size(), hopWeights.size());
  RouteNextHopSet nhops;
  {
    auto i = 0;
    for (const auto& portDescriptor : portDescriptors) {
      nhops.emplace(UnresolvedNextHop(
          BaseEcmpSetupHelperT::ip(portDescriptor), hopWeights[i++]));
    }
  }
  for (const auto& prefix : prefixes) {
    updater->addRoute(
        routerId_,
        folly::IPAddress(prefix.network),
        prefix.mask,
        ClientID::BGPD,
        RouteNextHopEntry(nhops, AdminDistance::EBGP, counterID));
  }
  updater->program();
}

template <typename IPAddrT>
RouteNextHopSet EcmpSetupTargetedPorts<IPAddrT>::setupMplsNexthops(
    const flat_set<PortDescriptor>& portDescriptors,
    std::map<PortDescriptor, LabelForwardingAction::LabelStack>& stacks,
    LabelForwardingAction::LabelForwardingType labelActionType,
    const std::vector<NextHopWeight>& weights) const {
  RouteNextHopSet nhops;
  std::vector<NextHopWeight> hopWeights;
  if (!weights.size()) {
    for (auto i = 0; i < portDescriptors.size(); ++i) {
      hopWeights.push_back(ECMP_WEIGHT);
    }
  } else {
    hopWeights = weights;
  }

  CHECK_EQ(portDescriptors.size(), hopWeights.size());
  auto i = 0;
  for (const auto& portDescriptor : portDescriptors) {
    auto itr = stacks.find(portDescriptor);
    if (itr != stacks.end() && !itr->second.empty()) {
      if (labelActionType == LabelForwardingAction::LabelForwardingType::PUSH) {
        nhops.emplace(UnresolvedNextHop(
            BaseEcmpSetupHelperT::ip(portDescriptor),
            hopWeights[i++],
            LabelForwardingAction(labelActionType, itr->second)));
      } else {
        CHECK(
            labelActionType ==
            LabelForwardingAction::LabelForwardingType::SWAP);
        nhops.emplace(UnresolvedNextHop(
            BaseEcmpSetupHelperT::ip(portDescriptor),
            hopWeights[i++],
            LabelForwardingAction(labelActionType, itr->second[0])));
      }
    } else if (
        labelActionType == LabelForwardingAction::LabelForwardingType::PHP ||
        labelActionType ==
            LabelForwardingAction::LabelForwardingType::POP_AND_LOOKUP) {
      nhops.emplace(UnresolvedNextHop(
          BaseEcmpSetupHelperT::ip(portDescriptor),
          hopWeights[i++],
          LabelForwardingAction(labelActionType)));
    } else {
      nhops.emplace(UnresolvedNextHop(
          BaseEcmpSetupHelperT::ip(portDescriptor), hopWeights[i++]));
    }
  }

  return nhops;
}

template <typename IPAddrT>
void EcmpSetupTargetedPorts<IPAddrT>::programMplsRoutes(
    std::unique_ptr<RouteUpdateWrapper> updater,
    const flat_set<PortDescriptor>& portDescriptors,
    std::map<PortDescriptor, LabelForwardingAction::LabelStack> stacks,
    const std::vector<LabelID>& labels,
    LabelForwardingAction::LabelForwardingType labelActionType,
    const std::vector<NextHopWeight>& weights,
    std::optional<RouteCounterID> /* counterID */) const {
  if (labels.empty()) {
    return;
  }
  auto nhops =
      setupMplsNexthops(portDescriptors, stacks, labelActionType, weights);
  for (const auto& label : labels) {
    MplsRoute route;
    route.topLabel_ref() = label;
    route.nextHops_ref() = util::fromRouteNextHopSet(nhops);
    updater->addRoute(ClientID::BGPD, route);
  }
  updater->program();
}
template <typename IPAddrT>
void EcmpSetupTargetedPorts<IPAddrT>::programIp2MplsRoutes(
    std::unique_ptr<RouteUpdateWrapper> updater,
    const boost::container::flat_set<PortDescriptor>& portDescriptors,
    std::map<PortDescriptor, LabelForwardingAction::LabelStack> stacks,
    const std::vector<RouteT>& prefixes,
    const std::vector<NextHopWeight>& weights) const {
  std::vector<NextHopWeight> hopWeights;
  std::vector<NextHopWeight> forwardingActions;

  auto nhops = setupMplsNexthops(
      portDescriptors,
      stacks,
      LabelForwardingAction::LabelForwardingType::PUSH,
      weights);
  for (const auto& prefix : prefixes) {
    updater->addRoute(
        routerId_,
        folly::IPAddress(prefix.network),
        prefix.mask,
        ClientID::BGPD,
        RouteNextHopEntry(nhops, AdminDistance::EBGP));
  }
  updater->program();
}

template <typename IPAddrT>
void EcmpSetupTargetedPorts<IPAddrT>::unprogramRoutes(
    std::unique_ptr<RouteUpdateWrapper> wrapper,
    const std::vector<RouteT>& prefixes) const {
  for (const auto& prefix : prefixes) {
    wrapper->delRoute(
        routerId_,
        folly::IPAddress(prefix.network),
        prefix.mask,
        ClientID::BGPD);
  }
  wrapper->program();
}

template <typename IPAddrT>
flat_set<PortDescriptor> EcmpSetupAnyNPorts<IPAddrT>::getPortDescs(
    int width) const {
  flat_set<PortDescriptor> portDescs;
  for (size_t w = 0; w < width; ++w) {
    portDescs.insert(getNextHops().at(w).portDesc);
  }
  return portDescs;
}

template <typename IPAddrT>
std::shared_ptr<SwitchState> EcmpSetupAnyNPorts<IPAddrT>::resolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    size_t numNextHops,
    bool useLinkLocal) const {
  return ecmpSetupTargetedPorts_.resolveNextHops(
      inputState, getPortDescs(numNextHops), useLinkLocal);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState> EcmpSetupAnyNPorts<IPAddrT>::resolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    const flat_set<PortDescriptor>& portDescs,
    bool useLinkLocal) const {
  return ecmpSetupTargetedPorts_.resolveNextHops(
      inputState, portDescs, useLinkLocal);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState> EcmpSetupAnyNPorts<IPAddrT>::unresolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    const flat_set<PortDescriptor>& portDescs,
    bool useLinkLocal) const {
  return ecmpSetupTargetedPorts_.unresolveNextHops(
      inputState, portDescs, useLinkLocal);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState> EcmpSetupAnyNPorts<IPAddrT>::unresolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    size_t numNextHops,
    bool useLinkLocal) const {
  return ecmpSetupTargetedPorts_.unresolveNextHops(
      inputState, getPortDescs(numNextHops), useLinkLocal);
}

template <typename IPAddrT>
void EcmpSetupAnyNPorts<IPAddrT>::programRoutes(
    std::unique_ptr<RouteUpdateWrapper> updater,
    size_t width,
    const std::vector<RouteT>& prefixes,
    const std::vector<NextHopWeight>& weights) const {
  ecmpSetupTargetedPorts_.programRoutes(
      std::move(updater), getPortDescs(width), prefixes, weights);
}

template <typename IPAddrT>
void EcmpSetupAnyNPorts<IPAddrT>::programRoutes(
    std::unique_ptr<RouteUpdateWrapper> updater,
    const flat_set<PortDescriptor>& portDescs,
    const std::vector<RouteT>& prefixes,
    const std::vector<NextHopWeight>& weights) const {
  ecmpSetupTargetedPorts_.programRoutes(
      std::move(updater), portDescs, prefixes, weights);
}

template <typename IPAddrT>
void EcmpSetupAnyNPorts<IPAddrT>::programIp2MplsRoutes(
    std::unique_ptr<RouteUpdateWrapper> updater,
    size_t width,
    const std::vector<RouteT>& prefixes,
    std::vector<LabelForwardingAction::LabelStack> stacks,
    const std::vector<NextHopWeight>& weights) const {
  auto ports = getPortDescs(width);
  std::map<PortDescriptor, LabelForwardingAction::LabelStack> port2Stack;
  auto i = 0;
  for (auto port : ports) {
    i = (i % stacks.size());
    port2Stack.emplace(port, stacks[i++]);
  }
  ecmpSetupTargetedPorts_.programIp2MplsRoutes(
      std::move(updater), ports, port2Stack, prefixes, weights);
}

template <typename IPAddrT>
void EcmpSetupAnyNPorts<IPAddrT>::unprogramRoutes(
    std::unique_ptr<RouteUpdateWrapper> wrapper,
    const std::vector<RouteT>& prefixes) const {
  ecmpSetupTargetedPorts_.unprogramRoutes(std::move(wrapper), prefixes);
}

template <typename IPAddrT>
std::vector<PortDescriptor> EcmpSetupAnyNPorts<IPAddrT>::ecmpPortDescs(
    int width) const {
  return ecmpSetupTargetedPorts_.ecmpPortDescs(width);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState>
MplsEcmpSetupTargetedPorts<IPAddrT>::setupECMPForwarding(
    const std::shared_ptr<SwitchState>& inputState,
    const boost::container::flat_set<PortDescriptor>& portDescriptors,
    const std::vector<NextHopWeight>& weights) const {
  std::vector<NextHopWeight> hopWeights;
  if (!weights.size()) {
    for (auto i = 0; i < portDescriptors.size(); ++i) {
      hopWeights.push_back(ECMP_WEIGHT);
    }
  } else {
    hopWeights = weights;
  }

  CHECK_EQ(portDescriptors.size(), hopWeights.size());
  auto outputState{inputState->clone()};
  LabelNextHopSet nhops;
  {
    auto i = 0;
    for (const auto& portDescriptor : portDescriptors) {
      auto iter = BaseEcmpSetupHelperT::portDesc2Vlan_.find(portDescriptor);
      auto vlanID = iter->second;
      auto nexthop = nhop(portDescriptor);
      nhops.emplace(LabelNextHop(
          nexthop.ip, InterfaceID(vlanID), hopWeights[i++], nexthop.action));
    }
  }

  outputState->getLabelForwardingInformationBase()->programLabel(
      &outputState,
      topLabel_,
      ClientID(0),
      AdminDistance::DIRECTLY_CONNECTED,
      std::move(nhops));
  return outputState;
}

template <typename IPAddrT>
void MplsEcmpSetupTargetedPorts<IPAddrT>::computeNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    std::optional<folly::MacAddress> nextHopMac) {
  BaseEcmpSetupHelperT::portDesc2Vlan_ = computePortDesc2Vlan(inputState);
  auto vlan2Subnet =
      computeVlan2Subnet(inputState, BaseEcmpSetupHelperT::kIsV6);
  int offset = 0;
  auto baseMac = folly::MacAddress("06:00:00:00:00:00").u64HBO();
  for (const auto& portDescAndVlan : BaseEcmpSetupHelperT::portDesc2Vlan_) {
    auto vlan = portDescAndVlan.second;
    auto subnetIp = IPAddrT(vlan2Subnet[vlan].first.str());
    auto bytes = subnetIp.toByteArray();
    // Add a offset to compute next in subnet next hop IP.
    // Essentially for vlan/l3 intf with subnet X, we
    // would compute next hops by incrementing last octet
    // of subnet.
    int lastOctet = bytes[bytes.size() - 1] + (++offset);
    // Fail if we goto 255 at the last oct
    CHECK_GT(255, lastOctet);
    bytes[bytes.size() - 1] = static_cast<uint8_t>(lastOctet);
    BaseEcmpSetupHelperT::nhops_.push_back(EcmpMplsNextHop<IPAddrT>(
        IPAddrT(bytes),
        portDescAndVlan.first,
        nextHopMac ? MacAddress::fromHBO(nextHopMac.value().u64HBO())
                   : MacAddress::fromHBO(baseMac + offset),
        InterfaceID(vlan),
        getLabelForwardingAction(portDescAndVlan.first)));
  }
}

template <typename IPAddrT>
EcmpMplsNextHop<IPAddrT> MplsEcmpSetupTargetedPorts<IPAddrT>::nhop(
    PortDescriptor portDesc) const {
  auto it = std::find_if(
      BaseEcmpSetupHelperT::nhops_.begin(),
      BaseEcmpSetupHelperT::nhops_.end(),
      [portDesc](const auto& nh) { return nh.portDesc == portDesc; });
  if (it == BaseEcmpSetupHelperT::nhops_.end()) {
    throw FbossError("Could not find a nhop for: ", portDesc);
  }
  return *it;
}

template <typename IPAddrT>
LabelForwardingAction
MplsEcmpSetupTargetedPorts<IPAddrT>::getLabelForwardingAction(
    PortDescriptor port) const {
  auto label = port.isAggregatePort() ? port.aggPortID() : port.phyPortID();

  LabelForwardingAction::LabelStack pushStack;
  for (auto i = 1; i <= 3; i++) {
    pushStack.push_back(label * 10 + i);
  }

  switch (actionType_) {
    case LabelForwardingAction::LabelForwardingType::PUSH:
      return LabelForwardingAction(actionType_, std::move(pushStack));

    case LabelForwardingAction::LabelForwardingType::SWAP:
      return LabelForwardingAction(actionType_, pushStack[0]);
    case LabelForwardingAction::LabelForwardingType::PHP:
    case LabelForwardingAction::LabelForwardingType::POP_AND_LOOKUP:
    case LabelForwardingAction::LabelForwardingType::NOOP:
      return LabelForwardingAction(actionType_);
  }
  return LabelForwardingAction(
      LabelForwardingAction::LabelForwardingType::NOOP);
}

template <typename IPAddrT>
flat_set<PortDescriptor> MplsEcmpSetupAnyNPorts<IPAddrT>::getPortDescs(
    int width) const {
  flat_set<PortDescriptor> portDescs;
  for (size_t w = 0; w < width; ++w) {
    portDescs.insert(getNextHops().at(w).portDesc);
  }
  return portDescs;
}

template <typename IPAddrT>
std::vector<PortDescriptor> MplsEcmpSetupAnyNPorts<IPAddrT>::ecmpPortDescs(
    int width) const {
  return mplsEcmpSetupTargetedPorts_.ecmpPortDescs(width);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState> MplsEcmpSetupAnyNPorts<IPAddrT>::resolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    size_t numNextHops,
    bool useLinkLocal) const {
  return mplsEcmpSetupTargetedPorts_.resolveNextHops(
      inputState, getPortDescs(numNextHops), useLinkLocal);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState> MplsEcmpSetupAnyNPorts<IPAddrT>::unresolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    size_t numNextHops,
    bool useLinkLocal) const {
  return mplsEcmpSetupTargetedPorts_.unresolveNextHops(
      inputState, getPortDescs(numNextHops), useLinkLocal);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState> MplsEcmpSetupAnyNPorts<IPAddrT>::resolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    const boost::container::flat_set<PortDescriptor>& portDescs,
    bool useLinkLocal) const {
  return mplsEcmpSetupTargetedPorts_.resolveNextHops(
      inputState, portDescs, useLinkLocal);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState> MplsEcmpSetupAnyNPorts<IPAddrT>::unresolveNextHops(
    const std::shared_ptr<SwitchState>& inputState,
    const boost::container::flat_set<PortDescriptor>& portDescs,
    bool useLinkLocal) const {
  return mplsEcmpSetupTargetedPorts_.unresolveNextHops(
      inputState, portDescs, useLinkLocal);
}

template <typename IPAddrT>
std::shared_ptr<SwitchState>
MplsEcmpSetupAnyNPorts<IPAddrT>::setupECMPForwarding(
    const std::shared_ptr<SwitchState>& inputState,
    size_t width,
    const std::vector<NextHopWeight>& weights) const {
  return mplsEcmpSetupTargetedPorts_.setupECMPForwarding(
      inputState, getPortDescs(width), weights);
}

template class BaseEcmpSetupHelper<
    folly::IPAddressV4,
    EcmpNextHop<folly::IPAddressV4>>;
template class BaseEcmpSetupHelper<
    folly::IPAddressV6,
    EcmpNextHop<folly::IPAddressV6>>;

template class BaseEcmpSetupHelper<
    folly::IPAddressV4,
    EcmpMplsNextHop<folly::IPAddressV4>>;
template class BaseEcmpSetupHelper<
    folly::IPAddressV6,
    EcmpMplsNextHop<folly::IPAddressV6>>;

template class EcmpSetupTargetedPorts<folly::IPAddressV4>;
template class EcmpSetupTargetedPorts<folly::IPAddressV6>;
template class EcmpSetupAnyNPorts<folly::IPAddressV4>;
template class EcmpSetupAnyNPorts<folly::IPAddressV6>;

template class MplsEcmpSetupTargetedPorts<folly::IPAddressV4>;
template class MplsEcmpSetupTargetedPorts<folly::IPAddressV6>;
template class MplsEcmpSetupAnyNPorts<folly::IPAddressV4>;
template class MplsEcmpSetupAnyNPorts<folly::IPAddressV6>;

} // namespace facebook::fboss::utility
