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

#include <sys/prctl.h>

using namespace android;

#ifdef LIBBINDER_CLIENT_CACHE
constexpr bool kUseLibbinderCache = true;
#else
constexpr bool kUseLibbinderCache = false;
#endif

const String16 kServerName = String16("testService");

class FooBar : public BBinder {
public:
    enum {
        TRANSACTION_REPEAT_STRING = IBinder::FIRST_CALL_TRANSACTION,
    };

    status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
        switch (code) {
            case TRANSACTION_REPEAT_STRING: {
                const char* str = data.readCString();
                return reply->writeCString(str == nullptr ? "<null>" : str);
            }
        }
        return BBinder::onTransact(code, data, reply, flags);
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

    binder::Status addService(const std::string& name, const sp<::IBinder>& service,
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
        // set max threadpool count
        status_t err = ProcessState::self()->setThreadPoolMaxThreadCount(3);
        ProcessState::self()->startThreadPool();
        ASSERT_TRUE(ProcessState::self()->isThreadPoolStarted() == true);
        ASSERT_TRUE(ProcessState::self()->getThreadPoolMaxTotalThreadCount() > 0);
    }

    void TearDown() override {
        // Code here will be called after each test.
    }

public:
    // You can define helper methods here.
    void addBinderToSm(const std::string& mServiceName, const sp<IBinder>& binder) {
        ASSERT_EQ(binder::Status::EX_NONE,
                  mBusm->addService(mServiceName, binder, false, 0).exceptionCode());
    }
    void checkService(const std::string& mServiceName, sp<IBinder>* _out) {
        os::Service service;
        ASSERT_EQ(binder::Status::EX_NONE,
                  mBusm->checkService(mServiceName, &service).exceptionCode());
        ASSERT_EQ(os::Service::Tag::binder, service.getTag());
        *_out = service.get<os::Service::Tag::binder>();
    }

    void cacheAndConfirmCacheHit(const sp<IBinder>& binder1, const sp<IBinder>& binder2) {
        // Add a service (in this case the default service manager)
        this->addBinderToSm(mServiceName, binder1);
        // Get the service. This caches it.
        sp<IBinder> result;
        this->checkService(mServiceName, &result);
        ASSERT_EQ(binder1, result);

        // Add the different binder and replace the service.
        // The cache should still hold the default service manager.
        this->addBinderToSm(mServiceName, binder2);

        this->checkService(mServiceName, &result);
        if (!kUseLibbinderCache) {
            // If cache is disabled, then we should get the newer binder
            EXPECT_EQ(binder2, result);
        } else {
            // If cache is enabled, we should get the binder to Service Manager.
            EXPECT_EQ(binder1, result);
        }
    }

    sp<BackendUnifiedServiceManager> mBusm;
    std::string mServiceName = "isub";
};

TEST_F(LibbinderCacheTest, AddLocalServiceAndConfirmCacheHit) {
    // A service name which is in the static list of cachable services
    std::string mServiceName = "isub";

    sp<IBinder> binder1 = IInterface::asBinder(defaultServiceManager());
    sp<IBinder> binder2 = IInterface::asBinder(this->mBusm);

    cacheAndConfirmCacheHit(binder1, binder2);
}

TEST_F(LibbinderCacheTest, AddRemoteServiceAndConfirmCacheHit) {
    // A service name which is in the static list of cachable services
    std::string mServiceName = "isub";

    sp<IBinder> binder1 = defaultServiceManager()->checkService(kServerName);
    ASSERT_TRUE(binder1 != nullptr);
    sp<IBinder> binder2 = IInterface::asBinder(mBusm);

    cacheAndConfirmCacheHit(binder1, binder2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    if (fork() == 0) {
        prctl(PR_SET_PDEATHSIG, SIGHUP);

        sp<IBinder> server = new FooBar();
        android::defaultServiceManager()->addService(kServerName, server);

        IPCThreadState::self()->joinThreadPool(true);
        exit(1); // should not reach
    }

    // This is not racey. Just giving these services some time to register before we call
    // checkService which sleeps for much longer.
    usleep(100000);

    return RUN_ALL_TESTS();
}
