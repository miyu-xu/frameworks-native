/*
 * Copyright (C) 2022 The Android Open Source Project
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

#define LOG_TAG "RpcTransportTipcTrusty"

#include <inttypes.h>
#include <trusty_ipc.h>

#include <binder/RpcSession.h>
#include <binder/RpcTransportTipcTrusty.h>
#include <log/log.h>

#include "../FdTrigger.h"
#include "../RpcState.h"
#include "TrustyStatus.h"

// Creating a definition to hook a mock implementation of send_msg for unit test purposes
#ifndef SEND_MSG_IMPL
#define SEND_MSG_IMPL send_msg
#endif
ssize_t SEND_MSG_IMPL(handle_t, ipc_msg_t*);

namespace android {

using namespace android::binder::impl;
using android::binder::borrowed_fd;
using android::binder::unique_fd;

constexpr size_t kMaxMessageSize = 4096;

// RpcTransport for Trusty.
class RpcTransportTipcTrusty : public RpcTransport {
public:
    explicit RpcTransportTipcTrusty(android::RpcTransportFd socket) : mSocket(std::move(socket)) {}
    ~RpcTransportTipcTrusty() { releaseMessage(); }

    status_t pollRead() override {
        auto status = ensureMessage(false);
        if (status != OK) {
            return status;
        }
        return mHaveMessage ? OK : WOULD_BLOCK;
    }

    status_t sendTrustyMsg(ipc_msg_t* msg, size_t msg_size) {
        ssize_t rc = SEND_MSG_IMPL(mSocket.fd.get(), msg);
        if (rc == ERR_NOT_ENOUGH_BUFFER) {
            // Peer is blocked, wait until it unblocks.
            // TODO: when tipc supports a send-unblocked handler,
            // save the message here in a queue and retry it asynchronously
            // when the handler gets called by the library
            uevent uevt;
            do {
                rc = ::wait(mSocket.fd.get(), &uevt, INFINITE_TIME);
                if (rc < 0) {
                    return statusFromTrusty(rc);
                }
                if (uevt.event & IPC_HANDLE_POLL_HUP) {
                    return DEAD_OBJECT;
                }
            } while (!(uevt.event & IPC_HANDLE_POLL_SEND_UNBLOCKED));

            // Retry the send, it should go through this time because
            // sending is now unblocked
            rc = SEND_MSG_IMPL(mSocket.fd.get(), msg);
        }
        if (rc < 0) {
            return statusFromTrusty(rc);
        }
        LOG_ALWAYS_FATAL_IF(static_cast<size_t>(rc) != msg_size,
                            "Sent the wrong number of bytes %zd!=%zu", rc, msg_size);
        return OK;
    }

    void takeIovs(iovec** src_iovs, size_t* src_niovs, iovec* dest_iovs, size_t* dest_niovs,
                  size_t* acc_size) {
        // copy up to 4096 bytes of slices into `dest_iovs`.
        // modify `src_iovs` to keep only what's left.
        *acc_size = 0;
        size_t num_acc_iov = 0;
        // No need to check for (*dest_niovs == 0) || (*src_niovs == 0) because the for loop and
        // initial if statement take care of those cases
        for (size_t current_iov = 0; current_iov < *src_niovs; current_iov++) {
            if (current_iov >= *dest_niovs) {
                // We do not have more iovs to fill
                break;
            }
            size_t curr_iov_len = ((*src_iovs)[current_iov]).iov_len;
            if (curr_iov_len < kMaxMessageSize) {
                // we don't need to break this iov, try to group it with more iovs
                if ((*acc_size + curr_iov_len) < kMaxMessageSize) {
                    // We can continue grouping iovs, increment the accumulated size and copy the
                    // iov to dest
                    *acc_size += curr_iov_len;
                    dest_iovs[num_acc_iov].iov_base = ((*src_iovs)[current_iov]).iov_base;
                    dest_iovs[num_acc_iov].iov_len = curr_iov_len;
                    num_acc_iov++;
                    continue;
                } else {
                    // We have reached the limit of what we can send, send the accumulated
                    // buffers back
                    break;
                }
            } else {
                // The current iov needs to be broken down in smaller chunks
                if (*acc_size >= kMaxMessageSize) {
                    // We don't have space left to send more data on the current dest iov
                    break;
                }
                // There is some space left on dest_iovs; use the remaining space to send the first
                // part of this iov
                size_t remaining_space = kMaxMessageSize - *acc_size;

                dest_iovs[num_acc_iov].iov_base = ((*src_iovs)[current_iov]).iov_base;
                dest_iovs[num_acc_iov].iov_len = remaining_space;

                // "Removing" copied data from the corresponding src iov by moving the iov_base and
                // decreasing its size.
                ((*src_iovs)[current_iov]).iov_base = static_cast<void*>(
                        static_cast<char*>(((*src_iovs)[current_iov]).iov_base) + remaining_space);
                ((*src_iovs)[current_iov]).iov_len -= remaining_space;
                *acc_size += remaining_space;
                if (((*src_iovs)[current_iov]).iov_len > 0) {
                    // "removing" the iovs that have been copied from src_iovs. We remove it
                    // before incrementing num_acc_iov because we need to continue sending the
                    // current iov on the next call
                    *src_iovs = &((*src_iovs)[num_acc_iov]);
                    *src_niovs -= num_acc_iov;
                    num_acc_iov++;
                    *dest_niovs = num_acc_iov;
                    return;
                }
                // We are done with this iov, move to the next one
                num_acc_iov++;
                break;
            }
        }
        // "removing" the iovs that have been copied from src_iovs by moving the base iov list
        // pointer
        *src_iovs = &((*src_iovs)[num_acc_iov]);
        *src_niovs -= num_acc_iov;
        *dest_niovs = num_acc_iov;
    }

    status_t interruptableWriteFully(
            FdTrigger* /*fdTrigger*/, iovec* iovs, int niovs,
            const std::optional<SmallFunction<status_t()>>& /*altPoll*/,
            const std::vector<std::variant<unique_fd, borrowed_fd>>* ancillaryFds) override {
        if (niovs < 0) {
            return BAD_VALUE;
        }

        size_t size = 0;
        for (int i = 0; i < niovs; i++) {
            size += iovs[i].iov_len;
        }

        handle_t msgHandles[IPC_MAX_MSG_HANDLES];
        ipc_msg_t msg{
                .num_iov = static_cast<uint32_t>(niovs),
                .iov = iovs,
                .num_handles = 0,
                .handles = nullptr,
        };

        if (ancillaryFds != nullptr && !ancillaryFds->empty()) {
            if (ancillaryFds->size() > IPC_MAX_MSG_HANDLES) {
                // This shouldn't happen because we check the FD count in RpcState.
                ALOGE("Saw too many file descriptors in RpcTransportCtxTipcTrusty: "
                      "%zu (max is %u). Aborting session.",
                      ancillaryFds->size(), IPC_MAX_MSG_HANDLES);
                return BAD_VALUE;
            }

            for (size_t i = 0; i < ancillaryFds->size(); i++) {
                msgHandles[i] =
                        std::visit([](const auto& fd) { return fd.get(); }, ancillaryFds->at(i));
            }

            msg.num_handles = ancillaryFds->size();
            msg.handles = msgHandles;
        }

        if (size <= kMaxMessageSize) {
            return sendTrustyMsg(&msg, size);
        } else {
            size_t num_iov_to_send = static_cast<size_t>(niovs);
            for (;;) {
                constexpr size_t kPartialMax = 5;
                size_t partial_niovs = kPartialMax;
                iovec partial_iovs[kPartialMax];
                size_t acc_size;

                takeIovs(&msg.iov, &num_iov_to_send, partial_iovs, &partial_niovs, &acc_size);
                if (partial_niovs <= 0) {
                    break;
                }

                ipc_msg_t curr_msg{
                        .num_iov = static_cast<uint32_t>(partial_niovs),
                        .iov = partial_iovs,
                        .num_handles = msg.num_handles,
                        .handles = msg.handles,
                };

                sendTrustyMsg(&curr_msg, acc_size);
                // We are only sending the ancillaryFds on the first message
                msg.num_handles = 0;
            }
        }
        return OK;
    }

    status_t interruptableReadFully(
            FdTrigger* /*fdTrigger*/, iovec* iovs, int niovs,
            const std::optional<SmallFunction<status_t()>>& /*altPoll*/,
            std::vector<std::variant<unique_fd, borrowed_fd>>* ancillaryFds) override {
        if (niovs < 0) {
            return BAD_VALUE;
        }

        // If iovs has one or more empty vectors at the end and
        // we somehow advance past all the preceding vectors and
        // pass some or all of the empty ones to sendmsg/recvmsg,
        // the call will return processSize == 0. In that case
        // we should be returning OK but instead return DEAD_OBJECT.
        // To avoid this problem, we make sure here that the last
        // vector at iovs[niovs - 1] has a non-zero length.
        while (niovs > 0 && iovs[niovs - 1].iov_len == 0) {
            niovs--;
        }
        if (niovs == 0) {
            // The vectors are all empty, so we have nothing to read.
            return OK;
        }

        while (true) {
            auto status = ensureMessage(true);
            if (status != OK) {
                return status;
            }

            LOG_ALWAYS_FATAL_IF(mMessageInfo.num_handles > IPC_MAX_MSG_HANDLES,
                                "Received too many handles %" PRIu32, mMessageInfo.num_handles);
            bool haveHandles = mMessageInfo.num_handles != 0;
            handle_t msgHandles[IPC_MAX_MSG_HANDLES];

            ipc_msg_t msg{
                    .num_iov = static_cast<uint32_t>(niovs),
                    .iov = iovs,
                    .num_handles = mMessageInfo.num_handles,
                    .handles = haveHandles ? msgHandles : 0,
            };
            ssize_t rc = read_msg(mSocket.fd.get(), mMessageInfo.id, mMessageOffset, &msg);
            if (rc < 0) {
                return statusFromTrusty(rc);
            }

            size_t processSize = static_cast<size_t>(rc);
            mMessageOffset += processSize;
            LOG_ALWAYS_FATAL_IF(mMessageOffset > mMessageInfo.len,
                                "Message offset exceeds length %zu/%zu", mMessageOffset,
                                mMessageInfo.len);

            if (haveHandles) {
                if (ancillaryFds != nullptr) {
                    ancillaryFds->reserve(ancillaryFds->size() + mMessageInfo.num_handles);
                    for (size_t i = 0; i < mMessageInfo.num_handles; i++) {
                        ancillaryFds->emplace_back(unique_fd(msgHandles[i]));
                    }

                    // Clear the saved number of handles so we don't accidentally
                    // read them multiple times
                    mMessageInfo.num_handles = 0;
                    haveHandles = false;
                } else {
                    ALOGE("Received unexpected handles %" PRIu32, mMessageInfo.num_handles);
                    // It should be safe to continue here. We could abort, but then
                    // peers could DoS us by sending messages with handles in them.
                    // Close the handles since we are ignoring them.
                    for (size_t i = 0; i < mMessageInfo.num_handles; i++) {
                        ::close(msgHandles[i]);
                    }
                }
            }

            // Release the message if all of it has been read
            if (mMessageOffset == mMessageInfo.len) {
                releaseMessage();
            }

            while (processSize > 0 && niovs > 0) {
                auto& iov = iovs[0];
                if (processSize < iov.iov_len) {
                    // Advance the base of the current iovec
                    iov.iov_base = reinterpret_cast<char*>(iov.iov_base) + processSize;
                    iov.iov_len -= processSize;
                    break;
                }

                // The current iovec was fully written
                processSize -= iov.iov_len;
                iovs++;
                niovs--;
            }
            if (niovs == 0) {
                LOG_ALWAYS_FATAL_IF(processSize > 0,
                                    "Reached the end of iovecs "
                                    "with %zd bytes remaining",
                                    processSize);
                return OK;
            }
        }
    }

    bool isWaiting() override { return mSocket.isInPollingState(); }

