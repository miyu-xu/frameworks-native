#ifndef __WIN32_SYS_IOCTL_H__
#define __WIN32_SYS_IOCTL_H__

#ifndef FIONBIO
#define FIONBIO 0x8004667E
#endif

#ifndef TIOCGWINSZ
#define TIOCGWINSZ 0x5413
#endif

#ifndef FIONREAD
#define FIONREAD 0x541B
#endif

#ifndef _IOC_NONE
#define _IOC_NONE  0U
#endif
#ifndef _IOC_WRITE
#define _IOC_WRITE 1U
#endif
#ifndef _IOC_READ
#define _IOC_READ  2U
#endif

#ifndef _IOC_NRBITS
#define _IOC_NRBITS  8
#endif
#ifndef _IOC_TYPEBITS
#define _IOC_TYPEBITS 8
#endif
#ifndef _IOC_SIZEBITS
#define _IOC_SIZEBITS 14
#endif
#ifndef _IOC_DIRBITS
#define _IOC_DIRBITS  2
#endif

#ifndef _IOC_NRMASK
#define _IOC_NRMASK ((1 << _IOC_NRBITS) - 1)
#endif
#ifndef _IOC_TYPEMASK
#define _IOC_TYPEMASK ((1 << _IOC_TYPEBITS) - 1)
#endif
#ifndef _IOC_SIZEMASK
#define _IOC_SIZEMASK ((1 << _IOC_SIZEBITS) - 1)
#endif
#ifndef _IOC_DIRMASK
#define _IOC_DIRMASK ((1 << _IOC_DIRBITS) - 1)
#endif

#ifndef _IOC_NRSHIFT
#define _IOC_NRSHIFT 0
#endif
#ifndef _IOC_TYPESHIFT
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#endif
#ifndef _IOC_SIZESHIFT
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#endif
#ifndef _IOC_DIRSHIFT
#define _IOC_DIRSHIFT (_IOC_SIZESHIFT + _IOC_SIZEBITS)
#endif

#ifndef _IOC
#define _IOC(dir, type, nr, size) \
    (((dir)  << _IOC_DIRSHIFT) | \
     ((type) << _IOC_TYPESHIFT) | \
     ((nr)   << _IOC_NRSHIFT) | \
     ((size) << _IOC_SIZESHIFT))
#endif

#ifndef _IOR
#define _IOR(type, nr, size) _IOC(_IOC_READ, (type), (nr), sizeof(size))
#endif

#ifndef _IOW
#define _IOW(type, nr, size) _IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#endif

#endif // __WIN32_SYS_IOCTL_H__