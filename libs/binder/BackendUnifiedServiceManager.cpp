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
#include "BackendUnifiedServiceManager.h"

#include <android-base/strings.h>
#include <android/os/IAccessor.h>
#include <binder/RpcSession.h>

#if defined(__BIONIC__) && !defined(__ANDROID_VNDK__)
#include <android-base/properties.h>
#endif

namespace android {

#ifdef LIBBINDER_CLIENT_CACHE
constexpr bool kUseCache = true;
#else
constexpr bool kUseCache = false;
#endif

using AidlServiceManager = android::os::IServiceManager;
using android::os::IAccessor;
using binder::Status;

static const char* kStaticCachableList[] = {
        // go/keep-sorted start
        "accessibility",
        "account",
        "activity",
        "alarm",
        "android.system.keystore2.IKeystoreService/default",
        "appops",
        "audio",
        "batterystats",
        "carrier_config",
        "connectivity",
        "content",
        "content_capture",
        "device_policy",
        "display",
        "dropbox",
        "econtroller",
        "graphicsstats",
        "input",
        "input_method",
        "isub",
        "jobscheduler",
        "legacy_permission",
        "location",
        "media.extractor",
        "media.metrics",
        "media.player",
        "media.resource_manager",
        "media_resource_monitor",
        "mount",
        "netd_listener",
        "netstats",
        "network_management",
        "nfc",
        "notification",
        "package",
        "package_native",
        "performance_hint",
        "permission",
        "permission_checker",
        "permissionmgr",
        "phone",
        "platform_compat",
        "power",
        "role",
        "sensorservice",
        "statscompanion",
        "telephony.registry",
        "thermalservice",
        "time_detector",
        "trust",
        "uimode",
        "user",
        "virtualdevice",
        "virtualdevice_native",
        "webviewupdate",
        "window",
        // go/keep-sorted end
};

bool BinderCacheWithInvalidation::isClientSideCachingEnabled(const std::string& serviceName) {
    if (ProcessState::self()->getThreadPoolMaxTotalThreadCount() <= 0) {
        ALOGW("Thread Pool max thread count is 0. Cannot cache binder as linkToDeath cannot be "
              "implemented. serviceName: %s",
              serviceName.c_str());
        return false;
    }
    for (const char* name : kStaticCachableList) {
        if (name == serviceName) {
            return true;
        }
    }
    return false;
}

Status BackendUnifiedServiceManager::updateCache(const std::string& serviceName,
                                                 const os::Service& service) {
    if (!kUseCache) {
        return Status::ok();
    }
    if (service.getTag() == os::Service::Tag::binder) {
        sp<IBinder> binder = service.get<os::Service::Tag::binder>();
        if (binder && mCacheForGetService->isClientSideCachingEnabled(serviceName) &&
            binder->isBinderAlive()) {
            return mCacheForGetService->setItem(serviceName, binder);
        }
    }
    return Status::ok();
}

bool BackendUnifiedServiceManager::returnIfCached(const std::string& serviceName,
                                                  os::Service* _out) {
    if (!kUseCache) {
        return false;
    }
    sp<IBinder> item = mCacheForGetService->getItem(serviceName);
    // TODO(b/363177618): Enable caching for binders which are always null.
    if (item != nullptr && item->isBinderAlive()) {
        *_out = os::Service::make<os::Service::Tag::binder>(item);
        return true;
    }
    return false;
}

BackendUnifiedServiceManager::BackendUnifiedServiceManager(const sp<AidlServiceManager>& impl)
      : mTheRealServiceManager(impl) {
    mCacheForGetService = std::make_shared<BinderCacheWithInvalidation>();
}

sp<AidlServiceManager> BackendUnifiedServiceManager::getImpl() {
    return mTheRealServiceManager;
}

Status BackendUnifiedServiceManager::getService(const ::std::string& name,
                                                sp<IBinder>* _aidl_return) {
    os::Service service;
    Status status = getService2(name, &service);
    *_aidl_return = service.get<os::Service::Tag::binder>();
    return status;
}

Status BackendUnifiedServiceManager::getService2(const ::std::string& name, os::Service* _out) {
    if (returnIfCached(name, _out)) {
        return Status::ok();
    }
    os::Service service;
    Status status = mTheRealServiceManager->getService2(name, &service);

    if (status.isOk()) {
        status = toBinderService(service, _out);
        if (status.isOk()) {
            return updateCache(name, service);
        }
    }
    return status;
}

Status BackendUnifiedServiceManager::checkService(const ::std::string& name, os::Service* _out) {
    os::Service service;
    if (returnIfCached(name, _out)) {
        return Status::ok();
    }

    Status status = mTheRealServiceManager->checkService(name, &service);
    if (status.isOk()) {
        status = toBinderService(service, _out);
        if (status.isOk()) {
            return updateCache(name, service);
        }
    }
    return status;
}

Status BackendUnifiedServiceManager::toBinderService(const os::Service& in, os::Service* _out) {
    switch (in.getTag()) {
        case os::Service::Tag::binder: {
            *_out = in;
            return Status::ok();
        }
        case os::Service::Tag::accessor: {
            sp<IBinder> accessorBinder = in.get<os::Service::Tag::accessor>();
            sp<IAccessor> accessor = interface_cast<IAccessor>(accessorBinder);
            if (accessor == nullptr) {
                ALOGE("Service#accessor doesn't have accessor. VM is maybe starting...");
                *_out = os::Service::make<os::Service::Tag::binder>(nullptr);
                return Status::ok();
            }
            auto request = [=] {
                os::ParcelFileDescriptor fd;
                Status ret = accessor->addConnection(&fd);
                if (ret.isOk()) {
                    return base::unique_fd(fd.release());
                } else {
                    ALOGE("Failed to connect to RpcSession: %s", ret.toString8().c_str());
                    return base::unique_fd(-1);
                }
            };
            auto session = RpcSession::make();
            status_t status = session->setupPreconnectedClient(base::unique_fd{}, request);
            if (status != OK) {
                ALOGE("Failed to set up preconnected binder RPC client: %s",
                      statusToString(status).c_str());
                return Status::fromStatusT(status);
            }
            session->setSessionSpecificRoot(accessorBinder);
            *_out = os::Service::make<os::Service::Tag::binder>(session->getRootObject());
            return Status::ok();
        }
        default: {
            LOG_ALWAYS_FATAL("Unknown service type: %d", in.getTag());
        }
    }
}

Status BackendUnifiedServiceManager::addService(const ::std::string& name,
                                                const sp<IBinder>& service, bool allowIsolated,
                                                int32_t dumpPriority) {
    return mTheRealServiceManager->addService(name, service, allowIsolated, dumpPriority);
}
Status BackendUnifiedServiceManager::listServices(int32_t dumpPriority,
                                                  ::std::vector<::std::string>* _aidl_return) {
    return mTheRealServiceManager->listServices(dumpPriority, _aidl_return);
}
Status BackendUnifiedServiceManager::registerForNotifications(
        const ::std::string& name, const sp<os::IServiceCallback>& callback) {
    return mTheRealServiceManager->registerForNotifications(name, callback);
}
Status BackendUnifiedServiceManager::unregisterForNotifications(
        const ::std::string& name, const sp<os::IServiceCallback>& callback) {
    return mTheRealServiceManager->unregisterForNotifications(name, callback);
}
Status BackendUnifiedServiceManager::isDeclared(const ::std::string& name, bool* _aidl_return) {
    return mTheRealServiceManager->isDeclared(name, _aidl_return);
}
Status BackendUnifiedServiceManager::getDeclaredInstances(
        const ::std::string& iface, ::std::vector<::std::string>* _aidl_return) {
    return mTheRealServiceManager->getDeclaredInstances(iface, _aidl_return);
}
Status BackendUnifiedServiceManager::updatableViaApex(
        const ::std::string& name, ::std::optional<::std::string>* _aidl_return) {
    return mTheRealServiceManager->updatableViaApex(name, _aidl_return);
}
Status BackendUnifiedServiceManager::getUpdatableNames(const ::std::string& apexName,
                                                       ::std::vector<::std::string>* _aidl_return) {
    return mTheRealServiceManager->getUpdatableNames(apexName, _aidl_return);
}
Status BackendUnifiedServiceManager::getConnectionInfo(
        const ::std::string& name, ::std::optional<os::ConnectionInfo>* _aidl_return) {
    return mTheRealServiceManager->getConnectionInfo(name, _aidl_return);
}
Status BackendUnifiedServiceManager::registerClientCallback(
        const ::std::string& name, const sp<IBinder>& service,
        const sp<os::IClientCallback>& callback) {
    return mTheRealServiceManager->registerClientCallback(name, service, callback);
}
Status BackendUnifiedServiceManager::tryUnregisterService(const ::std::string& name,
                                                          const sp<IBinder>& service) {
    return mTheRealServiceManager->tryUnregisterService(name, service);
}
Status BackendUnifiedServiceManager::getServiceDebugInfo(
        ::std::vector<os::ServiceDebugInfo>* _aidl_return) {
    return mTheRealServiceManager->getServiceDebugInfo(_aidl_return);
}

[[clang::no_destroy]] static std::once_flag gUSmOnce;
[[clang::no_destroy]] static sp<BackendUnifiedServiceManager> gUnifiedServiceManager;

static bool isSmInstalled() {
#ifndef BINDER_WITH_KERNEL_IPC
    return false;
#else
#if defined(__BIONIC__) && !defined(__ANDROID_VNDK__)
    return android::base::GetBoolProperty("servicemanager.installed", true);
#else
    return true;
#endif
#endif // BINDER_WITH_KERNEL_IPC
}

class LocalAccessorServiceManager : public android::os::BnServiceManager {
public:
    Status getService2(const std::string& name, android::os::Service* service) override {
        os::Service accessor;
        Status status = getInjectedAccessor(name, &accessor);
        if (!status.isOk() || accessor.getTag() != os::Service::Tag::accessor ||
            accessor.get<os::Service::Tag::accessor>() == nullptr) {
            *service = os::Service::make<os::Service::Tag::binder>(nullptr);
            return status;
        }
        *service = accessor;

        return Status::ok();
    }

