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

#pragma once

#ifndef ANDROID_LOG_STUB_ENABLE_BASE
#error "android-base logging is not enabled for this module"
#endif

#include <log/log.h>

#include <iostream>

namespace android::base {

enum LogSeverity {
    VERBOSE = ANDROID_LOG_VERBOSE,
    DEBUG = ANDROID_LOG_DEBUG,
    INFO = ANDROID_LOG_INFO,
    WARNING = ANDROID_LOG_WARN,
    ERROR = ANDROID_LOG_ERROR,
    FATAL_WITHOUT_ABORT = ANDROID_LOG_ERROR,
    FATAL = ANDROID_LOG_FATAL,
};

#define LOG(severity) LOGGING_PREAMBLE(severity) && LOG_STREAM(severity)

#define LOG_STREAM(severity) std::cerr

#define LOGGING_PREAMBLE(severity)                                           \
    (__android_log_stub_is_loggable(                                         \
             static_cast<android_LogPriority>(::android::base::severity)) && \
     ABORT_AFTER_LOG_EXPR_IF(::android::base::severity == ::android::base::FATAL, true))

struct LogAbortAfterFullExpr {
    ~LogAbortAfterFullExpr() __attribute__((noreturn)) { abort(); }
    explicit operator bool() const { return false; }
};
#define ABORT_AFTER_LOG_EXPR_IF(c, x) (((c) && ::android::base::LogAbortAfterFullExpr()) || (x))

} // namespace android::base
