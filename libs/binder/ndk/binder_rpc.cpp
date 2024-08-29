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
#include <arpa/inet.h>
#include <binder/IServiceManager.h>
#include <linux/vm_sockets.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <variant>

#include "ibinder_internal.h"
#include "status_internal.h"

using ::android::defaultServiceManager;
using ::android::IBinder;
using ::android::IServiceManager;
using ::android::OK;
using ::android::sp;
using ::android::status_t;
using ::android::String16;
using ::android::String8;
using ::android::binder::Status;

// #define LOG_ACCESSOR_DEBUG(...)
#define LOG_ACCESSOR_DEBUG(...) ALOGW(__VA_ARGS__)

struct ARpc_ConnectionInfo {
    std::variant<sockaddr_vm, sockaddr_un, sockaddr_in> addr;
};

struct ARpc_Accessor final : public ::android::RefBase {
    static ARpc_Accessor* make(const char* instance, const sp<IBinder>& binder) {
        LOG_ALWAYS_FATAL_IF(binder == nullptr, "ARpc_Accessor requires a non-null binder");
        status_t status = android::validateAccessor(String16(instance), binder);
        if (status != OK) {
            ALOGE("The given binder is not a valid IAccessor for %s. Status: %s", instance,
                  android::statusToString(status).c_str());
            return nullptr;
        }
        return new ARpc_Accessor(binder);
    }

    sp<IBinder> asBinder() { return mAccessorBinder; }

    ~ARpc_Accessor() { LOG_ACCESSOR_DEBUG("ARpc_Accessor dtor"); }

   private:
    ARpc_Accessor(sp<IBinder> accessor) : mAccessorBinder(accessor) {}
    ARpc_Accessor() = delete;
    sp<IBinder> mAccessorBinder;
};

struct ARpc_AccessorProvider {
   public:
    static ARpc_AccessorProvider* make(std::weak_ptr<android::AccessorProvider> cookie) {
        if (cookie.expired()) {
            ALOGE("Null AccessorProvider cookie from libbinder");
            return nullptr;
        }
        return new ARpc_AccessorProvider(cookie);
    }
    std::weak_ptr<android::AccessorProvider> mProviderCookie;

   private:
    ARpc_AccessorProvider() = delete;

    ARpc_AccessorProvider(std::weak_ptr<android::AccessorProvider> provider)
        : mProviderCookie(provider) {}
};

struct OnDeleteProviderHolder {
    OnDeleteProviderHolder(void* data, ARpc_AccessorProviderUserData_delete onDelete)
        : mData(data), mOnDelete(onDelete) {}
    ~OnDeleteProviderHolder() {
        if (mOnDelete) {
            mOnDelete(mData);
        }
    }
    void* mData;
    ARpc_AccessorProviderUserData_delete mOnDelete;
    // needs to be copyable for std::function, but we will never copy it
    OnDeleteProviderHolder(const OnDeleteProviderHolder&) {
        LOG_ALWAYS_FATAL("This object can't be copied!");
    }

   private:
    OnDeleteProviderHolder() = delete;
};

ARpc_AccessorProvider* ARpc_addAccessorProvider(ARpc_AccessorProvider_getAccessor provider,
                                                void* data,
                                                ARpc_AccessorProviderUserData_delete onDelete) {
    if (provider == nullptr) {
        ALOGE("Null provider passed to ARpc_addAccessorProvider");
        return nullptr;
    }
    if (data && onDelete == nullptr) {
        ALOGE("If a non-null data ptr is passed to ARpc_addAccessorProvider, then a "
              "ARpc_AccessorProviderUserData_delete callback must alse be passed to delete "
              "the data object once the ARpc_AccessorProvider is removed.");
        return nullptr;
    }
    // call the onDelete when the last reference of this goes away (when the
    // last reference to the generate std::function goes away).
    std::shared_ptr<OnDeleteProviderHolder> onDeleteHolder =
            std::make_shared<OnDeleteProviderHolder>(data, onDelete);
    android::RpcAccessorProvider generate = [provider,
                                             onDeleteHolder](const String16& name) -> sp<IBinder> {
        ARpc_Accessor* accessor = provider(String8(name).c_str(), onDeleteHolder->mData);
        if (accessor == nullptr) {
            ALOGE("The supplied ARpc_AccessorProvider_getAccessor returned nullptr");
            return nullptr;
        }
        sp<IBinder> binder = accessor->asBinder();
        ARpc_Accessor_delete(accessor);
        return binder;
    };

    std::weak_ptr<android::AccessorProvider> cookie =
            android::addAccessorProvider(std::move(generate));
    return ARpc_AccessorProvider::make(cookie);
}

