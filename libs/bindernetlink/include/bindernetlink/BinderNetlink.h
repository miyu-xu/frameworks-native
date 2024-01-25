/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include <memory>
#include <netlink/genl/genl.h>
#include <netlink/genl/mngt.h>
#include <netlink/genl/ctrl.h>
#include <netlink/netlink.h>
#include <linux/android/binderfs.h>
#if __has_include(<linux/android/binder_netlink.h>)
#include <linux/android/binder_netlink.h>
#else
// Copy of binder netlink header until include/uapi/linux/*.h are up to date
#ifndef _UAPI_LINUX_ANDROID_BINDER_NETLINK_H
#define _UAPI_LINUX_ANDROID_BINDER_NETLINK_H

#define BINDER_FAMILY_NAME	"binder"
#define BINDER_FAMILY_VERSION	1

/*
 * Define what kind of binder transactions should be reported.
 */
enum binder_flag {
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
    BINDER_CMD_REPORT_SETUP = 1,
    BINDER_CMD_REPORT,

    __BINDER_CMD_MAX,
    BINDER_CMD_MAX = (__BINDER_CMD_MAX - 1)
};

#define BINDER_MCGRP_REPORT	"report"

#endif /* _UAPI_LINUX_ANDROID_BINDER_NETLINK_H */

#endif // __has_include(<linux/android/binder_netlink.h>)

namespace android {

/**
 * struct SetupAttr - netlink attributes for BINDER_CMD_REPORT_SETUP
 * @param context The name of the target binder context
 * @param pid     The target process
 * @param flags   The flags to be set (see enum binder_netlink_flag)
 */
struct SetupAttr {
    char context[BINDERFS_MAX_NAME + 1];
    __u32 pid;
    __u32 flags;
};

/**
 * struct ReportAttr - netlink attributes for BINDER_CMD_REPORT
 * @param context The name of the target binder context
 * @param data    The attributes of the report
 */
struct ReportAttr {
  char context[BINDERFS_MAX_NAME + 1];
  __u32 data[BINDER_A_REPORT_MAX + 1];
};

/**
 * The netlink primitive functions talking to kernel binder netlink driver.
 */
class BinderNetlink {
private:
    /**
     * The unicast netlink socket for commands
     */
    std::unique_ptr<nl_sock, void (*)(nl_sock*)> mUcSock;

    /**
    * The multicast netlink socket for events
    */
    std::unique_ptr<nl_sock, void (*)(nl_sock*)> mMcSock;

    /**
     * The pid of the current process
     */
    pid_t mPid;

    /**
     * The generic netlink family ID
     */
    int mId;

    /**
     * The multicast group
     */
    int mGroup;

public:
    /**
     * Constructor
     */
    BinderNetlink();

    /**
     * Open the generic netlink socket and initialize it
     *
     * @return    0 on success, or <0 on error
     */
    int open();

    /**
     * Cleanup the generic netlink socket
     */
    void close();

    /**
     * Set what kinds of binder transactions are to be reported
     *
     * If pid is 0, the flags are applied to the whole binder context.
     * Otherwise, the flags are applied to the specific process only.
     * The per-context flags take effect unless the per-process flags
     * has BINDER_FLAG_OVERRIDE set.
     *
     * @param setup The netlink attributes for BINDER_CMD_REPORT_SETUP
     * @return 0 on success, or -1 on error
     */
    int setReport(struct SetupAttr* setup);

    /**
     * Receive next binder netlink report.
     *
     * @param report The netlink attributes for BINDER_CMD_REPORT
     * @return 0 on success, or -1 on error
     */
    int getReport(struct ReportAttr* report);
};

} // namespace android
