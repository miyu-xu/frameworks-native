/*
 * Copyright (C) 2025 The Android Open Source Project *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hals.h"
#include "keymint.h"
#include "utils.h"

namespace android {
namespace idlcli {

class CommandKeyMint;

namespace keymint {

class CommandGetHardwareInfo : public Command {
    std::string getDescription() const override { return "Retrieves HW info."; }

    std::string getUsageSummary() const override { return ""; }

    UsageDetails getUsageDetails() const override {
        UsageDetails details{};
        return details;
    }

    Status doArgs(Args& args) override {
        if (!args.empty()) {
            std::cerr << "Unexpected Arguments!" << std::endl;
            return USAGE;
        }
        return OK;
    }

    Status doMain(Args&& /*args*/) override {
        auto hal = getHal<aidl::IKeyMintDevice>();
        if (!hal) return UNAVAILABLE;

        aidl::KeyMintHardwareInfo hwInfo;
        auto status = hal->call(&aidl::IKeyMintDevice::getHardwareInfo, &hwInfo);
        if (!status.isOk()) {
            std::cerr << "Error: " << status.getDescription() << std::endl;
            return ERROR;
        }

        std::cout << "\n"
            << "   KeyMint HW Info:\n"
            << " * Version .......... : " << hwInfo.versionNumber << '\n'
            << " * Security level ... : " << aidl::toString(hwInfo.securityLevel) << '\n'
            << " * Name ............. : " << value_or_default(hwInfo.keyMintName, "<empty>") << '\n'
            << " * Author name ...... : " << value_or_default(hwInfo.keyMintAuthorName, "<empty>") << '\n'
            << " * Timestamp required : " << (hwInfo.timestampTokenRequired ? "true" : "false") << '\n'
            << std::endl;

        return OK;
    }
};

static const auto Command =
        CommandRegistry<CommandKeyMint>::Register<CommandGetHardwareInfo>("getHardwareInfo");

} // namespace keymint
} // namespace idlcli
} // namespace android
