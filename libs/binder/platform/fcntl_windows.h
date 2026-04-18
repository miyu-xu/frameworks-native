#ifndef __WIN32_DLFCN_H__
#define __WIN32_DLFCN_H__
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdio>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 04000
#endif
#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC 0x400
#endif

inline int fcntl(int fd, int cmd, ...) {
    va_list args;
    va_start(args, cmd);
    
    int result = -1;
    
    switch (cmd) {
        case F_DUPFD_CLOEXEC: {
            int arg = va_arg(args, int);
            int new_fd = _dup(fd);
            if (new_fd != -1) {
                _setmode(new_fd, _O_BINARY);
                result = new_fd;
            }
            break;
        }
        case F_SETFL: {
            int arg = va_arg(args, int);
            if (fd >= 0) {
                unsigned int mode = (arg & O_NONBLOCK) ? 1UL : 0UL;
                if (ioctlsocket(fd, FIONBIO, &mode) == 0) {
                    result = 0;
                }
            }
            break;
        }
        case F_GETFL: {
            result = 0;
            break;
        }
        default:
            errno = EINVAL;
            result = -1;
            break;
    }
    
    va_end(args);
    return result;
}

#endif // __WIN32_DLFCN_H__