#pragma once

#include <windows.h>
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <vector>

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>

namespace android {

class WindowsServiceManager : public IServiceManager {
public:
    static WindowsServiceManager* getInstance();
    
    // IServiceManager interface
    virtual sp<IBinder> getService(const String16& name) override;
    virtual sp<IBinder> checkService(const String16& name) override;
    virtual status_t addService(const String16& name, const sp<IBinder>& service,
                                bool allowIsolated = false,
                                int dumpFlags = IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT) override;
    virtual Vector<String16> listServices() override;
    virtual sp<IBinder> waitForService(const String16& name) override;
    virtual bool isDeclared(const String16& name) override;
    virtual Vector<String16> getDeclaredInstances(const String16& interface) override;
    virtual std::optional<String16> updatableViaApex(const String16& name) override;
    virtual status_t registerForNotifications(const String16& name,
                                              const sp<LocalRegistrationCallback>& callback) override;
    virtual status_t unregisterForNotifications(const String16& name,
                                                const sp<LocalRegistrationCallback>& callback) override;
    virtual status_t tryUnregister() override;
    virtual void reRegister() override;
    virtual void forcePersist(bool persist) override;
    virtual void setActiveServicesCallback(const std::function<bool(bool)>& callback) override;

private:
    WindowsServiceManager();
    ~WindowsServiceManager();
    
    static WindowsServiceManager* sInstance;
    static std::mutex sInstanceMutex;
    
    std::mutex mLock;
    std::map<String16, sp<IBinder>> mServices;
    std::map<String16, std::vector<sp<LocalRegistrationCallback>>> mNotificationCallbacks;
    
    // Helper methods
    void notifyServiceAdded(const String16& name, const sp<IBinder>& service);
};

} // namespace android
