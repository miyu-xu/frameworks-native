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

#define LOG_TAG "BinderNetlink"

// #define LOG_NDEBUG 0

#include <binder/BinderNetlink.h>
#include <errno.h>
#include <log/log.h>
#include <string.h>
#include <unistd.h>

namespace android {

int BinderNetlink::open() {
    ALOGV("Opening binder netlink socket");
    mFd.reset(socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC));
    if (!mFd.ok()) {
        ALOGE("Failed to open binder netlink socket: %s", strerror(errno));
        return -1;
    }

    // Netlink uses __u32 pid
    mPid = static_cast<__u32>(getpid());

    struct sockaddr_nl sa {
        .nl_family = AF_NETLINK, .nl_pid = mPid,
    };

    ALOGV("Binding binder netlink socket %d", mFd.get());
    if (bind(mFd.get(), reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) < 0) {
        ALOGE("Failed to bind binder netlink socket: %s", strerror(errno));
        mFd.reset();
        return -1;
    }

    if (getFamilyId() < 0) {
        ALOGW("Failed to get binder netlink family id");
        mFd.reset();
        return -1;
    }

    ALOGD("Binder Netlink socket[%d]: %d", mPid, mFd.get());

    return 0;
}

void BinderNetlink::close() {
    mFd.reset();
}

int BinderNetlink::send(__u16 nlmsgType, __u8 cmd, __u16 nlaType, const void* nlaData,
                        __u16 nlaLen) {
    if (NLA_ALIGN(nlaLen) + NLA_HDRLEN > BINDER_MSG_SIZE) {
        ALOGE("Oversized binder netlink data to send");
        return -ENOMEM;
    }

    struct GenlMsg msg {
        .nlh =
                {
                        .nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN),
                        .nlmsg_type = nlmsgType,
                        .nlmsg_flags = NLM_F_REQUEST,
                        .nlmsg_seq = 0,
                        .nlmsg_pid = mPid,
                },
        .glh = {
                .cmd = cmd,
                .version = BINDER_FAMILY_VERSION,
        },
    };

    struct nlattr* nla = reinterpret_cast<struct nlattr*>(GENLMSG_DATA(&msg.glh));
    nla->nla_type = nlaType;
    nla->nla_len = nlaLen + NLA_HDRLEN;
    memcpy(NLA_DATA(nla), nlaData, nlaLen);
    msg.nlh.nlmsg_len += NLA_ALIGN(nla->nla_len);

    struct sockaddr_nl sa {
        .nl_family = AF_NETLINK,
    };
    int ret = sendto(mFd.get(), &msg, msg.nlh.nlmsg_len, 0, reinterpret_cast<struct sockaddr*>(&sa),
                     sizeof(sa));
    if (ret < 0) {
        ALOGE("Failed to send (%d %d %d %d) to binder netlink: %s", nlmsgType, cmd, nlaType, nlaLen,
              strerror(errno));
    }
    ALOGV("Sent %d / %d bytes to binder netlink", ret, msg.nlh.nlmsg_len);

    return ret;
}

int BinderNetlink::send(__u16 nlmsgType, __u8 cmd, __u16 nlaType[], const void* nlaData[],
                        __u16 nlaLen[], int n) {
    __u32 len = 0;
    for (int i = 0; i < n; i++) {
        len += NLA_ALIGN(nlaLen[i]) + NLA_HDRLEN;
    }

    if (len > BINDER_MSG_SIZE) {
        ALOGE("Oversize binder netlink data to send: %d > %d", len, BINDER_MSG_SIZE);
        return -ENOMEM;
    }

    struct GenlMsg msg {
        .nlh =
                {
                        .nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN),
                        .nlmsg_type = nlmsgType,
                        .nlmsg_flags = NLM_F_REQUEST,
                        .nlmsg_seq = 0,
                        .nlmsg_pid = mPid,
                },
        .glh = {
                .cmd = cmd,
                .version = BINDER_FAMILY_VERSION,
        },
    };

    struct nlattr* nla = reinterpret_cast<struct nlattr*> GENLMSG_DATA(&msg.glh);
    for (int i = 0; i < n; i++) {
        nla->nla_type = nlaType[i];
        nla->nla_len = nlaLen[i] + NLA_HDRLEN;
        memcpy(NLA_DATA(nla), nlaData[i], nlaLen[i]);
        nla = reinterpret_cast<struct nlattr*>(reinterpret_cast<char*>(nla) +
                                               NLA_ALIGN(nla->nla_len));
    }
    msg.nlh.nlmsg_len += len;

    struct sockaddr_nl sa {
        .nl_family = AF_NETLINK,
    };

    int ret = sendto(mFd.get(), &msg, msg.nlh.nlmsg_len, 0, reinterpret_cast<struct sockaddr*>(&sa),
                     sizeof(sa));
    if (ret < 0) {
        ALOGE("Failed to send (%d %d %d) to binder netlink: %s", nlmsgType, cmd, n,
              strerror(errno));
    }
    ALOGV("Sent %d / %d bytes to binder netlink", ret, msg.nlh.nlmsg_len);

    return ret;
}

