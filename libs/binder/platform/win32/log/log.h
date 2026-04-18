#ifndef __LOGE_H__
#define __LOGE_H__
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

// Windows equivalent for log/log.h
enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,
};

#define ALOGV(...) printf("[VERBOSE] " __VA_ARGS__); printf("\n")
#define ALOGD(...) printf("[DEBUG] " __VA_ARGS__); printf("\n")
#define ALOGI(...) printf("[INFO] " __VA_ARGS__); printf("\n")
#define ALOGW(...) printf("[WARN] " __VA_ARGS__); printf("\n")
#define ALOGE(...) printf("[ERROR] " __VA_ARGS__); printf("\n")
#define ALOGF(...) { printf("[FATAL] " __VA_ARGS__); printf("\n"); exit(1); }

// Generic ALOG macro
#define ALOG(priority, tag, ...) \
    do { \
        switch (priority) { \
            case ANDROID_LOG_VERBOSE: ALOGV(__VA_ARGS__); break; \
            case ANDROID_LOG_DEBUG: ALOGD(__VA_ARGS__); break; \
            case ANDROID_LOG_INFO: ALOGI(__VA_ARGS__); break; \
            case ANDROID_LOG_WARN: ALOGW(__VA_ARGS__); break; \
            case ANDROID_LOG_ERROR: ALOGE(__VA_ARGS__); break; \
            case ANDROID_LOG_FATAL: ALOGF(__VA_ARGS__); break; \
            default: ALOGI(__VA_ARGS__); break; \
        } \
    } while(0)

// Define missing log level constants
#define LOG_VERBOSE ANDROID_LOG_VERBOSE
#define LOG_DEBUG ANDROID_LOG_DEBUG
#define LOG_INFO ANDROID_LOG_INFO
#define LOG_WARN ANDROID_LOG_WARN
#define LOG_ERROR ANDROID_LOG_ERROR
#define LOG_FATAL ANDROID_LOG_FATAL
#define LOG_SILENT ANDROID_LOG_SILENT

// Fatal logging
#define LOG_ALWAYS_FATAL(format, ...) \
    do { \
        printf("[FATAL] " format "\n", ##__VA_ARGS__); \
        exit(1); \
    } while(0)

// Always log macros
#define ALOGV_IF(cond, ...) \
    do { \
        if (cond) ALOGV(__VA_ARGS__); \
    } while(0)

#define ALOGD_IF(cond, ...) \
    do { \
        if (cond) ALOGD(__VA_ARGS__); \
    } while(0)

#define ALOGI_IF(cond, ...) \
    do { \
        if (cond) ALOGI(__VA_ARGS__); \
    } while(0)

#define ALOGW_IF(cond, ...) \
    do { \
        if (cond) ALOGW(__VA_ARGS__); \
    } while(0)

#define ALOGE_IF(cond, ...) \
    do { \
        if (cond) ALOGE(__VA_ARGS__); \
    } while(0)

// Add missing IF_ALOGV macro
#define IF_ALOGV() if (true)

// Add missing IF_ALOG macro
#define IF_ALOG(priority, tag) if (true)

// Add missing LOG_FATAL_IF macro
#define LOG_FATAL_IF(cond, ...) \
    do { \
        if ((cond)) { \
            printf("[FATAL] " __VA_ARGS__); printf("\n"); \
            exit(1); \
        } \
    } while(0)

// Add missing ALOG_ASSERT macro
#define ALOG_ASSERT(cond, ...) \
    do { \
        if (!(cond)) { \
            printf("[ASSERT] " __VA_ARGS__); printf("\n"); \
            exit(1); \
        } \
    } while(0)

// Missing Android logging functions for Windows compatibility
static inline int android_errorWriteLog(int tag, const char* subTag) {
    printf("[ERROR_WRITE] tag=%d, subTag=%s\n", tag, subTag);
    return 0;
}

// Additional logging macros
#define LOG_ALWAYS_FATAL_IF(cond, ...) do { \
    if ((cond)) { \
        printf("[FATAL] " __VA_ARGS__); printf("\n"); \
        exit(1); \
    } \
} while(0)

#endif