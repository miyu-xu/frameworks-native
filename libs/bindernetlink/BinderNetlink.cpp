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

#define LOG_TAG "BinderNetlink"

//#define LOG_NDEBUG 0

#include <bindernetlink/BinderNetlink.h>
#include <errno.h>
#include <log/log.h>
#include <string.h>
#include <unistd.h>

namespace android {

BinderNetlink::BinderNetlink() : mUcSock(nullptr, nl_socket_free),
                                 mMcSock(nullptr, nl_socket_free),
                                 mPid(0), mId(0) {
}

static int cbHandler(struct nl_msg* msg, void* arg) {
    struct nlmsghdr* nlh = nlmsg_hdr(msg);
    struct genlmsghdr* glh = static_cast<struct genlmsghdr*>(nlmsg_data(nlh));
    nlattr* nla = genlmsg_attrdata(glh, 0);
    int rem = genlmsg_attrlen(glh, 0);

    ALOGV("msg=%p arg=%p", msg, arg);
    ALOGV("nlh: pid=%d seq=%d, type=%d, len=%d", nlh->nlmsg_pid, nlh->nlmsg_seq, nlh->nlmsg_type,
          nlh->nlmsg_len);
    ALOGV("glh: cmd=%d, version=%d", glh->cmd, glh->version);
    ALOGV("nla: type=%d len=%d / %d", nla->nla_type, nla->nla_len, rem);

    if (!arg) {
        ALOGW("Invalid netlink callback arg");
        return NL_SKIP;
    }

    switch (glh->cmd) {
        case BINDER_CMD_REPORT_SETUP: {
            struct SetupAttr* reply = static_cast<struct SetupAttr*>(arg);
            nlattr* attrs[BINDER_A_CMD_MAX + 1];
            if (genlmsg_parse(nlh, 0, attrs, BINDER_A_CMD_MAX, nullptr) < 0) {
                ALOGW("Failed to parse BINDER_CMD_REPORT_SETUP");
                return NL_SKIP;
            }

            if (!attrs[BINDER_A_CMD_CONTEXT] ||
                    !attrs[BINDER_A_CMD_PID] ||
                    !attrs[BINDER_A_CMD_FLAGS]) {
                ALOGW("Invalid reply of BINDER_CMD_REPORT_SETUP");
                return NL_SKIP;
            }
            strncpy(reply->context, nla_get_string(attrs[BINDER_A_CMD_CONTEXT]),
                    BINDERFS_MAX_NAME + 1);
            reply->pid = nla_get_u32(attrs[BINDER_A_CMD_PID]);
            reply->flags = nla_get_u32(attrs[BINDER_A_CMD_FLAGS]);
            break;
        }
        case BINDER_CMD_REPORT: {
            int n = 0;
            struct ReportAttr* report = static_cast<struct ReportAttr*>(arg);
            nla_for_each_attr(nla, nla, rem, rem) {
                ALOGV("---: type %d len %d", nla_type(nla), nla_len(nla));
                switch (nla_type(nla)) {
                    case BINDER_A_REPORT_CONTEXT:
                        strncpy(report->context, nla_get_string(nla), BINDERFS_MAX_NAME + 1);
                        n++;
                        break;
                    case BINDER_A_REPORT_ERR:
                    case BINDER_A_REPORT_FROM_PID:
                    case BINDER_A_REPORT_FROM_TID:
                    case BINDER_A_REPORT_TO_PID:
                    case BINDER_A_REPORT_TO_TID:
                    case BINDER_A_REPORT_REPLY:
                    case BINDER_A_REPORT_FLAGS:
                    case BINDER_A_REPORT_CODE:
                    case BINDER_A_REPORT_DATA_SIZE:
                        report->data[nla_type(nla)] = nla_get_u32(nla);
                        n++;
                        break;
                    default:
                        ALOGW("Unknown report attribute: %d", nla_type(nla));
                        break;
                }
            }
            if (n != BINDER_A_REPORT_MAX) {
                ALOGW("Wrong number of NLAs %d, expect %d", n, BINDER_A_REPORT_MAX);
                return NL_SKIP;
            }
            break;
        }
        default:ALOGV("Ignore unknown glh cmd: %d", glh->cmd);
            break;
    }
    return NL_OK;
}

int BinderNetlink::open() {
    int ret;

    ALOGV("Opening binder netlink command socket");
    std::unique_ptr<nl_sock, decltype(&nl_socket_free)> ucsock(nl_socket_alloc(), nl_socket_free);
    if (!ucsock.get()) {
        ALOGE("Failed to allocate binder netlink command socket");
        return -1;
    }

    std::unique_ptr<nl_sock, decltype(&nl_socket_free)> mcsock(nl_socket_alloc(), nl_socket_free);
    if (!mcsock.get()) {
        ALOGE("Failed to allocate binder netlink event socket");
        return -1;
    }

    ret = genl_connect(ucsock.get());
    if (ret < 0) {
        ALOGE("Failed to open binder netlink command socket: %s", nl_geterror(ret));
        return -1;
    }

    ret = genl_connect(mcsock.get());
    if (ret < 0) {
        ALOGE("Failed to open binder netlink event socket: %s", nl_geterror(ret));
        return -1;
    }

    nl_socket_disable_seq_check(ucsock.get());
    nl_socket_disable_seq_check(mcsock.get());

    ret = genl_ctrl_resolve(ucsock.get(), BINDER_FAMILY_NAME);
    if (ret < 0) {
        ALOGW("Failed to get binder netlink family id: %s", nl_geterror(ret));
        return -1;
    }
    mId = ret;

    ret = genl_ctrl_resolve_grp(mcsock.get(), BINDER_FAMILY_NAME, BINDER_MCGRP_REPORT);
    if (ret < 0) {
        ALOGW("Failed to get binder netlink multicast group: %s", nl_geterror(ret));
        return -1;
    }
    mGroup = ret;

    ret = nl_socket_add_membership(mcsock.get(), mGroup);
    if (ret < 0) {
        ALOGW("Failed to join multicast group: %s", nl_geterror(ret));
        return -1;
    }

    mPid = static_cast<__u32>(getpid());
    mUcSock = std::move(ucsock);
    mMcSock = std::move(mcsock);
    ALOGD("Binder Netlink pid=%d id=%d group=%d", mPid, mId, mGroup);

    return 0;
}

void BinderNetlink::close() {
    mUcSock.reset();
    mMcSock.reset();
}

int BinderNetlink::setReport(struct SetupAttr* setup) {
    struct SetupAttr reply;
    int ret;

    if (!setup) {
        ALOGE("Valid SetupAttr required");
        return -1;
    }

    std::unique_ptr<nl_msg, decltype(&nlmsg_free)> msg(nlmsg_alloc(), nlmsg_free);
    if (!msg.get()) {
        ALOGW("Failed to allocate netlink message");
        return -1;
    }

    memset(&reply, 0, sizeof(SetupAttr));
    ret = nl_socket_modify_cb(mUcSock.get(), NL_CB_VALID, NL_CB_CUSTOM, cbHandler, &reply);
    if (ret < 0) {
        ALOGW("Failed to set unicast callback: %s", nl_geterror(ret));
        return -1;
    }

    genlmsg_put(msg.get(), NL_AUTO_PORT, NL_AUTO_SEQ, mId, 0, 0, BINDER_CMD_REPORT_SETUP,
                BINDER_FAMILY_VERSION);
    nla_put_string(msg.get(), BINDER_A_CMD_CONTEXT, setup->context);
    nla_put_u32(msg.get(), BINDER_A_CMD_PID, setup->pid);
    nla_put_u32(msg.get(), BINDER_A_CMD_FLAGS, setup->flags);

    ret = nl_send_auto(mUcSock.get(), msg.get());
    if (ret < 0) {
        ALOGW("Failed to send BINDER_CMD_REPORT_SETUP: %s", nl_geterror(ret));
        return -1;
    }

    ALOGD("Binder Netlink report setup %s[%d]: %d", setup->context, setup->pid, setup->flags);
    nl_recvmsgs_default(mUcSock.get());
    ALOGD("Binder Netlink report reply %s[%d]: %d", reply.context, reply.pid, reply.flags);
    if (strncmp(reply.context, setup->context, BINDERFS_MAX_NAME + 1) ||
            reply.pid != setup->pid || reply.flags != setup->flags) {
        ALOGE("Invalid reply of BINDER_CMD_REPORT_SETUP");
        return -1;
    }

    return 0;
}

int BinderNetlink::getReport(struct ReportAttr* report) {
    if (!report) {
        ALOGE("Valid ReportAttr required");
        return -1;
    }

    memset(report, 0, sizeof(ReportAttr));
    int ret = nl_socket_modify_cb(mMcSock.get(), NL_CB_VALID, NL_CB_CUSTOM, cbHandler, report);
    if (ret < 0) {
        ALOGW("Failed to set multicast callback: %s", nl_geterror(ret));
        return -1;
    }

    nl_recvmsgs_default(mMcSock.get());

    return 0;
}

} // namespace android
