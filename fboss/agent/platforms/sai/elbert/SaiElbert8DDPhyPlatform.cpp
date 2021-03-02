/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/platforms/sai/elbert/SaiElbert8DDPhyPlatform.h"

#include "fboss/agent/hw/switch_asics/Elbert8DDAsic.h"
#include "fboss/agent/platforms/common/elbert/facebook/Elbert8DDPimPlatformMapping.h"

namespace {
static auto constexpr kSaiBootType = "SAI_KEY_BOOT_TYPE";
static auto constexpr kSaiConfigFile = "SAI_KEY_INIT_CONFIG_FILE";
const std::string kSaiProfileDir = "/lib/firmware/fboss/credo/gearbox/";
const std::array<std::string, 8> kPhyConfigProfiles = {
    kSaiProfileDir + "Elbert_16Q_0.xml",
    kSaiProfileDir + "Elbert_16Q_1.xml",
    kSaiProfileDir + "Elbert_16Q_2.xml",
    kSaiProfileDir + "Elbert_16Q_3.xml",
    kSaiProfileDir + "Elbert_16Q_4.xml",
    kSaiProfileDir + "Elbert_16Q_5.xml",
    kSaiProfileDir + "Elbert_16Q_6.xml",
    kSaiProfileDir + "Elbert_16Q_7.xml"};

/*
 * saiProfileGetValue
 *
 * This function returns some key values to the SAI while doing
 * sai_api_initialize.
 * For SAI_KEY_BOOT_TYPE, currently we only return the cold boot type.
 * For SAI_KEY_INIT_CONFIG_FILE, the profile id tells SAI which default
 * configuration to pick up for the Phy.
 */
const char* FOLLY_NULLABLE
saiProfileGetValue(sai_switch_profile_id_t profileId, const char* variable) {
  if (strcmp(variable, kSaiBootType) == 0) {
    // TODO(rajank) Support warmboot
    return "cold";
  } else if (strcmp(variable, kSaiConfigFile) == 0) {
    if (profileId >= 0 && profileId <= 7) {
      return kPhyConfigProfiles[profileId].c_str();
    }
  }
  return nullptr;
}

/*
 * saiProfileGetNextValue
 *
 * This function lets SAI pick up next value for a given key. Currently this
 * returns null
 */
int saiProfileGetNextValue(
    sai_switch_profile_id_t /* profile_id */,
    const char** /* variable */,
    const char** /* value */) {
  return -1;
}

sai_service_method_table_t kSaiServiceMethodTable = {
    .profile_get_value = saiProfileGetValue,
    .profile_get_next_value = saiProfileGetNextValue,
};
} // namespace

namespace facebook::fboss {
SaiElbert8DDPhyPlatform::SaiElbert8DDPhyPlatform(
    std::unique_ptr<PlatformProductInfo> productInfo,
    uint8_t pimId,
    int phyId)
    : SaiHwPlatform(
          std::move(productInfo),
          std::make_unique<Elbert8DDPimPlatformMapping>()
              ->getPimPlatformMappingUniquePtr(pimId)),
      pimId_(pimId),
      phyId_(phyId) {
  asic_ = std::make_unique<Elbert8DDAsic>();
}

SaiElbert8DDPhyPlatform::~SaiElbert8DDPhyPlatform() {}

std::string SaiElbert8DDPhyPlatform::getHwConfig() {
  throw FbossError("SaiElbert8DDPhyPlatform doesn't support getHwConfig()");
}
HwAsic* SaiElbert8DDPhyPlatform::getAsic() const {
  return asic_.get();
}
uint32_t SaiElbert8DDPhyPlatform::numLanesPerCore() const {
  throw FbossError("SaiElbert8DDPhyPlatform doesn't support numLanesPerCore()");
}
std::vector<PortID> SaiElbert8DDPhyPlatform::getAllPortsInGroup(
    PortID /* portID */) const {
  throw FbossError("SaiElbert8DDPhyPlatform doesn't support FlexPort");
}
std::vector<FlexPortMode> SaiElbert8DDPhyPlatform::getSupportedFlexPortModes()
    const {
  throw FbossError("SaiElbert8DDPhyPlatform doesn't support FlexPort");
}
std::optional<sai_port_interface_type_t>
SaiElbert8DDPhyPlatform::getInterfaceType(
    TransmitterTechnology /* transmitterTech */,
    cfg::PortSpeed /* speed */) const {
  throw FbossError(
      "SaiElbert8DDPhyPlatform doesn't support getInterfaceType()");
}
bool SaiElbert8DDPhyPlatform::isSerdesApiSupported() {
  return true;
}
bool SaiElbert8DDPhyPlatform::supportInterfaceType() const {
  return false;
}
void SaiElbert8DDPhyPlatform::initLEDs() {
  throw FbossError("SaiElbert8DDPhyPlatform doesn't support initLEDs()");
}

sai_service_method_table_t* SaiElbert8DDPhyPlatform::getServiceMethodTable()
    const {
  return &kSaiServiceMethodTable;
}

const std::set<sai_api_t>& SaiElbert8DDPhyPlatform::getSupportedApiList()
    const {
  return getDefaultPhyAsicSupportedApis();
}
} // namespace facebook::fboss
