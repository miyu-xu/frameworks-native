#include <android/os/IServiceManager.h>

#include <binder/IServiceManagerFFI.h>
#include "BackendUnifiedServiceManager.h"

namespace android {
sp<android::os::IServiceManager>
getJavaServicemanagerImplPrivateDoNotUseExceptInTheOnePlaceItIsUsed() {
    return getBackendUnifiedServiceManager();
}

} // namespace android
