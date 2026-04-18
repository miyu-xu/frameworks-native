#include "namedpipe_vsock.h"
#include <iostream>
#include <stdexcept>

namespace android {

NamedPipeVsockServer::NamedPipeVsockServer() 
    : m_address(0, 0), m_pipeHandle(INVALID_HANDLE_VALUE), m_acceptThread(nullptr), m_running(false) {
}

NamedPipeVsockServer::~NamedPipeVsockServer() {
    stop();
}

bool NamedPipeVsockServer::start(const NamedPipeVsockAddress& address) {
    if (m_running) {
        return false;
    }
    
    m_address = address;
    
    if (!createPipeInstance()) {
        return false;
    }
    
    m_running = true;
    
    return true;
}

void NamedPipeVsockServer::stop() {
    HANDLE pipeHandle = INVALID_HANDLE_VALUE;
    HANDLE acceptThread = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
        pipeHandle = m_pipeHandle;
        m_pipeHandle = INVALID_HANDLE_VALUE;
        acceptThread = m_acceptThread;
        m_acceptThread = nullptr;
    }

    if (acceptThread != nullptr) {
        CancelSynchronousIo(acceptThread);
        CloseHandle(acceptThread);
    }

    if (pipeHandle != INVALID_HANDLE_VALUE) {
        CancelIoEx(pipeHandle, NULL);
        DisconnectNamedPipe(pipeHandle);
        CloseHandle(pipeHandle);
    }
}

HANDLE NamedPipeVsockServer::accept() {
    if (!m_running) {
        return INVALID_HANDLE_VALUE;
    }

    HANDLE acceptThread = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &acceptThread,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        return INVALID_HANDLE_VALUE;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running || m_pipeHandle == INVALID_HANDLE_VALUE) {
            CloseHandle(acceptThread);
            return INVALID_HANDLE_VALUE;
        }
        m_acceptThread = acceptThread;
    }

    HANDLE pipeHandle = m_pipeHandle;
    BOOL connected = ConnectNamedPipe(pipeHandle, NULL);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_acceptThread == acceptThread) {
            m_acceptThread = nullptr;
        }
    }
    CloseHandle(acceptThread);

    if (connected) {
        HANDLE clientHandle = INVALID_HANDLE_VALUE;
        if (!DuplicateHandle(GetCurrentProcess(), pipeHandle,
                            GetCurrentProcess(), &clientHandle,
                            0, FALSE, DUPLICATE_SAME_ACCESS)) {
            DWORD error = GetLastError();
            return INVALID_HANDLE_VALUE;
        }
        
        if (!createPipeInstance()) {
        }
        
        return clientHandle;
    }
    
    DWORD error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) {
        HANDLE clientHandle = INVALID_HANDLE_VALUE;
        if (!DuplicateHandle(GetCurrentProcess(), pipeHandle,
                            GetCurrentProcess(), &clientHandle,
                            0, FALSE, DUPLICATE_SAME_ACCESS)) {
            DWORD error = GetLastError();
            return INVALID_HANDLE_VALUE;
        }
        
        if (!createPipeInstance()) {
        }
        
        return clientHandle;
    }
    
    return INVALID_HANDLE_VALUE;
}

bool NamedPipeVsockServer::createPipeInstance() {
    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipeHandle);
    }

    m_pipeHandle = CreateNamedPipeA(
        m_address.pipeName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE |
        PIPE_READMODE_BYTE |
        PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        4096,
        4096,
        0,
        NULL
    );
    
    if (m_pipeHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return false;
    }
    
    return true;
}

NamedPipeVsockClient::NamedPipeVsockClient() 
    : m_address(0, 0), m_pipeHandle(INVALID_HANDLE_VALUE), m_connected(false) {
}

NamedPipeVsockClient::~NamedPipeVsockClient() {
    disconnect();
}

