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

#define LOG_TAG "BinderGenl"

//#define LOG_NDEBUG 0

#include <binder/BinderGenl.h>
#include <errno.h>
#include <log/log.h>
#include <string.h>
#include <unistd.h>

namespace android {

int BinderGenl::open() {
    int ret;
    struct sockaddr_nl sa;

    ALOGV("Opening binder genl socket");
    ret = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (ret < 0) {
        ALOGE("Failed to open binder genl socket: %s", strerror(ret));
        return ret;
    }
    fd = ret;

    pid = (__u32)getpid();
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_pid = pid;
    ALOGV("Binding binder genl socket %d", fd);
    ret = bind(fd, (struct sockaddr*) &sa, sizeof(sa));
    if (ret < 0) {
        ALOGE("Failed to bind binder genl socket: %s", strerror(ret));
        close();
        return -1;
    }

    if (getFamilyId() < 0) {
        ALOGE("Failed to get binder genl family id");
        close();
        return -1;
    }

    ALOGD("Binder genl socket[%d]: %d", pid, fd);

    return 0;
}

void BinderGenl::close() {
    if (fd > 0) {
        ALOGV("Closing binder genl socket %d", fd);
        ::close(fd);
        fd = 0;
    }
}

int BinderGenl::send(__u16 nlmsgType, __u8 cmd, __u16 nlaType, const void *nlaData, __u16 nlaLen) {
    struct GenlMsg msg;
    struct nlattr *nla;
    struct sockaddr_nl sa;
    int ret;

    if (NLA_ALIGN(nlaLen) + NLA_HDRLEN > BINDER_GENL_MSG_SIZE) {
        ALOGE("Oversized binder genl data to send");
        return -ENOMEM;
    }

    msg.nlh.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
    msg.nlh.nlmsg_type = nlmsgType;
    msg.nlh.nlmsg_flags = NLM_F_REQUEST;
    msg.nlh.nlmsg_seq = 0;
    msg.nlh.nlmsg_pid = pid;
    msg.glh.cmd = cmd;
    msg.glh.version = BINDER_GENL_FAMILY_VERSION;
    nla = (struct nlattr*) GENLMSG_DATA(&msg.glh);
    nla->nla_type = nlaType;
    nla->nla_len = nlaLen + NLA_HDRLEN;
    memcpy(NLA_DATA(nla), nlaData, nlaLen);
    msg.nlh.nlmsg_len += NLA_ALIGN(nla->nla_len);
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    ret = sendto(fd, &msg, msg.nlh.nlmsg_len, 0, (struct sockaddr *)&sa, sizeof(sa));
    if (ret < 0) {
        ALOGE("Failed to send (%d %d %d %d) to binder genl: %s", nlmsgType, cmd, nlaType, nlaLen,
              strerror(errno));
    }
    ALOGV("Sent %d / %d bytes to binder genl", ret, msg.nlh.nlmsg_len);

    return ret;
}

int BinderGenl::send(__u16 nlmsgType, __u8 cmd, __u16 nlaType[], const void *nlaData[],
                     __u16 nlaLen[], int n) {
    struct GenlMsg msg;
    struct nlattr *nla, *nla2;
    struct sockaddr_nl sa;
    int ret;
    __u16 len = 0;

    for (int i = 0; i < n; i++) {
        len += NLA_ALIGN(nlaLen[i]) + NLA_HDRLEN;
    }

    if (len > BINDER_GENL_MSG_SIZE) {
        ALOGE("Oversized binder genl data to send");
        return -ENOMEM;
    }

    msg.nlh.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
    msg.nlh.nlmsg_type = nlmsgType;
    msg.nlh.nlmsg_flags = NLM_F_REQUEST;
    msg.nlh.nlmsg_seq = 0;
    msg.nlh.nlmsg_pid = pid;
    msg.glh.cmd = cmd;
    msg.glh.version = BINDER_GENL_FAMILY_VERSION;
    nla = (struct nlattr*) GENLMSG_DATA(&msg.glh);
    for (int i = 0; i < n; i++) {
        nla->nla_type = nlaType[i];
        nla->nla_len = nlaLen[i] + NLA_HDRLEN;
        memcpy(NLA_DATA(nla), nlaData[i], nlaLen[i]);
        nla = (struct nlattr*)((char *)nla + NLA_ALIGN(nla->nla_len));
    }
    msg.nlh.nlmsg_len += len;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    ret = sendto(fd, &msg, msg.nlh.nlmsg_len, 0, (struct sockaddr *)&sa, sizeof(sa));
    if (ret < 0) {
        ALOGE("Failed to send (%d %d %d) to binder genl: %s", nlmsgType, cmd, n, strerror(errno));
    }
    ALOGV("Sent %d / %d bytes to binder genl", ret, msg.nlh.nlmsg_len);

    return ret;
}

int BinderGenl::recv(struct GenlMsg *msg, int len) {
    int ret;

    ret = ::recv(fd, msg, len, 0);
    ALOGV("Recv %d from binder genl", ret);
    ALOGV("nlh: %d %d %d %d %d",
          msg->nlh.nlmsg_len,
          msg->nlh.nlmsg_type,
          msg->nlh.nlmsg_flags,
          msg->nlh.nlmsg_seq,
          msg->nlh.nlmsg_pid);

    if (ret < 0) {
        ALOGE("Failed to recv binder genl data %d: %s", ret, strerror(errno));
        return ret;
    } else if (msg->nlh.nlmsg_type == NLMSG_ERROR) {
        ret = msg->error;
        ALOGE("Error msg received %d: %s ", ret, strerror(-ret));
        return ret;
    } else if (!NLMSG_OK(&msg->nlh, (__u32) ret)) {
        ALOGE("Wrong binder genl message data");
        return -EFAULT;
    }

    return 0;
}

int BinderGenl::getFamilyId() {
    int ret;

    ALOGV("Sending CTRL_CMD_GETFAMILY");
    ret = send(GENL_ID_CTRL, CTRL_CMD_GETFAMILY, CTRL_ATTR_FAMILY_NAME,
               BINDER_GENL_FAMILY_NAME, strlen(BINDER_GENL_FAMILY_NAME) + 1);
    if (ret < 0) {
        ALOGE("Failed to send CTRL_CMD_GETFAMILY");
        close();
        return -1;
    }

    struct GenlMsg msg;
    if (recv(&msg, sizeof(msg)) < 0) {
        ALOGE("Failed to recv reply of CTRL_CMD_GETFAMILY");
    }

    struct nlmsghdr *nlh;
    struct genlmsghdr *glh;
    struct nlattr *nla;
    nlh = &msg.nlh;
    glh = (struct genlmsghdr *)NLMSG_DATA(nlh);
    if (glh->cmd != CTRL_CMD_NEWFAMILY) {
        ALOGE("Wrong glh cmd %d, expect %d", glh->cmd, CTRL_CMD_NEWFAMILY);
        return -1;
    }

    int cur = 0;
    int payload = GENLMSG_PAYLOAD(nlh);
    while (cur < payload) {
        ALOGV("Checking NLA payload %d / %d", cur, payload);
        nla = (struct nlattr *)((char *)GENLMSG_DATA(glh) + cur);
        ALOGV("NLA type / len: %d / %d", nla->nla_type, nla->nla_len);
        cur += NLA_ALIGN(nla->nla_len);
        if (nla->nla_type == CTRL_ATTR_FAMILY_ID) {
            id = *(__u16*)NLA_DATA(nla);
            ALOGD("Binder genl family id is %d", id);
            return 0;
        }
    }

    return -1;
}

int BinderGenl::setReport(const char *context, __u32 pid, __u32 flags) {
    int ret;
    __u16 type[3] = { BINDER_GENL_A_CMD_CONTEXT, BINDER_GENL_A_CMD_PID, BINDER_GENL_A_CMD_FLAGS };
    __u16 len[3] = { (__u16)(strlen(context) + 1), (__u16)sizeof(pid), (__u16)sizeof(flags) };
    const void *data[3] = { (const void *)context, (const void *)&pid, (const void *)&flags };

    ALOGV("Sending BINDER_GENL_CMD_SET %s %d %d", context, pid, flags);
    ret = send(id, BINDER_GENL_CMD_SET, type, data, len, 3);
    if (ret < 0) {
        ALOGE("Failed to send BINDER_GENL_CMD_SET");
        return -1;
    }

    struct GenlMsg msg;
    if (recv(&msg, sizeof(msg)) < 0) {
        ALOGE("Failed to recv reply of BINDER_GENL_CMD_SET");
        return -1;
    }

    struct nlmsghdr *nlh;
    struct genlmsghdr *glh;
    struct nlattr *nla;
    nlh = &msg.nlh;
    glh = (struct genlmsghdr *)NLMSG_DATA(nlh);
    if (glh->cmd != BINDER_GENL_CMD_SET) {
        ALOGE("Wrong glh cmd %d, expect %d", glh->cmd, BINDER_GENL_CMD_SET);
        return -1;
    }

    nla = (struct nlattr *)GENLMSG_DATA(glh);
    if (nla->nla_type != BINDER_GENL_A_CMD_CONTEXT) {
        ALOGE("Wrong nla type %d, expect %d", nla->nla_type, BINDER_GENL_A_CMD_CONTEXT);
        return -1;
    }
    const char *name = (const char *)NLA_DATA(nla);
    if (strncmp(name, context, strlen(context))) {
        ALOGE("Wrong family name %s, expect %s", name, context);
        return -1;
    }

    nla = NLA_NEXT(nla);
    if (nla->nla_type != BINDER_GENL_A_CMD_PID) {
      ALOGE("Wrong nla type %d, expect %d", nla->nla_type, BINDER_GENL_A_CMD_PID);
      return -1;
    }
    __u32 echo = *(__u32 *)NLA_DATA(nla);
    if (echo != pid) {
      ALOGE("Wrong pid %d, expect %d", echo, pid);
      return -1;
    }

    nla = NLA_NEXT(nla);
    if (nla->nla_type != BINDER_GENL_A_CMD_FLAGS) {
        ALOGE("Wrong nla type %d, expect %d", nla->nla_type, BINDER_GENL_A_CMD_FLAGS);
        return -1;
    }
    echo = *(__u32 *)NLA_DATA(nla);
    if (echo != flags) {
        ALOGE("Wrong pid %d, expect %d", echo, flags);
        return -1;
    }

    ALOGD("Binder genl report flags %s[%d]: %d", context, pid, flags);

    return 0;
}

int BinderGenl::getReport(char *context, int size, __u32 report[__BINDER_GENL_A_REPORT_MAX]) {
    struct GenlMsg msg;
    if (recv(&msg, sizeof(msg)) < 0) {
        ALOGE("Failed to recv binder report");
    }

    struct nlmsghdr *nlh;
    struct genlmsghdr *glh;
    struct nlattr *nla;
    nlh = &msg.nlh;
    glh = (struct genlmsghdr *)NLMSG_DATA(nlh);
    if (glh->cmd != BINDER_GENL_CMD_REPORT) {
        ALOGE("Wrong glh cmd %d, expect %d", glh->cmd, BINDER_GENL_CMD_REPORT);
        return -1;
    }

    int attr = 0;
    int cur = 0;
    int payload = GENLMSG_PAYLOAD(nlh);
    while (cur < payload) {
        ALOGV("Checking NLA payload %d / %d", cur, payload);
        nla = (struct nlattr *)((char *)GENLMSG_DATA(glh) + cur);
        ALOGV("NLA type / len: %d / %d", nla->nla_type, nla->nla_len);
        cur += NLA_ALIGN(nla->nla_len);
        if (nla->nla_type <= 0 || nla->nla_type > BINDER_GENL_A_REPORT_MAX) {
            ALOGE("Invalid NLA type: %d", nla->nla_type);
            return -1;
        }
        if (nla->nla_type == BINDER_GENL_A_REPORT_CONTEXT) {
            strncpy(context, (const char*) NLA_DATA(nla), size);
            ALOGV("report[%d] = %s", nla->nla_type, context);
        } else {
            report[nla->nla_type] = *(__u32*) NLA_DATA(nla);
            ALOGV("report[%d] = %d", nla->nla_type, report[nla->nla_type]);
        }
        attr++;
    }

    if (attr != BINDER_GENL_A_REPORT_MAX) {
        ALOGE("Wrong NLAs %d, expect %d", attr, BINDER_GENL_A_REPORT_MAX);
        return -1;
    }

    return 0;
}

} // namespace android
