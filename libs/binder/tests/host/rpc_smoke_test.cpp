// Minimal RPC smoke test: creates an RPC server and client in the same process,
// sends a transaction, and verifies the echo reply.
// Returns 0 on success, non-zero with diagnostic message on failure.

#include <binder/Binder.h>
#include <binder/IBinder.h>
#include <binder/Parcel.h>
#include <binder/RpcServer.h>
#include <binder/RpcSession.h>
#include <binder/RpcTransportRaw.h>
#include <binder/unique_fd.h>
#include <utils/Errors.h>
#include <utils/String8.h>

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unistd.h>

using namespace android;

static constexpr uint32_t ECHO_TRANSACTION = 1;

// A BBinder subclass that echoes back the incoming data parcel.
class EchoBinder : public BBinder {
    status_t onTransact(uint32_t code, const Parcel &data, Parcel *reply,
                        uint32_t /*flags*/) override {
        if (code != ECHO_TRANSACTION) {
            return UNKNOWN_TRANSACTION;
        }
        int32_t val = data.readInt32();
        reply->writeInt32(val);
        return OK;
    }
};

int main() {
    char templateStr[] = "/tmp/binder_rpc_test_XXXXXX";
    int fd = mkstemp(templateStr);
    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }
    ::close(fd);
    std::string socketPath(templateStr);
    ::unlink(socketPath.c_str());

    sp<EchoBinder> service = new EchoBinder();
    sp<RpcServer> server = RpcServer::make();
    if (server == nullptr) {
        fprintf(stderr, "FAIL: RpcServer::make returned null\n");
        ::unlink(socketPath.c_str());
        return 1;
    }
    status_t s = server->setupUnixDomainServer(socketPath.c_str());
    if (s != OK) {
        fprintf(stderr, "FAIL: setupUnixDomainServer: %s\n",
                statusToString(s).c_str());
        ::unlink(socketPath.c_str());
        return 1;
    }
    server->setRootObject(sp<IBinder>(service));
    server->setMaxThreads(2);

    std::thread server_thread([&server]() { server->join(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    sp<RpcSession> client = RpcSession::make();
    if (client == nullptr) {
        fprintf(stderr, "FAIL: RpcSession::make returned null\n");
        (void)server->shutdown();
        server_thread.join();
        ::unlink(socketPath.c_str());
        return 1;
    }
    s = client->setupUnixDomainClient(socketPath.c_str());
    if (s != OK) {
        fprintf(stderr, "FAIL: setupUnixDomainClient: %s\n",
                statusToString(s).c_str());
        (void)server->shutdown();
        server_thread.join();
        ::unlink(socketPath.c_str());
        return 1;
    }

    sp<IBinder> remote = client->getRootObject();
    if (remote == nullptr) {
        fprintf(stderr, "FAIL: getRootObject returned null\n");
        client->shutdownAndWait(true);
        (void)server->shutdown();
        server_thread.join();
        ::unlink(socketPath.c_str());
        return 1;
    }

    Parcel data, reply;
    data.markForBinder(remote);
    data.writeInt32(42);
    s = remote->transact(ECHO_TRANSACTION, data, &reply, 0);
    if (s != OK) {
        fprintf(stderr, "FAIL: transact: %s\n", statusToString(s).c_str());
        client->shutdownAndWait(true);
        (void)server->shutdown();
        server_thread.join();
        ::unlink(socketPath.c_str());
        return 1;
    }

    int32_t echo_val = reply.readInt32();
    if (echo_val != 42) {
        fprintf(stderr, "FAIL: echo mismatch (value=%d, expected 42)\n",
                echo_val);
        client->shutdownAndWait(true);
        (void)server->shutdown();
        server_thread.join();
        ::unlink(socketPath.c_str());
        return 1;
    }

    ::unlink(socketPath.c_str());
    fprintf(stdout, "PASS: RPC ping-pong round-trip succeeded\n");
    fflush(stdout);

    // Use _exit to skip C++ destructor cleanup that would assert.
    // RpcServer::~RpcServer requires shutdown() first, but shutdown()
    // blocks indefinitely on macOS when there are active sessions.
    server_thread.detach();
    _exit(0);
}
