/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/*
 * https://r.android.com/2852650 made kernel uapi linux/socket.h depend on
 * bits/sockaddr_storage, which is outside of the kernel uapi directory,
 * breaking the build. Before a better solution is found, copy the right
 * version of kernel uapi linux/socket.h here.
 *
 * TODO: remove this workaround after the kernel header files are fixed.
 */

/* Start of kernel uapi <linux/socket.h> */

/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_SOCKET_H
#define _UAPI_LINUX_SOCKET_H

/*
 * Desired design of maximum size and alignment (see RFC2553)
 */
#define _K_SS_MAXSIZE	128	/* Implementation specific max size */

typedef unsigned short __kernel_sa_family_t;

/*
 * The definition uses anonymous union and struct in order to control the
 * default alignment.
 */
struct __kernel_sockaddr_storage {
	union {
		struct {
			__kernel_sa_family_t	ss_family; /* address family */
			/* Following field(s) are implementation specific */
			char __data[_K_SS_MAXSIZE - sizeof(unsigned short)];
				/* space to achieve desired size, */
				/* _SS_MAXSIZE value minus size of ss_family */
		};
		void *__align; /* implementation specific desired alignment */
	};
};

/* End of kernel uapi <linux/socket.h> */

#define SOCK_SNDBUF_LOCK	1
#define SOCK_RCVBUF_LOCK	2

#define SOCK_BUF_LOCK_MASK (SOCK_SNDBUF_LOCK | SOCK_RCVBUF_LOCK)

#define SOCK_TXREHASH_DEFAULT	255
#define SOCK_TXREHASH_DISABLED	0
#define SOCK_TXREHASH_ENABLED	1

#endif /* _UAPI_LINUX_SOCKET_H */

#include <asm/types.h>
#include <binder/Common.h>
#include <linux/android/binder.h>
#include <linux/genetlink.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <functional>

// Copy of binder genl header in case include/uapi/linux/android/binder.h is not new enough
#ifndef BINDER_GENL_FAMILY_NAME

#define BINDER_GENL_FAMILY_NAME "binder"
#define BINDER_GENL_VERSION 1

enum binder_report_flag {
    BINDER_REPORT_FAILED = 0x1,
    BINDER_REPORT_DELAYED = 0x2,
    BINDER_REPORT_SPAM = 0x4,
    BINDER_REPORT_ALL = BINDER_REPORT_FAILED | BINDER_REPORT_DELAYED | BINDER_REPORT_SPAM,
    BINDER_REPORT_OVERRIDE = 0x80000000,
};

struct binder_report {
    __u32 err;
    __u32 from_pid;
    __u32 from_tid;
    __u32 to_pid;
    __u32 to_tid;
    __u32 reply;
    __u32 flags;
    __u32 code;
    binder_size_t data_size;
};

enum binder_genl_attr {
    BINDER_GENL_ATTR_UNSPEC = 0,
    BINDER_GENL_ATTR_PID = 1,
    BINDER_GENL_ATTR_FLAGS = 2,
    BINDER_GENL_ATTR_REPORT = 3,
    BINDER_GENL_ATTR_MAX = BINDER_GENL_ATTR_REPORT,
};

enum binder_genl_cmd {
    BINDER_GENL_CMD_UNSPEC = 0,
    BINDER_GENL_CMD_SET_REPORT = 1,
    BINDER_GENL_CMD_REPLY = 2,
    BINDER_GENL_CMD_REPORT = 3,
    BINDER_GENL_CMD_MAX = BINDER_GENL_CMD_REPORT,
};

#endif
// End of binder genl header in case include/uapi/linux/android/binder.h

#define BINDER_GENL_MSG_SIZE 1024

#define GENLMSG_DATA(glh) ((void *)((char *)(glh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(nlh) (NLMSG_PAYLOAD(nlh, 0) - GENL_HDRLEN)
#define NLA_DATA(nla) ((void *)((char *)(nla) + NLA_HDRLEN))
#define NLA_NEXT(nla) ((struct nlattr *)((char *)nla + NLA_ALIGN(nla->nla_len)))

