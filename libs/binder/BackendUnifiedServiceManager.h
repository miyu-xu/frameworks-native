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
#pragma once

#include <android/os/BnServiceManager.h>
#include <android/os/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <unordered_map>
#include <unordered_set>

namespace android {

static std::unordered_set<std::string> static_cache_list = {
        std::string("permissionmgr"),
        std::string("legacy_permission"),
        std::string("media.resource_manager"),
};

class BinderCacheWithInvalidation {
    class BinderInvalidation : public IBinder::DeathRecipient {
    public:
        BinderInvalidation(std::unordered_map<std::string, sp<IBinder>>& cache, std::mutex& lock,
                           const std::string& key)
              : mCache(cache), mLock(lock), mKey(key) {}

        void binderDied(const android::wp<android::IBinder>& who) override {
            sp<IBinder> binder = who.promote();
            std::unique_lock<std::mutex> lock(mLock);
            if (auto it = mCache.find(mKey); it != mCache.end()) {
                if (it->second == binder) {
                    mCache.erase(mKey);
                }
            }
        }

    private:
        std::unordered_map<std::string, sp<IBinder>>& mCache;
        std::mutex& mLock;
        std::string mKey;
    };

public:
    sp<IBinder> getItem(const std::string& key) const {
        std::unique_lock<std::mutex> lock(mCacheMutex);
        if (auto it = mCache.find(key); it != mCache.end()) {
            return it->second;
        }
        return nullptr;
    }

    void addItem(const std::string& key, const sp<IBinder>& item) {
        std::string key_u16str = std::string(key);
        {
            std::unique_lock<std::mutex> lock(mCacheMutex);
            mCache[key_u16str] = item;
        }
        sp<BinderInvalidation> deathRecipient =
                sp<BinderInvalidation>::make(mCache, mCacheMutex, key_u16str);
        status_t status = item->linkToDeath(deathRecipient);
        if (status != android::OK) {
            ALOGE("Failed to linkToDeath. Error: %d", status);
            std::unique_lock<std::mutex> lock(mCacheMutex);
            mCache.erase(key_u16str);
        }
    }

    bool isClientSideCachingEnabled(const std::string& service_name) {
        if (ProcessState::self()->getThreadPoolMaxTotalThreadCount() <= 0) {
            return false;
        }
        return isClientSideCachingEnabledStatic(service_name) ||
                isClientSideCachingEnabledDynamic(service_name);
    }

    void addToDynamicAllowList(const std::string& service_name) {
        mDynamicAllowCacheList.insert(service_name);
    }

private:
    static bool isClientSideCachingEnabledStatic(const std::string& service_name) {
        return static_cache_list.contains(service_name);
    }

    bool isClientSideCachingEnabledDynamic(const std::string& service_name) {
        return mDynamicAllowCacheList.contains(service_name);
    }

    std::unordered_map<std::string, sp<IBinder>> mCache;
    mutable std::mutex mCacheMutex;
    std::unordered_set<std::string> mDynamicAllowCacheList;
};

class BackendUnifiedServiceManager : public android::os::BnServiceManager {
public:
    explicit BackendUnifiedServiceManager(const sp<os::IServiceManager>& impl);

    sp<os::IServiceManager> getImpl();
    binder::Status getService(const ::std::string& name, os::Service* out) override;
    binder::Status checkService(const ::std::string& name, os::Service* out) override;
    binder::Status addService(const ::std::string& name, const sp<IBinder>& service,
                              bool allowIsolated, int32_t dumpPriority) override;
    binder::Status listServices(int32_t dumpPriority,
                                ::std::vector<::std::string>* _aidl_return) override;
    binder::Status registerForNotifications(const ::std::string& name,
                                            const sp<os::IServiceCallback>& callback) override;
    binder::Status unregisterForNotifications(const ::std::string& name,
                                              const sp<os::IServiceCallback>& callback) override;
    binder::Status isDeclared(const ::std::string& name, bool* _aidl_return) override;
    binder::Status getDeclaredInstances(const ::std::string& iface,
                                        ::std::vector<::std::string>* _aidl_return) override;
    binder::Status updatableViaApex(const ::std::string& name,
                                    ::std::optional<::std::string>* _aidl_return) override;
    binder::Status getUpdatableNames(const ::std::string& apexName,
                                     ::std::vector<::std::string>* _aidl_return) override;
    binder::Status getConnectionInfo(const ::std::string& name,
                                     ::std::optional<os::ConnectionInfo>* _aidl_return) override;
    binder::Status registerClientCallback(const ::std::string& name, const sp<IBinder>& service,
                                          const sp<os::IClientCallback>& callback) override;
    binder::Status tryUnregisterService(const ::std::string& name,
                                        const sp<IBinder>& service) override;
    binder::Status getServiceDebugInfo(::std::vector<os::ServiceDebugInfo>* _aidl_return) override;

    // for legacy ABI
    const String16& getInterfaceDescriptor() const override {
        return mTheRealServiceManager->getInterfaceDescriptor();
    }

    IBinder* onAsBinder() override { return IInterface::asBinder(mTheRealServiceManager).get(); }

private:
    BinderCacheWithInvalidation mCacheForGetService;
    sp<os::IServiceManager> mTheRealServiceManager;
    void toBinderService(const os::Service& in, os::Service* _out);
};

sp<BackendUnifiedServiceManager> getBackendUnifiedServiceManager();

} // namespace android