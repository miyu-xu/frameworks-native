#define LOG_TAG "IPCThreadStateHost"

#include <binder/IPCThreadState.h>

#include <unistd.h>

#include <binder/Binder.h>
#include <log/log.h>

namespace android {

namespace {

thread_local IPCThreadState* gCurrentThreadState = nullptr;
bool gBackgroundSchedulingDisabled = false;

void logKernelBinderUnavailable(const char* api) {
    ALOGE("%s is unavailable in RPC-only host builds", api);
}

} // namespace

sp<BBinder> the_context_object;

IPCThreadState::IPCThreadState()
    : mProcess(ProcessState::selfOrNull()),
      mLastError(OK),
      mServingStackPointer(nullptr),
      mServingStackPointerGuard(nullptr),
      mCallingPid(getpid()),
      mCallingSid(nullptr),
      mCallingUid(getuid()),
      mWorkSource(kUnsetWorkSource),
      mPropagateWorkSource(false),
      mIsLooper(false),
      mIsFlushing(false),
      mHasExplicitIdentity(false),
      mStrictModePolicy(0),
      mLastTransactionBinderFlags(0),
      mCallRestriction(ProcessState::CallRestriction::NONE) {}

IPCThreadState::~IPCThreadState() = default;

IPCThreadState* IPCThreadState::self() {
    if (!gCurrentThreadState) {
        gCurrentThreadState = new IPCThreadState();
    }
    return gCurrentThreadState;
}

IPCThreadState* IPCThreadState::selfOrNull() {
    return gCurrentThreadState;
}

status_t IPCThreadState::freeze(pid_t, bool, uint32_t) {
    logKernelBinderUnavailable("IPCThreadState::freeze");
    return INVALID_OPERATION;
}

status_t IPCThreadState::getProcessFreezeInfo(pid_t, uint32_t*, uint32_t*) {
    logKernelBinderUnavailable("IPCThreadState::getProcessFreezeInfo");
    return INVALID_OPERATION;
}

status_t IPCThreadState::clearLastError() {
    status_t lastError = mLastError;
    mLastError = OK;
    return lastError;
}

pid_t IPCThreadState::getCallingPid() const {
    return mCallingPid;
}

const char* IPCThreadState::getCallingSid() const {
    return mCallingSid;
}

uid_t IPCThreadState::getCallingUid() const {
    return mCallingUid;
}

const IPCThreadState::SpGuard* IPCThreadState::pushGetCallingSpGuard(const SpGuard* guard) {
    const SpGuard* original = mServingStackPointerGuard;
    mServingStackPointerGuard = guard;
    return original;
}

void IPCThreadState::restoreGetCallingSpGuard(const SpGuard* guard) {
    mServingStackPointerGuard = guard;
}

void IPCThreadState::checkContextIsBinderForUse(const char*) const {}

void IPCThreadState::setStrictModePolicy(int32_t policy) {
    mStrictModePolicy = policy;
}

int32_t IPCThreadState::getStrictModePolicy() const {
    return mStrictModePolicy;
}

int64_t IPCThreadState::setCallingWorkSourceUid(uid_t uid) {
    int64_t token = mWorkSource;
    mWorkSource = uid;
    return token;
}

int64_t IPCThreadState::setCallingWorkSourceUidWithoutPropagation(uid_t uid) {
    return setCallingWorkSourceUid(uid);
}

uid_t IPCThreadState::getCallingWorkSourceUid() const {
    return mWorkSource == kUnsetWorkSource ? static_cast<uid_t>(kUnsetWorkSource)
                                          : static_cast<uid_t>(mWorkSource);
}

int64_t IPCThreadState::clearCallingWorkSource() {
    int64_t token = mWorkSource;
    mWorkSource = kUnsetWorkSource;
    return token;
}

void IPCThreadState::restoreCallingWorkSource(int64_t token) {
    mWorkSource = static_cast<int32_t>(token);
}

void IPCThreadState::clearPropagateWorkSource() {
    mPropagateWorkSource = false;
}

bool IPCThreadState::shouldPropagateWorkSource() const {
    return mPropagateWorkSource;
}

void IPCThreadState::setLastTransactionBinderFlags(int32_t flags) {
    mLastTransactionBinderFlags = flags;
}

int32_t IPCThreadState::getLastTransactionBinderFlags() const {
    return mLastTransactionBinderFlags;
}

void IPCThreadState::setCallRestriction(CallRestriction restriction) {
    mCallRestriction = restriction;
}

IPCThreadState::CallRestriction IPCThreadState::getCallRestriction() const {
    return mCallRestriction;
}

int64_t IPCThreadState::clearCallingIdentity() {
    const int64_t token = (static_cast<int64_t>(mCallingUid) << 32) |
            static_cast<uint32_t>(mCallingPid);
    mCallingPid = getpid();
    mCallingUid = getuid();
    mHasExplicitIdentity = false;
    return token;
}

void IPCThreadState::restoreCallingIdentity(int64_t) {
    mCallingPid = getpid();
    mCallingUid = getuid();
    mHasExplicitIdentity = true;
}

bool IPCThreadState::hasExplicitIdentity() {
    return mHasExplicitIdentity;
}

status_t IPCThreadState::setupPolling(int*) {
    logKernelBinderUnavailable("IPCThreadState::setupPolling");
    return INVALID_OPERATION;
}

status_t IPCThreadState::handlePolledCommands() {
    logKernelBinderUnavailable("IPCThreadState::handlePolledCommands");
    return INVALID_OPERATION;
}

void IPCThreadState::flushCommands() {}

bool IPCThreadState::flushIfNeeded() {
    return false;
}

void IPCThreadState::joinThreadPool(bool isMain) {
    mIsLooper = isMain;
}

void IPCThreadState::stopProcess(bool) {
    logKernelBinderUnavailable("IPCThreadState::stopProcess");
}

status_t IPCThreadState::transact(int32_t, uint32_t, const Parcel&, Parcel*, uint32_t) {
    logKernelBinderUnavailable("IPCThreadState::transact");
    return INVALID_OPERATION;
}

void IPCThreadState::incStrongHandle(int32_t, BpBinder*) {}

void IPCThreadState::decStrongHandle(int32_t) {}

void IPCThreadState::incWeakHandle(int32_t, BpBinder*) {}

void IPCThreadState::decWeakHandle(int32_t) {}

status_t IPCThreadState::attemptIncStrongHandle(int32_t) {
    logKernelBinderUnavailable("IPCThreadState::attemptIncStrongHandle");
    return INVALID_OPERATION;
}

void IPCThreadState::expungeHandle(int32_t, IBinder*) {}

status_t IPCThreadState::requestDeathNotification(int32_t, BpBinder*) {
    logKernelBinderUnavailable("IPCThreadState::requestDeathNotification");
    return INVALID_OPERATION;
}

status_t IPCThreadState::clearDeathNotification(int32_t, BpBinder*) {
    logKernelBinderUnavailable("IPCThreadState::clearDeathNotification");
    return INVALID_OPERATION;
}

status_t IPCThreadState::addFrozenStateChangeCallback(int32_t, BpBinder*) {
    logKernelBinderUnavailable("IPCThreadState::addFrozenStateChangeCallback");
    return INVALID_OPERATION;
}

status_t IPCThreadState::removeFrozenStateChangeCallback(int32_t, BpBinder*) {
    logKernelBinderUnavailable("IPCThreadState::removeFrozenStateChangeCallback");
    return INVALID_OPERATION;
}

void IPCThreadState::shutdown() {}

void IPCThreadState::disableBackgroundScheduling(bool disable) {
    gBackgroundSchedulingDisabled = disable;
}

bool IPCThreadState::backgroundSchedulingDisabled() {
    return gBackgroundSchedulingDisabled;
}

void IPCThreadState::blockUntilThreadAvailable() {}

void IPCThreadState::setTheContextObject(const sp<BBinder>&) {
    logKernelBinderUnavailable("IPCThreadState::setTheContextObject");
}

const void* IPCThreadState::getServingStackPointer() const {
    return mServingStackPointer;
}

status_t IPCThreadState::sendReply(const Parcel&, uint32_t) {
    return INVALID_OPERATION;
}

status_t IPCThreadState::waitForResponse(Parcel*, status_t*) {
    return INVALID_OPERATION;
}

status_t IPCThreadState::talkWithDriver(bool) {
    return INVALID_OPERATION;
}

status_t IPCThreadState::writeTransactionData(int32_t, uint32_t, int32_t, uint32_t, const Parcel&,
                                              status_t*) {
    return INVALID_OPERATION;
}

status_t IPCThreadState::getAndExecuteCommand() {
    return INVALID_OPERATION;
}

status_t IPCThreadState::executeCommand(int32_t) {
    return INVALID_OPERATION;
}

void IPCThreadState::processPendingDerefs() {}

void IPCThreadState::processPostWriteDerefs() {}

void IPCThreadState::clearCaller() {
    mCallingPid = getpid();
    mCallingSid = nullptr;
    mCallingUid = getuid();
}

void IPCThreadState::threadDestructor(void*) {}

void IPCThreadState::freeBuffer(const uint8_t*, size_t, const binder_size_t*, size_t) {}

void IPCThreadState::logExtendedError() {}

} // namespace android
