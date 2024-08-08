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

package android.os;

import android.os.ParcelFileDescriptor;

/**
 * Interface for accessing the RPC server of a service.
 *
 * @hide
 */
interface IAccessor {
    // The connection info was not available for this service.
    // This happens when the user-supplied callback fails to produce
    // valid connection info.
    const int ERROR_CONNECTION_INFO_NOT_FOUND = 0;
    // Failed to create the socket. Often happens when the process trying to create
    // the socket lacks the permissions to do so.
    const int ERROR_FAILED_TO_CREATE_SOCKET = 1;
    // Failed to connect to the socket. This can happen for many reasons, so be sure
    // log the error message and check it.
    const int ERROR_FAILED_TO_CONNECT_TO_SOCKET = 2;

    /**
     * Adds a connection to the RPC server of the service managed by the IAccessor.
     *
     * This method can be called multiple times to establish multiple distinct
     * connections to the same RPC server.
     *
     * @throws ServiceSpecificException with the one of the IAccessor::ERR_* constants.
     *
     * @return A file descriptor connected to the RPC session of the service managed
     *         by IAccessor.
     */
    String addConnection(inout @nullable ParcelFileDescriptor fd);

    /**
     * Get the instance name for the service this accessor is responsible for.
     *
     * This is used to verify the proxy binder is associated with the expected instance name.
     */
    String getInstanceName();

    // TODO(b/350941051): Add API for debugging.
}