binder_status_t ARpc_removeAccessorProvider(ARpc_AccessorProvider* provider) {
    if (provider == nullptr) {
        ALOGE("Attempting to remove a null ARpc_AccessorProvider");
        return STATUS_UNEXPECTED_NULL;
    }

    status_t status = android::removeAccessorProvider(provider->mProviderCookie);
    if (status == OK) {
        delete provider;
    }
    return PruneStatusT(status);
}

struct OnDeleteConnectionInfoHolder {
    OnDeleteConnectionInfoHolder(void* data, ARpc_ConnectionInfoProviderUserData_delete onDelete)
        : mData(data), mOnDelete(onDelete) {}
    ~OnDeleteConnectionInfoHolder() {
        if (mOnDelete) {
            mOnDelete(mData);
        }
    }
    void* mData;
    ARpc_ConnectionInfoProviderUserData_delete mOnDelete;
    // needs to be copyable for std::function, but we will never copy it
    OnDeleteConnectionInfoHolder(const OnDeleteConnectionInfoHolder&) {
        LOG_ALWAYS_FATAL("This object can't be copied!");
    }

   private:
    OnDeleteConnectionInfoHolder() = delete;
};

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
    std::shared_ptr<OnDeleteConnectionInfoHolder> onDeleteHolder =
            std::make_shared<OnDeleteConnectionInfoHolder>(data, onDelete);
    if (provider == nullptr) {
        ALOGE("Can't create a new ARpc_Accessor without a ARpc_ConnectionInfoProvider and it is "
              "null");
        return nullptr;
    }
    android::RpcSocketAddressProvider generate = [provider, onDeleteHolder](
                                                         const String16& name, sockaddr* outAddr,
                                                         size_t addrLen) -> status_t {
        std::unique_ptr<ARpc_ConnectionInfo> info(
                provider(String8(name).c_str(), onDeleteHolder->mData));
        if (info == nullptr) {
            ALOGE("The supplied ARpc_ConnectionInfoProvider returned nullptr");
            return android::NAME_NOT_FOUND;
        }
        if (auto addr = std::get_if<sockaddr_vm>(&info->addr)) {
            LOG_ALWAYS_FATAL_IF(addr->svm_family != AF_VSOCK, "ARpc_ConnectionInfo invalid family");
            if (addrLen < sizeof(sockaddr_vm)) {
                ALOGE("Provided outAddr is too small! Expecting %zu, got %zu", sizeof(sockaddr_vm),
                      addrLen);
                return android::BAD_VALUE;
            }
            LOG_ACCESSOR_DEBUG(
                    "Connection info provider found AF_VSOCK. family %d, port %d, cid %d",
                    addr->svm_family, addr->svm_port, addr->svm_cid);
            *reinterpret_cast<sockaddr_vm*>(outAddr) = *addr;
        } else if (auto addr = std::get_if<sockaddr_un>(&info->addr)) {
            LOG_ALWAYS_FATAL_IF(addr->sun_family != AF_UNIX, "ARpc_ConnectionInfo invalid family");
            if (addrLen < sizeof(sockaddr_un)) {
                ALOGE("Provided outAddr is too small! Expecting %zu, got %zu", sizeof(sockaddr_un),
                      addrLen);
                return android::BAD_VALUE;
            }
            *reinterpret_cast<sockaddr_un*>(outAddr) = *addr;
        } else if (auto addr = std::get_if<sockaddr_in>(&info->addr)) {
            LOG_ALWAYS_FATAL_IF(addr->sin_family != AF_INET, "ARpc_ConnectionInfo invalid family");
            if (addrLen < sizeof(sockaddr_in)) {
                ALOGE("Provided outAddr is too small! Expecting %zu, got %zu", sizeof(sockaddr_in),
                      addrLen);
                return android::BAD_VALUE;
            }
            *reinterpret_cast<sockaddr_in*>(outAddr) = *addr;
        } else {
            LOG_ALWAYS_FATAL(
                    "Unsupported address family type when trying to get ARpcConnection info. A "
                    "new variant was added to the ARpc_ConnectionInfo and this needs to be "
                    "updated.");
        }
        return OK;
    };
    sp<IBinder> accessorBinder = android::createAccessor(String16(instance), std::move(generate));
    if (accessorBinder == nullptr) {
        ALOGE("service manager did not get us an accessor");
        return nullptr;
    }
    LOG_ACCESSOR_DEBUG("service manager found an accessor, so returning one now from _new");
    return ARpc_Accessor::make(instance, accessorBinder);
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
    if (ptr == nullptr) {
        LOG_ALWAYS_FATAL("Failed to lookupOrCreateFromBinder");
    }
    ptr->incStrong(nullptr);
    return ptr;
}

