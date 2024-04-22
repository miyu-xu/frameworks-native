#include <android-base/properties.h>
#include <android/os/BnServiceManager.h>
#include <android/os/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

namespace android {

using AidlRegistrationCallback = IServiceManager::LocalRegistrationCallback;

using AidlServiceManager = android::os::IServiceManager;

class BackendUnifiedServiceManager : public android::os::BnServiceManager {
public:
    explicit BackendUnifiedServiceManager(const sp<AidlServiceManager>& impl)
          : mTheRealServiceManager(impl) {}

    sp<IServiceManager> getImpl() { return mTheRealServiceManager; }
    binder::Status getService(const ::std::string& name, sp<IBinder>* _aidl_return) override {
        return mTheRealServiceManager->getService(name, _aidl_return);
    }
    binder::Status checkService(const ::std::string& name, sp<IBinder>* _aidl_return) override {
        return mTheRealServiceManager->checkService(name, _aidl_return);
    }
    binder::Status addService(const ::std::string& name, const sp<IBinder>& service,
                              bool allowIsolated, int32_t dumpPriority) override {
        return mTheRealServiceManager->addService(name, service, allowIsolated, dumpPriority);
    }
    binder::Status listServices(int32_t dumpPriority,
                                ::std::vector<::std::string>* _aidl_return) override {
        return mTheRealServiceManager->listServices(dumpPriority, _aidl_return);
    }
    binder::Status registerForNotifications(const ::std::string& name,
                                            const sp<os::IServiceCallback>& callback) override {
        sp<os::IServiceCallbackDelegator> _callback;
        if (callback) {
            _callback = sp<os::IServiceCallbackDelegator>::cast(delegate(callback));
        }
        return mTheRealServiceManager->registerForNotifications(name, _callback);
    }
    binder::Status unregisterForNotifications(const ::std::string& name,
                                              const sp<os::IServiceCallback>& callback) override {
        sp<os::IServiceCallbackDelegator> _callback;
        if (callback) {
            _callback = sp<os::IServiceCallbackDelegator>::cast(delegate(callback));
        }
        return mTheRealServiceManager->unregisterForNotifications(name, _callback);
    }
    binder::Status isDeclared(const ::std::string& name, bool* _aidl_return) override {
        return mTheRealServiceManager->isDeclared(name, _aidl_return);
    }
    binder::Status getDeclaredInstances(const ::std::string& iface,
                                        ::std::vector<::std::string>* _aidl_return) override {
        return mTheRealServiceManager->getDeclaredInstances(iface, _aidl_return);
    }
    binder::Status updatableViaApex(const ::std::string& name,
                                    ::std::optional<::std::string>* _aidl_return) override {
        return mTheRealServiceManager->updatableViaApex(name, _aidl_return);
    }
    binder::Status getUpdatableNames(const ::std::string& apexName,
                                     ::std::vector<::std::string>* _aidl_return) override {
        return mTheRealServiceManager->getUpdatableNames(apexName, _aidl_return);
    }
    binder::Status getConnectionInfo(const ::std::string& name,
                                     ::std::optional<os::ConnectionInfo>* _aidl_return) override {
        return mTheRealServiceManager->getConnectionInfo(name, _aidl_return);
    }
    binder::Status registerClientCallback(const ::std::string& name, const sp<IBinder>& service,
                                          const sp<os::IClientCallback>& callback) override {
        sp<os::IClientCallbackDelegator> _callback;
        if (callback) {
            _callback = sp<os::IClientCallbackDelegator>::cast(delegate(callback));
        }
        return mTheRealServiceManager->registerClientCallback(name, service, _callback);
    }
    binder::Status tryUnregisterService(const ::std::string& name,
                                        const sp<IBinder>& service) override {
        return mTheRealServiceManager->tryUnregisterService(name, service);
    }
    binder::Status getServiceDebugInfo(::std::vector<os::ServiceDebugInfo>* _aidl_return) override {
        return mTheRealServiceManager->getServiceDebugInfo(_aidl_return);
    }

private:
    sp<AidlServiceManager> mTheRealServiceManager;
};

[[clang::no_destroy]] static std::once_flag gUSmOnce;
[[clang::no_destroy]] static sp<BackendUnifiedServiceManager> gUnifiedServiceManager;

sp<BackendUnifiedServiceManager> getBackendUnifiedServiceManager() {
    std::call_once(gUSmOnce, []() {
#if defined(__BIONIC__) && !defined(__ANDROID_VNDK__)
        /* wait for service manager */ {
            using std::literals::chrono_literals::operator""s;
            using android::base::WaitForProperty;
            while (!WaitForProperty("servicemanager.ready", "true", 1s)) {
                ALOGE("Waited for servicemanager.ready for a second, waiting another...");
            }
        }
#endif

        sp<AidlServiceManager> sm = nullptr;
        while (sm == nullptr) {
            sm = interface_cast<AidlServiceManager>(
                    ProcessState::self()->getContextObject(nullptr));
            if (sm == nullptr) {
                ALOGE("Waiting 1s on context object on %s.",
                      ProcessState::self()->getDriverName().c_str());
                sleep(1);
            }
        }

        gUnifiedServiceManager = sp<BackendUnifiedServiceManager>::make(sm);
    });

    return gUnifiedServiceManager;
}

sp<AidlServiceManager> getJavaServicemanagerImplPrivateDoNotUseExceptInTheOnePlaceItIsUsed() {
    return getBackendUnifiedServiceManager();
}

} // namespace android