    Status getService(const std::string& name, sp<IBinder>* _aidl_return) override {
        os::Service service;
        Status status = getService2(name, &service);
        *_aidl_return = service.get<os::Service::Tag::binder>();
        return status;
    }
    Status checkService(const std::string& name, android::os::Service* service) override {
        return getService2(name, service);
    }
    Status addService(const std::string&, const android::sp<android::IBinder>&, bool,
                      int32_t) override {
        return Status::ok();
    }
    Status listServices(int32_t, std::vector<std::string>* list) override {
        if (list == nullptr) return Status::fromExceptionCode(Status::EX_NULL_POINTER);
        listInjectedAccessors(list);
        return Status::ok();
    }
    Status registerForNotifications(const std::string&,
                                    const android::sp<android::os::IServiceCallback>&) override {
        // TODO does this make sense to have? This would allow services to appear
        // later and the clients to register to hear about them.
        return Status::ok();
    }
    Status unregisterForNotifications(const std::string&,
                                      const android::sp<android::os::IServiceCallback>&) override {
        return Status::ok();
    }
    Status isDeclared(const std::string& instance, bool* _aidl_return) override {
        std::vector<std::string> list;
        listInjectedAccessors(&list);
        // Declared instances must have the format
        // <interface>/instance like foo.bar.ISomething/instance
        if (std::find(list.begin(), list.end(), instance) != list.end() &&
            base::Tokenize(instance, "/").size() == 2) {
            *_aidl_return = true;
        } else {
            *_aidl_return = false;
        }
        return Status::ok();
    }
    Status getDeclaredInstances(const std::string& interface,
                                std::vector<std::string>* instances) override {
        if (instances == nullptr) return Status::fromExceptionCode(Status::EX_NULL_POINTER);
        std::vector<std::string> list;
        listInjectedAccessors(&list);
        std::for_each(list.begin(), list.end(), [&](const std::string& instance) {
            // Declared instances must have the format
            // <interface>/instance like foo.bar.ISomething/instance
            std::vector<std::string> tokens = base::Tokenize(instance, "/");
            if (tokens.size() == 2 && tokens[0] == interface) {
                instances->push_back(tokens[1]);
            }
        });

        return Status::ok();
    }
    Status updatableViaApex(const std::string&, std::optional<std::string>* _aidl_return) override {
        *_aidl_return = std::nullopt;
        return Status::ok();
    }
    Status getUpdatableNames(const std::string&, std::vector<std::string>*) override {
        return Status::ok();
    }
    Status getConnectionInfo(const std::string&,
                             std::optional<android::os::ConnectionInfo>* _aidl_return) override {
        *_aidl_return = std::nullopt;
        return Status::ok();
    }
    Status registerClientCallback(const std::string&, const android::sp<android::IBinder>&,
                                  const android::sp<android::os::IClientCallback>&) override {
        return Status::fromStatusT(android::INVALID_OPERATION);
    }
    Status tryUnregisterService(const std::string&, const android::sp<android::IBinder>&) override {
        return Status::ok();
    }
    Status getServiceDebugInfo(std::vector<android::os::ServiceDebugInfo>*) override {
        return Status::ok();
    }
};

class MultiServiceManager : public android::os::BnServiceManager {
public:
    MultiServiceManager(std::vector<sp<AidlServiceManager>>&& managers) : mManagers(managers) {
        LOG_ALWAYS_FATAL_IF(managers.empty(), "There must always be at least one service manager");
        for (const auto& manager : mManagers) {
            LOG_ALWAYS_FATAL_IF(manager == nullptr, "Don't add invalid managers...");
        }
    }

