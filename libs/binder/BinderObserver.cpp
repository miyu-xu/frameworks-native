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

#include <android-base/properties.h>
#include <binder/Binder.h>
#include <binder/Trace.h>
#include <fstream>

namespace android {

struct LatencyStats {
    uint64_t count = 0;
    uint64_t min_latency_ns = UINT64_MAX;
    uint64_t max_latency_ns = 0;
    double sum_latency_ns = 0;
    double sum_squared_latency_ns = 0;

    void add(uint64_t latency_ns) {
        count++;
        min_latency_ns = std::min(min_latency_ns, latency_ns);
        max_latency_ns = std::max(max_latency_ns, latency_ns);
        sum_latency_ns += latency_ns;
        sum_squared_latency_ns += (double)latency_ns * latency_ns;
    }

    double mean_ns() const { return count > 0 ? sum_latency_ns / count : 0; }

    double stddev_ns() const {
        if (count <= 1) return 0;
        double mean = mean_ns();
        double variance = (sum_squared_latency_ns / count) - (mean * mean);
        return std::sqrt(variance);
    }
};

bool BinderObserver::shouldFlush() {
    {
        std::lock_guard lock(mStorageLock);
        if (mDataBuffer.size() > (maxSizeOfDataBuffer / 2) * 3) return true;
    }

    auto now = std::chrono::system_clock::now();
    auto previousRunTime = lastRunTime.exchange(now);

    if (std::chrono::duration_cast<std::chrono::minutes>(now - previousRunTime).count() >= 1) {
        return true;
    } else {
        return false;
    }
}

void BinderObserver::addDataPoint(const IBinderObserver::BinderObserverData& data) {
    ALOGI("Adding data point to store");
    binder::ScopedTrace aidlTrace(ATRACE_TAG_AIDL, "addDataPoint");
    {
        std::lock_guard lock(mStorageLock);
        if (mDataBuffer.size() >= maxSizeOfDataBuffer) {
            // drop data. this shouldn't happen.
            return;
        }
        mDataBuffer.push_back(data);
    }
    if (shouldFlush()) {
        flushToDataStore();
    }
}

void BinderObserver::flushToDataStore() {
    binder::ScopedTrace aidlTrace(ATRACE_TAG_AIDL, "flushToDataStore");

    // IBinderObserver::BinderObserverData
    // newBuffer[4096/sizeof(IBinderObserver::BinderObserverData)] = {};

    std::ofstream logFile("/data/local/tmp/binder_latency.csv");
    if (!logFile.is_open()) {
        ALOGE("Failed to open binder latency log file");
        return;
    }

    // Write header
    logFile << "Handle,Code,Flags,SenderPid,SenderUid,Size,LatencyNs\n";

    BinderObserver::DataBuffer replacementBuffer;
    {
        std::lock_guard lock(mStorageLock);
        replacementBuffer.swap(mDataBuffer);
    }
    std::map<int32_t, LatencyStats> handleStats;
    std::map<uint32_t, LatencyStats> codeStats;
    LatencyStats overallStats;

    for (const IBinderObserver::BinderObserverData& record : replacementBuffer) {
        // ALOGE("FlushingData %u %u", datum.duration, datum.txn_code);
        uint64_t latencyNs = record.endTime - record.startTime;
        logFile << record.handle << "," << record.code << "," << record.flags << ","
                << record.senderPid << "," << record.senderUid << "," << record.size << ","
                << latencyNs << "\n";

        // Update statistics
        handleStats[record.handle].add(latencyNs);
        codeStats[record.code].add(latencyNs);
        overallStats.add(latencyNs);
    }
    logFile.close();
    ALOGI("Parth Binder latency stats: %" PRIu64
          " transactions, mean: %.2f us, stddev: %.2f us, min: %.2f us, max: %.2f us",
          overallStats.count, overallStats.mean_ns() / 1000.0, overallStats.stddev_ns() / 1000.0,
          overallStats.min_latency_ns / 1000.0, overallStats.max_latency_ns / 1000.0);
    // Log top 5 slowest interfaces by average latency
    std::vector<std::pair<int32_t, LatencyStats>> sortedHandles(handleStats.begin(),
                                                                handleStats.end());
    std::sort(sortedHandles.begin(), sortedHandles.end(),
              [](const auto& a, const auto& b) { return a.second.mean_ns() > b.second.mean_ns(); });

    ALOGI("Top 5 slowest binder interfaces:");
    for (size_t i = 0; i < std::min(size_t(5), sortedHandles.size()); i++) {
        ALOGI("  Handle %d: %" PRIu64 " calls, mean: %.2f us", sortedHandles[i].first,
              sortedHandles[i].second.count, sortedHandles[i].second.mean_ns() / 1000.0);
    }
}
} // namespace android