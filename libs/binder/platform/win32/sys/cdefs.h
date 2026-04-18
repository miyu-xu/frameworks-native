#ifndef __WIN32_SYS_CDEFS_H
#define __WIN32_SYS_CDEFS_H

#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

/*
 * uid_t must stay `int` to match NDK binder_parcel.h and binder/Parcel.h on this port.
 * pid_t follows MinGW-style _pid_t (int on 32-bit, __int64 on Win64).
 */
typedef int uid_t;

#ifndef _PID_T_
#define _PID_T_
#ifndef _WIN64
typedef int _pid_t;
#else
typedef __int64 _pid_t;
#endif

#ifndef NO_OLDNAMES
#undef pid_t
typedef _pid_t pid_t;
#endif
#endif /* Not _PID_T_ */

#endif /* __WIN32_SYS_CDEFS_H */
