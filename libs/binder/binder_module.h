/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef _BINDER_MODULE_H_
#define _BINDER_MODULE_H_

/* obtain structures and constants from the kernel header */

// TODO(b/31559095): bionic on host
#ifndef __ANDROID__
#define __packed __attribute__((__packed__))
#endif

// TODO(b/31559095): bionic on host
#if defined(B_PACK_CHARS) && !defined(_UAPI_LINUX_BINDER_H)
#undef B_PACK_CHARS
#endif

#include <linux/android/binder.h>
#include <sys/ioctl.h>

#if defined(ANDROID) || defined(__BIONIC__)
#define HAS_FLAT_BINDER_FLAG_INHERIT_RT 1
#define HAS_FLAT_BINDER_FLAG_SCHED_POLICY 1
#else
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
enum { BR_TRANSACTION_PENDING_FROZEN = _IO('r', 20) };
#endif
enum flat_binder_object_shifts {
    FLAT_BINDER_FLAG_SCHED_POLICY_SHIFT = 9,
};
#define HAS_FLAT_BINDER_FLAG_INHERIT_RT 0
#define HAS_FLAT_BINDER_FLAG_SCHED_POLICY 0
#endif // defined(ANDROID) || defined(__BIONIC__)

#endif // _BINDER_MODULE_H_
