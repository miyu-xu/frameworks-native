#include <android-base/file.h>
#include <binder/Parcel.h>

void writeString16(android::Parcel& parcel, const char* string);

std::string getCmdline(pid_t pid);
