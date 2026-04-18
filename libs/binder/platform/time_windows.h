#ifndef __WIN32_TIME_H__
#define __WIN32_TIME_H__
#include <windows.h>

#ifndef TIME_UTC
#define TIME_UTC 1
#endif

inline int timespec_get(struct timespec* ts, int base) {
    if (base != TIME_UTC) {
        errno = EINVAL;
        return 0;
    }
    
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    
    ull.QuadPart -= 116444736000000000ULL;
    
    ts->tv_sec = ull.QuadPart / 10000000;
    ts->tv_nsec = (ull.QuadPart % 10000000) * 100;
    
    return base;
}

#endif // __WIN32_TIME_H__
