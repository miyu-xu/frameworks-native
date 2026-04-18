#include "namedpipe_rpc_transport.h"
#include "namedpipe_vsock.h"

#include <fcntl.h>
#include <io.h>
#include <stdint.h>

#include <vector>
#include <functional>
#include <cstring>
#include <cstdio>
#include <iostream>

namespace android {

namespace {

struct NamedPipeWireHeader {
    uint32_t payloadSize;
    uint32_t handleCount;
};

static status_t getPeerProcessHandle(HANDLE pipeHandle, HANDLE* processHandle) {
    ULONG peerPid = 0;
    DWORD currentPid = GetCurrentProcessId();

    if (GetNamedPipeClientProcessId(pipeHandle, &peerPid) && peerPid != currentPid) {
        *processHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, peerPid);
        return *processHandle != nullptr ? OK : UNKNOWN_ERROR;
    }

    if (GetNamedPipeServerProcessId(pipeHandle, &peerPid) && peerPid != currentPid) {
        *processHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, peerPid);
        return *processHandle != nullptr ? OK : UNKNOWN_ERROR;
    }

    return NAME_NOT_FOUND;
}

static status_t duplicateFdForPeer(
        int fd, HANDLE peerProcess, uint64_t* duplicatedValue) {
    if (fd < 0) {
        return BAD_VALUE;
    }

    intptr_t osHandle = _get_osfhandle(fd);
    HANDLE sourceHandle = osHandle == -1
            ? reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd))
            : reinterpret_cast<HANDLE>(osHandle);
    HANDLE duplicatedHandle = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), sourceHandle, peerProcess, &duplicatedHandle, 0,
                         FALSE, DUPLICATE_SAME_ACCESS)) {
        return UNKNOWN_ERROR;
    }

    *duplicatedValue = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(duplicatedHandle));
    return OK;
}

static status_t adoptReceivedHandle(
        uint64_t rawHandleValue,
        std::vector<std::variant<binder::unique_fd, binder::borrowed_fd>>* ancillaryFds) {
    HANDLE receivedHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(rawHandleValue));
    if (ancillaryFds != nullptr) {
        ancillaryFds->emplace_back(
                binder::unique_fd(static_cast<int>(reinterpret_cast<intptr_t>(receivedHandle))));
    } else {
        CloseHandle(receivedHandle);
    }
    return OK;
}

}  // namespace

NamedPipeRpcTransport::NamedPipeRpcTransport(std::unique_ptr<NamedPipeVsockTransport> transport)
    : mTransport(std::move(transport)), mIsWaiting(false) {
}

NamedPipeRpcTransport::~NamedPipeRpcTransport() {
}

status_t NamedPipeRpcTransport::pollRead(void) {
    if (!mTransport) {
        return BAD_VALUE;
    }
    
    bool hasData = mTransport->pollRead();
    return hasData ? OK : WOULD_BLOCK;
}