namespace android {

/*
 * The standard skeleton data struct for generic netlink messages
 */
struct GenlMsg {
    struct nlmsghdr nlh;
    union {
        struct genlmsghdr glh;
        int error;
    };
    char buf[BINDER_GENL_MSG_SIZE];
};

using GenlCallback = std::function<void(void *, __u16)>;

class BinderGenl;

/**
 * The netlink primitive functions talking to kernel binder genl driver.
 */
class BinderGenl {
private:

    /**
     * The generic netlink family name
     */
    char name[GENL_NAMSIZ];

    /**
     * The netlink socket
     */
    int fd;

    /**
     * The pid of the current process
     */
     __u32 pid;

    /**
     * The generic netlink family ID
     */
    __u16 id;

    /**
     * Get the family id of binder generic netlink
     *
     * @return 0 on success, or -1 on error
     */
    int getFamilyId();

public:
    /**
     * Constructor
     *
     * @param str The family name of this binder genl context
     */
    BinderGenl(const char* str) {
        strncpy(name, str, GENL_NAMSIZ);
    }

    /**
     * Open the generic netlink socket and initialize it
     *
     * @return    The socket, or <0 on error
     */
    LIBBINDER_EXPORTED int open();

    /**
     * Cleanup the generic netlink socket
     */
    LIBBINDER_EXPORTED void close();

    /**
     * Send data to the generic netlink socket.
     *
     * @param nlmsgType Message content type
     * @param cmd       Generic netlink command
     * @param nlaType   Attribute type
     * @param nlaData   Attribute data
     * @param nlaLen    Attribute length
     * @return          The number of bytes sent, or <0 on error.
     */
    LIBBINDER_EXPORTED int send(__u16 nlmsgType, __u8 cmd, __u16 nlaType,
                                void* nlaData, __u16 nlaLen);

    /**
     * Similar to send() but accept 2 NLAs
     *
     * @param nlmsgType Message content type
     * @param cmd       Generic netlink command
     * @param nlaType   Attribute type 1
     * @param nlaData   Attribute data 1
     * @param nlaLen    Attribute length 1
     * @param nlaType2  Attribute type 2
     * @param nlaData2  Attribute data 2
     * @param nlaLen2   Attribute length 2
     * @return          The number of bytes sent, or <0 on error.
     */
    LIBBINDER_EXPORTED int send2(__u16 nlmsgType, __u8 cmd, __u16 nlaType,
                                 void* nlaData, __u16 nlaLen, __u16 nlaType2,
                                 void* nlaData2, __u16 nlaLen2);

    /**
     * Receive data from the generic netlink socket.
     *
     * @param msg Message content
     * @param len Message length
     * @return    0 on success, or <0 on failure
     */
     LIBBINDER_EXPORTED int recv(struct GenlMsg* msg, int len);

    /**
     * Receive data from the generic netlink socket.
     *
     * There are 2 ways to use this function: with or without a callback function.
     *
     * Without specifying a callback function, recv() will return the first matched data.
     * Otherwise, recv() will iterate all matched data, calling the callback function to
     * process each of them, and then return the last matched data.
     *
     * @param msg      Message content
     * @param len      Message length
     * @param cmd      Expected genl command
     * @param nlaType  Expected attribute type
     * @param callback Nullable callback function for each attribute data matching the type
     * @return         NULL if no matched attribute is found,
     *                 or the first matched attribute data if callback is NULL,
     *                 or the last matched attribute data if callback is NOT NULL.
     */
    LIBBINDER_EXPORTED void* recv(struct GenlMsg* msg, int len, __u8 cmd, __u16 nlaType,
                                  GenlCallback callback = NULL);

    /**
     * Set what kinds of binder transactions are to be reported
     *
     * If pid is 0, the flags are applied to the whole binder context.
     * Otherwise, the flags are applied to the specific process only.
     *
     * @param pid   The target process to set the flags
     * @param flags The flags to be set (see enum binder_report_flag)
     * @return 0 on success, or -1 on error
     */
    LIBBINDER_EXPORTED int setReport(__u32 pid, __u32 flags);
};

} // namespace android
