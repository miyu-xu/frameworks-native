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

#include <binder/RpcThreads.h>
#include <log/log.h>
#include <sys/mman.h>
#ifndef BINDER_RPC_TO_TRUSTY_TEST
#include <trusty/memref.h>
#else // BINDER_RPC_TO_TRUSTY_TEST
#include <BufferAllocator/BufferAllocator.h>
#include <binder/unique_fd.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "FileUtils.h"

constexpr size_t kPageSize = 4096;

#ifndef BINDER_RPC_TO_TRUSTY_TEST
// We can only create memref's over page-aligned regions from
// the heap or data section. We could use posix_memalign to allocate
// these pages on demand, but we have no way to tell when they're not
// needed anymore. We work around this by using a ring buffer of
// statically allocated pages.
constexpr size_t kPageRingBufferSize = 4;

static __attribute__((aligned(kPageSize))) char sPageRingBuffer[kPageRingBufferSize][kPageSize];
static size_t sPageRingBufferIdx = 0;
#endif

struct SizedString {
    uint64_t len;
    char chars[];
};

namespace android::binder {

#ifdef BINDER_NO_LIBBASE
bool ReadFdToString(borrowed_fd fd, std::string* content) {
    content->clear();

    SizedString* str =
            reinterpret_cast<SizedString*>(mmap(NULL, kPageSize, PROT_READ, 0, fd.get(), 0));
    LOG_ALWAYS_FATAL_IF(str == MAP_FAILED, "Failed to map Fd");
    LOG_ALWAYS_FATAL_IF(str->len + sizeof(str->len) > kPageSize, "String larger than page size");

    content->append(str->chars, str->len);
    int ret = munmap(str, kPageSize);
    LOG_ALWAYS_FATAL_IF(ret != 0, "Failed to unmap page");
    return true;
}
#endif

#ifndef BINDER_RPC_TO_TRUSTY_TEST
unique_fd mockFileDescriptor(std::string contents) {
    SizedString* str = reinterpret_cast<SizedString*>(sPageRingBuffer[sPageRingBufferIdx]);
    LOG_ALWAYS_FATAL_IF(contents.length() + sizeof(str->len) > kPageSize);
    str->len = contents.length();
    contents.copy(str->chars, str->len);

    int memref = memref_create(str, kPageSize, MMAP_FLAG_PROT_READ);
    LOG_ALWAYS_FATAL_IF(memref < 0, "Failed to create memref");

    sPageRingBufferIdx = (sPageRingBufferIdx + 1) % kPageRingBufferSize;
    return unique_fd{memref};
}
#else  // BINDER_RPC_TO_TRUSTY_TEST
// Create a DmaBuf allocated FD to send to Trusty that returns `contents` when
// read. The buffer format must match the format expected by ReadFdToString.
unique_fd mockFileDescriptor(std::string contents) {
    SizedString* str = nullptr;
    LOG_ALWAYS_FATAL_IF(contents.length() + sizeof(str->len) > kPageSize);

    BufferAllocator allocator;

    int ret = allocator.Alloc(kDmabufSystemHeapName, kPageSize, 0, 0 /* legacy align */);
    LOG_ALWAYS_FATAL_IF(ret < 0, "Could not allocate heap");
    unique_fd fd(ret);

    char* buf = (char*)mmap(0, kPageSize, PROT_WRITE, MAP_SHARED, fd.get(), 0);
    LOG_ALWAYS_FATAL_IF(buf == MAP_FAILED, "Could not map dmabuf");
    str = reinterpret_cast<SizedString*>(buf);
    str->len = contents.length();
    contents.copy(str->chars, str->len);
    ret = munmap(buf, kPageSize);
    int savedErrno = errno;
    LOG_ALWAYS_FATAL_IF(ret != 0, "Could not unmap buffer: %s", strerror(savedErrno));

    return fd;
}
#endif // BINDER_RPC_TO_TRUSTY_TEST

} // namespace android::binder
