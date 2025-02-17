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
#include <binder/Trace.h>

namespace android {
CallSession SpamBinderObserver::onCallStarted() {
    return CallSession{.timeOfCallSinceEpoch = std::chrono::system_clock::now().time_since_epoch()};
}

void SpamBinderObserver::flushDataForCurrentEpochSecond(std::chrono::seconds epochSecondBucket) {
    std::chrono::seconds bucketToFlush;
    std::vector<std::pair<uint32_t, SpamDataPerTxnCode>> storageToFlush;
    binder::ScopedTrace aidlTrace(ATRACE_TAG_AIDL,
                                  "SpamBinderObserver::flushDataForCurrentEpochSecond");

    std::map<uint32_t, std::string> codeToName;
    {
        std::lock_guard lock(mStorageLock);
        // move the data to a local variable to be flushed.
        bucketToFlush = mCurrentEpochSecondBucket;
        storageToFlush.swap(mStorage);
        mCurrentEpochSecondBucket = epochSecondBucket;
        codeToName = mTransactionCodeToName;
    }
    if (storageToFlush.size() == 0) {
        return;
    }

    std::thread([bucketToFlush = std::move(bucketToFlush), codeToName = std::move(codeToName),
                    storageToFlush = std::move(storageToFlush)]() {
        binder::ScopedTrace aidlTrace(ATRACE_TAG_AIDL,
                                        "SpamBinderObserver thread:  flushing data");

        for (auto& [transactionCode, spamData] : storageToFlush) {
            std::string name = codeToName.at(transactionCode);
            for (auto& [workSourceUid, dataPerWorkSourceUid] : spamData) {
                ALOGI("SpamBinderObserver::flushDataForCurrentEpochSecond: %s workSource:%u "
                      "counts: %u %u",
                      name.c_str(), workSourceUid, dataPerWorkSourceUid.countOfSuccessfulCalls,
                      dataPerWorkSourceUid.countOfCallsWithErrors);
            }
        }
    }).detach();
}

void SpamBinderObserver::onCallEnded(const CallSession& callSession, uint32_t transactionCode,
                                     const std::string& transactionName, uint32_t workSourceUid,
                                     int, int, bool isExceptionThrown) {
    ALOGI("SpamBinderObserver::onCallEnded: %s workSource:%u ", transactionName.c_str(),
          workSourceUid);

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
