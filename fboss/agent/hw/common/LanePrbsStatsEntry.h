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

#include "fboss/agent/if/gen-cpp2/ctrl_types.h"

#include <stdint.h>

extern "C" {
#include <bcm/types.h>
}

namespace facebook::fboss {

using std::chrono::milliseconds;
using std::chrono::steady_clock;

class LanePrbsStatsEntry {
 public:
  LanePrbsStatsEntry(int32_t laneId, int32_t gportId, double laneRate)
      : laneId_(laneId), gportId_(gportId), laneRate_(laneRate) {
    timeLastCleared_ = steady_clock::now();
    timeLastLocked_ = steady_clock::time_point();
  }

  int32_t getLaneId() const {
    return laneId_;
  }

  int32_t getGportId() const {
    return gportId_;
  }

  double getLaneRate() const {
    return laneRate_;
  }

  void lossOfLock() {
    if (locked_) {
      locked_ = false;
      accuErrorCount_ = 0;
      numLossOfLock_++;
    }
    timeLastCollect_ = steady_clock::now();
  }

  void locked() {
    steady_clock::time_point now = steady_clock::now();
    locked_ = true;
    accuErrorCount_ = 0;
    timeLastLocked_ = now;
    timeLastCollect_ = now;
  }

  void updateLaneStats(uint32 status) {
    if (!locked_) {
      locked();
      return;
    }
    steady_clock::time_point now = steady_clock::now();
    accuErrorCount_ += status;

    milliseconds duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - timeLastCollect_);
    // There shouldn't be a case where duration would be 0.
    // But just add a check here to be safe.
    if (duration.count() == 0) {
      return;
    }
    double ber = (status * 1000) / (laneRate_ * duration.count());
    if (ber > maxBer_) {
      maxBer_ = ber;
    }
    timeLastCollect_ = now;
  }

  phy::PrbsLaneStats getPrbsLaneStats() const {
    phy::PrbsLaneStats prbsLaneStats = phy::PrbsLaneStats();
    steady_clock::time_point now = steady_clock::now();
    *prbsLaneStats.laneId_ref() = laneId_;
    *prbsLaneStats.locked_ref() = locked_;
    if (!locked_) {
      *prbsLaneStats.ber_ref() = 0.;
    } else {
      milliseconds duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              timeLastCollect_ - timeLastLocked_);
      if (duration.count() == 0) {
        *prbsLaneStats.ber_ref() = 0.;
      } else {
        *prbsLaneStats.ber_ref() =
            (accuErrorCount_ * 1000) / (laneRate_ * duration.count());
      }
    }
    *prbsLaneStats.maxBer_ref() = maxBer_;
    *prbsLaneStats.numLossOfLock_ref() = numLossOfLock_;
    *prbsLaneStats.timeSinceLastLocked_ref() =
        (timeLastLocked_ == steady_clock::time_point())
        ? 0
        : std::chrono::duration_cast<std::chrono::seconds>(
              now - timeLastLocked_)
              .count();
    *prbsLaneStats.timeSinceLastClear_ref() =
        std::chrono::duration_cast<std::chrono::seconds>(now - timeLastCleared_)
            .count();
    return prbsLaneStats;
  }

  void clearLaneStats() {
    accuErrorCount_ = 0;
    maxBer_ = -1.;
    numLossOfLock_ = 0;
    timeLastLocked_ = locked_ ? timeLastCollect_ : steady_clock::time_point();
    timeLastCleared_ = steady_clock::now();
  }

 private:
  const int32_t laneId_ = -1;
  const int32_t gportId_ = -1;
  const double laneRate_ = 0.;
  bool locked_ = false;
  int64_t accuErrorCount_ = 0;
  double maxBer_ = -1.;
  int32_t numLossOfLock_ = 0;
  steady_clock::time_point timeLastLocked_;
  steady_clock::time_point timeLastCleared_;
  steady_clock::time_point timeLastCollect_;
};
} // namespace facebook::fboss
