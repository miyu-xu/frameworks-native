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

#include <android/binder_rpc.h>
#include <android/os/IAccessor.h>
#include <binder/IServiceManager.h>
#include <linux/vm_sockets.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <variant>

#include "ibinder_internal.h"
#include "status_internal.h"

using ::android::defaultServiceManager;
using ::android::IBinder;
using ::android::IServiceManager;
using ::android::sp;
using ::android::String16;
using ::android::String8;
using ::android::binder::Status;
using ::android::os::IAccessor;

struct ARpc_ConnectionInfo {
    // TODO support more connection types
    std::variant<sockaddr_vm, sockaddr_un> addr;
};

struct ARpc_Accessor final : public ::android::RefBase {
    static ARpc_Accessor* make(const char* instance, sp<IBinder> binder) {
        LOG_ALWAYS_FATAL_IF(binder == nullptr, "ARpc_Accessor requires a non-null binder");
        sp<IAccessor> accessor = checked_interface_cast<IAccessor>(binder);
        if (accessor == nullptr) {
            ALOGE("Attempting to create an ARpc_Accessor for %s but the AIBinder is not associated "
                  "with IAccessor",
                  instance);
            return nullptr;
        }
        String16 reportedInstance;
        Status status = accessor->getInstanceName(&reportedInstance);
        if (status.isOk()) {
            if (0 == strcmp(String8(reportedInstance).c_str(), instance)) {
                return new ARpc_Accessor(binder);
            } else {
                ALOGE("Instance %s doesn't match the Accessor's instance of %s", instance,
                      String8(reportedInstance).c_str());
                return nullptr;
            }
        }
        ALOGE("Failed to validate the binder being used to create a new ARpc_Accessor for %s with "
              "status: %s",
              instance, status.toString8().c_str());
        return nullptr;
    }

    sp<IBinder> asBinder() { return mAccessorBinder; }

    ~ARpc_Accessor() { ALOGI("ARpc_Accessor dtor"); }

   private:
    ARpc_Accessor(sp<IBinder> accessor) : mAccessorBinder(accessor) {}
    ARpc_Accessor() = delete;
    sp<IBinder> mAccessorBinder;
};

binder_exception_t ARpc_addAccessorProvider(ARpc_AccessorProvider provider, void* data,
                                            ARpc_AccessorProviderUserData_delete onDelete) {
    if (provider == nullptr) {
        ALOGE("Null provider passed to ARpc_addAccessorProvider");
        return EX_ILLEGAL_ARGUMENT;
    }
    if (data && onDelete == nullptr) {
        ALOGE("If a non-null data ptr is passed to ARpc_addAccessorProvider, then a "
              "ARpc_AccessorProviderUserData_delete callback must alse be passed to delete "
              "the data object once the AccessorProvider is removed.");
        return EX_ILLEGAL_ARGUMENT;
    }
    std::function<sp<IBinder>(const String16& name)> generate =
            [provider, data](const String16& name) -> sp<IBinder> {
        ARpc_Accessor* accessor = provider(String8(name).c_str(), data);
        if (accessor == nullptr) {
            ALOGI("The supplied ARpc_AccessorProvider returned nullptr");
            return nullptr;
        }
        sp<IBinder> binder = accessor->asBinder();
        ARpc_Accessor_delete(accessor);
        return binder;
    };
    std::function<void()> onDeleteCb;
    if (onDelete) {
        onDeleteCb = [data, onDelete] { onDelete(data); };
    }

    return PruneException(android::addAccessorProvider(std::move(generate), std::move(onDeleteCb)));
}