    Status getService2(const std::string& name, android::os::Service* service) override {
        for (const auto& manager : mManagers) {
            auto status = manager->getService2(name, service);
            if (!status.isOk()) return status;
            switch (service->getTag()) {
                case os::Service::Tag::binder: {
                    if (service->get<os::Service::Tag::binder>() != nullptr) {
                        return status;
                    }
                } break;
                case os::Service::Tag::accessor: {
                    if (service->get<os::Service::Tag::accessor>() != nullptr) {
                        return status;
                    }
                } break;
                default: {
                    LOG_ALWAYS_FATAL("Unknown service type: %d", service->getTag());
                }
            }
        }
        return Status::ok();
    }

    Status getService(const std::string& name, sp<IBinder>* service) override {
        for (const auto& manager : mManagers) {
            auto status = manager->getService(name, service);
            if (!status.isOk()) return status;
            if (*service != nullptr) {
                return status;
            }
        }
        return Status::ok();
    }

    Status checkService(const std::string& name, android::os::Service* service) override {
        for (const auto& manager : mManagers) {
            auto status = manager->checkService(name, service);
            if (!status.isOk()) return status;
            switch (service->getTag()) {
                case os::Service::Tag::binder: {
                    if (service->get<os::Service::Tag::binder>() != nullptr) {
                        return status;
                    }
                } break;
                case os::Service::Tag::accessor: {
                    if (service->get<os::Service::Tag::accessor>() != nullptr) {
                        return status;
                    }
                } break;
                default: {
                    LOG_ALWAYS_FATAL("Unknown service type: %d", service->getTag());
                }
            }
        }
        return Status::ok();
    }

