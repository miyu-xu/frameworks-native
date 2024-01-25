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

#include <binder/Common.h>
#include <binder/unique_fd.h>
#include <linux/genetlink.h>

// Copy of binder netlink header until include/uapi/linux/*.h are up to date
#ifndef _UAPI_LINUX_ANDROID_BINDER_NETLINK_H
#define _UAPI_LINUX_ANDROID_BINDER_NETLINK_H

#define BINDER_FAMILY_NAME "binder"
#define BINDER_FAMILY_VERSION 1

/**
 * enum binder_netlink_flag - Define what kind of binder transactions should be
 *   reported.
 */
enum binder_netlink_flag {
    BINDER_FLAG_FAILED = 1,
    BINDER_FLAG_ASYNC_FROZEN = 2,
    BINDER_FLAG_SPAM = 4,
    BINDER_FLAG_OVERRIDE = 8,
};

enum {
    BINDER_A_CMD_CONTEXT = 1,
    BINDER_A_CMD_PID,
    BINDER_A_CMD_FLAGS,

    __BINDER_A_CMD_MAX,
    BINDER_A_CMD_MAX = (__BINDER_A_CMD_MAX - 1)
};

enum {
    BINDER_A_REPORT_CONTEXT = 1,
    BINDER_A_REPORT_ERR,
    BINDER_A_REPORT_FROM_PID,
    BINDER_A_REPORT_FROM_TID,
    BINDER_A_REPORT_TO_PID,
    BINDER_A_REPORT_TO_TID,
    BINDER_A_REPORT_REPLY,
    BINDER_A_REPORT_FLAGS,
    BINDER_A_REPORT_CODE,
    BINDER_A_REPORT_DATA_SIZE,

    __BINDER_A_REPORT_MAX,
    BINDER_A_REPORT_MAX = (__BINDER_A_REPORT_MAX - 1)
};

enum {
    BINDER_CMD_SET = 1,
    BINDER_CMD_REPORT,

    __BINDER_CMD_MAX,
    BINDER_CMD_MAX = (__BINDER_CMD_MAX - 1)
};

#endif /* _UAPI_LINUX_ANDROID_BINDER_NETLINK_H */

#define BINDER_MSG_SIZE 1024

#define GENLMSG_DATA(glh) ((void*)((char*)(glh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(nlh) (NLMSG_PAYLOAD(nlh, 0) - GENL_HDRLEN)
#define NLA_DATA(nla) ((void*)((char*)(nla) + NLA_HDRLEN))
#define NLA_NEXT(nla) ((struct nlattr*)((char*)nla + NLA_ALIGN(nla->nla_len)))

namespace android {

using android::binder::unique_fd;

/*
 * The standard skeleton data struct for generic netlink messages
 */
struct GenlMsg {
    struct nlmsghdr nlh;
    union {
        struct genlmsghdr glh;
        int error;
    };
    char buf[BINDER_MSG_SIZE];
};

/**
 * The netlink primitive functions talking to kernel binder netlink driver.
 */
class BinderNetlink {
private:
    /**
     * The netlink socket
     */
    unique_fd mFd;

    /**
     * The pid of the current process
     */
    __u32 mPid;

    /**
     * The generic netlink family ID
     */
    __u16 mId;

    /**
     * Get the family id of binder generic netlink
     *
     * @return 0 on success, or -1 on error
     */
    int getFamilyId();

public:
    /**
     * Open the generic netlink socket and initialize it
     *
     * @return    0 on success, or <0 on error
     */
    LIBBINDER_EXPORTED int open();

    /**
     * Cleanup the generic netlink socket
     */
    LIBBINDER_EXPORTED void close();

    /**
     * Send one NLA to the generic netlink socket.
     *
     * @param nlmsgType Message content type
     * @param cmd       Generic netlink command
     * @param nlaType   Attribute type
     * @param nlaData   Attribute data
     * @param nlaLen    Attribute length (payload)
     * @return          The number of bytes sent, or <0 on error.
     */
    LIBBINDER_EXPORTED int send(__u16 nlmsgType, __u8 cmd, __u16 nlaType, const void* nlaData,
                                __u16 nlaLen);

    /**
     * Send multiple NLAs to the generic netlink socket
     *
     * @param nlmsgType Message content type
     * @param cmd       Generic netlink command
     * @param nlaType   Array of attribute type
     * @param nlaData   Array of attribute data
     * @param nlaLen    Array of attribute length (payload)
     * @param n         Array size
     * @return          The number of bytes sent, or <0 on error.
     */
    LIBBINDER_EXPORTED int send(__u16 nlmsgType, __u8 cmd, __u16 nlaType[], const void* nlaData[],
                                __u16 nlaLen[], int n);

    /**
     * Receive data from the generic netlink socket.
     *
     * @param msg Message content
     * @param len Message length
     * @return    0 on success, or <0 on failure
     */
    LIBBINDER_EXPORTED int recv(struct GenlMsg* msg, int len);

    /**
     * Set what kinds of binder transactions are to be reported
     *
     * If pid is 0, the flags are applied to the whole binder context.
     * Otherwise, the flags are applied to the specific process only.
     * The per-context flags take effect unless the per-process flags
     * has BINDER_FLAG_OVERRIDE set.
     *
     * @param context The name of the target binder context
     * @param pid     The target process
     * @param flags   The flags to be set (see enum binder_netlink_flag)
     * @return 0 on success, or -1 on error
     */
    LIBBINDER_EXPORTED int setReport(const char* context, __u32 pid, __u32 flags);

    /**
     * Receive next binder netlink report.
     *
     * @param context The name of the binder context
     * @param size    The buffer size of the parameter context
     * @param report  The u32 array to store the binder netlink report
     * @return 0 on success, or -1 on error
     */
    LIBBINDER_EXPORTED int getReport(char* context, int size, __u32 report[__BINDER_A_REPORT_MAX]);
};

} // namespace android
