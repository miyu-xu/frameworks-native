/*
 * Copyright (C) 2025 The Android Open Source Project
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

namespace android {

struct CallSession {
    // Binder interface descriptor.
    // public Class<? extends Binder> binderClass;
    // Binder transaction code.
    int transactionCode;
    // CPU time at the beginning of the call.
    long cpuTimeStarted;
    // System time at the beginning of the call.
    long timeStarted;
    // Should be set to one when an exception is thrown.
    bool exceptionThrown;
    // Detailed information should be recorded for this call when it ends.
    bool recordedCall;
};

class IBinderObserver {
public:
    /**
     * Called when a binder call starts.
     *
     * @return a CallSession to pass to the callEnded method.
     */
    virtual void onCallStarted(int code, int workSourceUid) = 0;
    /**
     * Called when a binder call stops.
     *
     * <li>This method will be called even when an exception is thrown by the binder stub
     * implementation.
     */
    virtual void onCallEnded(const CallSession& session, int parcelRequestSize, int parcelReplySize,
                int workSourceUid) = 0;

    /**
     * Called if an exception is thrown while executing the binder transaction.
     *
     * <li>BinderCallsStats#callEnded will be called afterwards.
     * <li>Do not throw an exception in this method, it will swallow the original exception
     * thrown by the binder transaction.
     */
    // void callThrewException(CallSession s, Exception exception);

};

/**
 * A session used by {@link Observer} in order to keep track of some data.
 */


} // namespace android