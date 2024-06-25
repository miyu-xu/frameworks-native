/*
 * Copyright (C) 2024 The Android Open Source Project
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
#include <gtest/gtest.h>

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include "fakeservicemanager/FakeServiceManager.h"

using namespace android;

#ifdef LIBBINDER_CLIENT_CACHE
constexpr bool kUseLibbinderCache = true;
#else
constexpr bool kUseLibbinderCache = false;
#endif

TEST(LibbinderCache, AddServiceAndConfirmCacheHit) {
    if (!kUseLibbinderCache) {
        GTEST_SKIP() << "Not valid in current configuration";
    }
    sp<IBinder> smBinder = IInterface::asBinder(defaultServiceManager());
    FakeServiceManager fakeservicemanager = FakeServiceManager();
    String16 serviceName = String16("isub");
    sp<IBinder> nullBinder = nullptr;

    // Add a service (in this case the default service manager)
    fakeservicemanager.addService(serviceName, smBinder);
    // Get the service. This caches it.
    sp<IBinder> targetBinder = fakeservicemanager.getService(serviceName);
    // Add the null binder and replace the service.
    // The cache should still hold the default service manager.
    fakeservicemanager.addService(serviceName, nullBinder);

    EXPECT_EQ(targetBinder, fakeservicemanager.getService(serviceName));
}
