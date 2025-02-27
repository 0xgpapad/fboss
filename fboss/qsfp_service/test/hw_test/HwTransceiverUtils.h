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
#include "fboss/qsfp_service/if/gen-cpp2/transceiver_types.h"

namespace facebook::fboss::utility {

class HwTransceiverUtils {
 public:
  static void verifyTransceiverSettings(
      const TransceiverInfo& transceiver,
      cfg::PortProfileID profile);

 private:
  static void verifyOpticsSettings(const TransceiverInfo& transceiver);
  static void verifyMediaInterfaceCompliance(
      const TransceiverInfo& transceiver,
      cfg::PortProfileID profile);
  static void verify10gProfile(
      const TransceiverInfo& transceiver,
      const TransceiverManagementInterface mgmtInterface,
      const std::vector<MediaInterfaceId>& mediaInterfaces);
  static void verify100gProfile(
      const TransceiverManagementInterface mgmtInterface,
      const std::vector<MediaInterfaceId>& mediaInterfaces);
  static void verify200gProfile(
      const TransceiverManagementInterface mgmtInterface,
      const std::vector<MediaInterfaceId>& mediaInterfaces);
  static void verify400gProfile(
      const TransceiverManagementInterface mgmtInterface,
      const std::vector<MediaInterfaceId>& mediaInterfaces);
  static void verifyCopper100gProfile(
      const TransceiverInfo& transceiver,
      const std::vector<MediaInterfaceId>& mediaInterfaces);
};

} // namespace facebook::fboss::utility
