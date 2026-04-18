#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <binder/Parcel.h>
#include <utils/Log.h>
#include <utils/String16.h>

namespace android {

// Windows stub implementation for IResultReceiver
class IResultReceiver : public IInterface {
public:
    DECLARE_META_INTERFACE(ResultReceiver)
    
    virtual void send(int32_t resultCode) = 0;
};

// Implementation of asInterface for IResultReceiver
sp<IResultReceiver> IResultReceiver::asInterface(const sp<IBinder>& obj) {
    // For RPC-only build, return nullptr as this is not needed
    ALOGW("IResultReceiver::asInterface called in RPC-only build, returning nullptr");
    return nullptr;
}

// Define the interface descriptor
const String16& IResultReceiver::getInterfaceDescriptor() const {
    static String16 descriptor("android.os.IResultReceiver");
    return descriptor;
}

// Implement the BnInterface for IResultReceiver
class BnResultReceiver : public BnInterface<IResultReceiver> {
public:
    virtual status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0) {
        ALOGW("BnResultReceiver::onTransact called in RPC-only build");
        return UNKNOWN_TRANSACTION;
    }
};

} // namespace android
