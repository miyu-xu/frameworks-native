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

#include <binder/IPCThreadState.h>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>

// #include <shared_mutex>

namespace android {
class BinderObserver : public IBinderObserver {
    class DataBuffer : public std::vector<IBinderObserver::BinderObserverData> {
    public:
        DataBuffer() { reserve(maxSizeOfDataBuffer); }
    };

public:
    void addDataPoint(const IBinderObserver::BinderObserverData& data) override;
    bool shouldFlush();
    void flushToDataStore() override;

private:
    // statsd limits pushs to 4kb, so the max push size should be 4kb
    // std::array<IBinderObserver::BinderObserverData> dataBuffer;
    static const size_t maxSizeOfDataBuffer = 10000;
    // maybe make this a shared_ptr to make it easier to swap?
    // IBinderObserver::BinderObserverData dataBuffer[maxSizeOfDataBuffer];
    DataBuffer mDataBuffer;
    // std::vector<IBinderObserver::BinderObserverData> dataBuffer;

    // std::atomic<size_t> mTail;
    std::mutex mStorageLock;
    std::atomic<std::chrono::time_point<std::chrono::system_clock>> lastRunTime;
};
} // namespace android
