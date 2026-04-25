#include <cutils/ashmem.h>
#include <cutils/native_handle.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int ashmem_valid(int fd) {
    return fd >= 0 && fcntl(fd, F_GETFD) != -1 ? 1 : 0;
}

int ashmem_create_region(const char*, size_t) {
    errno = ENOSYS;
    return -1;
}

int ashmem_set_prot_region(int, int) {
    errno = ENOSYS;
    return -1;
}

int ashmem_pin_region(int, size_t, size_t) {
    errno = ENOSYS;
    return -1;
}

int ashmem_unpin_region(int, size_t, size_t) {
    errno = ENOSYS;
    return -1;
}

int ashmem_get_size_region(int) {
    errno = ENOSYS;
    return -1;
}

int native_handle_close(const native_handle_t* h) {
    if (!h) return -1;

    for (int i = 0; i < h->numFds; i++) {
        if (h->data[i] >= 0) {
            (void)close(h->data[i]);
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

    const size_t size = sizeof(native_handle_t) + sizeof(int) * (numFds + numInts);
    native_handle_t* h = reinterpret_cast<native_handle_t*>(malloc(size));
    if (!h) return nullptr;

    h->version = sizeof(native_handle_t);
    h->numFds = numFds;
    h->numInts = numInts;
    return h;
}

void native_handle_set_fdsan_tag(const native_handle_t*) {}

void native_handle_unset_fdsan_tag(const native_handle_t*) {}

native_handle_t* native_handle_clone(const native_handle_t* handle) {
    if (!handle) return nullptr;

    native_handle_t* h = native_handle_create(handle->numFds, handle->numInts);
    if (!h) return nullptr;

    for (int i = 0; i < handle->numFds; i++) {
        if (handle->data[i] < 0) {
            h->data[i] = handle->data[i];
            continue;
        }
        int dupFd = dup(handle->data[i]);
        if (dupFd < 0) {
            native_handle_close(h);
            native_handle_delete(h);
            return nullptr;
        }
        h->data[i] = dupFd;
    }

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