    Status addService(const std::string& name, const android::sp<android::IBinder>& service,
                      bool allowIsolated, int32_t dumpsysPriority) override {
        for (const auto& manager : mManagers) {
            auto status = manager->addService(name, service, allowIsolated, dumpsysPriority);
            if (!status.isOk()) return status;
        }
        return Status::ok();
    }

    Status listServices(int32_t dumpPriority, std::vector<std::string>* _aidl_return) override {
        for (const auto& manager : mManagers) {
            std::vector<std::string> services;
            auto status = manager->listServices(dumpPriority, &services);
            if (!status.isOk()) return status;
            _aidl_return->insert(_aidl_return->end(), services.begin(), services.end());
        }
        return Status::ok();
    }

    Status registerForNotifications(
            const std::string& name,
            const android::sp<android::os::IServiceCallback>& callback) override {
        for (const auto& manager : mManagers) {
            auto status = manager->registerForNotifications(name, callback);
            if (!status.isOk()) return status;
        }
        return Status::ok();
    }

    Status unregisterForNotifications(
            const std::string& name,
            const android::sp<android::os::IServiceCallback>& callback) override {
        Status status;
        for (const auto& manager : mManagers) {
            auto status = manager->unregisterForNotifications(name, callback);
            if (!status.isOk()) {
                // Log each failure because we will lose the status value after
                // trying the next manager
                ALOGE("Failed to unregister for notifications for %s with status: %s", name.c_str(),
                      status.toString8().c_str());
            }
        }
        return status;
    }

    Status isDeclared(const std::string& name, bool* _aidl_return) override {
        bool isDeclared = false;
        for (const auto& manager : mManagers) {
            bool ret = false;
            auto status = manager->isDeclared(name, &ret);
            if (!status.isOk()) return status;
            if (ret) isDeclared = true;
        }
        *_aidl_return = isDeclared;
        return Status::ok();
    }

