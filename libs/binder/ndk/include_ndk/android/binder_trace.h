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

#if defined(__ANDROID_VENDOR__)
#include <android/llndk-versioning.h>
#else
#if !defined(__INTRODUCED_IN_LLNDK)
#define __INTRODUCED_IN_LLNDK(level) __attribute__((annotate("introduced_in_llndk=" #level)))
#endif
#endif  // __ANDROID_VENDOR__

#include <android/binder_ibinder.h>

__BEGIN_DECLS

/**
 * Writes a tracing message to indicate that the given section of code has begun. This call must be
 * followed by a corresponding call to {@link ABinderTrace_endSection} on the same thread. No info
 * about the provided binder is traced right now.
 *
 * Available since API level 36.
 */
void ABinderTrace_beginSection(const char* _Nonnull sectionName, const AIBinder* _Nonnull binder)
        __INTRODUCED_IN(36) __INTRODUCED_IN_LLNDK(202504);

/**
 * Writes a tracing message to indicate that a given section of code has ended. This call must be
 * preceded by a corresponding call to {@link ABinderTrace_beginSection} on the same thread. Calling
 * this method will mark the end of the most recently begun section of code, so care must be taken
 * to ensure that {@link ABinderTrace_beginSection}/{@link ABinderTrace_endSection} pairs are
 * properly nested and called from the same thread.
 *
 * Available since API level 36.
 */
void ABinderTrace_endSection() __INTRODUCED_IN(36) __INTRODUCED_IN_LLNDK(202504);

__END_DECLS