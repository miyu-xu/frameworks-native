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
#include <binder/IServiceManager.h>

#include "ibinder_internal.h"
#include "status_internal.h"

using ::android::defaultServiceManager;
using ::android::IBinder;
using ::android::IServiceManager;
using ::android::sp;
using ::android::String16;
using ::android::String8;

struct ARpc_ConnectionInfo {
    // TODO use sockaddr instead
    uint32_t port;
    uint32_t cid;
};

struct ARpc_Accessor : public ::android::RefBase {
    ARpc_Accessor(sp<IBinder> accessor) : mAccessorBinder(accessor) {
        LOG_ALWAYS_FATAL_IF(accessor == nullptr, "ARpc_Accessor requires a non-null binder");
    }
    // for ndk
    sp<AIBinder> asAIBinder() {
        sp<AIBinder> binder = ABpBinder::lookupOrCreateFromBinder(mAccessorBinder);
        if (!binder) {
            ALOGE("Failed to lookupOrCreateFromBinder");
            return nullptr;
        }
        // TODO FIXME hack to keep this thing alive for a bit... I don't think
        // this is the right thing to do here. Why is it creating a new binder?
        // So many questions.
        binder.get()->incStrong(nullptr);

        // TODO what about if's a BBinder?
        return binder;
    }
    // for libbinder
    sp<IBinder> asBinder() { return mAccessorBinder; }
    ~ARpc_Accessor() {
        ALOGI("ARpc_Accessor dtor");
        // TODO can we clean up the associated binder in libbinder?
    }

   private:
    ARpc_Accessor() = delete;
    sp<IBinder> mAccessorBinder;
};

binder_exception_t ARpc_addAccessorProvider(ARpc_AccessorProvider provider, void* data) {
    if (provider == nullptr) {
        return EX_ILLEGAL_ARGUMENT;
    }
    std::function<sp<IBinder>(const String16& name)> generate =
            [provider, data](const String16& name) -> sp<IBinder> {
        ARpc_Accessor* accessor = provider(String8(name).c_str(), data);
        if (accessor == nullptr) {
            ALOGI("The supplied ARpc_AccessorProvider returned nullptr");
            return nullptr;
        }
        return accessor->asBinder();
    };
    sp<IServiceManager> sm = defaultServiceManager();

    return PruneException(sm->addAccessorProvider(generate));
}

ARpc_Accessor* ARpc_Accessor_new(const char* instance, ARpc_ConnectionInfoProvider provider,
                                 void* data) {
    std::function<std::optional<android::RpcConnectionInfo>(const String16& name)> generate;
    if (provider) {
        generate = [provider,
                    data](const String16& name) -> std::optional<android::RpcConnectionInfo> {
            ARpc_ConnectionInfo* info = provider(String8(name).c_str(), data);
            if (info == nullptr) {
                ALOGI("The supplied ARpc_ConnectionInfoProvider returned nullptr");
                return std::nullopt;
            }
            return android::RpcConnectionInfo(info->port, info->cid);
        };
    } else {
        ALOGE("Can't create a new ARpc_Accessor without a ARpc_ConnectionInfoProvider and it is "
              "null");
        return nullptr;
    }
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> accessorBinder = sm->getAccessor(String16(instance), generate, data);
    if (accessorBinder) {
        ALOGE("service manager found an accessor, so returning one now from _new");
        return new ARpc_Accessor(accessorBinder);
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
    ALOGE("ARpc_Accessor_asBinder.");
    // TODO is this what is being cleaned up?
    // should we inc ref here?
    AIBinder* ptr = accessor->asAIBinder().get();
    if (ptr) {
        // TODO which incStrong is really necessary here...
        ptr->incStrong(nullptr);
        ALOGE("asAIBinder succes! ptr is not null.");
    } else {
        ALOGE("asAIBinder returned nullptr...");
    }
    return ptr;
}

// TODO add methods to the IAccessor to get the instance string? The data* can't
// be passed - need to understand if that's an issue! Likely not because this
// is a proxy binder. Need to make sure of that.
ARpc_Accessor* ARpc_Accessor_fromBinder(const char* instance, AIBinder* binder) {
    // TODO make sure this is only a proxy binder. Can't be a local binder
    // because that local Accessor impl needs to use the void* data
    if (!binder) {
        ALOGE("binder argument is null");
        return nullptr;
    }
    if (binder->asABBinder()) {
        // TODO make this message more clear?
        ALOGE("Binder is local ABBinder and can not be used for fromBinder");
        return nullptr;
    }
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> accessorBinder = binder->getBinder();
    android::status_t status = sm->addAccessorBinder(String16(instance), accessorBinder);
    if (status == android::OK) {
        return new ARpc_Accessor(accessorBinder);
    } else {
        ALOGE("libbinder rejected this accessor binder with return: %s",
              android::statusToString(status).c_str());
        return nullptr;
    }
}

ARpc_ConnectionInfo* ARpc_ConnectionInfo_new(uint32_t port, uint32_t cid) {
    return new ARpc_ConnectionInfo(port, cid);
}

void ARpc_ConnectionInfo_delete(ARpc_ConnectionInfo* info) {
    delete info;
}
