/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef FRAMEWORK_NATIVE_CMDS_IDLCLI_UTILS_H_
#define FRAMEWORK_NATIVE_CMDS_IDLCLI_UTILS_H_

#include <android/binder_enums.h>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace android {
namespace idlcli {

namespace overrides {

namespace details {

template <typename T>
inline std::istream &operator>>(std::istream &stream, T &out) {
    auto pos = stream.tellg();
    auto tmp = +out;
    auto min = +std::numeric_limits<T>::min();
    auto max = +std::numeric_limits<T>::max();
    stream >> tmp;
    if (!stream) {
        return stream;
    }
    if (tmp < min || tmp > max) {
        stream.seekg(pos);
        stream.setstate(std::ios_base::failbit);
        return stream;
    }
    out = tmp;
    return stream;
}

} // namespace details

// override for default behavior of treating as a character
inline std::istream &operator>>(std::istream &stream, int8_t &out) {
    return details::operator>>(stream, out);
}

// override for default behavior of treating as a character
inline std::istream &operator>>(std::istream &stream, uint8_t &out) {
    return details::operator>>(stream, out);
}

} // namespace overrides

template <typename T, typename R = ndk::enum_range<T>>
inline std::istream &operator>>(std::istream &stream, T &out) {
    using overrides::operator>>;
    auto validRange = R();
    auto pos = stream.tellg();
    std::underlying_type_t<T> in;
    T tmp;
    stream >> in;
    if (!stream) {
        return stream;
    }
    tmp = static_cast<T>(in);
    if (tmp < *validRange.begin() || tmp > *std::prev(validRange.end())) {
        stream.seekg(pos);
        stream.setstate(std::ios_base::failbit);
        return stream;
    }
    out = tmp;
    return stream;
}

inline std::string_view value_or_default(const std::string& str, std::string_view default_literal) {
    return str.empty() ? default_literal : std::string_view{str};
}

} // namespace idlcli
} // namespace android

#endif // FRAMEWORK_NATIVE_CMDS_IDLCLI_UTILS_H_
