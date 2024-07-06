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

#include "binderRpcTestCommon.h"
#include "binderRpcTestFixture.h"

#include "../RpcWireFormat.h"

using android::binder::ReadFdToString;

namespace android {

TEST_P(BinderRpc, FileDescriptorTransportRejectNone) {
    if (socketType() == SocketType::TIPC) {
        GTEST_SKIP() << "File descriptor tests not supported on Trusty (yet)";
    }

    auto proc = createRpcTestSocketServerProcess({
            .clientFileDescriptorTransportMode = RpcSession::FileDescriptorTransportMode::NONE,
            .serverSupportedFileDescriptorTransportModes =
                    {RpcSession::FileDescriptorTransportMode::UNIX},
            .allowConnectFailure = true,
    });
    EXPECT_TRUE(proc.proc->sessions.empty()) << "session connections should have failed";
    proc.proc->terminate();
    proc.proc->setCustomExitStatusCheck([](int wstatus) {
        EXPECT_TRUE(WIFSIGNALED(wstatus) && WTERMSIG(wstatus) == SIGTERM)
                << "server process failed incorrectly: " << WaitStatusToString(wstatus);
    });
    proc.expectAlreadyShutdown = true;
}

TEST_P(BinderRpc, FileDescriptorTransportRejectUnix) {
    if (socketType() == SocketType::TIPC) {
        GTEST_SKIP() << "File descriptor tests not supported on Trusty (yet)";
    }

    auto proc = createRpcTestSocketServerProcess({
            .clientFileDescriptorTransportMode = RpcSession::FileDescriptorTransportMode::UNIX,
            .serverSupportedFileDescriptorTransportModes =
                    {RpcSession::FileDescriptorTransportMode::NONE},
            .allowConnectFailure = true,
    });
    EXPECT_TRUE(proc.proc->sessions.empty()) << "session connections should have failed";
    proc.proc->terminate();
    proc.proc->setCustomExitStatusCheck([](int wstatus) {
        EXPECT_TRUE(WIFSIGNALED(wstatus) && WTERMSIG(wstatus) == SIGTERM)
                << "server process failed incorrectly: " << WaitStatusToString(wstatus);
    });
    proc.expectAlreadyShutdown = true;
}

TEST_P(BinderRpc, FileDescriptorTransportOptionalUnix) {
    if (socketType() == SocketType::TIPC) {
        GTEST_SKIP() << "File descriptor tests not supported on Trusty (yet)";
    }

    auto proc = createRpcTestSocketServerProcess({
            .clientFileDescriptorTransportMode = RpcSession::FileDescriptorTransportMode::NONE,
            .serverSupportedFileDescriptorTransportModes =
                    {RpcSession::FileDescriptorTransportMode::NONE,
                     RpcSession::FileDescriptorTransportMode::UNIX},
    });

    android::os::ParcelFileDescriptor out;
    auto status = proc.rootIface->echoAsFile("hello", &out);
    EXPECT_EQ(status.transactionError(), FDS_NOT_ALLOWED) << status;
}

TEST_P(BinderRpc, ReceiveFile) {
    if (socketType() == SocketType::TIPC) {
        GTEST_SKIP() << "File descriptor tests not supported on Trusty (yet)";
    }

    auto proc = createRpcTestSocketServerProcess({
            .clientFileDescriptorTransportMode = RpcSession::FileDescriptorTransportMode::UNIX,
            .serverSupportedFileDescriptorTransportModes =
                    {RpcSession::FileDescriptorTransportMode::UNIX},
    });

    android::os::ParcelFileDescriptor out;
    auto status = proc.rootIface->echoAsFile("hello", &out);
    if (!supportsFdTransport()) {
        EXPECT_EQ(status.transactionError(), BAD_VALUE) << status;
        return;
    }
    ASSERT_TRUE(status.isOk()) << status;

    std::string result;
    ASSERT_TRUE(ReadFdToString(out.get(), &result));
    ASSERT_EQ(result, "hello");
}

TEST_P(BinderRpc, SendFiles) {
    if (socketType() == SocketType::TIPC) {
        GTEST_SKIP() << "File descriptor tests not supported on Trusty (yet)";
    }

    auto proc = createRpcTestSocketServerProcess({
            .clientFileDescriptorTransportMode = RpcSession::FileDescriptorTransportMode::UNIX,
            .serverSupportedFileDescriptorTransportModes =
                    {RpcSession::FileDescriptorTransportMode::UNIX},
    });

    std::vector<android::os::ParcelFileDescriptor> files;
    files.emplace_back(android::os::ParcelFileDescriptor(mockFileDescriptor("123")));
    files.emplace_back(android::os::ParcelFileDescriptor(mockFileDescriptor("a")));
    files.emplace_back(android::os::ParcelFileDescriptor(mockFileDescriptor("b")));
    files.emplace_back(android::os::ParcelFileDescriptor(mockFileDescriptor("cd")));

    android::os::ParcelFileDescriptor out;
    auto status = proc.rootIface->concatFiles(files, &out);
    if (!supportsFdTransport()) {
        EXPECT_EQ(status.transactionError(), BAD_VALUE) << status;
        return;
    }
    ASSERT_TRUE(status.isOk()) << status;

    std::string result;
    EXPECT_TRUE(ReadFdToString(out.get(), &result));
    EXPECT_EQ(result, "123abcd");
}

TEST_P(BinderRpc, SendMaxFiles) {
    if (!supportsFdTransport()) {
        GTEST_SKIP() << "Would fail trivially (which is tested by BinderRpc::SendFiles)";
    }

    auto transportMode = RpcSession::FileDescriptorTransportMode::UNIX;
    auto proc = createRpcTestSocketServerProcess({
            .clientFileDescriptorTransportMode = transportMode,
            .serverSupportedFileDescriptorTransportModes = {transportMode},
    });

    size_t maxFds = getRpcTransportModeMaxFds(transportMode);
    std::vector<android::os::ParcelFileDescriptor> files;
    for (size_t i = 0; i < maxFds; i++) {
        files.emplace_back(android::os::ParcelFileDescriptor(mockFileDescriptor("a")));
    }

    android::os::ParcelFileDescriptor out;
    auto status = proc.rootIface->concatFiles(files, &out);
    ASSERT_TRUE(status.isOk()) << status;

    std::string result;
    EXPECT_TRUE(ReadFdToString(out.get(), &result));
    EXPECT_EQ(result, std::string(maxFds, 'a'));
}

TEST_P(BinderRpc, SendTooManyFiles) {
    if (!supportsFdTransport()) {
        GTEST_SKIP() << "Would fail trivially (which is tested by BinderRpc::SendFiles)";
    }

    auto transportMode = RpcSession::FileDescriptorTransportMode::UNIX;
    auto proc = createRpcTestSocketServerProcess({
            .clientFileDescriptorTransportMode = transportMode,
            .serverSupportedFileDescriptorTransportModes = {transportMode},
    });

    size_t maxFds = getRpcTransportModeMaxFds(transportMode);
    std::vector<android::os::ParcelFileDescriptor> files;
    for (size_t i = 0; i < maxFds + 1; i++) {
        files.emplace_back(android::os::ParcelFileDescriptor(mockFileDescriptor("a")));
    }

    android::os::ParcelFileDescriptor out;
    auto status = proc.rootIface->concatFiles(files, &out);
    EXPECT_EQ(status.transactionError(), BAD_VALUE) << status;
}

TEST_P(BinderRpc, AppendInvalidFd) {
    if (socketType() == SocketType::TIPC) {
        GTEST_SKIP() << "File descriptor tests not supported on Trusty (yet)";
    }

    auto proc = createRpcTestSocketServerProcess({
            .clientFileDescriptorTransportMode = RpcSession::FileDescriptorTransportMode::UNIX,
            .serverSupportedFileDescriptorTransportModes =
                    {RpcSession::FileDescriptorTransportMode::UNIX},
    });

    int badFd = fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 0);
    ASSERT_NE(badFd, -1);

    // Close the file descriptor so it becomes invalid for dup
    close(badFd);

    Parcel p1;
    p1.markForBinder(proc.rootBinder);
    p1.writeInt32(3);
    EXPECT_EQ(OK, p1.writeFileDescriptor(badFd, false));

    Parcel pRaw;
    pRaw.markForBinder(proc.rootBinder);
    EXPECT_EQ(OK, pRaw.appendFrom(&p1, 0, p1.dataSize()));

    pRaw.setDataPosition(0);
    EXPECT_EQ(3, pRaw.readInt32());
    ASSERT_EQ(-1, pRaw.readFileDescriptor());
}

} // namespace android
