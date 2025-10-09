#include "utils.h"

void writeString16(android::Parcel& parcel, const char* string)
{
    if (string != nullptr)
    {
        parcel.writeString16(android::String16(string));
    }
    else
    {
        parcel.writeInt32(-1);
    }
}

std::string getCmdline(pid_t pid)
{
    std::string content;
    const std::string path = "/proc/" + std::to_string(pid) + "/cmdline";
    return android::base::ReadFileToString(path, &content)
        ? content
        : "<" + path + ": " + strerror(errno) + ">";
}
