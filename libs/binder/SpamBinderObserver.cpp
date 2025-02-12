/*
 * Copyright (C) 2025 The Android Open Source Project
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
#include "SpamBinderObserver.h"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

#include <binder/Binder.h>

namespace android {
CallSession SpamBinderObserver::onCallStarted() {
    return CallSession{.timeOfCallSinceEpoch = std::chrono::system_clock::now().time_since_epoch()};
}

void SpamBinderObserver::flushDataForCurrentEpochSecond(std::chrono::seconds epochSecondBucket) {
    std::chrono::seconds bucketToFlush;
    std::vector<std::pair<uint32_t, SpamDataPerTxnCode>> storageToFlush;
    std::map<uint32_t, std::string> map;
    {
        std::lock_guard lock(mStorageLock);
        // move the data to a local variable to be flushed.
        bucketToFlush = mCurrentEpochSecondBucket;
        storageToFlush.swap(mStorage);
        mCurrentEpochSecondBucket = epochSecondBucket;
        map = mTransactionCodeToName;
    }
    std::thread thread([bucketToFlush, &storageToFlush, &map] {
        for (auto& [transactionCode, spamData] : storageToFlush) {
            for (auto& [workSourceUid, dataPerWorkSourceUid] : spamData) {
                std::string name = map[transactionCode];
                ALOGE("SpamBinderObserver::flushDataForCurrentEpochSecond: %s workSource:%u "
                      "counts: %u %u",
                      name.c_str(), workSourceUid, dataPerWorkSourceUid.countOfSuccessfulCalls,
                      dataPerWorkSourceUid.countOfCallsWithErrors);
            }
        }
    });
}
void SpamBinderObserver::onCallEnded(const CallSession& callSession, uint32_t transactionCode,
                                     const std::string& transactionName, uint32_t workSourceUid,
                                     int, int, bool isExceptionThrown) {
    std::chrono::seconds epochSecondBucket =
            std::chrono::duration_cast<std::chrono::seconds>(callSession.timeOfCallSinceEpoch);
    if (epochSecondBucket < mCurrentEpochSecondBucket) {
        // drop data if the epoch bucket is older than the current one.
        return;
    } else if (epochSecondBucket > mCurrentEpochSecondBucket) {
        // flush data if the epoch bucket is newer than the current one.
        // flush data for the current epoch bucket.
        // update currentEpochSecond.
        flushDataForCurrentEpochSecond(mCurrentEpochSecondBucket);
    }
    std::lock_guard lock(mStorageLock);
    mTransactionCodeToName.insert({transactionCode, transactionName});
    for (auto& [code, workSourceIdToDataMap] : mStorage) {
        if (code == transactionCode) {
            if (auto it = workSourceIdToDataMap.find(workSourceUid);
                it != workSourceIdToDataMap.end()) {
                isExceptionThrown ? ++it->second.countOfCallsWithErrors
                                  : ++it->second.countOfSuccessfulCalls;
            } else {
                DataPerWorkSourceUid data;
                isExceptionThrown ? ++data.countOfCallsWithErrors : ++data.countOfSuccessfulCalls;
                workSourceIdToDataMap.insert({workSourceUid, data});
            }
        }
    }
}

} // namespace android
