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

#include <android/binder_api_level.h>
#include <android/binder_trace.h>

struct ScopedTrace {
    inline ScopedTrace(const char* name, const AIBinder* binder) {
        if API_LEVEL_AT_LEAST (36, 202504) {
            ABinderTrace_beginSection(name, binder);
        }
    }
    inline ScopedTrace(const char* name) {
        if API_LEVEL_AT_LEAST (36, 202504) {
            ABinderTrace_beginSection(name, nullptr);
        }
    }

    inline ~ScopedTrace() {
        if API_LEVEL_AT_LEAST (36, 202504) {
            ABinderTrace_endSection();
        }
    }
};