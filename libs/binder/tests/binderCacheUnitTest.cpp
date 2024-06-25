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

#include <android/os/IServiceManager.h>
#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include "../BackendUnifiedServiceManager.h"
#include "fakeservicemanager/FakeServiceManager.h"

using namespace android;

#ifdef LIBBINDER_CLIENT_CACHE
constexpr bool kUseLibbinderCache = true;
#else
constexpr bool kUseLibbinderCache = false;
#endif

class MockAidlServiceManager : public os::IServiceManagerDefault {
public:
    MockAidlServiceManager() : innerSm() {}

    binder::Status checkService(const ::std::string& name, os::Service* _out) {
        sp<IBinder> binder = innerSm.getService(String16(name.c_str()));
        *_out = os::Service::make<os::Service::Tag::binder>(binder);
        return binder::Status::ok();
    }

    binder::Status addService(const std::string& name, const sp<::IBinder>& service,
                              bool allowIsolated, int32_t dumpPriority) override {
        return binder::Status::fromStatusT(
                innerSm.addService(String16(name.c_str()), service, allowIsolated, dumpPriority));
    }

    FakeServiceManager innerSm;
};

TEST(LibbinderCache, AddServiceAndConfirmCacheHit) {
    sp<BackendUnifiedServiceManager> busm =
            sp<BackendUnifiedServiceManager>::make(sp<MockAidlServiceManager>::make());

    // A service name which is in the static list of cachable services
    std::string serviceName = "isub";
    sp<IBinder> binder1 = IInterface::asBinder(defaultServiceManager());
    sp<IBinder> binder2 = IInterface::asBinder(busm);

    // Add a service (in this case the default service manager)
    ASSERT_EQ(binder::Status::EX_NONE,
              busm->addService(serviceName, binder1, false, 0).exceptionCode());
    // Get the service. This caches it.
    os::Service targetService;
    ASSERT_EQ(binder::Status::EX_NONE,
              busm->checkService(serviceName, &targetService).exceptionCode());
    ASSERT_EQ(os::Service::Tag::binder, targetService.getTag());
    ASSERT_EQ(binder1, targetService.get<os::Service::Tag::binder>());

    // Add the different binder and replace the service.
    // The cache should still hold the default service manager.
    ASSERT_EQ(binder::Status::EX_NONE,
              busm->addService(serviceName, binder2, false, 0).exceptionCode());
    os::Service resultService;
    ASSERT_EQ(binder::Status::EX_NONE,
              busm->checkService(serviceName, &resultService).exceptionCode());

    if (!kUseLibbinderCache) {
        // If cache is disabled, then we should get the nullBinder

        EXPECT_EQ(binder2, targetService.get<os::Service::Tag::binder>());
    } else {
        // If cache is enabled, we should get the binder to Service Manager.
        EXPECT_EQ(binder1, targetService.get<os::Service::Tag::binder>());
    }
}