int BinderNetlink::recv(struct GenlMsg* msg, int len) {
    int ret = ::recv(mFd.get(), msg, len, 0);
    ALOGV("Recv %d from binder netlink", ret);
    ALOGV("nlh: %d %d %d %d %d", msg->nlh.nlmsg_len, msg->nlh.nlmsg_type, msg->nlh.nlmsg_flags,
          msg->nlh.nlmsg_seq, msg->nlh.nlmsg_pid);

    if (ret < 0) {
        ALOGE("Failed to recv binder netlink data %d: %s", ret, strerror(errno));
        return ret;
    } else if (msg->nlh.nlmsg_type == NLMSG_ERROR) {
        ret = msg->error;
        ALOGE("Error msg received %d: %s ", ret, strerror(-ret));
        return ret;
    } else if (!NLMSG_OK(&msg->nlh, static_cast<__u32>(ret))) {
        ALOGE("Wrong binder netlink message data");
        return -EFAULT;
    }

    return 0;
}

int BinderNetlink::getFamilyId() {
    ALOGV("Sending CTRL_CMD_GETFAMILY");
    int ret = send(GENL_ID_CTRL, CTRL_CMD_GETFAMILY, CTRL_ATTR_FAMILY_NAME, BINDER_FAMILY_NAME,
                   strlen(BINDER_FAMILY_NAME) + 1);
    if (ret < 0) {
        ALOGE("Failed to send CTRL_CMD_GETFAMILY");
        return -1;
    }

    struct GenlMsg msg;
    if (recv(&msg, sizeof(msg)) < 0) {
        ALOGE("Failed to recv reply of CTRL_CMD_GETFAMILY");
        return -1;
    }

    if (msg.glh.cmd != CTRL_CMD_NEWFAMILY) {
        ALOGW("Wrong glh cmd %d, expect %d", msg.glh.cmd, CTRL_CMD_NEWFAMILY);
        return -1;
    }

    int cur = 0;
    int payload = GENLMSG_PAYLOAD(&msg.nlh);
    char* data = reinterpret_cast<char*>(GENLMSG_DATA(&msg.glh));
    while (cur < payload) {
        ALOGV("Checking NLA payload %d / %d", cur, payload);
        struct nlattr* nla = reinterpret_cast<struct nlattr*>(data + cur);
        ALOGV("NLA type / len: %d / %d", nla->nla_type, nla->nla_len);
        cur += NLA_ALIGN(nla->nla_len);
        if (nla->nla_type == CTRL_ATTR_FAMILY_ID) {
            mId = *(reinterpret_cast<__u16*>(NLA_DATA(nla)));
            ALOGD("Binder Netlink family id is %d", mId);
            return 0;
        }
    }

    return -1;
}

