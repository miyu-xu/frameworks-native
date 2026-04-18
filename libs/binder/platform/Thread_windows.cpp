/*
 * Windows stub implementation for Thread class
 * This file provides minimal implementations needed for Windows compilation
 */

#include <utils/Thread.h>
#include <utils/Log.h>
#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <utils/Timers.h>
#include <utils/RefBase.h>
#include <utils/ThreadDefs.h>

#include <windows.h>
#include <process.h>

namespace android {

// Windows stub implementation for androidSetThreadName
void androidSetThreadName(const char* name) {
    // On Windows, we can set thread names using a different API
    // For now, just log the thread name
    ALOGV("Thread name set to: %s", name);
}

// Thread class implementation for Windows
Thread::Thread(bool canCallJava)
    : mCanCallJava(canCallJava),
      mThread(0),
      mLock(),
      mThreadExitedCondition(),
      mStatus(WOULD_BLOCK),
      mExitPending(false),
      mRunning(false)
#if defined(__ANDROID__)
      , mTid(-1)
#endif
{
}

Thread::~Thread()
{
}

status_t Thread::run(const char* name, int32_t priority, size_t stack)
{
    Mutex::Autolock _l(mLock);
    
    if (mRunning) {
        // thread already started
        return INVALID_OPERATION;
    }
    
    // reset status and exit pending to their defaults
    mStatus = WOULD_BLOCK;
    mExitPending = false;
    mRunning = true;
    
    // Create the thread
    uintptr_t result = _beginthreadex(NULL, stack, 
        (unsigned int (__stdcall *)(void *))&Thread::_threadLoop, this, 0, &mThread);
    
    if (result == 0) {
        mRunning = false;
        return UNKNOWN_ERROR;
    }
    
    return OK;
}

void Thread::requestExit()
{
    Mutex::Autolock _l(mLock);
    mExitPending = true;
}

status_t Thread::readyToRun()
{
    return OK;
}

status_t Thread::requestExitAndWait()
{
    Mutex::Autolock _l(mLock);
    if (mThread == 0) {
        return WOULD_BLOCK;
    }
    
    mExitPending = true;
    
    if (mRunning) {
        mThreadExitedCondition.wait(mLock);
    }
    
    return mStatus;
}

status_t Thread::join()
{
    Mutex::Autolock _l(mLock);
    if (mThread == 0) {
        return WOULD_BLOCK;
    }
    
    if (mRunning) {
        mThreadExitedCondition.wait(mLock);
    }
    
    return mStatus;
}

bool Thread::isRunning() const
{
    Mutex::Autolock _l(mLock);
    return mRunning;
}

bool Thread::exitPending() const
{
    Mutex::Autolock _l(mLock);
    return mExitPending;
}

int Thread::_threadLoop(void* user)
{
    Thread* const self = static_cast<Thread*>(user);
    
    sp<Thread> strong(self->mHoldSelf);
    wp<Thread> weak(strong);
    self->mHoldSelf.clear();
    
    bool first = true;
    
    do {
        bool result;
        if (first) {
            first = false;
            self->mStatus = self->readyToRun();
            if (self->mStatus != OK) {
                break;
            }
            
            result = (self->mExitPending) ? false : self->threadLoop();
        } else {
            result = self->threadLoop();
        }
        
        // establish a scope for the lock
        {
            Mutex::Autolock _l(self->mLock);
            if (result == false || self->mExitPending) {
                self->mExitPending = true;
                self->mRunning = false;
                self->mThreadExitedCondition.broadcast();
                break;
            }
        }
        
        // Release our strong reference, to let a chance to the thread
        // to die a peaceful death.
        strong.clear();
        // And immediately reacquire a strong reference for the next loop
        strong = weak.promote();
    } while (strong != 0);
    
    return 0;
}

} // namespace android
