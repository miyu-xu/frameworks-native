#ifndef __WIN32_SYS_MMAN_H__
#define __WIN32_SYS_MMAN_H__
#include <windows.h>
#include <memoryapi.h>


#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
#define PROT_NONE   0x0

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED    ((void*)-1)

inline void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    DWORD protect = 0;
    if (prot & PROT_WRITE) {
        protect = PAGE_READWRITE;
    } else if (prot & PROT_READ) {
        protect = PAGE_READONLY;
    } else {
        protect = PAGE_NOACCESS;
    }

    DWORD desiredAccess = FILE_MAP_READ;
    if (prot & PROT_WRITE) {
        desiredAccess = FILE_MAP_WRITE;
    }

    HANDLE hFile = INVALID_HANDLE_VALUE;
    if (fd != -1) {
        hFile = (HANDLE)_get_osfhandle(fd);
    }

    HANDLE hMapping = CreateFileMapping(hFile, NULL, protect, 0, length, NULL);
    if (hMapping == NULL) {
        return MAP_FAILED;
    }

    void* result = MapViewOfFile(hMapping, desiredAccess, 0, offset, length);
    CloseHandle(hMapping);
    
    return result ? result : MAP_FAILED;
}

inline int munmap(void* addr, size_t length) {
    return UnmapViewOfFile(addr) ? 0 : -1;
}

inline int mprotect(void* addr, size_t len, int prot) {
    DWORD newProtect = 0;
    if (prot & PROT_WRITE) {
        newProtect = PAGE_READWRITE;
    } else if (prot & PROT_READ) {
        newProtect = PAGE_READONLY;
    } else {
        newProtect = PAGE_NOACCESS;
    }
    
    DWORD oldProtect;
    return VirtualProtect(addr, len, newProtect, &oldProtect) ? 0 : -1;
}

#endif // __WIN32_SYS_MMAN_H__