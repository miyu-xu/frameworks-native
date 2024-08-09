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

#include <android/binder_ibinder.h>
#include <sys/socket.h>

__BEGIN_DECLS

struct ARpc_Accessor;
typedef struct ARpc_Accessor ARpc_Accessor;
struct ARpc_AccessorProvider;
typedef struct ARpc_AccessorProvider ARpc_AccessorProvider;
struct ARpc_ConnectionInfo;
typedef struct ARpc_ConnectionInfo ARpc_ConnectionInfo;

/**
 * These APIs provide a way for clients of binder services to be able to get a
 * binder object of that service through the existing libbinder/libbinder_ndk
 * Service Manager APIs when that service is using RPC Binder over sockets
 * instead kernel binder.
 *
 * Some of these APIs are used on Android hosts when kernel binder is supported
 * and the usual servicemanager process is available. Some of these APIs are
 * only required when there is no kernel binder or extra servicemanager process
 * such as the case of microdroid or similar VMs.
 */

/**
 * This callback is responsible for returning ARpc_Accessor objects for a given
 * service instance. These ARpc_Accessor objects are implemented by
 * libbinder_ndk and backed by implementations of android::os::IAccessor in
 * libbinder.
 *
 * \param instance name of the service like
 *        `android.hardware.vibrator.IVibrator/default`
 * \param data a pointer to data that the callback needs to provide an
 *        ARpc_Accessor for the given `instance`.
 * \return The ARpc_Accessor associated with the service `instance`. This
 *        callback gives up ownership of the object once it returns it. The
 *        caller of this callback (libbinder_ndk) is responsible for deleting it
 *        with ARpc_Accessor_delete.
 */
typedef ARpc_Accessor* (*ARpc_AccessorProvider_getAccessor)(const char* instance, void* data);

/**
 * This callback is responsible deleting the `void* data` object that is passed
 * in to ARpc_addAccessorProvider for the ARpc_AccessorProvider_getAccessor to use. That
 * object is owned by the ARpc_AccessorProvider and must remain valid for the
 * lifetime of the callback because it may be called and use the object.
 * This _delete callback is called after the ARpc_AccessorProvider is remove and
 * is guaranteed never to be called again.
 *
 * \param data a pointer to data that the ARpc_AccessorProvider_getAccessor uses which is to
 *        be deleted by this call.
 */
typedef void (*ARpc_AccessorProviderUserData_delete)(void* data);

/**
 * Inject an ARpc_AccessorProvider_getAccessor callback into the process for
 * the Service Manager APIs to use to retrieve ARpc_Accessor objects associated
 * with different RPC Binder services.
 *
 * \param provider callback that returns ARpc_Accessors for libbinder to set up
 *        RPC clients with.
 * \param data pointer that is passed to the ARpc_AccessorProvider callback.
 *        IMPORTANT: The ARpc_AccessorProvider now OWNS that object that data
 *        points to. It can be used as necessary in the callback. The data MUST
 *        remain valid for the lifetime of the provider callback.
 *        Do not attempt to give ownership of the same object to different
 *        providers throguh multiple calls to this function because the first
 *        one to be deleted will call the onDelete callback.
 * \param onDelete callback used to delete the objects that `data` points to.
 *        This is called after ARpc_AccessorProvider is guaranteed to never be
 *        called again. Before this callback is called, `data` must remain
 *        valid.
 * \return Exceptions for errors or EX_NONE on success.
 */
ARpc_AccessorProvider* ARpc_addAccessorProvider(ARpc_AccessorProvider_getAccessor provider,
                                                void* data,
                                                ARpc_AccessorProviderUserData_delete onDelete)
        __INTRODUCED_IN(36);

/**
 * Remove an ARpc_AccessorProvider from libbinder. This will remove references
 *        from the ARpc_AccessorProvider and will no longer call the
 *        ARpc_AccessorProvider_getAccessor callback.
 *
 * Note: The `data` object that was used when adding the accessor will be
 *       deleted by the ARpc_AccessorProviderUserData_delete callback at some
 *       point after this call. Do not use the object and do not try to delete
 *       it through any other means.
 *
 * \param provider to be removed and deleted
 *
 * \return for errors or STATUS_OK on successful removal. The provider is not
 *         deleted when an error is returned. This is only possible if the
 *         provider was already removed, was never added, or is null.
 */
binder_status_t ARpc_removeAccessorProvider(ARpc_AccessorProvider* provider) __INTRODUCED_IN(36);

/**
 * Callback returns RPC connection information for libbinder to use to to
 * connect to a socket that a given service is listening on. This is needed to
 * create an ARpc_Accessor so it can connect to these services.
 *
 * \param instance name of the service to connect to
 * \param data userdata for this callback. The pointer is provided in
 *        ARpc_Accessor_new.
 * \return ARpc_ConnectionInfo with socket connection information for `instance`
 */
