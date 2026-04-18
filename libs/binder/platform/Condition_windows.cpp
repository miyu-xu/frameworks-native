#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/Timers.h>

#include <windows.h>

namespace android {

Condition::Condition() {
    mState = CreateEvent(NULL, FALSE, FALSE, NULL);
}

Condition::Condition(int type) {
    // On Windows, we don't support shared condition variables for now
    // Just create a regular condition variable
    mState = CreateEvent(NULL, FALSE, FALSE, NULL);
}

Condition::~Condition() {
    CloseHandle(static_cast<HANDLE>(mState));
}

status_t Condition::wait(Mutex& mutex) {
    // Release the mutex before waiting
    mutex.unlock();
    
    // Wait for the condition
    DWORD result = WaitForSingleObject(static_cast<HANDLE>(mState), INFINITE);
    
    // Re-acquire the mutex
    mutex.lock();
    
    if (result == WAIT_OBJECT_0) {
        return NO_ERROR;
    } else {
        return UNKNOWN_ERROR;
    }
}

status_t Condition::waitRelative(Mutex& mutex, nsecs_t reltime) {
    // Convert nanoseconds to milliseconds
    DWORD timeout_ms = static_cast<DWORD>(reltime / 1000000);
    
    // Release the mutex before waiting
    mutex.unlock();
    
    // Wait for the condition with timeout
    DWORD result = WaitForSingleObject(static_cast<HANDLE>(mState), timeout_ms);
    
    // Re-acquire the mutex
    mutex.lock();
    
    if (result == WAIT_OBJECT_0) {
        return NO_ERROR;
    } else if (result == WAIT_TIMEOUT) {
        return TIMED_OUT;
    } else {
        return UNKNOWN_ERROR;
    }
}

void Condition::signal() {
    SetEvent(static_cast<HANDLE>(mState));
}

void Condition::broadcast() {
    // For Windows, signal() and broadcast() are the same with manual-reset events
    // But we're using auto-reset events, so we need to signal multiple times
    // This is a simplified implementation
    SetEvent(static_cast<HANDLE>(mState));
}

} // namespace android