int BinderNetlink::setReport(const char* context, __u32 pid, __u32 flags) {
    __u16 type[3] = {
            BINDER_A_CMD_CONTEXT,
            BINDER_A_CMD_PID,
            BINDER_A_CMD_FLAGS,
    };
    __u16 len[3] = {
            static_cast<__u16>(strlen(context) + 1),
            static_cast<__u16>(sizeof(pid)),
            static_cast<__u16>(sizeof(flags)),
    };
    const void* data[3] = {
            reinterpret_cast<const void*>(context),
            reinterpret_cast<const void*>(&pid),
            reinterpret_cast<const void*>(&flags),
    };

    ALOGV("Sending BINDER_CMD_SET %s %d %d", context, pid, flags);
    int ret = send(mId, BINDER_CMD_SET, type, data, len, 3);
    if (ret < 0) {
        ALOGE("Failed to send BINDER_CMD_SET");
        return -1;
    }

    struct GenlMsg msg;
    if (recv(&msg, sizeof(msg)) < 0) {
        ALOGE("Failed to recv reply of BINDER_CMD_SET");
        return -1;
    }

    if (msg.glh.cmd != BINDER_CMD_SET) {
        ALOGE("Wrong glh cmd %d, expect %d", msg.glh.cmd, BINDER_CMD_SET);
        return -1;
    }

    struct nlattr* nla = reinterpret_cast<struct nlattr*>(GENLMSG_DATA(&msg.glh));
    if (nla->nla_type != BINDER_A_CMD_CONTEXT) {
        ALOGE("Wrong nla type %d, expect %d", nla->nla_type, BINDER_A_CMD_CONTEXT);
        return -1;
    }
    const char* name = reinterpret_cast<const char*>(NLA_DATA(nla));
    if (strncmp(name, context, strlen(context))) {
        ALOGE("Wrong family name %s, expect %s", name, context);
        return -1;
    }

    nla = NLA_NEXT(nla);
    if (nla->nla_type != BINDER_A_CMD_PID) {
        ALOGE("Wrong nla type %d, expect %d", nla->nla_type, BINDER_A_CMD_PID);
        return -1;
    }
    __u32 echo = *(reinterpret_cast<__u32*>(NLA_DATA(nla)));
    if (echo != pid) {
        ALOGE("Wrong pid %d, expect %d", echo, pid);
        return -1;
    }

    nla = NLA_NEXT(nla);
    if (nla->nla_type != BINDER_A_CMD_FLAGS) {
        ALOGE("Wrong nla type %d, expect %d", nla->nla_type, BINDER_A_CMD_FLAGS);
        return -1;
    }
    echo = *(reinterpret_cast<__u32*>(NLA_DATA(nla)));
    if (echo != flags) {
        ALOGE("Wrong pid %d, expect %d", echo, flags);
        return -1;
    }

    ALOGD("Binder Netlink report flags %s[%d]: %d", context, pid, flags);

    return 0;
}

int BinderNetlink::getReport(char* context, int size, __u32 report[__BINDER_A_REPORT_MAX]) {
    struct GenlMsg msg;
    if (recv(&msg, sizeof(msg)) < 0) {
        ALOGE("Failed to recv binder report");
    }

    if (msg.glh.cmd != BINDER_CMD_REPORT) {
        ALOGE("Wrong glh cmd %d, expect %d", msg.glh.cmd, BINDER_CMD_REPORT);
        return -1;
    }

    int attr = 0;
    int cur = 0;
    int payload = GENLMSG_PAYLOAD(&msg.nlh);
    char* data = reinterpret_cast<char*>(GENLMSG_DATA(&msg.glh));
    while (cur < payload) {
        ALOGV("Checking NLA payload %d / %d", cur, payload);
        struct nlattr* nla = reinterpret_cast<struct nlattr*>(data + cur);
        ALOGV("NLA type / len: %d / %d", nla->nla_type, nla->nla_len);
        cur += NLA_ALIGN(nla->nla_len);
        if (nla->nla_type <= 0 || nla->nla_type > BINDER_A_REPORT_MAX) {
            ALOGE("Invalid NLA type: %d", nla->nla_type);
            return -1;
        }
        if (nla->nla_type == BINDER_A_REPORT_CONTEXT) {
            strncpy(context, reinterpret_cast<const char*>(NLA_DATA(nla)), size);
            ALOGV("report[%d] = %s", nla->nla_type, context);
        } else {
            report[nla->nla_type] = *(reinterpret_cast<__u32*>(NLA_DATA(nla)));
            ALOGV("report[%d] = %d", nla->nla_type, report[nla->nla_type]);
        }
        attr++;
    }

    if (attr != BINDER_A_REPORT_MAX) {
        ALOGE("Wrong NLAs %d, expect %d", attr, BINDER_A_REPORT_MAX);
        return -1;
    }

    return 0;
}

} // namespace android
