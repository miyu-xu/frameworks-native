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

// TODO docs everwhere!
//
struct ARpc_Accessor;
typedef struct ARpc_Accessor ARpc_Accessor;

// this callback is called any time the libbinder service manager APIs
// are trying to get a service and it returns an AIBinder that is
// libbinder’s IAccessor
typedef ARpc_Accessor* (*ARpc_AccessorProvider)(const char* instance,
                                                void* data)__INTRODUCED_IN(34);

// Called once in the client for libbinder to be able to get IAccessor
// instances
binder_exception_t ARpc_addAccessorProvider(ARpc_AccessorProvider, void* data) __INTRODUCED_IN(34);

/** Process B is the local service that connects to the socket and
 *  creates the FD for the libbinder IAccessor implementation to return
 */

struct ARpc_ConnectionInfo;
typedef struct ARpc_ConnectionInfo ARpc_ConnectionInfo;

// gets connection info for a given service. This is needed when
// connection info isn’t available immediately and is instead queried
// when a client tries to get a service.
typedef ARpc_ConnectionInfo* (*ARpc_ConnectionInfoProvider)(const char* instance,
                                                            void* data)__INTRODUCED_IN(34);

// Always needs a connection info provider, with optional callbacks in the
// future
ARpc_Accessor* ARpc_Accessor_new(const char* instance, ARpc_ConnectionInfoProvider provider,
                                 void* data) __INTRODUCED_IN(34);

void ARpc_Accessor_delete(ARpc_Accessor* accessor) __INTRODUCED_IN(34);

AIBinder* ARpc_Accessor_asBinder(ARpc_Accessor* accessor) __INTRODUCED_IN(34);

ARpc_Accessor* ARpc_Accessor_fromBinder(const char* instance, AIBinder* accessorBinder)
        __INTRODUCED_IN(34);

// Used in the implementation of ARpc_connectionInfoProvider
ARpc_ConnectionInfo* ARpc_ConnectionInfo_new(uint32_t port, uint32_t cid) __INTRODUCED_IN(34);
void ARpc_ConnectionInfo_delete(ARpc_ConnectionInfo* info) __INTRODUCED_IN(34);

__END_DECLS
