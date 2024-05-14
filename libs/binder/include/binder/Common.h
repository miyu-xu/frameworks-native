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

// https://www.gnu.org/software/gnulib/manual/html_node/Exported-Symbols-of-Shared-Libraries.html
#if BUILDING_LIBBINDER
# define LIBBINDER_EXPORTED __attribute__((__visibility__("default")))
#else
# define LIBBINDER_EXPORTED
#endif

// For stuff that is exported but probably shouldn't be.
//
// Needed, at least in part, because the test binaries are using internal
// headers and accessing these symbols directly.
#define LIBBINDER_INTERNAL_EXPORTED LIBBINDER_EXPORTED
