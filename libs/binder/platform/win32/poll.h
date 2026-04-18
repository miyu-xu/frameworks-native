#ifndef __WIN32_POLL_H__
#define __WIN32_POLL_H__
#ifndef POLLIN
#define POLLIN  0x001
#endif
#ifndef POLLOUT
#define POLLOUT 0x004
#endif
#ifndef POLLERR
#define POLLERR 0x008
#endif
#ifndef POLLHUP
#define POLLHUP 0x010
#endif

#include <winsock2.h>
inline int windows_poll(struct pollfd* fds, unsigned int nfds, int timeout) {
    return WSAPoll(fds, nfds, timeout);
}

#endif // __WIN32_POLL_H__