bool NamedPipeVsockClient::connect(const NamedPipeVsockAddress& address) {
    if (m_connected) {
        disconnect();
    }
    
    m_address = address;
    
    
    m_pipeHandle = CreateFileA(
        m_address.pipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (m_pipeHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        m_connected = false;
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    if (!SetNamedPipeHandleState(m_pipeHandle, &mode, NULL, NULL)) {
        DWORD error = GetLastError();
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
        m_connected = false;
        return false;
    }
    
    m_connected = true;
    return true;
}

void NamedPipeVsockClient::disconnect() {
    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
    }
    m_connected = false;
}

std::unique_ptr<class NamedPipeVsockTransport> NamedPipeVsockClient::getTransport() {
    if (!m_connected || m_pipeHandle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    HANDLE duplicatedHandle = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), m_pipeHandle,
                        GetCurrentProcess(), &duplicatedHandle,
                        0, FALSE, DUPLICATE_SAME_ACCESS)) {
        DWORD error = GetLastError();
        return nullptr;
    }
    
    return std::make_unique<NamedPipeVsockTransport>(duplicatedHandle);
}

NamedPipeVsockTransport::NamedPipeVsockTransport(HANDLE pipeHandle)
    : m_pipeHandle(pipeHandle) {
}

NamedPipeVsockTransport::~NamedPipeVsockTransport() {
    close();
}

int NamedPipeVsockTransport::read(void* buffer, size_t size) {
    if (m_pipeHandle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    DWORD bytesRead = 0;
    BOOL success = ReadFile(
        m_pipeHandle,
        buffer,
        static_cast<DWORD>(size),
        &bytesRead,
        NULL
    );
    
    if (!success) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE) {
        } else {
        }
        return -1;
    }
    
    return static_cast<int>(bytesRead);
}

int NamedPipeVsockTransport::write(const void* buffer, size_t size) {
    if (m_pipeHandle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    DWORD bytesWritten = 0;
    BOOL success = WriteFile(
        m_pipeHandle,
        buffer,
        static_cast<DWORD>(size),
        &bytesWritten,
        NULL
    );
    
    if (!success) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE) {
        } else {
        }
        return -1;
    }
    
    return static_cast<int>(bytesWritten);
}

void NamedPipeVsockTransport::close() {
    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
    }
}

bool NamedPipeVsockTransport::pollRead() {
    if (m_pipeHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(m_pipeHandle, NULL, 0, NULL, &bytesAvailable, NULL)) {
        DWORD error = GetLastError();
        return false;
    }
    
    return bytesAvailable > 0;
}

bool NamedPipeVsockTransport::interrupt() {
    close();
    return true;
}

bool NamedPipeVsockTransport::send(const void* data, size_t size) {
    return write(data, size) >= 0;
}

bool NamedPipeVsockTransport::receive(void* data, size_t size) {
    return read(data, size) >= 0;
}

int NamedPipeVsockTransport::receiveFully(void* data, size_t size) {
    if (m_pipeHandle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(m_pipeHandle, NULL, 0, NULL, &bytesAvailable, NULL)) {
        DWORD error = GetLastError();
        return -static_cast<int>(error);
    }
    
    if (bytesAvailable < size) {
    }
    
    uint8_t* buffer = static_cast<uint8_t*>(data);
    size_t totalBytesRead = 0;
    
    while (totalBytesRead < size) {
        DWORD bytesRead = 0;
        BOOL success = ReadFile(
            m_pipeHandle,
            buffer + totalBytesRead,
            static_cast<DWORD>(size - totalBytesRead),
            &bytesRead,
            NULL
        );
        
        if (!success) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
            } else if (error == ERROR_MORE_DATA) {
                totalBytesRead += bytesRead;
                continue;
            } else {
            }
            return -static_cast<int>(error);
        }
        
        if (bytesRead == 0) {
            return -1;
        }
        
        totalBytesRead += bytesRead;
    }
    
    return static_cast<int>(totalBytesRead);
}


} // namespace android
