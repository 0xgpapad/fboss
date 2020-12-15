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

#include "fboss/agent/platforms/common/PlatformMapping.h"

#include <gtest/gtest.h>

namespace facebook {
namespace fboss {
namespace test {
class PlatformMappingTest : public ::testing::Test {
 public:
  void SetUp() override {}

  void setExpection(
      int numPort,
      int numIphy,
      int numXphy,
      int numTcvr,
      std::vector<cfg::PortProfileID>& profiles) {
    expectedNumPort_ = numPort;
    expectedNumIphy_ = numIphy;
    expectedNumXphy_ = numXphy;
    expectedNumTcvr_ = numTcvr;
    expectedProfiles_ = std::move(profiles);
  }

  void verify(PlatformMapping* mapping) {
    EXPECT_EQ(expectedNumPort_, mapping->getPlatformPorts().size());

    for (auto profile : expectedProfiles_) {
      auto supportedProfile = mapping->getPortProfileConfig(profile);
      auto platformSupportedProfile = mapping->getPortProfileConfig(
          PlatformPortProfileConfigMatcher(profile, std::nullopt));
      EXPECT_TRUE(supportedProfile.has_value());
      EXPECT_TRUE(platformSupportedProfile.has_value());
      EXPECT_EQ(supportedProfile, platformSupportedProfile);
    }

    int numIphy = 0, numXphy = 0, numTcvr = 0;
    for (const auto& chip : mapping->getChips()) {
      switch (*chip.second.type_ref()) {
        case phy::DataPlanePhyChipType::IPHY:
          numIphy++;
          break;
        case phy::DataPlanePhyChipType::XPHY:
          numXphy++;
          break;
        case phy::DataPlanePhyChipType::TRANSCEIVER:
          numTcvr++;
          break;
        default:
          break;
      }
    }
    EXPECT_EQ(expectedNumIphy_, numIphy);
    EXPECT_EQ(expectedNumXphy_, numXphy);
    EXPECT_EQ(expectedNumTcvr_, numTcvr);
  }

 private:
  int expectedNumPort_{0};
  int expectedNumIphy_{0};
  int expectedNumXphy_{0};
  int expectedNumTcvr_{0};
  std::vector<cfg::PortProfileID> expectedProfiles_;
};
} // namespace test
} // namespace fboss
} // namespace facebook
