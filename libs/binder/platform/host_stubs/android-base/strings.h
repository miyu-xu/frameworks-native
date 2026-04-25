#pragma once

#include <string_view>

namespace android {
namespace base {

inline bool ConsumePrefix(std::string_view* str, std::string_view prefix) {
    if (str->substr(0, prefix.size()) == prefix) {
        str->remove_prefix(prefix.size());
        return true;
    }
    return false;
}

} // namespace base
} // namespace android
