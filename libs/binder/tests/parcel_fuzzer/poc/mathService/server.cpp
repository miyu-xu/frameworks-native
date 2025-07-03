#include "server.h"

namespace aidl {
namespace android {
namespace test {
namespace math {

::ndk::ScopedAStatus mathService::multiply(int32_t in_a, int32_t in_b, int32_t* _aidl_return) {
    if(mCallBack) {
        mCallBack->multiply(in_a, in_b);
    }

    return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus mathService::setCallback(const std::shared_ptr<::aidl::android::test::math::IMathCallBack>& in_mathCallBack) {
    mCallBack = in_mathCallBack;

    return ndk::ScopedAStatus::ok();
}

}  // namespace math
}  // namespace test
}  // namespace android
}  // namespace aidl