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

#include <android-base/logging.h>
#include <android/os/IServiceManager.h>
#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include "../BackendUnifiedServiceManager.h"
#include "fakeservicemanager/FakeServiceManager.h"

#include <sys/prctl.h>
#include <thread>

using namespace android;

#ifdef LIBBINDER_CLIENT_CACHE
constexpr bool kUseLibbinderCache = true;
#else
constexpr bool kUseLibbinderCache = false;
#endif

// A service name which is in the static list of cachable services
const std::string kCachedServiceName = "isub";

#define EXPECT_OK(status)                 \
    do {                                  \
        binder::Status stat = (status);   \
        EXPECT_TRUE(stat.isOk()) << stat; \
    } while (false)

const String16 kServerName = String16("binderCacheUnitTest");

class FooBar : public BBinder {
public:
    status_t onTransact(uint32_t, const Parcel&, Parcel*, uint32_t) {
        // exit the server
        std::thread([] { exit(EXIT_FAILURE); }).detach();
        return OK;
    }
    void killServer(sp<IBinder> binder) {
        Parcel data, reply;
        binder->transact(0, data, &reply, 0);
    }
};

class MockAidlServiceManager : public os::IServiceManagerDefault {
public:
    MockAidlServiceManager() : innerSm() {}

    binder::Status checkService(const ::std::string& name, os::Service* _out) override {
        sp<IBinder> binder = innerSm.getService(String16(name.c_str()));
        os::ServiceWithCacheInfo serviceWithCache{};
        serviceWithCache.service = binder;
        serviceWithCache.isClientSideCacheable = false;
        *_out = os::Service::make<os::Service::Tag::serviceWithCacheInfo>(serviceWithCache);
        return binder::Status::ok();
    }

    binder::Status addService(const std::string& name, const sp<IBinder>& service,
                              bool allowIsolated, int32_t dumpPriority) override {
        return binder::Status::fromStatusT(
                innerSm.addService(String16(name.c_str()), service, allowIsolated, dumpPriority));
    }

    FakeServiceManager innerSm;
};

class LibbinderCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        mBusm = sp<BackendUnifiedServiceManager>::make(sp<MockAidlServiceManager>::make());
    }

    void TearDown() override {}

public:
    // You can define helper methods here.
    void addBinderToSm(const std::string& kCachedServiceName, const sp<IBinder>& binder) {
        EXPECT_OK(mBusm->addService(kCachedServiceName, binder, false, 0));
    }
    void checkService(const std::string& serviceName, sp<IBinder>* _out) {
        os::Service service;
        EXPECT_OK(mBusm->checkService(serviceName, &service));
        ASSERT_EQ(os::Service::Tag::serviceWithCacheInfo, service.getTag());
        *_out = service.get<os::Service::Tag::serviceWithCacheInfo>()->service;
    }

    void cacheAndConfirmCacheHit(const sp<IBinder>& binder1, const sp<IBinder>& binder2) {
        // Add a service
        addBinderToSm(kCachedServiceName, binder1);
        // Get the service. This caches it.
        sp<IBinder> result;
        checkService(kCachedServiceName, &result);
        ASSERT_EQ(binder1, result);

        // Add the different binder and replace the service.
        // The cache should still hold the original binder.
        addBinderToSm(kCachedServiceName, binder2);

        checkService(kCachedServiceName, &result);
        if (kUseLibbinderCache) {
            // If cache is enabled, we should get the binder to Service Manager.
            EXPECT_EQ(binder1, result);
        } else {
            // If cache is disabled, then we should get the newer binder
            EXPECT_EQ(binder2, result);
        }
    }

    sp<BackendUnifiedServiceManager> mBusm;
};

TEST_F(LibbinderCacheTest, AddLocalServiceAndConfirmCacheHit) {
    sp<IBinder> binder1 = sp<BBinder>::make();
    sp<IBinder> binder2 = sp<BBinder>::make();

    cacheAndConfirmCacheHit(binder1, binder2);
}

TEST_F(LibbinderCacheTest, AddRemoteServiceAndConfirmCacheHit) {
    sp<IBinder> binder1 = defaultServiceManager()->checkService(kServerName);
    ASSERT_NE(binder1, nullptr);
    sp<IBinder> binder2 = IInterface::asBinder(mBusm);

    cacheAndConfirmCacheHit(binder1, binder2);
}

TEST_F(LibbinderCacheTest, RemoveFromCacheOnServerDeath) {
    sp<IBinder> binder1 = defaultServiceManager()->checkService(kServerName);
    FooBar foo = FooBar();

    addBinderToSm(kCachedServiceName, binder1);

    // Check Service, this caches the binder
    sp<IBinder> result;
    checkService(kCachedServiceName, &result);
    ASSERT_EQ(binder1, result);

    // Kill the server, this should remove from cache.
    foo.killServer(binder1);
    pid_t pid;
    ASSERT_EQ(OK, binder1->getDebugPid(&pid));
    system(("kill -9 " + std::to_string(pid)).c_str());

    sp<IBinder> binder2 = sp<BBinder>::make();

    // Add new service with the same name.
    // This will replace the service in FakeServiceManager.
    addBinderToSm(kCachedServiceName, binder2);
    sp<IBinder> result2;
    // Confirm that new service is returned instead of old.
    checkService(kCachedServiceName, &result2);
    ASSERT_EQ(binder2, result2);
}

TEST_F(LibbinderCacheTest, NullBinderNotCached) {
    sp<IBinder> binder1 = nullptr;
    sp<IBinder> binder2 = sp<BBinder>::make();

    // Check for a cacheble service which isn't registered.
    // FakeServiceManager should return nullptr.
    // This shouldn't be cached.
    sp<IBinder> result;
    checkService(kCachedServiceName, &result);
    ASSERT_EQ(binder1, result);

    // Add the same service
    addBinderToSm(kCachedServiceName, binder2);

    // This should return the newly added service.
    checkService(kCachedServiceName, &result);
    EXPECT_EQ(binder2, result);
}

TEST_F(LibbinderCacheTest, DoNotCacheServiceNotInList) {
    sp<IBinder> binder1 = sp<BBinder>::make();
    sp<IBinder> binder2 = sp<BBinder>::make();
    std::string serviceName = "NewLibbinderCacheTest";
    // Add a service
    addBinderToSm(serviceName, binder1);
    // Get the service. This shouldn't caches it.
    sp<IBinder> result;
    checkService(serviceName, &result);
    ASSERT_EQ(binder1, result);

    // Add the different binder and replace the service.
    addBinderToSm(serviceName, binder2);

    // Confirm that we get the new service
    checkService(serviceName, &result);
    EXPECT_EQ(binder2, result);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    if (fork() == 0) {
        prctl(PR_SET_PDEATHSIG, SIGHUP);

        // Start a FooBar service and add it to the servicemanager.
        sp<IBinder> server = new FooBar();
        defaultServiceManager()->addService(kServerName, server);

        IPCThreadState::self()->joinThreadPool(true);
        exit(1); // should not reach
    }

    status_t err = ProcessState::self()->setThreadPoolMaxThreadCount(3);
    ProcessState::self()->startThreadPool();
    CHECK_EQ(ProcessState::self()->isThreadPoolStarted(), true);
    CHECK_GT(ProcessState::self()->getThreadPoolMaxTotalThreadCount(), 0);

    auto binder = defaultServiceManager()->waitForService(kServerName);
    CHECK_NE(nullptr, binder.get());
    return RUN_ALL_TESTS();
}
