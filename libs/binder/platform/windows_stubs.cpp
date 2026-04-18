#include <cutils/ashmem.h>
#include <cutils/native_handle.h>
#include <android/binder_ibinder.h>
#include <android/binder_process.h>
#include <android/binder_shell.h>
#include <android/binder_stability.h>
#include <errno.h>
#include <cstdlib>
#include <io.h>
#include <windows.h>


// ashmem stubs for Windows
int ashmem_valid(int fd) {
    // For host-RPC use, treat a valid CRT fd as "ashmem-valid enough".
    if (fd < 0) return 0;
    intptr_t h = _get_osfhandle(fd);
    return h != -1 ? 1 : 0;
}

int ashmem_create_region(const char *name, size_t size) {
    // On Windows, we don't have ashmem, return error
    errno = ENOSYS;
    return -1;
}

int ashmem_set_prot_region(int fd, int prot) {
    // On Windows, we don't have ashmem, return error
    errno = ENOSYS;
    return -1;
}

int ashmem_pin_region(int fd, size_t offset, size_t len) {
    // On Windows, we don't have ashmem, return error
    errno = ENOSYS;
    return -1;
}

int ashmem_unpin_region(int fd, size_t offset, size_t len) {
    // On Windows, we don't have ashmem, return error
    errno = ENOSYS;
    return -1;
}

int ashmem_get_size_region(int fd) {
    // On Windows, we don't have ashmem, return error
    errno = ENOSYS;
    return -1;
}

// native_handle stubs for Windows
int native_handle_close(const native_handle_t* h) {
    if (!h) return -1;

    // Close only fd slots. Integer payload slots are metadata and must be kept as-is.
    for (int i = 0; i < h->numFds; i++) {
        if (h->data[i] >= 0) {
            (void)_close(h->data[i]);
        }
    }
    return 0;
}

int native_handle_close_with_tag(const native_handle_t* h) {
    return native_handle_close(h);
}

native_handle_t* native_handle_init(char* storage, int numFds, int numInts) {
    if (!storage || numFds < 0 || numInts < 0) return nullptr;
    
    native_handle_t* h = reinterpret_cast<native_handle_t*>(storage);
    h->version = sizeof(native_handle_t);
    h->numFds = numFds;
    h->numInts = numInts;
    return h;
}

native_handle_t* native_handle_create(int numFds, int numInts) {
    if (numFds < 0 || numInts < 0) return nullptr;
    
    size_t size = sizeof(native_handle_t) + sizeof(int) * (numFds + numInts);
    native_handle_t* h = reinterpret_cast<native_handle_t*>(malloc(size));
    if (!h) return nullptr;
    
    h->version = sizeof(native_handle_t);
    h->numFds = numFds;
    h->numInts = numInts;
    return h;
}

void native_handle_set_fdsan_tag(const native_handle_t* handle) {
    // On Windows, we don't have fdsan, so do nothing
}

void native_handle_unset_fdsan_tag(const native_handle_t* handle) {
    // On Windows, we don't have fdsan, so do nothing
}

native_handle_t* native_handle_clone(const native_handle_t* handle) {
    if (!handle) return nullptr;

    native_handle_t* h = native_handle_create(handle->numFds, handle->numInts);
    if (!h) return nullptr;

    // Duplicate fd slots to avoid double-close and cross-handle lifetime coupling.
    for (int i = 0; i < handle->numFds; i++) {
        if (handle->data[i] < 0) {
            h->data[i] = handle->data[i];
            continue;
        }
        int dup_fd = _dup(handle->data[i]);
        if (dup_fd < 0) {
            native_handle_close(h);
            native_handle_delete(h);
            return nullptr;
        }
        h->data[i] = dup_fd;
    }

    // Copy integer payload slots.
    for (int i = 0; i < handle->numInts; i++) {
        h->data[handle->numFds + i] = handle->data[handle->numFds + i];
    }
    
    return h;
}

int native_handle_delete(native_handle_t* h) {
    if (!h) return -1;
    
    free(h);
    return 0;
}


// androidSetThreadName stub for Windows
#ifdef __cplusplus
extern "C" {
#endif

void androidSetThreadName(const char* name) {
    // On Windows, we can set thread name using SetThreadDescription
    // This requires Windows 10 version 1607 or later
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32) {
        typedef HRESULT (WINAPI *SetThreadDescriptionFunc)(HANDLE, PCWSTR);
        SetThreadDescriptionFunc setThreadDescription = 
            (SetThreadDescriptionFunc)GetProcAddress(kernel32, "SetThreadDescription");
        
        if (setThreadDescription) {
            // Convert UTF-8 to UTF-16
            int wlen = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
            if (wlen > 0) {
                wchar_t* wname = new wchar_t[wlen];
                MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, wlen);
                setThreadDescription(GetCurrentThread(), wname);
                delete[] wname;
                return; // Successfully set thread name using modern API
            }
        }
    }
}

// Missing NDK Binder platform APIs on Windows host builds.
// These are required by Rust binder consumers but are not available in our host build.
bool AIBinder_isHandlingTransaction() {
    // Kernel binder thread state is unavailable on host Windows build.
    return false;
}

#ifdef __cplusplus
} // extern "C"
#endif
