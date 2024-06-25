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

using namespace android;

#ifdef LIBBINDER_CLIENT_CACHE
constexpr bool kUseLibbinderCache = true;
#else
constexpr bool kUseLibbinderCache = false;
#endif

#define EXPECT_OK(status)                        \
    do {                                         \
        binder::Status stat = (status); \
        EXPECT_TRUE(stat.isOk()) << stat;        \
    } while (false)

const String16 kServerName = String16("binderCacheUnitTest");

class FooBar : public BBinder {
public:
    enum {
        TRANSACTION_REPEAT_STRING = IBinder::LAST_CALL_TRANSACTION,
    };
    status_t onTransact(uint32_t , const Parcel& , Parcel* , uint32_t) {
        // exit the server
        exit(EXIT_SUCCESS);
    }
    void killServer(sp<IBinder> binder) {
        Parcel data, reply;
        binder->transact(TRANSACTION_REPEAT_STRING, data, &reply, FLAG_CLEAR_BUF);
    }
};

class MockAidlServiceManager : public os::IServiceManagerDefault {
public:
    MockAidlServiceManager() : innerSm() {}

    binder::Status checkService(const ::std::string& name, os::Service* _out) override {
        sp<IBinder> binder = innerSm.getService(String16(name.c_str()));
        *_out = os::Service::make<os::Service::Tag::binder>(binder);
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
    void addBinderToSm(const std::string& mServiceName, const sp<IBinder>& binder) {
        EXPECT_OK(mBusm->addService(mServiceName, binder, false, 0));
    }
    void checkService(const std::string& mServiceName, sp<IBinder>* _out) {
        os::Service service;
        EXPECT_OK(mBusm->checkService(mServiceName, &service));
        ASSERT_EQ(os::Service::Tag::binder, service.getTag());
        *_out = service.get<os::Service::Tag::binder>();
    }

    void cacheAndConfirmCacheHit(const sp<IBinder>& binder1, const sp<IBinder>& binder2) {
        // Add a service
        addBinderToSm(mServiceName, binder1);
        // Get the service. This caches it.
        sp<IBinder> result;
        checkService(mServiceName, &result);
        ASSERT_EQ(binder1, result);

        // Add the different binder and replace the service.
        // The cache should still hold the original binder.
        addBinderToSm(mServiceName, binder2);

        checkService(mServiceName, &result);
        if (kUseLibbinderCache) {
            // If cache is enabled, we should get the binder to Service Manager.
            EXPECT_EQ(binder1, result);
        } else {
            // If cache is disabled, then we should get the newer binder
            EXPECT_EQ(binder2, result);
        }
    }

    sp<BackendUnifiedServiceManager> mBusm;
    // A service name which is in the static list of cachable services
    std::string mServiceName = "isub";
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

    // Add service
    addBinderToSm(mServiceName, binder1);

    // Check Service, this caches the binder
    sp<IBinder> result;
    checkService(mServiceName, &result);
    ASSERT_EQ(binder1, result);

    // Kill the server, this should remove from cache.
    foo.killServer(binder1);
    usleep(50);
    sp<IBinder> binder2 = sp<BBinder>::make();

    // Add new service with the same name.
    // This will replace the service in FakeServiceManager.
    addBinderToSm(mServiceName, binder2);
    sp<IBinder> result2;
    // Confirm that new service is returned instead of old.
    checkService(mServiceName, &result2);
    ASSERT_EQ(binder2, result2);
}

TEST_F(LibbinderCacheTest, NullBinderNotCached) {
    sp<IBinder> binder1 = nullptr;
    sp<IBinder> binder2 = sp<BBinder>::make();

    // Check for a cacheble service which isn't registered.
    // FakeServiceManager should return nullptr.
    // This shouldn't be cached.
    sp<IBinder> result;
    checkService(mServiceName, &result);
    ASSERT_EQ(binder1, result);

    // Add the same service
    addBinderToSm(mServiceName, binder2);

    // This should return the newly added service.
    checkService(mServiceName, &result);
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