    Status getDeclaredInstances(const std::string& iface,
                                std::vector<std::string>* _aidl_return) override {
        for (const auto& manager : mManagers) {
            std::vector<std::string> services;
            auto status = manager->getDeclaredInstances(iface, &services);
            if (!status.isOk()) return status;
            _aidl_return->insert(_aidl_return->end(), services.begin(), services.end());
        }
        return Status::ok();
    }

    Status updatableViaApex(const std::string& name,
                            std::optional<std::string>* _aidl_return) override {
        std::optional<std::string> updatable = std::nullopt;
        for (const auto& manager : mManagers) {
            auto status = manager->updatableViaApex(name, &updatable);
            if (!status.isOk()) return status;
            if (updatable) {
                *_aidl_return = updatable;
                return status;
            }
        }
        *_aidl_return = std::nullopt;
        return Status::ok();
    }

    Status getUpdatableNames(const std::string& apexName,
                             std::vector<std::string>* _aidl_return) override {
        for (const auto& manager : mManagers) {
            std::vector<std::string> names;
            auto status = manager->getUpdatableNames(apexName, &names);
            if (!status.isOk()) return status;
            _aidl_return->insert(_aidl_return->end(), names.begin(), names.end());
        }
        return Status::ok();
    }

    Status getConnectionInfo(const std::string& name,
                             std::optional<android::os::ConnectionInfo>* _aidl_return) override {
        std::optional<android::os::ConnectionInfo> info = std::nullopt;
        for (const auto& manager : mManagers) {
            auto status = manager->getConnectionInfo(name, &info);
            if (!status.isOk()) return status;
            if (info) {
                *_aidl_return = info;
                return status;
            }
        }
        return Status::ok();
    }

    Status registerClientCallback(
            const std::string& name, const android::sp<android::IBinder>& service,
            const android::sp<android::os::IClientCallback>& callback) override {
        for (const auto& manager : mManagers) {
            auto status = manager->registerClientCallback(name, service, callback);
            // only want one successful registration for a callback
            if (status.isOk()) return status;
        }
        return Status::ok();
    }

    Status tryUnregisterService(const std::string& name,
                                const android::sp<android::IBinder>& service) override {
        for (const auto& manager : mManagers) {
            auto status = manager->tryUnregisterService(name, service);
            if (!status.isOk()) return status;
        }
        return Status::ok();
    }

    Status getServiceDebugInfo(std::vector<android::os::ServiceDebugInfo>* _aidl_return) override {
        for (const auto& manager : mManagers) {
            std::vector<android::os::ServiceDebugInfo> debug;
            auto status = manager->getServiceDebugInfo(&debug);
            if (!status.isOk()) return status;
            _aidl_return->insert(_aidl_return->end(), debug.begin(), debug.end());
        }
        return Status::ok();
    }

private:
    std::vector<sp<AidlServiceManager>> mManagers;
};

sp<BackendUnifiedServiceManager> getBackendUnifiedServiceManager() {
    std::call_once(gUSmOnce, []() {
#if defined(__BIONIC__) && !defined(__ANDROID_VNDK__)
        /* wait for service manager */
        if (isSmInstalled()) {
            using std::literals::chrono_literals::operator""s;
            using android::base::WaitForProperty;
            while (!WaitForProperty("servicemanager.ready", "true", 1s)) {
                ALOGE("Waited for servicemanager.ready for a second, waiting another...");
            }
        }
#endif

        sp<AidlServiceManager> sm = nullptr;
        while (isSmInstalled() && sm == nullptr) {
            sm = interface_cast<AidlServiceManager>(
                    ProcessState::self()->getContextObject(nullptr));
            if (sm == nullptr) {
                ALOGE("Waiting 1s on context object on %s.",
                      ProcessState::self()->getDriverName().c_str());
                sleep(1);
            }
        }
        std::vector<sp<AidlServiceManager>> sms;
        sms.push_back(sp<LocalAccessorServiceManager>::make());
        if (sm) {
            sms.push_back(sm);
        } else {
            ALOGI("There is no kernel binder servicemanager process, so only direct binder RPC "
                  "service management is supported.");
        }
        gUnifiedServiceManager = sp<BackendUnifiedServiceManager>::make(
                sp<MultiServiceManager>::make(std::move(sms)));
    });

    return gUnifiedServiceManager;
}

} // namespace android
