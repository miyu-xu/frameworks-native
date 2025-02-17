/*
 * Copyright (C) 2021 The Android Open Source Project
 *
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

#pragma once

#include <binder/Binder.h>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>

// #include <shared_mutex>

namespace android {
class BinderObserver : public IBinderObserver {
public:
    BinderObserver() {
        mStorage.reserve(20);
        mCurrentEpochSecondBucket = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch());
    }
    CallSession onCallStarted() override;
    void onCallEnded(const CallSession& callSession, uint32_t transactionCode,
                     const std::string& transactionName, uint32_t workSourceUid, int parcelRequestSize,
                     int parcelReplySize, bool isExceptionThrown) override;
private:
    std::chrono::seconds mCurrentEpochSecondBucket;
    struct DataPerWorkSourceUid {
        uint32_t countOfSuccessfulCalls = 0;
        uint32_t countOfCallsWithErrors = 0;
    };
    typedef std::map<uint32_t /*workSourceUid*/, DataPerWorkSourceUid> DataPerTxnCode;
    void flushDataForCurrentEpochSecond(std::chrono::seconds epochSecondBucket);
    std::vector<std::pair<uint32_t /*transactionCode*/, DataPerTxnCode>> mStorage;
    std::mutex mStorageLock;
    std::map<uint32_t, std::string> mTransactionCodeToName;
};
} // namespace android