status_t NamedPipeRpcTransport::interruptableWriteFully(
        FdTrigger* fdTrigger, iovec* iovs, int niovs,
        const std::optional<binder::impl::SmallFunction<status_t()>>& altPoll,
        const std::vector<std::variant<binder::unique_fd, binder::borrowed_fd>>* ancillaryFds) {
    if (!mTransport) {
        return BAD_VALUE;
    }

    size_t totalSize = 0;
    for (int i = 0; i < niovs; i++) {
        totalSize += iovs[i].iov_len;
    }

    std::vector<uint8_t> buffer(totalSize);
    size_t offset = 0;
    for (int i = 0; i < niovs; i++) {
        memcpy(buffer.data() + offset, iovs[i].iov_base, iovs[i].iov_len);
        offset += iovs[i].iov_len;
    }

    std::vector<uint64_t> duplicatedHandles;
    if (ancillaryFds != nullptr && !ancillaryFds->empty()) {
        HANDLE peerProcess = nullptr;
        status_t processStatus = getPeerProcessHandle(mTransport->getPipeHandle(), &peerProcess);
        if (processStatus != OK) {
            return processStatus;
        }

        duplicatedHandles.reserve(ancillaryFds->size());
        for (const auto& fd : *ancillaryFds) {
            int rawFd = std::visit([](const auto& value) { return value.get(); }, fd);
            uint64_t duplicatedValue = 0;
            status_t duplicateStatus = duplicateFdForPeer(rawFd, peerProcess, &duplicatedValue);
            if (duplicateStatus != OK) {
                CloseHandle(peerProcess);
                return duplicateStatus;
            }
            duplicatedHandles.push_back(duplicatedValue);
        }

        CloseHandle(peerProcess);
    }

    NamedPipeWireHeader header{
            .payloadSize = static_cast<uint32_t>(totalSize),
            .handleCount = static_cast<uint32_t>(duplicatedHandles.size()),
    };

    std::vector<uint8_t> packet(sizeof(header) + duplicatedHandles.size() * sizeof(uint64_t) +
                                buffer.size());
    memcpy(packet.data(), &header, sizeof(header));
    if (!duplicatedHandles.empty()) {
        memcpy(packet.data() + sizeof(header), duplicatedHandles.data(),
               duplicatedHandles.size() * sizeof(uint64_t));
    }
    memcpy(packet.data() + sizeof(header) + duplicatedHandles.size() * sizeof(uint64_t),
           buffer.data(), buffer.size());

    if (!mTransport->send(packet.data(), packet.size())) {
        return UNKNOWN_ERROR;
    }
    return OK;
}

status_t NamedPipeRpcTransport::interruptableReadFully(
        FdTrigger* fdTrigger, iovec* iovs, int niovs,
        const std::optional<binder::impl::SmallFunction<status_t()>>& altPoll,
        std::vector<std::variant<binder::unique_fd, binder::borrowed_fd>>* ancillaryFds) {
    if (!mTransport) {
        return BAD_VALUE;
    }

    size_t totalSize = 0;
    for (int i = 0; i < niovs; i++) {
        totalSize += iovs[i].iov_len;
    }

    auto loadNextPacket = [&]() -> status_t {
        mPendingPayload.clear();
        mPendingPayloadOffset = 0;
        mPendingAncillaryFds.clear();

        NamedPipeWireHeader header{};
        int headerResult = mTransport->receiveFully(&header, sizeof(header));
        if (headerResult < 0) {
            int windowsError = -headerResult;
            std::fprintf(stderr, "NamedPipeRpcTransport header read failed: winerr=%d\n",
                         windowsError);
            switch (windowsError) {
                case ERROR_BROKEN_PIPE:
                    return DEAD_OBJECT;
                case ERROR_MORE_DATA:
                    return NOT_ENOUGH_DATA;
                case ERROR_IO_PENDING:
                    return WOULD_BLOCK;
                case ERROR_TIMEOUT:
                    return TIMED_OUT;
                default:
                    return UNKNOWN_ERROR;
            }
        }

        for (uint32_t i = 0; i < header.handleCount; i++) {
            uint64_t rawHandleValue = 0;
            int handleResult = mTransport->receiveFully(&rawHandleValue, sizeof(rawHandleValue));
            if (handleResult < 0) {
                std::fprintf(stderr, "NamedPipeRpcTransport handle read failed: winerr=%d\n",
                             -handleResult);
                return UNKNOWN_ERROR;
            }
            status_t adoptStatus = adoptReceivedHandle(rawHandleValue, &mPendingAncillaryFds);
            if (adoptStatus != OK) {
                return adoptStatus;
            }
        }

        mPendingPayload.resize(header.payloadSize);
        int result = mTransport->receiveFully(mPendingPayload.data(), mPendingPayload.size());
        if (result < 0) {
            int windowsError = -result;
            std::fprintf(stderr, "NamedPipeRpcTransport payload read failed: winerr=%d\n",
                         windowsError);
            switch (windowsError) {
                case ERROR_BROKEN_PIPE:
                    return DEAD_OBJECT;
                case ERROR_MORE_DATA:
                    return NOT_ENOUGH_DATA;
                case ERROR_IO_PENDING:
                    return WOULD_BLOCK;
                case ERROR_TIMEOUT:
                    return TIMED_OUT;
                default:
                    return UNKNOWN_ERROR;
            }
        }

        return OK;
    };

    if (ancillaryFds != nullptr) {
        ancillaryFds->clear();
    }

    size_t bytesCopied = 0;
    bool ancillaryMoved = false;
    for (int i = 0; i < niovs; i++) {
        uint8_t* dest = static_cast<uint8_t*>(iovs[i].iov_base);
        size_t destOffset = 0;

        while (destOffset < iovs[i].iov_len) {
            if (mPendingPayloadOffset >= mPendingPayload.size()) {
                status_t status = loadNextPacket();
                if (status != OK) {
                    return status;
                }
            }

            if (!ancillaryMoved && mPendingPayloadOffset == 0 && ancillaryFds != nullptr &&
                !mPendingAncillaryFds.empty()) {
                ancillaryFds->reserve(mPendingAncillaryFds.size());
                for (auto& fd : mPendingAncillaryFds) {
                    ancillaryFds->emplace_back(std::move(fd));
                }
                ancillaryMoved = true;
            }

            size_t packetRemaining = mPendingPayload.size() - mPendingPayloadOffset;
            size_t copySize = std::min(packetRemaining, iovs[i].iov_len - destOffset);

            memcpy(dest + destOffset, mPendingPayload.data() + mPendingPayloadOffset, copySize);
            destOffset += copySize;
            bytesCopied += copySize;
            mPendingPayloadOffset += copySize;

            if (mPendingPayloadOffset >= mPendingPayload.size()) {
                mPendingPayload.clear();
                mPendingPayloadOffset = 0;
                mPendingAncillaryFds.clear();
            }
        }
    }

    if (bytesCopied != totalSize) {
        return BAD_VALUE;
    }

    return OK;
}

