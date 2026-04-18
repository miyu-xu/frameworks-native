#ifndef __WIN32_LINUX_VM_SOCKETS_H__
#define __WIN32_LINUX_VM_SOCKETS_H__

#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VM Sockets address family */
#ifndef AF_VSOCK
#define AF_VSOCK 40
#endif

#ifndef VMADDR_CID_ANY
#define VMADDR_CID_ANY -1U
#endif

#ifndef VMADDR_CID_HYPERVISOR
#define VMADDR_CID_HYPERVISOR 0
#endif

#ifndef VMADDR_CID_LOCAL
#define VMADDR_CID_LOCAL 1
#endif

#ifndef VMADDR_CID_HOST
#define VMADDR_CID_HOST 2
#endif

#ifndef VMADDR_PORT_ANY
#define VMADDR_PORT_ANY -1U
#endif

/* VM Sockets address structure */
struct sockaddr_vm {
    unsigned short svm_family;
    unsigned short svm_reserved1;
    unsigned int svm_port;
    unsigned int svm_cid;
    unsigned char svm_zero[sizeof(struct sockaddr) -
                           sizeof(unsigned short) -
                           sizeof(unsigned short) -
                           sizeof(unsigned int) -
                           sizeof(unsigned int)];
};

#ifdef __cplusplus
}
#endif

#endif /* __WIN32_LINUX_VM_SOCKETS_H__ */
