/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#pragma once
#include "fboss/agent/gen-cpp2/switch_config_types.h"
#include "fboss/agent/hw/gen-cpp2/hardware_stats_types.h"
#include "fboss/agent/types.h"

// Forward declarations
namespace facebook::fboss {
class HwSwitch;
class TxPacket;
} // namespace facebook::fboss

namespace facebook::fboss::utility {

using HwPortStatsFunc = typename std::function<std::map<PortID, HwPortStats>(
    const std::vector<PortID>&)>;

bool ensureSendPacketSwitched(
    HwSwitch* hwSwitch,
    std::unique_ptr<TxPacket> pkt,
    const std::vector<PortID>& portIds,
    HwPortStatsFunc getHwPortStats);

bool ensureSendPacketOutOfPort(
    HwSwitch* hwSwitch,
    std::unique_ptr<TxPacket> pkt,
    PortID portID,
    const std::vector<PortID>& ports,
    HwPortStatsFunc getHwPortStats,
    std::optional<uint8_t> queue = std::nullopt);

bool waitPortStatsCondition(
    std::function<bool(const std::map<PortID, HwPortStats>&)> conditionFn,
    const std::vector<PortID>& portIds,
    uint32_t retries,
    std::chrono::duration<uint32_t, std::milli> msBetweenRetry,
    HwPortStatsFunc getHwPortStats);

} // namespace facebook::fboss::utility
