/*
 * Copyright (C) 2005 The Android Open Source Project
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

#define LOG_TAG "ProcessState"

#include <binder/ProcessState.h>
#include <sys/mman.h>

#include <android-base/strings.h>
#include <binder/BpBinder.h>
#include <binder/Functional.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/Stability.h>
#include <utils/AndroidThreads.h>
#include <utils/String8.h>
#include <utils/Thread.h>

#include "Static.h"
#include "Utils.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>

#define DEFAULT_MAX_BINDER_THREADS 15







// -------------------------------------------------------------------------

namespace android {

using namespace android::binder::impl;
using android::binder::unique_fd;

namespace {

constexpr const char* kRpcOnlyDriver = "binder-rpc-host";

}

class PoolThread : public Thread
{
public:
    explicit PoolThread(bool isMain)
        : mIsMain(isMain)
    {
    }

protected:
    virtual bool threadLoop()
    {
        IPCThreadState::self()->joinThreadPool(mIsMain);
        return false;
    }

    const bool mIsMain;
};

sp<ProcessState> ProcessState::self()
{
    return init(kRpcOnlyDriver, false /*requireDefault*/);
}

sp<ProcessState> ProcessState::initWithDriver(const char* driver)
{
    return init(driver != nullptr ? driver : kRpcOnlyDriver, true /*requireDefault*/);
}

sp<ProcessState> ProcessState::selfOrNull()
{
    return init(nullptr, false /*requireDefault*/);
}

[[clang::no_destroy]] static sp<ProcessState> gProcess;
[[clang::no_destroy]] static std::mutex gProcessMutex;

static void verifyNotForked(bool forked) {
    LOG_ALWAYS_FATAL_IF(forked, "libbinder ProcessState can not be used after fork");
}

sp<ProcessState> ProcessState::init(const char* driver, bool requireDefault) {
    if (driver == nullptr) {
        std::lock_guard<std::mutex> l(gProcessMutex);
        if (gProcess) {
            verifyNotForked(gProcess->mForked);
        }
        return gProcess;
    }

    [[clang::no_destroy]] static std::once_flag gProcessOnce;
    std::call_once(gProcessOnce, [&](){
        // we must install these before instantiating the gProcess object,
        // otherwise this would race with creating it, and there could be the
        // possibility of an invalid gProcess object forked by another thread
        // before these are installed
        int ret = pthread_atfork(ProcessState::onFork, ProcessState::parentPostFork,
                                 ProcessState::childPostFork);
        LOG_ALWAYS_FATAL_IF(ret != 0, "pthread_atfork error %s", strerror(ret));

        std::lock_guard<std::mutex> l(gProcessMutex);
        gProcess = sp<ProcessState>::make(driver);
    });

    if (requireDefault) {
        // Detect if we are trying to initialize with a different driver, and
        // consider that an error. ProcessState will only be initialized once above.
        LOG_ALWAYS_FATAL_IF(gProcess->getDriverName() != driver,
                            "ProcessState was already initialized with %s,"
                            " can't initialize with %s.",
                            gProcess->getDriverName().c_str(), driver);
    }

    verifyNotForked(gProcess->mForked);
    return gProcess;
}

sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    sp<IBinder> context = getStrongProxyForHandle(0);

    if (context) {
        // The root object is special since we get it directly from the driver, it is never
        // written by Parcell::writeStrongBinder.
        internal::Stability::markCompilationUnit(context.get());
    } else {
        ALOGW("Not able to get context object on %s.", mDriverName.c_str());
    }

    return context;
}

void ProcessState::onFork() {
    // make sure another thread isn't currently retrieving ProcessState
    gProcessMutex.lock();
}

void ProcessState::parentPostFork() {
    gProcessMutex.unlock();
}

void ProcessState::childPostFork() {
    // another thread might call fork before gProcess is instantiated, but after
    // the thread handler is installed
    if (gProcess) {
        gProcess->mForked = true;
    }
    gProcessMutex.unlock();
}

void ProcessState::startThreadPool()
{
    std::unique_lock<std::mutex> _l(mLock);
    if (!mThreadPoolStarted) {
        if (mMaxThreads == 0) {
            // see also getThreadPoolMaxTotalThreadCount
            ALOGW("Extra binder thread started, but 0 threads requested. Do not use "
                  "*startThreadPool when zero threads are requested.");
        }
        mThreadPoolStarted = true;
        spawnPooledThread(true);
    }
}

