/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <functional>
#include <optional>

namespace android::binder {

// Copy of `android::base::function_ref`, see it for documentation.
template <class Signature>
class function_ref;

template <class Ret, class... Args>
class function_ref<Ret(Args...)> final {
public:
    constexpr function_ref() noexcept = delete;
    constexpr function_ref(const function_ref& other) noexcept = default;
    constexpr function_ref& operator=(const function_ref&) noexcept = default;

    using RawFunc = Ret(Args...);

    function_ref(RawFunc* funcptr) noexcept { *this = funcptr; }

    template <class Callable,
              class = std::enable_if_t<
                      std::is_invocable_r_v<Ret, Callable, Args...> &&
                      !std::is_same_v<function_ref, std::remove_reference_t<Callable>>>>
    function_ref(Callable&& c) noexcept {
        *this = std::forward<Callable>(c);
    }

    function_ref& operator=(RawFunc* funcptr) noexcept {
        mTypeErasedFunction = [](uintptr_t funcptr, Args... args) -> Ret {
            return (reinterpret_cast<RawFunc*>(funcptr))(std::forward<Args>(args)...);
        };
        mCallable = reinterpret_cast<uintptr_t>(funcptr);
        return *this;
    }

    template <class Callable,
              class = std::enable_if_t<
                      std::is_invocable_r_v<Ret, Callable, Args...> &&
                      !std::is_same_v<function_ref, std::remove_reference_t<Callable>>>>
    function_ref& operator=(Callable&& c) noexcept {
        mTypeErasedFunction = [](uintptr_t callable, Args... args) -> Ret {
            // Generate a lambda that remembers the type of the passed
            // |Callable|.
            return (*reinterpret_cast<std::remove_reference_t<Callable>*>(callable))(
                    std::forward<Args>(args)...);
        };
        mCallable = reinterpret_cast<uintptr_t>(&c);
        return *this;
    }

    Ret operator()(Args... args) const {
        return mTypeErasedFunction(mCallable, std::forward<Args>(args)...);
    }

private:
    using TypeErasedFunc = Ret(uintptr_t, Args...);
    TypeErasedFunc* mTypeErasedFunction;
    uintptr_t mCallable;
};

} // namespace android::binder

namespace android::binder::impl {

template <typename F>
struct scope_guard {
    std::optional<F> f;
    ~scope_guard() {
        if (f.has_value()) std::move(f.value())();
    }
    void release() { f.reset(); }
};

template <typename F>
scope_guard<F> make_scope_guard(F f) {
    return scope_guard<F>{.f = std::make_optional(std::move(f))};
}

} // namespace android::binder::impl
