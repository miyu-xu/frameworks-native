#ifndef __wIN32_SYS_UIO_H
#define __wIN32_SYS_UIO_H
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdio>

// Define missing types
struct iovec {
    void* iov_base;
    size_t iov_len;
};

// Add missing writev function implementation
inline ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        ssize_t written = _write(fd, iov[i].iov_base, iov[i].iov_len);
        if (written < 0) {
            if (total > 0) return total; // Return partial write
            return -1;
        }
        total += written;
        if (written < static_cast<ssize_t>(iov[i].iov_len)) {
            return total; // Partial write
        }
    }
    return total;
}

#endif // __wIN32_SYS_UIO_H