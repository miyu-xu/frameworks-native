#pragma once

#include <fcntl.h>
#include <sys/socket.h>

#include <binder/unique_fd.h>

#include "../OS.h"
#include "../Utils.h"

namespace android::binder::os {

inline status_t setCloseOnExec(borrowed_fd fd) {
#ifdef PLATFORM_WINDOWS
    // Windows handles are non-inheritable by default in the socket paths used
    // by the host build. MinGW does not provide the POSIX F_GETFD/F_SETFD
    // commands, so there is nothing to apply here.
    (void)fd;
    return OK;
#else
    int flags = TEMP_FAILURE_RETRY(fcntl(fd.get(), F_GETFD));
    if (flags == -1) {
        return -errno;
    }
    if (TEMP_FAILURE_RETRY(fcntl(fd.get(), F_SETFD, flags | FD_CLOEXEC)) == -1) {
        return -errno;
    }
    return OK;
#endif
}

inline status_t setSocketHostFlags(borrowed_fd fd) {
#ifdef PLATFORM_MACOS
    status_t status = setCloseOnExec(fd);
    if (status != OK) {
        return status;
    }
    return setNonBlocking(fd);
#else
    (void)fd;
    return OK;
#endif
}

inline unique_fd makeHostSocket(int domain, int type, int protocol) {
#ifdef PLATFORM_MACOS
    unique_fd fd(TEMP_FAILURE_RETRY(socket(domain, type, protocol)));
    if (!fd.ok()) {
        return fd;
    }
    if (status_t status = setSocketHostFlags(fd); status != OK) {
        errno = -status;
        return {};
    }
    return fd;
#else
    return unique_fd(TEMP_FAILURE_RETRY(socket(domain, type | SOCK_CLOEXEC | SOCK_NONBLOCK,
                                               protocol)));
#endif
}

inline unique_fd acceptHostSocket(borrowed_fd fd) {
#ifdef PLATFORM_MACOS
    unique_fd accepted(TEMP_FAILURE_RETRY(accept(fd.get(), nullptr, nullptr)));
    if (!accepted.ok()) {
        return accepted;
    }
    if (status_t status = setSocketHostFlags(accepted); status != OK) {
        errno = -status;
        return {};
    }
    return accepted;
#else
    return unique_fd(TEMP_FAILURE_RETRY(
            accept4(fd.get(), nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK)));
#endif
}

inline status_t makeHostSocketPair(int domain, int type, int protocol, int socks[2]) {
#ifdef PLATFORM_WINDOWS
    SOCKET nativeSocks[2];
    if (TEMP_FAILURE_RETRY(socketpair(domain, type, protocol, nativeSocks)) < 0) {
        return -errno;
    }
    socks[0] = static_cast<int>(nativeSocks[0]);
    socks[1] = static_cast<int>(nativeSocks[1]);
    return OK;
#elif defined(PLATFORM_MACOS)
    if (TEMP_FAILURE_RETRY(socketpair(domain, type, protocol, socks)) < 0) {
        return -errno;
    }

    unique_fd first(socks[0]);
    unique_fd second(socks[1]);

    status_t status = setSocketHostFlags(first);
    if (status != OK) {
        return status;
    }
    status = setSocketHostFlags(second);
    if (status != OK) {
        return status;
    }

    socks[0] = first.release();
    socks[1] = second.release();
    return OK;
#else
    if (TEMP_FAILURE_RETRY(socketpair(domain, type | SOCK_CLOEXEC | SOCK_NONBLOCK, protocol,
                                      socks)) < 0) {
        return -errno;
    }
    return OK;
#endif
}

} // namespace android::binder::os
