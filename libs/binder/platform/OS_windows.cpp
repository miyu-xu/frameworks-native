#include "OS.h"
#include <binder/RpcTransport.h>
#include <binder/RpcTransportRaw.h>
#include <binder/unique_fd.h>
#include <utils/Errors.h>
#include <utils/Timers.h>
#include <log/log.h>
#include <sys/socket.h>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <climits>
#include <random>

int set_non_blocking(socket_t sock) {
    unsigned int mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
}

int get_random_bytes(uint8_t* data, size_t size) {
    HCRYPTPROV hProv;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        std::random_device rd;
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            data[i] = dist(rd);
        }
        return 0;
    }
    
    BOOL success = CryptGenRandom(hProv, static_cast<DWORD>(size), data);
    CryptReleaseContext(hProv, 0);
    
    return success ? 0 : -1;
}

fd_t fd_dup(fd_t fd) {
    HANDLE new_handle;
    if (!DuplicateHandle(GetCurrentProcess(), fd,
                        GetCurrentProcess(), &new_handle,
                        0, FALSE, DUPLICATE_SAME_ACCESS)) {
        return INVALID_FD_HANDLE;
    }
    return new_handle;
}

namespace android::binder::os {

void trace_begin(uint64_t, const char*) {
}

void trace_end(uint64_t) {
}

void trace_int(uint64_t, const char*, int32_t) {
}

uint64_t get_trace_enabled_tags() {
    return 0;
}

uint64_t GetThreadId() {
    return GetCurrentThreadId();
}

bool report_sysprop_change() {
    return false;
}

status_t setNonBlocking(borrowed_fd fd) {
    return set_non_blocking(fd.get()) == 0 ? OK : UNKNOWN_ERROR;
}

status_t getRandomBytes(uint8_t* data, size_t size) {
    return get_random_bytes(data, size) == 0 ? OK : UNKNOWN_ERROR;
}

status_t dupFileDescriptor(int oldFd, int* newFd) {
    fd_t new_handle = fd_dup(reinterpret_cast<fd_t>(static_cast<intptr_t>(oldFd)));
    if (new_handle == INVALID_FD_HANDLE) {
        return UNKNOWN_ERROR;
    }
    *newFd = static_cast<int>(reinterpret_cast<intptr_t>(new_handle));
    return OK;
}

ssize_t sendMessageOnSocket(const RpcTransportFd& socket, iovec* iovs, int niovs,
                           const std::vector<std::variant<unique_fd, borrowed_fd>>* ancillaryFds) {
    if (ancillaryFds != nullptr && !ancillaryFds->empty()) {
        return -1;
    }
    
    for (int i = 0; i < niovs; ++i) {
        if (iovs[i].iov_len == 16) {
            uint8_t* data = static_cast<uint8_t*>(iovs[i].iov_base);
            uint32_t* command = reinterpret_cast<uint32_t*>(data);
            uint32_t* bodySize = reinterpret_cast<uint32_t*>(data + 4);
            
            *command = htonl(*command);
            *bodySize = htonl(*bodySize);
        }
    }
    
    WSABUF* buffers = new WSABUF[niovs];
    for (int i = 0; i < niovs; ++i) {
        buffers[i].buf = reinterpret_cast<CHAR*>(iovs[i].iov_base);
        buffers[i].len = static_cast<ULONG>(iovs[i].iov_len);
    }
    
    DWORD bytes_sent = 0;
    int result = WSASend(socket.fd.get(), buffers, niovs, &bytes_sent, 0, NULL, NULL);
    
    delete[] buffers;
    
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        switch (error) {
            case WSAEWOULDBLOCK:
                errno = EAGAIN;
                break;
            case WSAECONNRESET:
                errno = ECONNRESET;
                break;
            case WSAECONNABORTED:
                errno = ECONNABORTED;
                break;
            case WSAENETDOWN:
                errno = ENETDOWN;
                break;
            case WSAENETRESET:
                errno = ENETRESET;
                break;
            case WSAENETUNREACH:
                errno = ENETUNREACH;
                break;
            case WSAEHOSTUNREACH:
                errno = EHOSTUNREACH;
                break;
            case WSAETIMEDOUT:
                errno = ETIMEDOUT;
                break;
            case WSAECONNREFUSED:
                errno = ECONNREFUSED;
                break;
            default:
                errno = EIO;
                break;
        }
        return -1;
    }
    return static_cast<ssize_t>(bytes_sent);
}

