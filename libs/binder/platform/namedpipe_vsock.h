#pragma once

#include <windows.h>
#include <mutex>
#include <string>
#include <memory>

namespace android {

struct NamedPipeVsockAddress {
    std::string pipeName;
    unsigned int cid;
    unsigned int port;
    
    NamedPipeVsockAddress(unsigned int cid, unsigned int port) 
        : cid(cid), port(port) {
        pipeName = "\\\\.\\pipe\\binder_rpc_vsock_" + 
                   std::to_string(cid) + "_" + 
                   std::to_string(port);
    }
    
    std::string toString() const {
        return "namedpipe://" + pipeName + " (cid=" + std::to_string(cid) + 
               ", port=" + std::to_string(port) + ")";
    }
};

class NamedPipeVsockServer {
public:
    NamedPipeVsockServer();
    ~NamedPipeVsockServer();
    
    bool start(const NamedPipeVsockAddress& address);
    
    void stop();
    
    HANDLE accept();
    
    bool isRunning() const { return m_running; }
    
    const NamedPipeVsockAddress& getAddress() const { return m_address; }

private:
    NamedPipeVsockAddress m_address;
    HANDLE m_pipeHandle;
    HANDLE m_acceptThread;
    bool m_running;
    mutable std::mutex m_mutex;
    
    bool createPipeInstance();
};

class NamedPipeVsockClient {
public:
    NamedPipeVsockClient();
    ~NamedPipeVsockClient();
    
    bool connect(const NamedPipeVsockAddress& address);
    
    void disconnect();
    
    bool isConnected() const { return m_connected; }
    
    HANDLE getPipeHandle() const { return m_pipeHandle; }
    
    std::unique_ptr<class NamedPipeVsockTransport> getTransport();

private:
    NamedPipeVsockAddress m_address;
    HANDLE m_pipeHandle;
    bool m_connected;
};

class NamedPipeVsockTransport {
public:
    NamedPipeVsockTransport(HANDLE pipeHandle);
    ~NamedPipeVsockTransport();
    
    int read(void* buffer, size_t size);
    
    int write(const void* buffer, size_t size);
    
    bool isConnected() const { return m_pipeHandle != INVALID_HANDLE_VALUE; }
    
    void close();
    
    bool pollRead();
    bool interrupt();
    bool send(const void* data, size_t size);
    bool receive(void* data, size_t size);
    int receiveFully(void* data, size_t size);
    HANDLE getPipeHandle() const { return m_pipeHandle; }

private:
    HANDLE m_pipeHandle;
};

} // namespace android
