/*
 * Windows stub implementation for system/thread_defs.h
 * This file provides minimal definitions needed for Windows compilation
 */

#ifndef _WIN32_SYSTEM_THREAD_DEFS_H
#define _WIN32_SYSTEM_THREAD_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

// Android thread priority constants
enum {
    ANDROID_PRIORITY_LOWEST         = 19,
    ANDROID_PRIORITY_BACKGROUND     = 10,
    ANDROID_PRIORITY_NORMAL         = 0,
    ANDROID_PRIORITY_FOREGROUND     = -2,
    ANDROID_PRIORITY_DISPLAY        = -4,
    ANDROID_PRIORITY_URGENT_DISPLAY = -8,
    ANDROID_PRIORITY_AUDIO          = -16,
    ANDROID_PRIORITY_URGENT_AUDIO   = -19,
    ANDROID_PRIORITY_HIGHEST        = -20,
    ANDROID_PRIORITY_DEFAULT        = ANDROID_PRIORITY_NORMAL,
    ANDROID_PRIORITY_MORE_FAVORABLE = -1,
    ANDROID_PRIORITY_LESS_FAVORABLE = 1,
};

#ifdef __cplusplus
}
#endif

#endif // _WIN32_SYSTEM_THREAD_DEFS_H