ARpc_Accessor* ARpc_Accessor_fromBinder(const char* instance, AIBinder* binder) {
    if (!binder) {
        ALOGE("binder argument is null");
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

binder_status_t ARpc_Accessor_delegateAccessor(const char* instance, AIBinder* accessor,
                                               AIBinder** outDelegator) {
    if (instance == nullptr || accessor == nullptr) {
        ALOGE("Arguments to ARpc_Accessor_delegateBinder are null");
        return STATUS_UNEXPECTED_NULL;
    }
    sp<IBinder> accessorBinder = accessor->getBinder();

    sp<IBinder> delegator;
    status_t status = android::delegateAccessor(String16(instance), accessorBinder, &delegator);
    if (status != OK) {
        return PruneStatusT(status);
    }
    sp<AIBinder> binder = ABpBinder::lookupOrCreateFromBinder(delegator);
    *outDelegator = binder.get();
    // FIXME document this
    (*outDelegator)->incStrong(nullptr);
    return OK;
}

ARpc_ConnectionInfo* ARpc_ConnectionInfo_new(const sockaddr* addr, socklen_t len) {
    if (addr == nullptr || len < 0 || static_cast<size_t>(len) < sizeof(sa_family_t)) {
        ALOGE("Invalid arguments in Arpc_Connection_new");
        return nullptr;
    }
    // socklen_t was int32_t on 32-bit and uint32_t on 64 bit.
    size_t socklen = len < 0 || static_cast<uintmax_t>(len) > SIZE_MAX ? 0 : len;

    if (addr->sa_family == AF_VSOCK) {
        if (len != sizeof(sockaddr_vm)) {
            ALOGE("Incorrect size of %zu for AF_VSOCK sockaddr_vm. Expecting %zu", socklen,
                  sizeof(sockaddr_vm));
            return nullptr;
        }
        sockaddr_vm vm = *reinterpret_cast<const sockaddr_vm*>(addr);
        LOG_ACCESSOR_DEBUG("Arpc_ConnectionInfo_new found AF_VSOCK. family %d, port %d, cid %d",
                           vm.svm_family, vm.svm_port, vm.svm_cid);
        return new ARpc_ConnectionInfo(vm);
    } else if (addr->sa_family == AF_UNIX) {
        if (len != sizeof(sockaddr_un)) {
            ALOGE("Incorrect size of %zu for AF_UNIX sockaddr_un. Expecting %zu", socklen,
                  sizeof(sockaddr_un));
            return nullptr;
        }
        sockaddr_un un = *reinterpret_cast<const sockaddr_un*>(addr);
        LOG_ACCESSOR_DEBUG("Arpc_ConnectionInfo_new found AF_UNIX. family %d, path %s",
                           un.sun_family, un.sun_path);
        return new ARpc_ConnectionInfo(un);
    } else if (addr->sa_family == AF_INET) {
        if (len != sizeof(sockaddr_in)) {
            ALOGE("Incorrect size of %zu for AF_INET sockaddr_in. Expecting %zu", socklen,
                  sizeof(sockaddr_in));
            return nullptr;
        }
        sockaddr_in in = *reinterpret_cast<const sockaddr_in*>(addr);
        LOG_ACCESSOR_DEBUG("Arpc_ConnectionInfo_new found AF_INET. family %d, address %s, port %d",
                           in.sin_family, inet_ntoa(in.sin_addr), ntohs(in.sin_port));
        return new ARpc_ConnectionInfo(in);
    }

    ALOGE("ARpc APIs only support AF_VSOCK right now but the supplied sockadder::sa_family is: %hu",
          addr->sa_family);
    return nullptr;
}

void ARpc_ConnectionInfo_delete(ARpc_ConnectionInfo* info) {
    delete info;
}
