#ifndef __WIN32_SYS_SOCKET_H__
#define __WIN32_SYS_SOCKET_H__
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

// Windows类型定义
using socket_t = SOCKET;
using fd_t = HANDLE;
using nfds_t = unsigned long;
#define INVALID_SOCKET_HANDLE INVALID_SOCKET
#define INVALID_FD_HANDLE INVALID_HANDLE_VALUE

// Windows错误处理
#define PLATFORM_ERRNO WSAGetLastError()
#define PLATFORM_STRERROR(code) strerror(code)

struct sockaddr_un {
ADDRESS_FAMILY sun_family;
char sun_path[108];
};

/** The type of fields like `sa_family`. */
typedef unsigned short sa_family_t;

#ifndef AF_UNIX
#define AF_UNIX 1
#endif

#ifndef AF_VSOCK
#define AF_VSOCK 40
#endif

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0x80000
#endif

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0x800
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0x40
#endif

#ifndef TCP_NODELAY
#define TCP_NODELAY 0x0001
#endif

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0
#endif

#ifndef EINVAL
#define EINVAL WSAEINVAL
#endif
#ifndef EAGAIN
#define EAGAIN WSAEWOULDBLOCK
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef EINTR
#define EINTR WSAEINTR
#endif
#ifndef EACCES
#define EACCES WSAEACCES
#endif
#ifndef EADDRINUSE
#define EADDRINUSE WSAEADDRINUSE
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif
#ifndef EALREADY
#define EALREADY WSAEALREADY
#endif
#ifndef EBADF
#define EBADF WSAEBADF
#endif
#ifndef ECONNABORTED
#define ECONNABORTED WSAECONNABORTED
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#endif
#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif
#ifndef EDESTADDRREQ
#define EDESTADDRREQ WSAEDESTADDRREQ
#endif
#ifndef EFAULT
#define EFAULT WSAEFAULT
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH WSAEHOSTUNREACH
#endif
#ifndef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#endif
#ifndef EISCONN
#define EISCONN WSAEISCONN
#endif
#ifndef ELOOP
#define ELOOP WSAELOOP
#endif
#ifndef EMFILE
#define EMFILE WSAEMFILE
#endif
#ifndef EMSGSIZE
#define EMSGSIZE WSAEMSGSIZE
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG WSAENAMETOOLONG
#endif
#ifndef ENETDOWN
#define ENETDOWN WSAENETDOWN
#endif
#ifndef ENETRESET
#define ENETRESET WSAENETRESET
#endif
#ifndef ENETUNREACH
#define ENETUNREACH WSAENETUNREACH
#endif
#ifndef ENOBUFS
#define ENOBUFS WSAENOBUFS
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT WSAENOPROTOOPT
#endif
#ifndef ENOTCONN
#define ENOTCONN WSAENOTCONN
#endif
#ifndef ENOTSOCK
#define ENOTSOCK WSAENOTSOCK
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP WSAEOPNOTSUPP
#endif
#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT WSAEPFNOSUPPORT
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT WSAEPROTONOSUPPORT
#endif
#ifndef EPROTOTYPE
#define EPROTOTYPE WSAEPROTOTYPE
#endif
#ifndef ERANGE
#define ERANGE WSAERANGE
#endif
#ifndef ESHUTDOWN
#define ESHUTDOWN WSAESHUTDOWN
#endif
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#endif
#ifndef ETOOMANYREFS
#define ETOOMANYREFS WSAETOOMANYREFS
#endif
#ifndef EUSERS
#define EUSERS WSAEUSERS
#endif

#ifndef CMSG_FIRSTHDR
#define CMSG_FIRSTHDR(mhdr) ((struct cmsghdr*)((char*)(mhdr)->msg_control))
#endif
#ifndef CMSG_NXTHDR
#define CMSG_NXTHDR(mhdr, cmsg) ((struct cmsghdr*)((char*)(cmsg) + ALIGN((cmsg)->cmsg_len)))
#endif
#ifndef CMSG_DATA
#define CMSG_DATA(cmsg) ((unsigned char*)((cmsg) + 1))
#endif
#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (ALIGN(sizeof(struct cmsghdr)) + ALIGN(len))
#endif
#ifndef CMSG_LEN
#define CMSG_LEN(len) (ALIGN(sizeof(struct cmsghdr)) + (len))
#endif

#ifndef _Nullable
#if defined(__clang__) && __has_feature(nullability)
#define _Nullable __attribute__((__nullable__))
#else
#define _Nullable
#endif
#endif

#ifndef _Nonnull
#if defined(__clang__) && __has_feature(nullability)
#define _Nonnull __attribute__((__nonnull__))
#else
#define _Nonnull
#endif
#endif

#ifndef _Null_unspecified
#if defined(__clang__) && __has_feature(nullability)
#define _Null_unspecified __attribute__((__null_unspecified__))
#else
#define _Null_unspecified
#endif
#endif


#define TEMP_FAILURE_RETRY(expression) \
    ({ \
        decltype(expression) _result; \
        do { \
            _result = (expression); \
        } while (_result == (decltype(expression))-1 && errno == EINTR); \
        _result; \
    })


struct msghdr {
    void* msg_name;
    socklen_t msg_namelen;
    struct iovec* msg_iov;
    size_t msg_iovlen;
    void* msg_control;
    size_t msg_controllen;
    int msg_flags;
};

struct cmsghdr {
    socklen_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};

inline int socketpair(int domain, int type, int protocol, SOCKET sv[2]) {
    SOCKET listener = socket(domain, type, protocol);
    if (listener == INVALID_SOCKET) return -1;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = domain;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // Let system choose port
    
    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listener);
        return -1;
    }
    
    if (listen(listener, 1) == SOCKET_ERROR) {
        closesocket(listener);
        return -1;
    }
    
    int addrlen = sizeof(addr);
    if (getsockname(listener, (struct sockaddr*)&addr, &addrlen) == SOCKET_ERROR) {
        closesocket(listener);
        return -1;
    }
    
    sv[0] = socket(domain, type, protocol);
    if (sv[0] == INVALID_SOCKET) {
        closesocket(listener);
        return -1;
    }
    
    if (connect(sv[0], (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listener);
        closesocket(sv[0]);
        return -1;
    }
    
    sv[1] = accept(listener, NULL, NULL);
    if (sv[1] == INVALID_SOCKET) {
        closesocket(listener);
        closesocket(sv[0]);
        return -1;
    }
    
    closesocket(listener);
    return 0;
}

inline SOCKET accept4(SOCKET sock, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    return accept(sock, addr, addrlen);
}

#endif // __WIN32_SYS_SOCKET_H__
