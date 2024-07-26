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

import android.os.Service;

/**
 * @hide
 */
oneway interface IInternalServiceCallback {
    /**
     * Called when a service is registered.
     * This callback should only be used in BackendUnifiedServiceManager and servicemanager.
     *
     * @param name the service name that has been registered with
     * @param service the service that is registered
     */
    void onRegistration(@utf8InCpp String name, in Service service);
}
