
#include <windows.h>
#include <io.h>
#include <stdint.h>
#include <binder/unique_fd.h>

namespace android::binder {

bool Pipe(unique_fd* read, unique_fd* write, int flags) {
    HANDLE read_handle, write_handle;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = (flags & O_CLOEXEC) ? FALSE : TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    if (!CreatePipe(&read_handle, &write_handle, &sa, 0)) {
        return false;
    }
    
    int read_fd = _open_osfhandle((intptr_t)read_handle, _O_RDONLY);
    int write_fd = _open_osfhandle((intptr_t)write_handle, _O_WRONLY);
    
    if (read_fd == -1 || write_fd == -1) {
        if (read_fd != -1) _close(read_fd);
        if (write_fd != -1) _close(write_fd);
        CloseHandle(read_handle);
        CloseHandle(write_handle);
        return false;
    }
    
    read->reset(read_fd);
    write->reset(write_fd);
    return true;
}

} // namespace android::binder