bool ProcessState::becomeContextManager()
{
    // For RPC-only, context manager functionality is not supported
    ALOGW("becomeContextManager not supported in RPC-only mode");
    return false;
}

// Get references to userspace objects held by the kernel binder driver
// Writes up to count elements into buf, and returns the total number
// of references the kernel has, which may be larger than count.
// buf may be NULL if count is 0.  The pointers returned by this method
// should only be used for debugging and not dereferenced, they may
// already be invalid.
ssize_t ProcessState::getKernelReferences(size_t buf_count, uintptr_t* buf)
{
    // For RPC-only, kernel references are not available
    ALOGW("getKernelReferences not supported in RPC-only mode");
    return -1;
}

// Queries the driver for the current strong reference count of the node
// that the handle points to. Can only be used by the servicemanager.
//
// Returns -1 in case of failure, otherwise the strong reference count.
ssize_t ProcessState::getStrongRefCountForNode(const sp<BpBinder>& binder) {
    if (binder->isRpcBinder()) return -1;

    // For RPC-only, kernel reference counts are not available
    ALOGW("getStrongRefCountForNode not supported in RPC-only mode");
    return -1;
}

void ProcessState::setCallRestriction(CallRestriction restriction) {
    LOG_ALWAYS_FATAL_IF(IPCThreadState::selfOrNull() != nullptr,
        "Call restrictions must be set before the threadpool is started.");

    mCallRestriction = restriction;
}

ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
    const size_t N=mHandleToObject.size();
    if (N <= (size_t)handle) {
        handle_entry e;
        e.binder = nullptr;
        e.refs = nullptr;
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return nullptr;
    }
    return &mHandleToObject.editItemAt(handle);
}

// see b/166779391: cannot change the VNDK interface, so access like this
extern sp<BBinder> the_context_object;

sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;
    std::function<void()> postTask;

    std::unique_lock<std::mutex> _l(mLock);

    if (handle == 0 && the_context_object != nullptr) return the_context_object;

    handle_entry* e = lookupHandleLocked(handle);

    if (e != nullptr) {
        // We need to create a new BpBinder if there isn't currently one, OR we
        // are unable to acquire a weak reference on this current one.  The
        // attemptIncWeak() is safe because we know the BpBinder destructor will always
        // call expungeHandle(), which acquires the same lock we are holding now.
        // We need to do this because there is a race condition between someone
        // releasing a reference on this BpBinder, and a new reference on its handle
        // arriving from the driver.
        IBinder* b = e->binder;
        if (b == nullptr || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {
                // Special case for context manager...
                // The context manager is the only object for which we create
                // a BpBinder proxy without already holding a reference.
                // Perform a dummy transaction to ensure the context manager
                // is registered before we create the first local reference
                // to it (which will occur when creating the BpBinder).
                // If a local reference is created for the BpBinder when the
                // context manager is not present, the driver will fail to
                // provide a reference to the context manager, but the
                // driver API does not return status.
                //
                // Note that this is not race-free if the context manager
                // dies while this code runs.

                IPCThreadState* ipc = IPCThreadState::self();

                CallRestriction originalCallRestriction = ipc->getCallRestriction();
                ipc->setCallRestriction(CallRestriction::NONE);

                Parcel data;
                status_t status = ipc->transact(
                        0, IBinder::PING_TRANSACTION, data, nullptr, 0);

                ipc->setCallRestriction(originalCallRestriction);

                if (status == DEAD_OBJECT)
                   return nullptr;
            }

            sp<BpBinder> b = BpBinder::PrivateAccessor::create(handle, &postTask);
            e->binder = b.get();
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            // This little bit of nastyness is to allow us to add a primary
            // reference to the remote proxy when this team doesn't have one
            // but another team is sending the handle to us.
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    _l.unlock();

    if (postTask) postTask();

    return result;
}

void ProcessState::expungeHandle(int32_t handle, IBinder* binder)
{
    std::unique_lock<std::mutex> _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);

    // This handle may have already been replaced with a new BpBinder
    // (if someone failed the AttemptIncWeak() above); we don't want
    // to overwrite it.
    if (e && e->binder == binder) e->binder = nullptr;
}

String8 ProcessState::makeBinderThreadName() {
    int32_t s = mThreadPoolSeq.fetch_add(1, std::memory_order_release);
    pid_t pid = getpid();

    std::string_view driverName = mDriverName.c_str();
    android::base::ConsumePrefix(&driverName, "/dev/");

    String8 name;
    name.appendFormat("%.*s:%d_%X", static_cast<int>(driverName.length()), driverName.data(), pid,
                      s);
    return name;
}

