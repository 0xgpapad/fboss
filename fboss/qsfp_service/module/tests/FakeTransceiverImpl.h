// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "fboss/qsfp_service/module/TransceiverImpl.h"

namespace facebook {
namespace fboss {

// This class contains a fake implementation of a transceiver. It overrides the
// readTransceiver, writeTransceiver and some other methods. It uses a fake
// eeprom map, the reads read from the map and the writes modify the map.
class FakeTransceiverImpl : public TransceiverImpl {
 public:
  FakeTransceiverImpl(
      int module,
      std::array<uint8_t, 128>& lowerPage,
      std::map<int, std::array<uint8_t, 128>>& upperPages) {
    module_ = module;
    moduleName_ = folly::to<std::string>(module);
    std::copy(lowerPage.begin(), lowerPage.end(), pageLower_.begin());
    upperPages_.insert(upperPages.begin(), upperPages.end());
  }
  /* This function is used to read the SFP EEprom */
  int readTransceiver(int dataAddress, int offset, int len, uint8_t* fieldValue)
      override;
  /* write to the eeprom (usually to change the page setting) */
  int writeTransceiver(
      int dataAddress,
      int offset,
      int len,
      uint8_t* fieldValue) override;
  /* This function detects if a SFP is present on the particular port */
  bool detectTransceiver() override;
  /* Returns the name for the port */
  folly::StringPiece getName() override;
  int getNum() const override;

 private:
  int module_{0};
  std::string moduleName_;
  int page_{0};
  std::map<int, std::array<uint8_t, 128>> upperPages_;
  std::array<uint8_t, 128> pageLower_;
};

class SffDacTransceiver : public FakeTransceiverImpl {
 public:
  explicit SffDacTransceiver(int module);
};

class SffCwdm4Transceiver : public FakeTransceiverImpl {
 public:
  explicit SffCwdm4Transceiver(int module);
};

class SffFr1Transceiver : public FakeTransceiverImpl {
 public:
  explicit SffFr1Transceiver(int module);
};

class BadSffCwdm4Transceiver : public FakeTransceiverImpl {
 public:
  explicit BadSffCwdm4Transceiver(int module);
};

class Cmis200GTransceiver : public FakeTransceiverImpl {
 public:
  explicit Cmis200GTransceiver(int module);
};

} // namespace fboss
} // namespace facebook
