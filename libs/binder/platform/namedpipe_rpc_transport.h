#pragma once

#include <windows.h>
#include <memory>
#include <vector>

#include <binder/RpcTransport.h>
#include <binder/Functional.h>
#include <binder/unique_fd.h>

#include "namedpipe_vsock.h"

namespace android {

class NamedPipeRpcTransport : public RpcTransport {
public:
    explicit NamedPipeRpcTransport(std::unique_ptr<NamedPipeVsockTransport> transport);
    ~NamedPipeRpcTransport() override;

    [[nodiscard]] status_t pollRead(void) override;
    [[nodiscard]] status_t interruptableWriteFully(
            FdTrigger* fdTrigger, iovec* iovs, int niovs,
            const std::optional<binder::impl::SmallFunction<status_t()>>& altPoll,
            const std::vector<std::variant<binder::unique_fd, binder::borrowed_fd>>*
                    ancillaryFds) override;
    [[nodiscard]] status_t interruptableReadFully(
            FdTrigger* fdTrigger, iovec* iovs, int niovs,
            const std::optional<binder::impl::SmallFunction<status_t()>>& altPoll,
            std::vector<std::variant<binder::unique_fd, binder::borrowed_fd>>* ancillaryFds) override;
    [[nodiscard]] bool isWaiting() override;

private:
    std::unique_ptr<NamedPipeVsockTransport> mTransport;
    bool mIsWaiting;
    std::vector<uint8_t> mPendingPayload;
    size_t mPendingPayloadOffset = 0;
    std::vector<std::variant<binder::unique_fd, binder::borrowed_fd>> mPendingAncillaryFds;
};

class NamedPipeRpcTransportCtx : public RpcTransportCtx {
public:
    NamedPipeRpcTransportCtx();
    ~NamedPipeRpcTransportCtx() override;

    [[nodiscard]] std::unique_ptr<RpcTransport> newTransport(
            android::RpcTransportFd fd, FdTrigger *fdTrigger) const override;
    [[nodiscard]] std::vector<uint8_t> getCertificate(
            RpcCertificateFormat format) const override;

private:
};

class NamedPipeRpcTransportCtxFactory : public RpcTransportCtxFactory {
public:
    NamedPipeRpcTransportCtxFactory();
    ~NamedPipeRpcTransportCtxFactory() override;

    [[nodiscard]] std::unique_ptr<RpcTransportCtx> newServerCtx() const override;
    [[nodiscard]] std::unique_ptr<RpcTransportCtx> newClientCtx() const override;
    [[nodiscard]] const char *toCString() const override;
};

} // namespace android