bool NamedPipeRpcTransport::isWaiting() {
    return mIsWaiting;
}

NamedPipeRpcTransportCtx::NamedPipeRpcTransportCtx()
    : RpcTransportCtx() {
}

NamedPipeRpcTransportCtx::~NamedPipeRpcTransportCtx() {
}

std::unique_ptr<RpcTransport> NamedPipeRpcTransportCtx::newTransport(
        android::RpcTransportFd fd, FdTrigger* fdTrigger) const {
    if (!fd.fd.ok()) {
        return nullptr;
    }

    intptr_t osHandle = _get_osfhandle(fd.fd.get());
    HANDLE pipeHandle = osHandle == -1
            ? reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd.fd.get()))
            : reinterpret_cast<HANDLE>(osHandle);
    if (pipeHandle == INVALID_HANDLE_VALUE || pipeHandle == nullptr) {
        return nullptr;
    }

    HANDLE ownedPipeHandle = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), pipeHandle, GetCurrentProcess(), &ownedPipeHandle, 0,
                         FALSE, DUPLICATE_SAME_ACCESS)) {
        return nullptr;
    }

    auto transport = std::make_unique<NamedPipeVsockTransport>(ownedPipeHandle);
    if (!transport) {
        CloseHandle(ownedPipeHandle);
        return nullptr;
    }
    
    if (!transport->isConnected()) {
        return nullptr;
    }
    
    
    auto rpcTransport = std::make_unique<NamedPipeRpcTransport>(std::move(transport));
    
    return rpcTransport;
}

std::vector<uint8_t> NamedPipeRpcTransportCtx::getCertificate(
        RpcCertificateFormat format) const {
    return {};
}

NamedPipeRpcTransportCtxFactory::NamedPipeRpcTransportCtxFactory()
    : RpcTransportCtxFactory() {
}

NamedPipeRpcTransportCtxFactory::~NamedPipeRpcTransportCtxFactory() {
}

std::unique_ptr<RpcTransportCtx> NamedPipeRpcTransportCtxFactory::newServerCtx() const {
    auto ctx = std::make_unique<NamedPipeRpcTransportCtx>();
    return ctx;
}

std::unique_ptr<RpcTransportCtx> NamedPipeRpcTransportCtxFactory::newClientCtx() const {
    auto ctx = std::make_unique<NamedPipeRpcTransportCtx>();
    return ctx;
}

const char* NamedPipeRpcTransportCtxFactory::toCString() const {
    return "namedpipe";
}

} // namespace android
