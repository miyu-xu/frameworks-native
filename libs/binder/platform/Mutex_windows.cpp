#include <utils/Mutex.h>
#include <windows.h>

namespace android {

Mutex::Mutex() {
    _init();
}

Mutex::Mutex(const char* name) {
    _init();
}

Mutex::Mutex(int type, const char* name) {
    _init();
    // Note: Windows doesn't support process-shared mutexes in the same way as pthreads
    // For simplicity, we'll ignore the type parameter for now
}

Mutex::~Mutex() {
    if (mState) {
        DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(mState));
        delete reinterpret_cast<CRITICAL_SECTION*>(mState);
        mState = nullptr;
    }
}

void Mutex::_init() {
    mState = new CRITICAL_SECTION();
    InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(mState));
}

status_t Mutex::lock() {
    EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(mState));
    return 0;
}

void Mutex::unlock() {
    LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(mState));
}

status_t Mutex::tryLock() {
    if (TryEnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(mState))) {
        return 0;
    }
    return -1;
}

} // namespace android
