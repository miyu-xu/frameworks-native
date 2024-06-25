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

#include "ClientSideCacheList.h"

#include <unordered_set>

namespace android {
const std::unordered_set<std::u16string>& get_cachelist_set() {
    static const std::unordered_set<std::u16string>* cache_list =
            new std::unordered_set<std::u16string>({
                    std::u16string(u"permissionmgr"),
                    std::u16string(u"legacy_permission"),
                    std::u16string(u"media.resource_manager"),
            });
    return *cache_list;
}

bool ClientSideCacheList::allow_client_side_caching(std::u16string_view service_name) {
    return get_cachelist_set().contains(std::u16string(service_name));
}
} // namespace android