typedef ARpc_ConnectionInfo* (*ARpc_ConnectionInfoProvider)(const char* instance,
                                                            void* data)__INTRODUCED_IN(36);
/**
 * This callback is responsible deleting the `void* data` object that is passed
 * in to ARpc_Accessor_new for the ARpc_ConnectionInfoProvider to use. That
 * object is owned by the ARpc_Accessor and must remain valid for the
 * lifetime the Accessor because it may be used by the connection info provider
 * callback..
 * This _delete callback is called after the ARpc_Accessor is remove and
 * is guaranteed never to be called again.
 *
 * \param data a pointer to data that the ARpc_AccessorProvider uses which is to
 *        be deleted by this call.
 */
typedef void (*ARpc_ConnectionInfoProviderUserData_delete)(void* data);

/**
 * Create a new ARpc_Accessor. This creates an IAccessor object in libbinder
 * that can use the info from the ARpc_ConnectionInfoProvider to connect to a
 * socket that the service with `instance` name is listening to.
 *
 * \param instance name of the service that is listening on the socket
 * \param provider callback that can get the socket connection information for the
 *           instance. This connection information may be dynamic, so the
 *           provider will be called any time a new connection is required.
 * \param data pointer that is passed to the ARpc_ConnectionInfoProvider callback.
 *        IMPORTANT: The ARpc_ConnectionInfoProvider now OWNS that object that data
 *        points to. It can be used as necessary in the callback. The data MUST
 *        remain valid for the lifetime of the provider callback.
 *        Do not attempt to give ownership of the same object to different
 *        providers throguh multiple calls to this function because the first
 *        one to be deleted will call the onDelete callback.
 * \param onDelete callback used to delete the objects that `data` points to.
 *        This is called after ARpc_ConnectionInfoProvider is guaranteed to never be
 *        called again. Before this callback is called, `data` must remain
 *        valid.
 * \return an ARpc_Accessor instance. This is deleted by the caller once it is
 *         no longer needed.
 */
ARpc_Accessor* ARpc_Accessor_new(const char* instance, ARpc_ConnectionInfoProvider provider,
                                 void* data, ARpc_ConnectionInfoProviderUserData_delete onDelete)
        __INTRODUCED_IN(36);

/**
 * Delete an ARpc_Accessor
 *
 * \param accessor to delete
 */
void ARpc_Accessor_delete(ARpc_Accessor* accessor) __INTRODUCED_IN(36);

/**
 * Return the AIBinder associated with an ARpc_Accessor. This can be used to
 * send the Accessor to another process or even register it with servicemanager.
 *
 * \param accessor to get the AIBinder for
 * \return binder of the supplied accessor with one strong ref count
 */
AIBinder* ARpc_Accessor_asBinder(ARpc_Accessor* accessor) __INTRODUCED_IN(36);

/**
 * Return the ARpc_Accessor associated with an AIBinder. The instance must match
 * the ARpc_Accessor implementation, and the AIBinder must a proxy binder for a
 * remote service (ABpBinder).
 * This can be used when receivng an AIBinder from another process that the
 * other process obtained from ARpc_Accessor_asBinder.
 *
 * \param instance name of the service that the Accessor is responsible for.
 * \param accessorBinder proxy binder from another processes ARpc_Accessor.
 * \return ARpc_Accessor representing the other processes ARpc_Accessor
 *         implementation. This function does not take ownership of the
 *         ARpc_Accessor (so the caller needs to delete with
 *         ARpc_Accessor_delete), and it preserves the recount of the bidner
 *         object.
 */
ARpc_Accessor* ARpc_Accessor_fromBinder(const char* instance, AIBinder* accessorBinder)
        __INTRODUCED_IN(36);

/**
 * Create a new ARpc_ConnectionInfo with sockaddr. This can be supported socket
 * types like sockaddr_vm (vsock) and sockaddr_un (Unix Domain Sockets).
 *
 * \param addr sockaddr pointer that can come from supported socket
 *        types like sockaddr_vm (vsock) and sockaddr_un (Unix Domain Sockets).
 * \param len length of the concrete sockaddr type being used. Like
 *        sizeof(sockaddr_vm) when sockaddr_vm is used.
 * \return the connection info based on the given sockaddr
 */
ARpc_ConnectionInfo* ARpc_ConnectionInfo_new(const sockaddr* addr, socklen_t len)
        __INTRODUCED_IN(36);

/**
 * Delete an ARpc_ConnectionInfo object that was created with
 * ARpc_ConnectionInfo_new.
 * It is up to the caller of this function to delete this object unless it
 * explicitly passes ownership of the object.
 *
 * \param info object to be deleted
 */
void ARpc_ConnectionInfo_delete(ARpc_ConnectionInfo* info) __INTRODUCED_IN(36);

__END_DECLS
