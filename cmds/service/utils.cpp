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
    return android::base::ReadFileToString("/proc/" + std::to_string(pid) + "/cmdline", &content) 
        ? std::string{content.c_str()} 
        : "";
}
