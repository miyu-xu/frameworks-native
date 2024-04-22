#include <android/os/IServiceManager.h>

#include <BackendUnifiedServiceManager.h>
#include <binder/IServiceManagerFFI.h>

namespace android {
sp<android::os::IServiceManager>
getJavaServicemanagerImplPrivateDoNotUseExceptInTheOnePlaceItIsUsed() {
    return getBackendUnifiedServiceManager();
}

} // namespace android
