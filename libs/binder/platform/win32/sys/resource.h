#ifndef __WIN32_SYS_RESOURCE_H__
#define __WIN32_SYS_RESOURCE_H__
#include <windows.h>
#include <processthreadsapi.h>

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN -1

struct rusage {
    struct timeval ru_utime; /* user CPU time used */
    struct timeval ru_stime; /* system CPU time used */
    long   ru_maxrss;        /* maximum resident set size */
    long   ru_ixrss;         /* integral shared memory size */
    long   ru_idrss;         /* integral unshared data size */
    long   ru_isrss;         /* integral unshared stack size */
    long   ru_minflt;        /* page reclaims (soft page faults) */
    long   ru_majflt;        /* page faults (hard page faults) */
    long   ru_nswap;         /* swaps */
    long   ru_inblock;       /* block input operations */
    long   ru_oublock;       /* block output operations */
    long   ru_msgsnd;        /* IPC messages sent */
    long   ru_msgrcv;        /* IPC messages received */
    long   ru_nsignals;      /* signals received */
    long   ru_nvcsw;         /* voluntary context switches */
    long   ru_nivcsw;        /* involuntary context switches */
};


inline int getrusage(int who, struct rusage* usage) {
    if (usage == nullptr) {
        errno = EINVAL;
        return -1;
    }

    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (!GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        errno = EINVAL;
        return -1;
    }

    ULARGE_INTEGER userTimeUL, kernelTimeUL;
    userTimeUL.LowPart = userTime.dwLowDateTime;
    userTimeUL.HighPart = userTime.dwHighDateTime;
    kernelTimeUL.LowPart = kernelTime.dwLowDateTime;
    kernelTimeUL.HighPart = kernelTime.dwHighDateTime;

    usage->ru_utime.tv_sec = static_cast<long>(userTimeUL.QuadPart / 10000000);
    usage->ru_utime.tv_usec = static_cast<long>((userTimeUL.QuadPart % 10000000) / 10);
    
    usage->ru_stime.tv_sec = static_cast<long>(kernelTimeUL.QuadPart / 10000000);
    usage->ru_stime.tv_usec = static_cast<long>((kernelTimeUL.QuadPart % 10000000) / 10);

    usage->ru_maxrss = 0;
    usage->ru_ixrss = 0;
    usage->ru_idrss = 0;
    usage->ru_isrss = 0;
    usage->ru_minflt = 0;
    usage->ru_majflt = 0;
    usage->ru_nswap = 0;
    usage->ru_inblock = 0;
    usage->ru_oublock = 0;
    usage->ru_msgsnd = 0;
    usage->ru_msgrcv = 0;
    usage->ru_nsignals = 0;
    usage->ru_nvcsw = 0;
    usage->ru_nivcsw = 0;

    return 0;
}

#endif // __WIN32_SYS_RESOURCE_H__