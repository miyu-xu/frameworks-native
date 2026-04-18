#ifndef __WIN32_STDATOMIC_H
#define __WIN32_STDATOMIC_H
// Add atomic_bool type definition
#include <atomic>
using atomic_bool = std::atomic<bool>;

// C11 stdatomic.h compatibility for Windows
#include <stdint.h>

// Define atomic types for C compatibility
using atomic_int_least32_t = std::atomic<int32_t>;
using atomic_uint_least32_t = std::atomic<uint32_t>;
using atomic_int_least64_t = std::atomic<int64_t>;
using atomic_uint_least64_t = std::atomic<uint64_t>;

// Define memory order constants using a different naming convention to avoid conflicts
namespace windows_compat {
    constexpr std::memory_order mem_order_relaxed = std::memory_order_relaxed;
    constexpr std::memory_order mem_order_consume = std::memory_order_consume;
    constexpr std::memory_order mem_order_acquire = std::memory_order_acquire;
    constexpr std::memory_order mem_order_release = std::memory_order_release;
    constexpr std::memory_order mem_order_acq_rel = std::memory_order_acq_rel;
    constexpr std::memory_order mem_order_seq_cst = std::memory_order_seq_cst;
}

// Define macros with different names to avoid conflicts with std::memory_order_* constants
#define win_memory_order_relaxed windows_compat::mem_order_relaxed
#define win_memory_order_consume windows_compat::mem_order_consume
#define win_memory_order_acquire windows_compat::mem_order_acquire
#define win_memory_order_release windows_compat::mem_order_release
#define win_memory_order_acq_rel windows_compat::mem_order_acq_rel
#define win_memory_order_seq_cst windows_compat::mem_order_seq_cst

// Define atomic functions for C compatibility
template<typename T>
inline T atomic_load_explicit(const volatile std::atomic<T>* obj, std::memory_order order) {
    return obj->load(order);
}

template<typename T>
inline void atomic_store_explicit(volatile std::atomic<T>* obj, T desired, std::memory_order order) {
    obj->store(desired, order);
}

template<typename T>
inline T atomic_fetch_add_explicit(volatile std::atomic<T>* obj, T arg, std::memory_order order) {
    return obj->fetch_add(arg, order);
}

template<typename T>
inline T atomic_fetch_sub_explicit(volatile std::atomic<T>* obj, T arg, std::memory_order order) {
    return obj->fetch_sub(arg, order);
}

template<typename T>
inline T atomic_fetch_and_explicit(volatile std::atomic<T>* obj, T arg, std::memory_order order) {
    return obj->fetch_and(arg, order);
}

template<typename T>
inline T atomic_fetch_or_explicit(volatile std::atomic<T>* obj, T arg, std::memory_order order) {
    return obj->fetch_or(arg, order);
}

template<typename T>
inline T atomic_fetch_xor_explicit(volatile std::atomic<T>* obj, T arg, std::memory_order order) {
    return obj->fetch_xor(arg, order);
}

template<typename T>
inline bool atomic_compare_exchange_strong_explicit(volatile std::atomic<T>* obj, T* expected, T desired, std::memory_order success, std::memory_order failure) {
    return obj->compare_exchange_strong(*expected, desired, success, failure);
}

template<typename T>
inline bool atomic_compare_exchange_weak_explicit(volatile std::atomic<T>* obj, T* expected, T desired, std::memory_order success, std::memory_order failure) {
    return obj->compare_exchange_weak(*expected, desired, success, failure);
}

inline void atomic_thread_fence(std::memory_order order) {
    std::atomic_thread_fence(order);
}

inline void atomic_signal_fence(std::memory_order order) {
    std::atomic_signal_fence(order);
}
#endif // __WIN32_STDATOMIC_H