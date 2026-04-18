#ifndef __WIN32_ANDROID_BASE_STRINGS_H__
#define __WIN32_ANDROID_BASE_STRINGS_H__
namespace android {
namespace base {
bool ConsumePrefix(std::string_view* str, std::string_view prefix) {
    if (str->substr(0, prefix.size()) == prefix) {
        str->remove_prefix(prefix.size());
        return true;
    }
    return false;
}
} // namespace base
} // namespace android
#endif // __WIN32_ANDROID_BASE_STRINGS_H__