void ProcessState::spawnPooledThread(bool isMain)
{
    if (mThreadPoolStarted) {
        String8 name = makeBinderThreadName();
        ALOGV("Spawning new pooled thread, name=%s\n", name.c_str());
        sp<Thread> t = sp<PoolThread>::make(isMain);
        t->run(name.c_str());
        mKernelStartedThreads++;
    }
    // TODO: if startThreadPool is called on another thread after the process
    // starts up, the kernel might think that it already requested those
    // binder threads, and additional won't be started. This is likely to
    // cause deadlocks, and it will also cause getThreadPoolMaxTotalThreadCount
    // to return too high of a value.
}

status_t ProcessState::setThreadPoolMaxThreadCount(size_t maxThreads) {
    LOG_ALWAYS_FATAL_IF(mThreadPoolStarted && maxThreads < mMaxThreads,
           "Binder threadpool cannot be shrunk after starting");
    status_t result = NO_ERROR;
    // For RPC-only, we just set the thread count without kernel interaction
    mMaxThreads = maxThreads;
    return result;
}

size_t ProcessState::getThreadPoolMaxTotalThreadCount() const {
    // Need to read `mKernelStartedThreads` before `mThreadPoolStarted` (with
    // non-relaxed memory ordering) to avoid a race like the following:
    //
    // thread A: if (mThreadPoolStarted) { // evaluates false
    // thread B: mThreadPoolStarted = true;
    // thread B: mKernelStartedThreads++;
    // thread A: size_t kernelStarted = mKernelStartedThreads;
    // thread A: LOG_ALWAYS_FATAL_IF(kernelStarted != 0, ...);
    size_t kernelStarted = mKernelStartedThreads;

    if (mThreadPoolStarted) {
        size_t max = mMaxThreads;
        size_t current = mCurrentThreads;

        LOG_ALWAYS_FATAL_IF(kernelStarted > max + 1,
                            "too many kernel-started threads: %zu > %zu + 1", kernelStarted, max);

        // calling startThreadPool starts a thread
        size_t threads = 1;

        // the kernel is configured to start up to mMaxThreads more threads
        threads += max;

        // Users may call IPCThreadState::joinThreadPool directly. We don't
        // currently have a way to count this directly (it could be added by
        // adding a separate private joinKernelThread method in IPCThreadState).
        // So, if we are in a race between the kernel thread variable being
        // incremented in this file and mCurrentThreads being incremented
        // in IPCThreadState, temporarily forget about the extra join threads.
        // This is okay, because most callers of this method only care about
        // having 0, 1, or more threads.
        if (current > kernelStarted) {
            threads += current - kernelStarted;
        }

        return threads;
    }

    // must not be initialized or maybe has poll thread setup, we
    // currently don't track this in libbinder
    LOG_ALWAYS_FATAL_IF(kernelStarted != 0, "Expecting 0 kernel started threads but have %zu",
                        kernelStarted);
    return mCurrentThreads;
}

bool ProcessState::isThreadPoolStarted() const {
    return mThreadPoolStarted;
}

bool ProcessState::isDriverFeatureEnabled(const DriverFeature feature) {
    // For RPC-only, driver features are not available
    return false;
}

status_t ProcessState::enableOnewaySpamDetection(bool enable) {
    // For RPC-only, oneway spam detection is not supported
    ALOGW("enableOnewaySpamDetection not supported in RPC-only mode");
    return NO_ERROR;
}

void ProcessState::giveThreadPoolName() {
    androidSetThreadName(makeBinderThreadName().c_str());
}

String8 ProcessState::getDriverName() {
    return mDriverName;
}

ProcessState::ProcessState(const char* driver)
      : mDriverName(String8(driver)),
        mDriverFD(-1),
        mVMStart(MAP_FAILED),
        mExecutingThreadsCount(0),
        mMaxThreads(DEFAULT_MAX_BINDER_THREADS),
        mCurrentThreads(0),
        mKernelStartedThreads(0),
        mStarvationStartTime(never()),
        mForked(false),
        mThreadPoolStarted(false),
        mThreadPoolSeq(1),
        mCallRestriction(CallRestriction::NONE) {
    // For RPC-only, we don't open any driver file
    // Just initialize with default values






















}

ProcessState::~ProcessState()
{
    // For RPC-only, no cleanup needed
}

} // namespace android
