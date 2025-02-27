// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <folly/init/Init.h>
#include <folly/logging/xlog.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <string.h>
#include <sysexits.h>
#include <memory>
#include "fboss/lib/platforms/PlatformMode.h"
#include "fboss/lib/platforms/PlatformProductInfo.h"
#include "fboss/platform/helpers/Utils.h"
#include "fboss/platform/weutil/WeutilDarwin.h"

using namespace facebook::fboss::platform::helpers;
using namespace facebook::fboss::platform;
using namespace facebook::fboss;

DEFINE_bool(json, false, "output in JSON format");
DECLARE_string(fruid_filepath);

std::unique_ptr<WeutilInterface> get_plat_weutil(void) {
  PlatformProductInfo prodInfo(FLAGS_fruid_filepath);
  prodInfo.initialize();
  if (prodInfo.getMode() == PlatformMode::DARWIN) {
    return std::make_unique<WeutilDarwin>();
  }

  XLOG(INFO) << "The platform (" << toString(prodInfo.getMode())
             << ") is not supported" << std::endl;
  return nullptr;
}

/*
 * This utility program will output Chassis info for Darwin
 */
int main(int argc, char* argv[]) {
  folly::init(&argc, &argv, true);
  gflags::SetCommandLineOptionWithMode(
      "minloglevel", "0", gflags::SET_FLAGS_DEFAULT);

  std::unique_ptr<WeutilInterface> weutilInstance = get_plat_weutil();
  if (weutilInstance) {
    if (FLAGS_json) {
      weutilInstance->printInfoJson();
    } else {
      weutilInstance->printInfo();
    }
  } else {
    showDeviceInfo();
  }

  return EX_OK;
}
