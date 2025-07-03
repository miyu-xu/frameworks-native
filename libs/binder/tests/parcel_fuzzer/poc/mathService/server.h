#pragma once

#include <aidl/android/test/math/BnMath.h>
#include <aidl/android/test/math/IMathCallBack.h>

namespace aidl {
namespace android {
namespace test {
namespace math {

class mathService : public BnMath {
    public:
        ::ndk::ScopedAStatus multiply(int32_t in_a, int32_t in_b, int32_t* _aidl_return);
        ::ndk::ScopedAStatus setCallback(const std::shared_ptr<::aidl::android::test::math::IMathCallBack>& in_mathCallBack);

    private:
        std::shared_ptr<::aidl::android::test::math::IMathCallBack> mCallBack;
};

}  // namespace math
}  // namespace test
}  // namespace android
}  // namespace aidl