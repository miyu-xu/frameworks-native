/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <pthread.h>

#include <condition_variable>
#include <thread>

#include <log/log.h>

namespace android {

#ifdef BINDER_RPC_SINGLE_THREADED
class RpcMutex {
public:
    void lock() {}
    void unlock() {}
};

class RpcMutexUniqueLock {
public:
    RpcMutexUniqueLock(RpcMutex&) {}
    void unlock() {}
};

class RpcMutexLockGuard {
public:
    RpcMutexLockGuard(RpcMutex&) {}
};

class RpcConditionVariable {
public:
    void notify_one() {}
    void notify_all() {}

    void wait(RpcMutexUniqueLock&) {}

    template <typename Predicate>
    void wait(RpcMutexUniqueLock&, Predicate stop_waiting) {
        LOG_ALWAYS_FATAL_IF(!stop_waiting(), "RpcConditionVariable::wait condition not met");
    }

    template <typename Duration>
    std::cv_status wait_for(RpcMutexUniqueLock&, const Duration&) {
        return std::cv_status::no_timeout;
    }

    template <typename Duration, typename Predicate>
    bool wait_for(RpcMutexUniqueLock&, const Duration&, Predicate stop_waiting) {
        return stop_waiting();
    }
};

class RpcMaybeThread {
public:
    RpcMaybeThread() = default;
    RpcMaybeThread(const RpcMaybeThread&) = delete;

    RpcMaybeThread& operator=(RpcMaybeThread&& other) noexcept {
        LOG_ALWAYS_FATAL_IF(mThunk != nullptr, "RpcMaybeThread destroyed before join was called");

        mThunk = other.mThunk;
        mCall = other.mCall;
        mDelete = other.mDelete;

        other.mThunk = nullptr;
        other.mCall = nullptr;
        other.mDelete = nullptr;

        return *this;
    }

    RpcMaybeThread(RpcMaybeThread&& other) noexcept { *this = std::move(other); }

    template <typename Function, typename... Args>
    RpcMaybeThread(Function&& f, Args&&... args) {
        auto thunk = new auto([f = std::move(f), ... args = std::move(args)]() mutable {
            std::move(f)(std::move(args)...);
        });
        mThunk = (void*)thunk;
        mCall = [](void* t) { (*(decltype(thunk))t)(); };
        mDelete = [](void* t) { delete (decltype(thunk))t; };
    }

    ~RpcMaybeThread() {
        LOG_ALWAYS_FATAL_IF(mThunk != nullptr, "RpcMaybeThread destroyed before join was called");
    }

    void join() {
        if (mThunk) {
            // Move mThunk into a temporary so we can clear mThunk before
            // executing the callback. This avoids infinite recursion if
            // the callee then calls join() again directly or indirectly.
            void* thunk = mThunk;
            mThunk = nullptr;
            mCall(thunk);
            mDelete(thunk);
        }
    }
    void detach() { join(); }

    class id {
    public:
        bool operator==(const id&) const { return true; }
        bool operator!=(const id&) const { return false; }
        bool operator<(const id&) const { return false; }
        bool operator<=(const id&) const { return true; }
        bool operator>(const id&) const { return false; }
        bool operator>=(const id&) const { return true; }
    };

    id get_id() const { return id(); }

private:
    using TypeErasedFunc = void(void*);

    void* mThunk = nullptr;
    TypeErasedFunc* mCall = nullptr;
    TypeErasedFunc* mDelete = nullptr;
};

namespace rpc_this_thread {
static inline RpcMaybeThread::id get_id() {
    return RpcMaybeThread::id();
}
} // namespace rpc_this_thread

static inline void rpcJoinIfSingleThreaded(RpcMaybeThread& t) {
    t.join();
}
#else  // BINDER_RPC_SINGLE_THREADED
using RpcMutex = std::mutex;
using RpcMutexUniqueLock = std::unique_lock<std::mutex>;
using RpcMutexLockGuard = std::lock_guard<std::mutex>;
using RpcConditionVariable = std::condition_variable;
using RpcMaybeThread = std::thread;
namespace rpc_this_thread = std::this_thread;

static inline void rpcJoinIfSingleThreaded(RpcMaybeThread&) {}
#endif // BINDER_RPC_SINGLE_THREADED

} // namespace android