ARpc_Accessor* ARpc_Accessor_new(const char* instance, ARpc_ConnectionInfoProvider provider,
                                 void* data, ARpc_ConnectionInfoProviderUserData_delete onDelete) {
    if (instance == nullptr) {
        ALOGE("Instance argument must be valid when calling ARpc_Accessor_new");
        return nullptr;
    }
    if (data && onDelete == nullptr) {
        ALOGE("If a non-null data ptr is passed to ARpc_Accessor_new, then a "
              "ARpc_ConnectionInfoProviderUserData_delete callback must alse be passed to delete "
              "the data object once the ARpc_Accessor is deleted.");
        return nullptr;
    }
    android::RpcSocketAddressProvider generate;
    if (provider) {
        generate = [provider, data](const String16& name, sockaddr* outAddr, size_t addrLen) {
            ARpc_ConnectionInfo* info = provider(String8(name).c_str(), data);
            if (info == nullptr) {
                ALOGI("The supplied ARpc_ConnectionInfoProvider returned nullptr");
                return;
            }
            if (auto addr = std::get_if<sockaddr_vm>(&info->addr)) {
                LOG_ALWAYS_FATAL_IF(addr->svm_family != AF_VSOCK,
                                    "ARpc_ConnectionInfo invalid family");
                if (addrLen < sizeof(sockaddr_vm)) {
                    ALOGE("Provided outAddr is too small! Expecting %zu, got %zu",
                          sizeof(sockaddr_vm), addrLen);
                    ARpc_ConnectionInfo_delete(info);
                    return;
                }
                ALOGI("Connection info provider found AF_VSOCK. family %d, port %d, cid %d",
                      addr->svm_family, addr->svm_port, addr->svm_cid);
                *outAddr = *reinterpret_cast<sockaddr*>(addr);
            } else if (auto addr = std::get_if<sockaddr_un>(&info->addr)) {
                LOG_ALWAYS_FATAL_IF(addr->sun_family != AF_UNIX,
                                    "ARpc_ConnectionInfo invalid family");
                if (addrLen < sizeof(sockaddr_un)) {
                    ALOGE("Provided outAddr is too small! Expecting %zu, got %zu",
                          sizeof(sockaddr_un), addrLen);
                    ARpc_ConnectionInfo_delete(info);
                    return;
                }
                *outAddr = *reinterpret_cast<sockaddr*>(addr);
            } else {
                ALOGE("Unsupported address family type when trying to get ARpcConnection info");
            }
            ARpc_ConnectionInfo_delete(info);
        };
    } else {
        ALOGE("Can't create a new ARpc_Accessor without a ARpc_ConnectionInfoProvider and it is "
              "null");
        return nullptr;
    }
    std::function<void()> onDeleteCb;
    if (onDelete) {
        onDeleteCb = [data, onDelete] { onDelete(data); };
    }
    sp<IBinder> accessorBinder =
            android::createAccessor(String16(instance), std::move(generate), std::move(onDeleteCb));
    if (accessorBinder) {
        ALOGE("service manager found an accessor, so returning one now from _new");
        return ARpc_Accessor::make(instance, accessorBinder);
    } else {
        ALOGE("service manager did not get us an accessor");
        return nullptr;
    }
}

void ARpc_Accessor_delete(ARpc_Accessor* accessor) {
    delete accessor;
}

AIBinder* ARpc_Accessor_asBinder(ARpc_Accessor* accessor) {
    if (!accessor) {
        ALOGE("ARpc_Accessor argument is null.");
        return nullptr;
    }

    sp<IBinder> binder = accessor->asBinder();
    sp<AIBinder> aBinder = ABpBinder::lookupOrCreateFromBinder(binder);
    AIBinder* ptr = aBinder.get();
    if (ptr) {
        ptr->incStrong(nullptr);
    } else {
        ALOGE("Failed to lookupOrCreateFromBinder");
    }
    return ptr;
}

ARpc_Accessor* ARpc_Accessor_fromBinder(const char* instance, AIBinder* binder) {
    if (!binder) {
        ALOGE("binder argument is null");
        return nullptr;
    }
    if (binder->asABBinder()) {
        ALOGE("Binder is local ABBinder and can not be used for ARpc_Accessor_fromBinder");
        return nullptr;
    }
    sp<IBinder> accessorBinder = binder->getBinder();
    if (accessorBinder) {
        return ARpc_Accessor::make(instance, accessorBinder);
    } else {
        ALOGE("Attempting to get an ARpc_Accessor for %s but AIBinder::getBinder returned null",
              instance);
        return nullptr;
    }
}

ARpc_ConnectionInfo* ARpc_ConnectionInfo_new(const sockaddr* addr, socklen_t len) {
    if (addr == nullptr || len < 0 || static_cast<size_t>(len) < sizeof(sa_family_t)) {
        ALOGE("Invalid arguments in Arpc_Connection_new");
        return nullptr;
    }

    if (addr->sa_family == AF_VSOCK) {
        if (len != sizeof(sockaddr_vm)) {
            ALOGE("Incorrect size of %zu for AF_VSOCK sockaddr_vm. Expecting %zu", len,
                  sizeof(sockaddr_vm));
            return nullptr;
        }
        sockaddr_vm vm = *reinterpret_cast<const sockaddr_vm*>(addr);
        ALOGI("Arpc_ConnectionInfo_new found AF_VSOCK. family %d, port %d, cid %d", vm.svm_family,
              vm.svm_port, vm.svm_cid);

        // TODO need to delete this when done with it.
        return new ARpc_ConnectionInfo(*reinterpret_cast<const sockaddr_vm*>(addr));
    }

    ALOGE("ARpc APIs only support AF_VSOCK right now");
    return nullptr;
}

void ARpc_ConnectionInfo_delete(ARpc_ConnectionInfo* info) {
    delete info;
}