private:
    status_t ensureMessage(bool wait) {
        int rc;
        if (mHaveMessage) {
            LOG_ALWAYS_FATAL_IF(mMessageOffset >= mMessageInfo.len, "No data left in message");
            return OK;
        }

        /* TODO: interruptible wait, maybe with a timeout??? */
        uevent uevt;
        rc = ::wait(mSocket.fd.get(), &uevt, wait ? INFINITE_TIME : 0);
        if (rc < 0) {
            if (rc == ERR_TIMED_OUT && !wait) {
                // If we timed out with wait==false, then there's no message
                return OK;
            }
            return statusFromTrusty(rc);
        }
        if (!(uevt.event & IPC_HANDLE_POLL_MSG)) {
            /* No message, terminate here and leave mHaveMessage false */
            if (uevt.event & IPC_HANDLE_POLL_HUP) {
                // Peer closed the connection. We need to preserve the order
                // between MSG and HUP from FdTrigger.cpp, which means that
                // getting MSG&HUP should return OK instead of DEAD_OBJECT.
                return DEAD_OBJECT;
            }
            return OK;
        }

        rc = get_msg(mSocket.fd.get(), &mMessageInfo);
        if (rc < 0) {
            return statusFromTrusty(rc);
        }

        mHaveMessage = true;
        mMessageOffset = 0;
        return OK;
    }

    void releaseMessage() {
        if (mHaveMessage) {
            put_msg(mSocket.fd.get(), mMessageInfo.id);
            mHaveMessage = false;
        }
    }

    android::RpcTransportFd mSocket;

    bool mHaveMessage = false;
    ipc_msg_info mMessageInfo;
    size_t mMessageOffset;
};

// RpcTransportCtx for Trusty.
class RpcTransportCtxTipcTrusty : public RpcTransportCtx {
public:
    std::unique_ptr<RpcTransport> newTransport(android::RpcTransportFd socket,
                                               FdTrigger*) const override {
        return std::make_unique<RpcTransportTipcTrusty>(std::move(socket));
    }
    std::vector<uint8_t> getCertificate(RpcCertificateFormat) const override { return {}; }
};

std::unique_ptr<RpcTransportCtx> RpcTransportCtxFactoryTipcTrusty::newServerCtx() const {
    return std::make_unique<RpcTransportCtxTipcTrusty>();
}

std::unique_ptr<RpcTransportCtx> RpcTransportCtxFactoryTipcTrusty::newClientCtx() const {
    return std::make_unique<RpcTransportCtxTipcTrusty>();
}

const char* RpcTransportCtxFactoryTipcTrusty::toCString() const {
    return "trusty";
}

std::unique_ptr<RpcTransportCtxFactory> RpcTransportCtxFactoryTipcTrusty::make() {
    return std::unique_ptr<RpcTransportCtxFactoryTipcTrusty>(
            new RpcTransportCtxFactoryTipcTrusty());
}

} // namespace android