ssize_t receiveMessageFromSocket(const RpcTransportFd& socket, iovec* iovs, int niovs,
                                std::vector<std::variant<unique_fd, borrowed_fd>>* ancillaryFds) {
    if (ancillaryFds != nullptr) {
        ancillaryFds->clear();
    }
    
    WSABUF* buffers = new WSABUF[niovs];
    for (int i = 0; i < niovs; ++i) {
        buffers[i].buf = reinterpret_cast<CHAR*>(iovs[i].iov_base);
        buffers[i].len = static_cast<ULONG>(iovs[i].iov_len);
    }
    
    DWORD bytes_received = 0;
    DWORD flags = 0;
    int result = WSARecv(socket.fd.get(), buffers, niovs, &bytes_received, &flags, NULL, NULL);
    
    delete[] buffers;
    
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        switch (error) {
            case WSAEWOULDBLOCK:
                errno = EAGAIN;
                break;
            case WSAECONNRESET:
                errno = ECONNRESET;
                break;
            case WSAECONNABORTED:
                errno = ECONNABORTED;
                break;
            case WSAENETDOWN:
                errno = ENETDOWN;
                break;
            case WSAENETRESET:
                errno = ENETRESET;
                break;
            case WSAENETUNREACH:
                errno = ENETUNREACH;
                break;
            case WSAEHOSTUNREACH:
                errno = EHOSTUNREACH;
                break;
            case WSAETIMEDOUT:
                errno = ETIMEDOUT;
                break;
            case WSAECONNREFUSED:
                errno = ECONNREFUSED;
                break;
            default:
                errno = EIO;
                break;
        }
        return -1;
    }
    
    for (int i = 0; i < niovs; ++i) {
        if (iovs[i].iov_len == 16) {
            uint8_t* data = static_cast<uint8_t*>(iovs[i].iov_base);
            uint32_t* command = reinterpret_cast<uint32_t*>(data);
            uint32_t* bodySize = reinterpret_cast<uint32_t*>(data + 4);
            
            *command = ntohl(*command);
            *bodySize = ntohl(*bodySize);
        }
    }
    
    return static_cast<ssize_t>(bytes_received);
}

std::unique_ptr<RpcTransportCtxFactory> makeDefaultRpcTransportCtxFactory() {
    return RpcTransportCtxFactoryRaw::make();
}

} // namespace android::binder::os

extern "C" nsecs_t systemTime(int clock) {
    if (clock < SYSTEM_TIME_REALTIME || clock > SYSTEM_TIME_BOOTTIME) {
        std::abort();
    }

    if (clock == SYSTEM_TIME_REALTIME) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                .count();
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
}

extern "C" int toMillisecondTimeoutDelay(nsecs_t referenceTime, nsecs_t timeoutTime) {
    if (timeoutTime <= referenceTime) return 0;
    const uint64_t timeoutDelay = static_cast<uint64_t>(timeoutTime - referenceTime);
    if (timeoutDelay > static_cast<uint64_t>((INT_MAX - 1) * 1000000LL)) return -1;
    return static_cast<int>((timeoutDelay + 999999ULL) / 1000000ULL);
}

extern "C" LIBBINDER_EXPORTED int __android_log_print(int, const char*, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const int result = std::vfprintf(stderr, fmt, args);
    va_end(args);
    return result;
}


