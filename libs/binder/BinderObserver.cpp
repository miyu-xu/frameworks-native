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
#include "BinderObserver.h"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

#include <binder/Binder.h>
#include <binder/Trace.h>
#include <android-base/properties.h>

namespace android {
CallSession BinderObserver::onCallStarted() {
    return CallSession{.timeOfCallSinceEpoch = std::chrono::system_clock::now().time_since_epoch()};
}

void BinderObserver::flushDataForCurrentEpochSecond(std::chrono::seconds epochSecondBucket) {
    int flush_prop = base::GetIntProperty("persist.device_config.binder_observer_native.flush_prop", -1);

    std::chrono::seconds bucketToFlush;
    std::vector<std::pair<uint32_t, DataPerTxnCode>> storageToFlush;
    storageToFlush.reserve(20);
    std::string log =  "BinderObserver::flushDataForCurrentEpochSecond " + std::to_string(flush_prop);
    binder::ScopedTrace aidlTrace(ATRACE_TAG_AIDL, log.c_str());

    std::map<uint32_t, std::string> codeToName;
    {
        std::lock_guard lock(mStorageLock);
        // move the data to a local variable to be flushed.
        bucketToFlush = mCurrentEpochSecondBucket;
        storageToFlush.swap(mStorage);
        mCurrentEpochSecondBucket = epochSecondBucket;
        codeToName = mTransactionCodeToName;
    }
    if (storageToFlush.empty()) {
        return;
    }

    std::thread([bucketToFlush = std::move(bucketToFlush), codeToName = std::move(codeToName),
                    storageToFlush = std::move(storageToFlush)]() {
        binder::ScopedTrace aidlTrace(ATRACE_TAG_AIDL,
                                        "BinderObserver thread:  flushing data");

        for (auto& [transactionCode, data] : storageToFlush) {
            std::string name = codeToName.at(transactionCode);
            for (auto& [workSourceUid, dataPerWorkSourceUid] : data) {
                ALOGI("BinderObserver::flushDataForCurrentEpochSecond: %s workSource:%u "
                      "counts: %u %u",
                      name.c_str(), workSourceUid, dataPerWorkSourceUid.countOfSuccessfulCalls,
                      dataPerWorkSourceUid.countOfCallsWithErrors);
            }
        }
    }).detach();
}

void BinderObserver::onCallEnded(const CallSession& callSession, uint32_t transactionCode,
                                     const std::string& transactionName, uint32_t workSourceUid,
                                     int, int, bool isExceptionThrown) {
    ALOGI("BinderObserver::onCallEnded: %s workSource:%u ", transactionName.c_str(),
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
          return;
        }
    }
    DataPerTxnCode map;
    DataPerWorkSourceUid data;
    isExceptionThrown ? ++data.countOfCallsWithErrors : ++data.countOfSuccessfulCalls;
    map[workSourceUid] = data;
    mStorage.push_back(std::make_pair(transactionCode, map));
}

} // namespace android
