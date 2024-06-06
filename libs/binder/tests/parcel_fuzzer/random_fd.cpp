/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <fuzzbinder/random_fd.h>

#include <cutils/ashmem.h>
#include <log/log.h>

#include <fcntl.h>
#include <functional>

namespace android {

using binder::unique_fd;

std::vector<unique_fd> getRandomFds(FuzzedDataProvider* provider) {
    const char* fdType;

    std::vector<unique_fd> fds = provider->PickValueInArray<
            std::function<std::vector<unique_fd>()>>(
            {[&]() {
                 fdType = "ashmem";
                 std::vector<unique_fd> ret;
                 ret.push_back(unique_fd(
                         ashmem_create_region("binder test region",
                                              provider->ConsumeIntegralInRange<size_t>(0, 4096))));
                 return ret;
             },
             [&]() {
                 fdType = "/dev/null";
                 std::vector<unique_fd> ret;
                 ret.push_back(unique_fd(open("/dev/null", O_RDWR)));
                 return ret;
             },
             [&]() {
                 fdType = "pipefd";

                 int pipefds[2];

                 int flags = O_CLOEXEC;
                 if (provider->ConsumeBool()) flags |= O_DIRECT;

                 // TODO(b/236812909): also test blocking
                 if (true) flags |= O_NONBLOCK;

                 LOG_ALWAYS_FATAL_IF(0 != pipe2(pipefds, flags), "pipe2 failed for %d", flags);

                 if (provider->ConsumeBool()) std::swap(pipefds[0], pipefds[1]);

                 std::vector<unique_fd> ret;
                 ret.push_back(unique_fd(pipefds[0]));
                 ret.push_back(unique_fd(pipefds[1]));
                 return ret;
             },
             [&]() {
                 fdType = "tempfd";
                 char name[PATH_MAX];
#if defined(__ANDROID__)
                 snprintf(name, sizeof(name), "/data/local/tmp/android-tempfd-test-%d-XXXXXX",
                          getpid());
#else
                 snprintf(name, sizeof(name), "/tmp/android-tempfd-test-%d-XXXXXX", getpid());
#endif
                 unique_fd fd(mkstemp(name));
                 LOG_ALWAYS_FATAL_IF(!fd.ok(), "Failed to create file %s, errno: %d", name, errno);
                 unlink(name);
                 if (provider->ConsumeBool()) {
                     const auto res = TEMP_FAILURE_RETRY(
                             ftruncate(fd.get(),
                                       provider->ConsumeIntegralInRange<size_t>(0, 4096)));
                     LOG_ALWAYS_FATAL_IF(-1 == res, "Failed to truncate file, errno: %d", errno);
                 }

                 std::vector<unique_fd> ret;
                 ret.emplace_back(std::move(fd));
                 return ret;
             }

            })();

    for (const auto& fd : fds) {
        LOG_ALWAYS_FATAL_IF(!fd.ok(), "%d %s", fd.get(), fdType);
    }

    return fds;
}

} // namespace android
